#include "medialistmodel.h"
#include "../../managers/artworkmanager.h"
#include "../../managers/libraryartworkmanager.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <services/admin/adminservice.h>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <QSet>
#include <QVector>
#include <QMutableHashIterator>
#include <QMutableSetIterator>
#include <algorithm>
#include <exception>
#include <utility>

namespace {




constexpr int kMaxConcurrentImageFetches = 4;
constexpr int kMaxConcurrentRatingFetches = 2;

QString buildImageIdentity(const MediaItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(item.getPrimaryImageId())
        .arg(item.images.primaryTag)
        .arg(item.images.thumbTag)
        .arg(item.images.backdropTag)
        .arg(item.images.logoTag)
        .arg(item.images.parentPrimaryTag)
        .arg(item.images.parentThumbTag)
        .arg(item.images.parentBackdropTag)
        .arg(item.images.parentImageItemId);
}

bool isResumeSourceForItem(const MediaItem& item, const QString& sourceItemId)
{
    const QString trimmedSourceItemId = sourceItemId.trimmed();
    if (trimmedSourceItemId.isEmpty()) {
        return false;
    }

    const QString trimmedResumeItemId = item.resumeItemId.trimmed();
    return item.hasResumeContext && !trimmedResumeItemId.isEmpty() &&
           trimmedResumeItemId == trimmedSourceItemId;
}

bool matchesItemOrResumeSource(const MediaItem& item, const QString& itemId)
{
    const QString trimmedItemId = itemId.trimmed();
    if (trimmedItemId.isEmpty()) {
        return false;
    }

    return item.id.trimmed() == trimmedItemId ||
           isResumeSourceForItem(item, trimmedItemId);
}

void preserveResumeContext(MediaItem& target, const MediaItem& source)
{
    if (target.hasResumeContext || !source.hasResumeContext) {
        return;
    }

    target.hasResumeContext = true;
    target.resumeItemId = source.resumeItemId;
    target.resumeUserData = source.resumeUserData;
}

MediaItem mergeItemUpdate(const MediaItem& currentItem,
                          const MediaItem& updatedItem)
{
    if (currentItem.id == updatedItem.id) {
        MediaItem mergedItem = updatedItem;
        preserveResumeContext(mergedItem, currentItem);
        return mergedItem;
    }

    if (isResumeSourceForItem(currentItem, updatedItem.id)) {
        MediaItem mergedItem = currentItem;
        mergedItem.resumeUserData = updatedItem.userData;
        return mergedItem;
    }

    return updatedItem;
}

} 

MediaListModel::MediaListModel(int imageMaxWidth, QEmbyCore* core, QObject *parent)
    : QAbstractListModel(parent), m_imageMaxWidth(imageMaxWidth), m_core(core)
{
    m_imageRequestContext = new QObject(this);
    setProperty("artworkPolicyEligible", true);
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this, [this](const QString& key) {
        if (!key.endsWith(QLatin1String(ConfigKeys::AutoArtworkLayout)) &&
            !key.endsWith(QLatin1String(ConfigKeys::ArtworkLayoutMode)) &&
            key != ConfigKeys::DefaultLibraryView &&
            !key.endsWith(QLatin1String(ConfigKeys::LibraryViewMode))) return;
        QList<int> rows;
        for (int row = 0; row < m_items.size(); ++row)
            if (m_priorityImageIds.contains(m_items[row].id)) rows.append(row);
        clearImageCache();
        setPriorityRows(rows);
        if (!m_items.isEmpty()) Q_EMIT dataChanged(index(0, 0), index(m_items.size() - 1, 0), {PosterPixmapRole, ArtworkLayoutRole});
    });
    connect(LibraryArtworkManager::instance(), &LibraryArtworkManager::policyChanged, this, [this]() {
        reportVisibleArtworkEvidence();
        if (!m_items.isEmpty()) Q_EMIT dataChanged(index(0, 0), index(m_items.size() - 1, 0), {ArtworkLayoutRole});
    }, Qt::QueuedConnection);
    connect(ArtworkManager::instance(), &ArtworkManager::frameChanged, this, [this](const QString& key) {
        if (m_imageRequestsSuspended) return;
        for (auto it = m_animationKeys.cbegin(); it != m_animationKeys.cend(); ++it) {
            if (it.value() == key && m_priorityImageIds.contains(it.key())) {
                const auto frame = ArtworkManager::instance()->currentFrame(key);
                if (!frame.isNull()) {
                    m_imageCache.insert(it.key(), QPixmap::fromImage(frame));
                    queueImageDataChanged(it.key());
                }
            }
        }
    });
    m_imageNotifyTimer = new QTimer(this);
    m_imageNotifyTimer->setSingleShot(true);
    m_imageNotifyTimer->setTimerType(Qt::PreciseTimer);
    connect(m_imageNotifyTimer, &QTimer::timeout, this,
            &MediaListModel::flushPendingImageDataChanges);
}

