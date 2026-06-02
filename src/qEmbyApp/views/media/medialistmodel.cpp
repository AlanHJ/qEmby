#include "medialistmodel.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <QSet>
#include <QVector>
#include <QMutableHashIterator>
#include <algorithm>
#include <utility>

namespace {

constexpr int kMaxConcurrentImageFetches = 6;

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
    m_imageNotifyTimer = new QTimer(this);
    m_imageNotifyTimer->setSingleShot(true);
    m_imageNotifyTimer->setTimerType(Qt::PreciseTimer);
    connect(m_imageNotifyTimer, &QTimer::timeout, this,
            &MediaListModel::flushPendingImageDataChanges);
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

    if (role == ItemDataRole) {
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

        const_cast<MediaListModel*>(this)->ensureImageRequested(item);
        return QVariant(); 
    }
    return QVariant();
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
    if (m_items.isEmpty() || newItems.isEmpty()) {
        beginResetModel();
        m_items = newItems;
        if (newItems.isEmpty()) {
            clearImageCache();
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
            beginRemoveRows(QModelIndex(), i, i);
            m_items.removeAt(i);
            endRemoveRows();
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
            
            m_items[newIdx] = newItem;
            Q_EMIT dataChanged(index(newIdx, 0), index(newIdx, 0), {ItemDataRole});
        } 
        else {
            
            
            beginMoveRows(QModelIndex(), oldIdx, oldIdx, QModelIndex(), newIdx);
            m_items.move(oldIdx, newIdx);
            endMoveRows();
            
            
            m_items[newIdx] = newItem;
            Q_EMIT dataChanged(index(newIdx, 0), index(newIdx, 0), {ItemDataRole});
        }
    }

    
    QMutableHashIterator<QString, QPixmap> it(m_imageCache);
    while (it.hasNext()) {
        it.next();
        if (!newIds.contains(it.key())) {
            it.remove();
        }
    }
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
                m_imageCache.remove(cachedItemId);
                m_loadingImages.remove(cachedItemId);
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
            m_imageCache.remove(cachedItemId);
            m_loadingImages.remove(cachedItemId);
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

            
            m_imageCache.remove(cachedItemId);
            m_loadingImages.remove(cachedItemId);
            m_imageCache.remove(itemId);
            m_loadingImages.remove(itemId);
            
            break; 
        }
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
        ensureImageRequested(item);
    }

    m_priorityImageIds = priorityIds;
    scheduleImageFetches();
}

void MediaListModel::ensureImageRequested(const MediaItem& item)
{
    if (item.id.isEmpty() || m_imageCache.contains(item.id) ||
        m_loadingImages.contains(item.id)) {
        return;
    }

    QString imgType = "Primary";
    QString imgTag = item.images.primaryTag;

    if (m_preferThumb) {
        if (ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages, true)) {
            auto best = item.images.bestThumb();
            imgTag = best.first;
            imgType = best.second;
        } else {
            if (!item.images.thumbTag.isEmpty()) {
                imgType = "Thumb";
                imgTag = item.images.thumbTag;
            } else if (!item.images.backdropTag.isEmpty()) {
                imgType = "Backdrop";
                imgTag = item.images.backdropTag;
            }
        }
    } else if (imgTag.isEmpty() &&
               ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages,
                                                  true)) {
        auto best = item.images.bestPoster();
        imgTag = best.first;
        imgType = best.second;
    }

    QString targetImageId = item.getPrimaryImageId();
    bool usingParentImage = false;
    if (!imgTag.isEmpty() && item.images.isParentTag(imgTag) &&
        !item.images.parentImageItemId.isEmpty()) {
        targetImageId = item.images.parentImageItemId;
        usingParentImage = true;
    }

    if (imgTag.isEmpty()) {
        return;
    }

    PendingImageRequest request;
    request.targetImageId = targetImageId;
    request.imageType = imgType;
    request.imageTag = imgTag;
    request.maxWidth = usingParentImage ? m_imageMaxWidth * 2 : m_imageMaxWidth;
    enqueueImageFetch(item.id, request);
}

void MediaListModel::enqueueImageFetch(const QString& itemId,
                                       const PendingImageRequest& request)
{
    if (itemId.isEmpty() || m_imageCache.contains(itemId) ||
        m_loadingImages.contains(itemId)) {
        return;
    }

    m_loadingImages.insert(itemId);
    m_pendingImageRequests.insert(itemId, request);
    m_pendingImageOrder.append(itemId);
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
        executeImageFetch(safeThis, itemId, request.targetImageId,
                          request.imageType, request.imageTag,
                          request.maxWidth, m_imageRequestGeneration, m_core);
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
    QPointer<MediaListModel> safeThis, QString itemId, QString targetImageId,
    QString imgType, QString imgTag, int maxWidth, int generation,
    QEmbyCore* core) {
    try {
        
        QPixmap pix = co_await core->mediaService()->fetchImage(targetImageId, imgType, imgTag, maxWidth);
        
        
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

        
        safeThis->m_imageCache.insert(itemId, pix);
        safeThis->m_loadingImages.remove(itemId);

        
        int currentRow = -1;
        for (int i = 0; i < safeThis->m_items.size(); ++i) {
            if (safeThis->m_items.at(i).id == itemId) {
                currentRow = i;
                break;
            }
        }

        
        
        if (currentRow >= 0) {
            safeThis->queueImageDataChanged(itemId);
        }
        finishFetch();
    } catch (...) {
        
        if (safeThis) {
            if (generation == safeThis->m_imageRequestGeneration) {
                safeThis->m_loadingImages.remove(itemId);
                safeThis->m_activeImageFetches =
                    qMax(0, safeThis->m_activeImageFetches - 1);
            }
            safeThis->scheduleImageFetches();
        }
    }
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