MediaListModel::~MediaListModel()
{
    ArtworkManager::instance()->unsubscribe(this);
    ++m_imageRequestGeneration;
    ++m_ratingRequestGeneration;
    m_loadingRatings.clear();
    m_pendingRatingIds.clear();
    m_loadingImages.clear();
    m_pendingImageRequests.clear();
    m_pendingImageOrder.clear();
    m_priorityImageIds.clear();
    m_activeImageFetches = 0;
    QObject* requestContext = m_imageRequestContext;
    m_imageRequestContext = nullptr;
    delete requestContext;
}

void MediaListModel::suspendImageRequests()
{
    ArtworkManager::instance()->unsubscribe(this);
    const int activeCount = m_activeImageFetches;
    const int pendingCount = m_pendingImageRequests.size();
    m_imageRequestsSuspended = true;
    ++m_imageRequestGeneration;
    ++m_ratingRequestGeneration;
    m_loadingRatings.clear();
    m_pendingRatingIds.clear();
    m_activeRatingFetches = 0;

    
    
    
    QObject* previousRequestContext = m_imageRequestContext;
    m_imageRequestContext = new QObject(this);
    delete previousRequestContext;

    m_loadingImages.clear();
    m_pendingImageRequests.clear();
    m_pendingImageOrder.clear();
    m_priorityImageIds.clear();
    m_pendingImageNotifyIds.clear();
    m_activeImageFetches = 0;
    if (m_imageNotifyTimer) {
        m_imageNotifyTimer->stop();
    }

    if (activeCount > 0 || pendingCount > 0) {
        qDebug() << "[MediaListModel] cancelled image request generation"
                 << "| active=" << activeCount
                 << "| pending=" << pendingCount
                 << "| cached=" << m_imageCache.size()
                 << "| generation=" << m_imageRequestGeneration;
    }
}

void MediaListModel::resumeImageRequests()
{
    m_imageRequestsSuspended = false;
}

void MediaListModel::clearImageCache()
{
    if (m_imageCache.isEmpty() && m_loadingImages.isEmpty() &&
        m_pendingImageRequests.isEmpty() && m_failedImageItems.isEmpty() &&
        m_activeImageFetches == 0) {
        return;
    }

    const bool hasRequests = !m_loadingImages.isEmpty() ||
                             !m_pendingImageRequests.isEmpty() ||
                             m_activeImageFetches > 0;
    if (hasRequests) {
        const bool wasSuspended = m_imageRequestsSuspended;
        suspendImageRequests();
        m_imageRequestsSuspended = wasSuspended;
    }
    m_imageCache.clear();
    ArtworkManager::instance()->unsubscribe(this);
    m_animationKeys.clear();
    m_failedImageItems.clear();
}

void MediaListModel::setPreferThumb(bool prefer)
{
    if (m_preferThumb == prefer) {
        return;
    }

    m_preferThumb = prefer;
    if (!m_items.isEmpty()) {
        clearImageCache();
    }
}

void MediaListModel::clearFailedImageItems()
{
    m_failedImageItems.clear();
}

int MediaListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant MediaListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size()) {
        return QVariant();
    }
    
    const MediaItem& item = m_items.at(index.row());

    if (role == ArtworkLayoutRole) {
        if (item.type == QLatin1String("MusicAlbum")) return int(ArtworkPolicyUtils::Layout::Square);
        auto* manager = LibraryArtworkManager::instance();
        return int(manager->layout(manager->libraryForItem(item)));
    }
    if (role == ItemDataRole) {
        if (item.communityRating > 0.0) {
            return QVariant::fromValue(item);
        }
        const double cachedRating = m_ratingCache.value(item.id, 0.0);
        if (cachedRating > 0.0) {
            MediaItem ratedItem = item;
            ratedItem.communityRating = cachedRating;
            return QVariant::fromValue(ratedItem);
        }
        const_cast<MediaListModel*>(this)->ensureRatingRequested(item);
        return QVariant::fromValue(item);
    }
    else if (role == Qt::ToolTipRole) {
        if (!ConfigStore::instance()->get<bool>(ConfigKeys::ShowMediaTooltips,
                                                true)) {
            return QVariant();
        }
        return buildTooltipText(item);
    }
    else if (role == PosterPixmapRole) {
        if (m_imageCache.contains(item.id)) {
            return m_imageCache.value(item.id);
        }

        
        const_cast<MediaListModel*>(this)->ensureImageRequested(item, true);
        return QVariant(); 
    }
    return QVariant();
}

void MediaListModel::ensureRatingRequested(const MediaItem& item)
{
    if (m_imageRequestsSuspended || item.id.isEmpty() ||
        m_ratingCache.contains(item.id) || m_loadingRatings.contains(item.id) ||
        !m_core || !m_core->mediaService() ||
        !ConfigStore::instance()->get<bool>(ConfigKeys::ShowCoverRating, true)) {
        return;
    }

    if (item.type != QLatin1String("Movie") &&
        item.type != QLatin1String("Series") &&
        item.type != QLatin1String("Season") &&
        item.type != QLatin1String("Episode") &&
        item.type != QLatin1String("Video") &&
        item.type != QLatin1String("MusicVideo")) {
        return;
    }

    m_loadingRatings.insert(item.id);
    m_pendingRatingIds.append(item.id);
    scheduleRatingFetches();
}

void MediaListModel::scheduleRatingFetches()
{
    if (!ConfigStore::instance()->get<bool>(ConfigKeys::ShowCoverRating, true)) {
        for (const QString& itemId : std::as_const(m_pendingRatingIds)) {
            m_loadingRatings.remove(itemId);
        }
        m_pendingRatingIds.clear();
        return;
    }
    while (!m_imageRequestsSuspended &&
           m_activeRatingFetches < kMaxConcurrentRatingFetches &&
           !m_pendingRatingIds.isEmpty()) {
        const QString itemId = m_pendingRatingIds.takeFirst();
        const bool stillPresent = std::any_of(
            m_items.cbegin(), m_items.cend(),
            [&itemId](const MediaItem& item) { return item.id == itemId; });
        if (!stillPresent) {
            m_loadingRatings.remove(itemId);
            continue;
        }
        ++m_activeRatingFetches;
        QCoro::connect(fetchRating(itemId, m_ratingRequestGeneration),
                       this, []() {});
    }
}

QCoro::Task<void> MediaListModel::fetchRating(QString itemId, int generation)
{
    QPointer<MediaListModel> safeThis(this);
    QPointer<MediaService> service(m_core ? m_core->mediaService() : nullptr);
    double rating = 0.0;
    try {
        if (service) {
            rating = co_await service->getItemCommunityRating(itemId);
        }
    } catch (const std::exception& error) {
        qWarning() << "[MediaListModel] rating request failed"
                   << "| itemId=" << itemId << "| error=" << error.what();
    }

    if (!safeThis || generation != safeThis->m_ratingRequestGeneration) {
        co_return;
    }
    safeThis->m_loadingRatings.remove(itemId);
    safeThis->m_activeRatingFetches =
        qMax(0, safeThis->m_activeRatingFetches - 1);
    safeThis->m_ratingCache.insert(itemId, rating);
    if (rating > 0.0) {
        for (int row = 0; row < safeThis->m_items.size(); ++row) {
            if (safeThis->m_items.at(row).id == itemId) {
                const QModelIndex itemIndex = safeThis->index(row, 0);
                Q_EMIT safeThis->dataChanged(itemIndex, itemIndex,
                                             {ItemDataRole});
                break;
            }
        }
    }
    safeThis->scheduleRatingFetches();
}

void MediaListModel::setImageMaxWidth(int maxWidth)
{
    const int normalizedWidth = qBound(96, maxWidth, 1024);
    if (normalizedWidth == m_imageMaxWidth) {
        return;
    }

    if (normalizedWidth > m_imageMaxWidth &&
        normalizedWidth <= m_imageMaxWidth * 6 / 5) {
        return;
    }

    const bool requiresHigherResolution = normalizedWidth > m_imageMaxWidth;
    m_imageMaxWidth = normalizedWidth;
    if (requiresHigherResolution && !m_items.isEmpty()) {
        clearImageCache();
    }
}

void MediaListModel::setForceNetworkImages(bool forceNetwork)
{
    if (m_forceNetworkImages == forceNetwork) {
        return;
    }

    m_forceNetworkImages = forceNetwork;
    if (!m_items.isEmpty()) {
        clearImageCache();
    }
}

QString MediaListModel::buildTooltipText(const MediaItem &item) const {
    QStringList lines;

    if ((item.type == QStringLiteral("Episode") ||
         item.type == QStringLiteral("Season")) &&
        !item.seriesName.isEmpty()) {
        lines << item.seriesName;
    }

    QString title = item.name.trimmed();
    if (item.type == QStringLiteral("Episode") && item.parentIndexNumber >= 0 &&
        item.indexNumber >= 0) {
        title = QStringLiteral("S%1E%2  %3")
                    .arg(item.parentIndexNumber, 2, 10, QChar('0'))
                    .arg(item.indexNumber, 2, 10, QChar('0'))
                    .arg(item.name);
    }
    lines << title;

    QStringList metaParts;
    if (item.type == QStringLiteral("Season") && item.parentIndexNumber >= 0) {
        metaParts << tr("Season %1").arg(item.parentIndexNumber);
    }
    if (item.productionYear > 0) {
        metaParts << QString::number(item.productionYear);
    }
    if (!item.premiereDate.trimmed().isEmpty()) {
        const QString trimmedDate = item.premiereDate.trimmed();
        const int len = trimmedDate.length();
        if (len > 0) metaParts << trimmedDate.left(qMin(len, 10));
    }
    if (item.runTimeTicks > 0) {
        const long long minutes = item.runTimeTicks / 10000000 / 60;
        if (minutes > 0) {
            metaParts << tr("%1 min").arg(minutes);
        }
    }
    if (item.type == QStringLiteral("Season") && item.recursiveItemCount > 0) {
        metaParts << tr("%1 Episodes").arg(item.recursiveItemCount);
    }
    if (!metaParts.isEmpty()) {
        lines << metaParts.join(QStringLiteral(" • "));
    }

    const QString overview = item.overview.simplified();
    if (!overview.isEmpty()) {
        lines << overview;
    }

    return lines.join(QLatin1Char('\n'));
}


void MediaListModel::setItems(const QList<MediaItem>& newItems) {
    LibraryArtworkManager::instance()->observeItems(newItems);
    if (m_items.isEmpty() || newItems.isEmpty()) {
        beginResetModel();
        m_items = newItems;
        if (newItems.isEmpty()) {
            m_artworkSizes.clear();
            clearImageCache();
            ++m_ratingRequestGeneration;
            m_ratingCache.clear();
            m_loadingRatings.clear();
            m_pendingRatingIds.clear();
            m_activeRatingFetches = 0;
        }
        endResetModel();
        return;
    }

    
    QSet<QString> newIds;
    for (const auto& item : newItems) {
        newIds.insert(item.id);
    }

    
    
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!newIds.contains(m_items[i].id)) {
            const QString removedItemId = m_items[i].id;
            beginRemoveRows(QModelIndex(), i, i);
            m_items.removeAt(i);
            endRemoveRows();
            invalidateItemImageRequest(removedItemId);
        }
    }

    
    for (int newIdx = 0; newIdx < newItems.size(); ++newIdx) {
        const MediaItem& newItem = newItems[newIdx];
        
        
        
        
        int oldIdx = -1;
        for (int j = newIdx; j < m_items.size(); ++j) {
            if (m_items[j].id == newItem.id) {
                oldIdx = j;
                break;
            }
        }

        if (oldIdx == -1) {
            
            beginInsertRows(QModelIndex(), newIdx, newIdx);
            m_items.insert(newIdx, newItem);
            endInsertRows();
        } 
        else if (oldIdx == newIdx) {
            
            const bool imageChanged =
                buildImageIdentity(m_items[newIdx]) !=
                buildImageIdentity(newItem);
            m_items[newIdx] = newItem;
            QVector<int> roles = {ItemDataRole};
            if (imageChanged) {
                invalidateItemImageRequest(newItem.id);
                roles.append(PosterPixmapRole);
            }
            Q_EMIT dataChanged(index(newIdx, 0), index(newIdx, 0), roles);
        } 
        else {
            
            
            beginMoveRows(QModelIndex(), oldIdx, oldIdx, QModelIndex(), newIdx);
            m_items.move(oldIdx, newIdx);
            endMoveRows();
            
            
            const bool imageChanged =
                buildImageIdentity(m_items[newIdx]) !=
                buildImageIdentity(newItem);
            m_items[newIdx] = newItem;
            QVector<int> roles = {ItemDataRole};
            if (imageChanged) {
                invalidateItemImageRequest(newItem.id);
                roles.append(PosterPixmapRole);
            }
            Q_EMIT dataChanged(index(newIdx, 0), index(newIdx, 0), roles);
        }
    }

    
    QMutableHashIterator<QString, QPixmap> it(m_imageCache);
    while (it.hasNext()) {
        it.next();
        if (!newIds.contains(it.key())) {
            it.remove();
        }
    }
    for (auto size = m_artworkSizes.begin(); size != m_artworkSizes.end();) {
        if (!newIds.contains(size.key())) size = m_artworkSizes.erase(size);
        else ++size;
    }
    QMutableSetIterator<QString> failedIt(m_failedImageItems);
    while (failedIt.hasNext()) {
        if (!newIds.contains(failedIt.next())) {
            failedIt.remove();
        }
    }
    QMutableHashIterator<QString, double> ratingIt(m_ratingCache);
    while (ratingIt.hasNext()) {
        ratingIt.next();
        if (!newIds.contains(ratingIt.key())) {
            ratingIt.remove();
        }
    }
}

ArtworkPolicyUtils::State MediaListModel::loadedArtworkState() const
{
    QHash<QString, ArtworkPolicyUtils::Evidence> evidence;
    for (const auto& item : m_items) {
        if (item.type == QLatin1String("Person") || item.type == QLatin1String("Episode") ||
            item.type == QLatin1String("Season") || item.type == QLatin1String("TvChannel")) continue;
        const auto cover = item.images.bestPoster();
        ArtworkPolicyUtils::Evidence entry;
        entry.identity = buildImageIdentity(item);
        entry.hasImage = !cover.first.isEmpty();
        entry.requested = m_priorityImageIds.contains(item.id);
        const auto observed = m_artworkSizes.constFind(item.id);
        if (observed != m_artworkSizes.cend() && observed->first == entry.identity) {
            entry.size = observed->second;
            entry.resolved = entry.size.isValid();
        }
        evidence.insert(item.id, entry);
    }
    return ArtworkPolicyUtils::classify(evidence, false);
}

MediaItem MediaListModel::getItem(const QModelIndex& index) const {
    if (!index.isValid() || index.row() >= m_items.size()) {
        return MediaItem();
    }
    return m_items.at(index.row());
}

void MediaListModel::updateItem(const MediaItem& updatedItem) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (matchesItemOrResumeSource(m_items[i], updatedItem.id)) {
            const QString cachedItemId = m_items[i].id;
            const MediaItem mergedItem = mergeItemUpdate(m_items[i], updatedItem);
            const bool imageChanged =
                buildImageIdentity(m_items[i]) != buildImageIdentity(mergedItem);

            
            m_items[i] = mergedItem;

            if (imageChanged) {
                invalidateItemImageRequest(cachedItemId);
            }
            
            
            QModelIndex idx = index(i, 0);
            QVector<int> roles = {ItemDataRole};
            if (imageChanged) {
                roles.append(PosterPixmapRole);
            }
            Q_EMIT dataChanged(idx, idx, roles);
            break; 
        }
    }
}

void MediaListModel::prependOrUpdateItem(const MediaItem& item, int maxItems)
{
    const QString itemId = item.id.trimmed();
    if (itemId.isEmpty()) {
        return;
    }

    for (int i = 0; i < m_items.size(); ++i) {
        if (matchesItemOrResumeSource(m_items[i], itemId)) {
            updateItem(item);
            return;
        }
    }

    beginInsertRows(QModelIndex(), 0, 0);
    m_items.prepend(item);
    endInsertRows();

    if (maxItems > 0) {
        while (m_items.size() > maxItems) {
            const int removeIndex = m_items.size() - 1;
            const QString cachedItemId = m_items.at(removeIndex).id;
            beginRemoveRows(QModelIndex(), removeIndex, removeIndex);
            m_items.removeAt(removeIndex);
            endRemoveRows();
            invalidateItemImageRequest(cachedItemId);
        }
    }
}




void MediaListModel::removeItem(const QString& itemId) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (matchesItemOrResumeSource(m_items[i], itemId)) {
            const QString cachedItemId = m_items[i].id;
            
            beginRemoveRows(QModelIndex(), i, i);
            
            
            m_items.removeAt(i);
            
            
            endRemoveRows();

            
            invalidateItemImageRequest(cachedItemId);
            if (itemId != cachedItemId) invalidateItemImageRequest(itemId);
            
            break; 
        }
    }
}

void MediaListModel::reportVisibleArtworkEvidence()
{
    auto* manager = LibraryArtworkManager::instance();
    if (!manager->enabled() || !property("artworkPolicyEligible").toBool()) return;
    QList<MediaItem> visible;
    for (const auto& item : m_items)
        if (m_priorityImageIds.contains(item.id)) visible.append(item);
    manager->requestEvidence(visible);
    for (const auto& item : visible) {
        const auto identity = m_imageEvidence.constFind(item.id);
        const auto pix = m_imageCache.constFind(item.id);
        if (identity != m_imageEvidence.cend() && pix != m_imageCache.cend())
            manager->observeImage(item, identity->first, identity->second, pix->size());
    }
}

void MediaListModel::setPriorityRows(const QList<int>& rows)
{
    QStringList priorityIds;
    priorityIds.reserve(rows.size());

    for (const int row : rows) {
        if (row < 0 || row >= m_items.size()) {
            continue;
        }

        const MediaItem& item = m_items.at(row);
        if (item.id.isEmpty() || priorityIds.contains(item.id)) {
            continue;
        }

        priorityIds.append(item.id);
    }

    m_priorityImageIds = priorityIds;
    reportVisibleArtworkEvidence();
    ArtworkManager::instance()->unsubscribe(this);
    for (const auto& id : priorityIds)
        ArtworkManager::instance()->subscribe(m_animationKeys.value(id), this, !m_imageRequestsSuspended);

    for (auto it = m_pendingImageRequests.begin();
         it != m_pendingImageRequests.end(); ++it) {
        it->highPriority = m_priorityImageIds.contains(it.key());
    }

    for (const int row : rows) {
        if (row < 0 || row >= m_items.size()) {
            continue;
        }
        ensureImageRequested(m_items.at(row), true);
    }

    scheduleImageFetches();
}

void MediaListModel::ensureImageRequested(const MediaItem& item,
                                          bool highPriority)
{
    if (m_imageRequestsSuspended) {
        return;
    }

    if (highPriority && m_pendingImageRequests.contains(item.id)) {
        m_pendingImageRequests[item.id].highPriority = true;
    }

    if (item.id.isEmpty() || m_imageCache.contains(item.id) ||
        m_loadingImages.contains(item.id) ||
        m_failedImageItems.contains(item.id)) {
        return;
    }

    PendingImageRequest request;
    request.highPriority = highPriority;
    request.imageIdentity = buildImageIdentity(item);
    QSet<QString> seenCandidates;

    auto appendCandidate = [&](const QString& tag, const QString& type) {
        const QString trimmedTag = tag.trimmed();
        const QString trimmedType = type.trimmed();
        if (trimmedTag.isEmpty() || trimmedType.isEmpty()) {
            return;
        }

        QString targetImageId = item.getPrimaryImageId();
        if (item.images.isParentTag(trimmedTag) &&
            !item.images.parentImageItemId.isEmpty()) {
            targetImageId = item.images.parentImageItemId;
        }

        if (targetImageId.trimmed().isEmpty()) {
            return;
        }

        const QString candidateKey =
            QStringLiteral("%1|%2|%3")
                .arg(targetImageId, trimmedType, trimmedTag);
        if (seenCandidates.contains(candidateKey)) {
            return;
        }
        seenCandidates.insert(candidateKey);

        ImageCandidate candidate;
        candidate.targetImageId = targetImageId;
        candidate.imageType = trimmedType;
        candidate.imageTag = trimmedTag;
        candidate.maxWidth = m_imageMaxWidth;
        request.candidates.append(candidate);
    };

    const bool adaptiveImages =
        ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages, true);

    auto* artwork = LibraryArtworkManager::instance();
    const QString library = artwork->libraryForItem(item);
    const bool policyEligible = property("artworkPolicyEligible").toBool() && !library.isEmpty() &&
        (item.type == "Movie" || item.type == "Series" || item.type == "Video" || item.type == "BoxSet");
    const auto layout = policyEligible ? artwork->layout(library) : ArtworkPolicyUtils::Layout::Default;
    if (property("automaticPageArtwork").toBool()) {
        const auto cover = item.images.bestPoster();
        appendCandidate(cover.first, cover.second);
    }
    if (policyEligible && artwork->enabled() && !artwork->hasManualOverride(library))
        appendCandidate(item.images.primaryTag, QStringLiteral("Primary"));
    const bool preferThumb = layout == ArtworkPolicyUtils::Layout::Portrait ? false :
        layout == ArtworkPolicyUtils::Layout::Landscape ? true : m_preferThumb;
    if (preferThumb) {
        if (adaptiveImages) {
            appendCandidate(item.images.thumbTag, QStringLiteral("Thumb"));
            appendCandidate(item.images.backdropTag, QStringLiteral("Backdrop"));
            appendCandidate(item.images.primaryTag, QStringLiteral("Primary"));
            appendCandidate(item.images.parentBackdropTag,
                            QStringLiteral("Backdrop"));
            appendCandidate(item.images.parentThumbTag, QStringLiteral("Thumb"));
            appendCandidate(item.images.parentPrimaryTag,
                            QStringLiteral("Primary"));
        } else {
            appendCandidate(item.images.thumbTag, QStringLiteral("Thumb"));
            appendCandidate(item.images.backdropTag, QStringLiteral("Backdrop"));
        }
    } else if (adaptiveImages) {
        appendCandidate(item.images.primaryTag, QStringLiteral("Primary"));
        appendCandidate(item.images.thumbTag, QStringLiteral("Thumb"));
        appendCandidate(item.images.backdropTag, QStringLiteral("Backdrop"));
        appendCandidate(item.images.parentPrimaryTag, QStringLiteral("Primary"));
        appendCandidate(item.images.parentBackdropTag,
                        QStringLiteral("Backdrop"));
    } else {
        appendCandidate(item.images.primaryTag, QStringLiteral("Primary"));
    }

    if (request.candidates.isEmpty()) {
        
        
        
        m_failedImageItems.insert(item.id);
        qDebug() << "[MediaListModel] image request skipped: no image metadata"
                 << "| itemId=" << item.id
                 << "| itemType=" << item.type
                 << "| primaryImageItemId="
                 << item.images.primaryImageItemId
                 << "| hasPrimary=" << !item.images.primaryTag.isEmpty()
                 << "| hasThumb=" << !item.images.thumbTag.isEmpty()
                 << "| hasBackdrop=" << !item.images.backdropTag.isEmpty()
                 << "| hasParentPrimary="
                 << !item.images.parentPrimaryTag.isEmpty();
        return;
    }

    const ImageCandidate& first = request.candidates.first();
    qDebug() << "[MediaListModel] image request queued"
             << "| itemId=" << item.id
             << "| candidateCount=" << request.candidates.size()
             << "| firstTargetImageId=" << first.targetImageId
             << "| firstImageType=" << first.imageType
             << "| firstHasTag=" << !first.imageTag.isEmpty();

    enqueueImageFetch(item.id, request);
}

void MediaListModel::enqueueImageFetch(const QString& itemId,
                                       const PendingImageRequest& request)
{
    if (itemId.isEmpty() || request.candidates.isEmpty() ||
        m_imageCache.contains(itemId) || m_loadingImages.contains(itemId)) {
        return;
    }

    m_loadingImages.insert(itemId);
    m_pendingImageRequests.insert(itemId, request);
    m_pendingImageOrder.append(itemId);
    scheduleImageFetches();
}

void MediaListModel::enqueueImageRetry(const QString& itemId,
                                       const PendingImageRequest& request)
{
    if (itemId.isEmpty() || request.candidates.isEmpty() ||
        request.candidateIndex < 0 ||
        request.candidateIndex >= request.candidates.size() ||
        m_imageCache.contains(itemId)) {
        return;
    }

    m_loadingImages.insert(itemId);
    m_pendingImageRequests.insert(itemId, request);
    m_pendingImageOrder.removeAll(itemId);
    m_pendingImageOrder.prepend(itemId);
    scheduleImageFetches();
}

void MediaListModel::scheduleImageFetches()
{
    while (m_activeImageFetches < kMaxConcurrentImageFetches &&
           !m_pendingImageRequests.isEmpty()) {
        const QString itemId = takeNextPendingImageId();
        if (itemId.isEmpty()) {
            return;
        }

        const PendingImageRequest request = m_pendingImageRequests.take(itemId);
        m_pendingImageOrder.removeAll(itemId);
        ++m_activeImageFetches;

        QPointer<MediaListModel> safeThis(this);
        QCoro::connect(
            executeImageFetch(safeThis, itemId, request,
                              m_imageRequestGeneration, m_core),
            this, []() {});
    }
}

QString MediaListModel::takeNextPendingImageId()
{
    for (const QString& itemId : std::as_const(m_priorityImageIds)) {
        if (m_pendingImageRequests.contains(itemId)) {
            return itemId;
        }
    }

    while (!m_pendingImageOrder.isEmpty()) {
        const QString itemId = m_pendingImageOrder.takeFirst();
        if (m_pendingImageRequests.contains(itemId)) {
            return itemId;
        }
    }

    if (!m_pendingImageRequests.isEmpty()) {
        return m_pendingImageRequests.constBegin().key();
    }

    return {};
}


QCoro::Task<void> MediaListModel::executeImageFetch(
    QPointer<MediaListModel> safeThis, QString itemId,
    PendingImageRequest request, int generation, QEmbyCore* core) {
    try {
        if (request.candidateIndex < 0 ||
            request.candidateIndex >= request.candidates.size() || !safeThis ||
            !core ||
            !core->mediaService()) {
            if (safeThis && generation == safeThis->m_imageRequestGeneration) {
                safeThis->m_loadingImages.remove(itemId);
                safeThis->m_activeImageFetches =
                    qMax(0, safeThis->m_activeImageFetches - 1);
                safeThis->scheduleImageFetches();
            }
            co_return;
        }

        const ImageCandidate candidate =
            request.candidates.at(request.candidateIndex);
        QPointer<QObject> requestContext(safeThis->m_imageRequestContext);
        const ImageFetchPolicy fetchPolicy =
            safeThis->m_forceNetworkImages
                ? ImageFetchPolicy::NetworkOnly
                : ImageFetchPolicy::CachePreferred;

        
        const auto asset = co_await ArtworkManager::instance()->fetch(core->mediaService(), core->adminService(),
            candidate.targetImageId, candidate.imageType, candidate.imageTag,
            candidate.maxWidth,
            request.highPriority ? ImageRequestPriority::High
                                 : ImageRequestPriority::Normal,
            requestContext.data(), fetchPolicy);
        QPixmap pix = QPixmap::fromImage(asset.preview);
        
        
        if (!safeThis) {
            co_return; 
        }

        auto finishFetch = [safeThis, generation]() {
            if (!safeThis) {
                return;
            }
            if (generation == safeThis->m_imageRequestGeneration) {
                safeThis->m_activeImageFetches =
                    qMax(0, safeThis->m_activeImageFetches - 1);
            }
            safeThis->scheduleImageFetches();
        };

        if (generation != safeThis->m_imageRequestGeneration) {
            finishFetch();
            co_return;
        }

        if (!safeThis->isImageRequestCurrent(itemId, request)) {
            qDebug() << "[MediaListModel] discarded stale image result"
                     << "| itemId=" << itemId
                     << "| candidateIndex=" << request.candidateIndex;
            finishFetch();
            co_return;
        }

        if (pix.isNull() && request.transientRetryCount == 0) {
            
            
            
            
            
            PendingImageRequest retryRequest = request;
            ++retryRequest.transientRetryCount;
            qDebug() << "[MediaListModel] retrying transient empty image result"
                     << "| itemId=" << itemId
                     << "| candidateIndex=" << request.candidateIndex
                     << "| generation=" << generation;
            QTimer::singleShot(
                250, safeThis.data(),
                [safeThis, itemId, retryRequest, generation]() {
                    if (!safeThis ||
                        generation != safeThis->m_imageRequestGeneration ||
                        safeThis->m_imageRequestsSuspended ||
                        !safeThis->isImageRequestCurrent(itemId,
                                                         retryRequest)) {
                        if (safeThis &&
                            generation == safeThis->m_imageRequestGeneration) {
                            safeThis->m_loadingImages.remove(itemId);
                        }
                        return;
                    }
                    safeThis->enqueueImageRetry(itemId, retryRequest);
                });
            finishFetch();
            co_return;
        }

        if (pix.isNull()) {
            safeThis->retryNextImageCandidate(itemId, request, generation);
            finishFetch();
            co_return;
        }

        
        safeThis->m_imageCache.insert(itemId, pix);
        safeThis->m_imageEvidence.insert(itemId, {candidate.imageType, candidate.imageTag});
        if (asset.animated) {
            safeThis->m_animationKeys.insert(itemId, asset.key);
            ArtworkManager::instance()->subscribe(asset.key, safeThis.data(),
                safeThis->m_priorityImageIds.contains(itemId) && !safeThis->m_imageRequestsSuspended);
        }
        safeThis->m_loadingImages.remove(itemId);

        
        int currentRow = -1;
        for (int i = 0; i < safeThis->m_items.size(); ++i) {
            if (safeThis->m_items.at(i).id == itemId) {
                currentRow = i;
                break;
            }
        }

        
        
        if (currentRow >= 0) {
            const auto cover = safeThis->m_items.at(currentRow).images.bestPoster();
            if (candidate.imageType == cover.second && candidate.imageTag == cover.first) {
                const QPair<QString, QSize> observed{request.imageIdentity, asset.size};
                if (safeThis->m_artworkSizes.value(itemId) != observed) {
                    safeThis->m_artworkSizes.insert(itemId, observed);
                    Q_EMIT safeThis->artworkEvidenceChanged();
                }
            }
            LibraryArtworkManager::instance()->observeImage(safeThis->m_items.at(currentRow),
                candidate.imageType, candidate.imageTag, asset.size);
            safeThis->queueImageDataChanged(itemId);
        }
        finishFetch();
    } catch (const std::exception& e) {
        qWarning() << "[MediaListModel] image request threw exception"
                   << "| itemId=" << itemId
                   << "| candidateIndex=" << request.candidateIndex
                   << "| error=" << e.what();
        if (safeThis) {
            if (generation == safeThis->m_imageRequestGeneration) {
                if (safeThis->isImageRequestCurrent(itemId, request)) {
                    safeThis->retryNextImageCandidate(itemId, request,
                                                      generation);
                }
                safeThis->m_activeImageFetches =
                    qMax(0, safeThis->m_activeImageFetches - 1);
            }
            safeThis->scheduleImageFetches();
        }
    } catch (...) {
        qWarning() << "[MediaListModel] image request threw unknown exception"
                   << "| itemId=" << itemId
                   << "| candidateIndex=" << request.candidateIndex;
        if (safeThis) {
            if (generation == safeThis->m_imageRequestGeneration) {
                if (safeThis->isImageRequestCurrent(itemId, request)) {
                    safeThis->retryNextImageCandidate(itemId, request,
                                                      generation);
                }
                safeThis->m_activeImageFetches =
                    qMax(0, safeThis->m_activeImageFetches - 1);
            }
            safeThis->scheduleImageFetches();
        }
    }
}

void MediaListModel::retryNextImageCandidate(const QString& itemId,
                                             PendingImageRequest request,
                                             int generation)
{
    if (generation != m_imageRequestGeneration ||
        !isImageRequestCurrent(itemId, request)) {
        return;
    }

    const int previousIndex = request.candidateIndex;
    ++request.candidateIndex;
    if (request.candidateIndex >= request.candidates.size()) {
        
        
        
        
        m_loadingImages.remove(itemId);
        m_failedImageItems.insert(itemId);
        qWarning() << "[MediaListModel] all image candidates exhausted"
                   << "| itemId=" << itemId
                   << "| candidateCount=" << request.candidates.size()
                   << "| retrySuppressedUntilRefresh=" << true;
        return;
    }

    const ImageCandidate& next = request.candidates.at(request.candidateIndex);
    qWarning() << "[MediaListModel] retrying fallback image candidate"
               << "| itemId=" << itemId
               << "| failedCandidateIndex=" << previousIndex
               << "| nextCandidateIndex=" << request.candidateIndex
               << "| nextTargetImageId=" << next.targetImageId
               << "| nextImageType=" << next.imageType
               << "| nextHasTag=" << !next.imageTag.isEmpty();
    enqueueImageRetry(itemId, request);
}

bool MediaListModel::isImageRequestCurrent(
    const QString& itemId, const PendingImageRequest& request) const
{
    for (const MediaItem& item : m_items) {
        if (item.id == itemId) {
            return buildImageIdentity(item) == request.imageIdentity;
        }
    }
    return false;
}

void MediaListModel::invalidateItemImageRequest(const QString& itemId)
{
    if (m_artworkSizes.remove(itemId)) Q_EMIT artworkEvidenceChanged();
    m_imageEvidence.remove(itemId);
    const QString animation = m_animationKeys.take(itemId);
    if (!m_animationKeys.values().contains(animation))
        ArtworkManager::instance()->subscribe(animation, this, false);
    m_imageCache.remove(itemId);
    m_loadingImages.remove(itemId);
    m_failedImageItems.remove(itemId);
    m_pendingImageRequests.remove(itemId);
    m_pendingImageOrder.removeAll(itemId);
    m_pendingImageNotifyIds.remove(itemId);
}

void MediaListModel::queueImageDataChanged(const QString& itemId)
{
    if (itemId.isEmpty()) {
        return;
    }

    m_pendingImageNotifyIds.insert(itemId);
    if (m_imageNotifyTimer && !m_imageNotifyTimer->isActive()) {
        m_imageNotifyTimer->start(16);
    }
}

void MediaListModel::flushPendingImageDataChanges()
{
    if (m_pendingImageNotifyIds.isEmpty()) {
        return;
    }

    QStringList itemIds = m_pendingImageNotifyIds.values();
    m_pendingImageNotifyIds.clear();

    QList<int> rows;
    rows.reserve(itemIds.size());
    for (const QString& itemId : std::as_const(itemIds)) {
        for (int row = 0; row < m_items.size(); ++row) {
            if (m_items.at(row).id == itemId) {
                rows.append(row);
                break;
            }
        }
    }

    std::sort(rows.begin(), rows.end());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());

    for (const int row : std::as_const(rows)) {
        if (row < 0 || row >= m_items.size()) {
            continue;
        }
        Q_EMIT dataChanged(index(row, 0), index(row, 0), {PosterPixmapRole});
    }
}
