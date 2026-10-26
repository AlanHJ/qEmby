#include "libraryartworkmanager.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <services/manager/servermanager.h>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <QDebug>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>

LibraryArtworkManager* LibraryArtworkManager::instance() { static LibraryArtworkManager manager; return &manager; }
LibraryArtworkManager::LibraryArtworkManager()
{
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(750);
    connect(&m_saveTimer, &QTimer::timeout, this, &LibraryArtworkManager::saveCache);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &LibraryArtworkManager::saveCache);
    connect(this, &LibraryArtworkManager::policyChanged, this, [this]() { m_saveTimer.start(); });
    connect(this, &LibraryArtworkManager::scanProgress, this, [this]() { m_saveTimer.start(); });
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this, [this](const QString& key) {
        if (!m_core) return;
        if (key == ConfigKeys::forServer(serverId(), ConfigKeys::AutoArtworkLayout) ||
            key == ConfigKeys::forServer(serverId(), ConfigKeys::ArtworkLayoutMode) ||
            key == ConfigKeys::DefaultLibraryView ||
            key == ConfigKeys::forServer(serverId(), ConfigKeys::ScanLibraryArtwork) ||
            key == ConfigKeys::forServer(serverId(), ConfigKeys::ScanLibraryArtworkEndpoint) ||
            key.endsWith(QLatin1String(ConfigKeys::LibraryViewMode))) configure();
    });
}
QString LibraryArtworkManager::cachePath() const
{
    const QString root = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/artwork-policy-v1";
    QDir().mkpath(root);
    return root + '/' + QString::fromLatin1(QCryptographicHash::hash(m_identity.toUtf8(), QCryptographicHash::Sha256).toHex()) + ".json";
}
void LibraryArtworkManager::saveCache()
{
    if (m_identity.isEmpty() || m_libraries.isEmpty()) return;
    QJsonObject libraries;
    for (auto it = m_libraries.cbegin(); it != m_libraries.cend(); ++it) {
        QJsonObject entries;
        for (auto entry = it->evidence.cbegin(); entry != it->evidence.cend(); ++entry)
            entries.insert(entry.key(), QJsonArray{entry->identity, entry->size.width(), entry->size.height(), entry->hasImage, entry->resolved});
        libraries.insert(it.key(), QJsonObject{{"items", entries}, {"nextOffset", it->nextOffset},
            {"complete", it->complete}, {"verifiedAt", it->verifiedAt.toString(Qt::ISODate)}});
    }
    const QByteArray bytes = QJsonDocument(QJsonObject{{"version", 1},
        {"savedAt", QDateTime::currentDateTimeUtc().toString(Qt::ISODate)}, {"libraries", libraries}}).toJson(QJsonDocument::Compact);
    if (bytes.size() > 16 * 1024 * 1024) { qWarning() << "[LibraryArtworkManager] Policy cache exceeds disk budget"; return; }
    QSaveFile file(cachePath());
    if (file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size()) file.commit();
}
void LibraryArtworkManager::loadCache()
{
    QFile file(cachePath());
    if (!file.open(QIODevice::ReadOnly) || file.size() > 16 * 1024 * 1024) return;
    const auto root = QJsonDocument::fromJson(file.readAll()).object();
    if (root["version"].toInt() != 1) return;
    const auto saved = QDateTime::fromString(root["savedAt"].toString(), Qt::ISODate);
    if (!saved.isValid() || saved.secsTo(QDateTime::currentDateTimeUtc()) > 24 * 3600) return;
    const auto libraries = root["libraries"].toObject();
    for (auto it = libraries.begin(); it != libraries.end(); ++it) {
        Library library;
        const auto object = it.value().toObject();
        const auto items = object["items"].toObject();
        for (auto entry = items.begin(); entry != items.end(); ++entry) {
            const auto values = entry.value().toArray();
            if (values.size() != 5) continue;
            const QSize size(values[1].toInt(), values[2].toInt());
            library.evidence.insert(entry.key(), {values[0].toString(), size, values[3].toBool(), values[4].toBool() && size.isValid()});
            m_owners.insert(entry.key(), it.key());
        }
        library.nextOffset = qMax(0, object["nextOffset"].toInt());
        library.complete = object["complete"].toBool();
        library.verifiedAt = QDateTime::fromString(object["verifiedAt"].toString(), Qt::ISODate);
        library.state = ArtworkPolicyUtils::classify(library.evidence, library.complete);
        m_libraries.insert(it.key(), library);
    }
    qDebug() << "[LibraryArtworkManager] Restored artwork evidence | libraries=" << m_libraries.size();
}
QString LibraryArtworkManager::serverId() const { return m_core ? m_core->serverManager()->activeProfile().id : QString(); }
bool LibraryArtworkManager::enabled() const
{
    return m_core && defaultLayoutMode() == QLatin1String("auto");
}
QString LibraryArtworkManager::defaultLayoutMode() const
{
    const auto* store = ConfigStore::instance();
    const QString mode = store->get<QString>(ConfigKeys::forServer(serverId(), ConfigKeys::ArtworkLayoutMode));
    if (mode == QLatin1String("auto") || mode == QLatin1String("poster") || mode == QLatin1String("tile"))
        return mode;
    
    if (store->get<bool>(ConfigKeys::forServer(serverId(), ConfigKeys::AutoArtworkLayout), false))
        return QStringLiteral("auto");
    return store->get<QString>(ConfigKeys::DefaultLibraryView, QStringLiteral("poster")) == QLatin1String("tile")
        ? QStringLiteral("tile") : QStringLiteral("poster");
}
QString LibraryArtworkManager::fallbackViewMode() const
{
    const QString mode = defaultLayoutMode();
    if (mode != QLatin1String("auto")) return mode;
    return ConfigStore::instance()->get<QString>(ConfigKeys::DefaultLibraryView, QStringLiteral("poster")) == QLatin1String("tile")
        ? QStringLiteral("tile") : QStringLiteral("poster");
}
QString LibraryArtworkManager::endpointFingerprint() const
{
    if (!m_core || !m_core->serverManager()->activeProfile().isValid()) return {};
    return QString::fromLatin1(QCryptographicHash::hash(
        m_core->serverManager()->activeProfile().url.toUtf8(), QCryptographicHash::Sha256).toHex());
}
bool LibraryArtworkManager::scanAuthorized() const
{
    const QString endpoint = endpointFingerprint();
    return !endpoint.isEmpty() && endpoint == ConfigStore::instance()->get<QString>(
        ConfigKeys::forServer(serverId(), ConfigKeys::ScanLibraryArtworkEndpoint));
}
bool LibraryArtworkManager::hasManualOverride(QString id) const
{
    return !ConfigStore::instance()->get<QString>(ConfigKeys::forLibrary(serverId(), id, ConfigKeys::LibraryViewMode)).isEmpty();
}
bool LibraryArtworkManager::landscape(QString id) const
{
    return layout(id) == ArtworkPolicyUtils::Layout::Landscape;
}
ArtworkPolicyUtils::Layout LibraryArtworkManager::layout(QString id) const
{
    using ArtworkPolicyUtils::Layout;
    const Layout fallback = fallbackViewMode() == QLatin1String("tile") ? Layout::Landscape : Layout::Portrait;
    if (id.isEmpty()) return fallback;
    const auto manual = ConfigStore::instance()->get<QString>(ConfigKeys::forLibrary(serverId(), id, ConfigKeys::LibraryViewMode));
    if (!manual.isEmpty()) return manual == QLatin1String("tile") ? Layout::Landscape : Layout::Portrait;
    if (!enabled()) return fallback;
    const auto state = m_libraries.value(id).state;
    if (ArtworkPolicyUtils::isLandscape(state)) return Layout::Landscape;
    return state == ArtworkPolicyUtils::State::HasPortrait || state == ArtworkPolicyUtils::State::MixedOrSquare ? Layout::Portrait : fallback;
}
QString LibraryArtworkManager::libraryForItem(const MediaItem& item) const
{
    if (m_libraryIds.contains(item.owningLibraryId)) return item.owningLibraryId;
    if (m_libraryIds.contains(item.id)) return item.id;
    if (m_libraryIds.contains(item.parentId)) return item.parentId;
    const QString owner = m_owners.value(item.id);
    return m_libraryIds.contains(owner) ? owner : QString();
}
void LibraryArtworkManager::init(QEmbyCore* core)
{
    if (m_core == core) return;
    m_core = core;
    if (!core) return;
    const auto profile = core->serverManager()->activeProfile();
    m_identity = profile.id + '|' + profile.userId + '|' + profile.url;
    loadCache();
    connect(core->mediaService(), &MediaService::imageInvalidated, this, &LibraryArtworkManager::invalidate);
    connect(core->mediaService(), &MediaService::libraryArtworkPage, this,
            [this](const QString& id, const QList<MediaItem>& items, int, int total, bool completeScope) {
        m_libraryIds.insert(id);
        observeItems(items);
        if (!enabled() || !completeScope || total < 0) return;
        auto& library = m_libraries[id];
        library.expectedTotal = total;
        for (const auto& item : items) library.coveredItems.insert(item.id);
        library.rangeComplete = library.coveredItems.size() >= total && total > 0;
        updatePolicy(id);
    });
    connect(core->mediaService(), &MediaService::itemDetailFetched, this,
            [this](const MediaItem& item) { observeItems({item}); });
    connect(core->serverManager(), &ServerManager::activeServerChanged, this, [this]() {
        saveCache();
        m_saveTimer.stop();
        ++m_generation;
        delete m_scanContext; m_scanContext = nullptr;
        m_sessionLibraries.insert(m_identity, m_libraries);
        m_sessionOwners.insert(m_identity, m_owners);
        const auto profile = m_core->serverManager()->activeProfile();
        m_identity = profile.id + '|' + profile.userId + '|' + profile.url;
        m_libraries = m_sessionLibraries.value(m_identity);
        m_owners = m_sessionOwners.value(m_identity);
        m_libraryIds.clear();
        if (m_libraries.isEmpty()) loadCache();
        configure();
    });
    configure();
}
void LibraryArtworkManager::configure()
{
    qDebug() << "[LibraryArtworkManager] Layout configuration applied"
             << "| defaultMode=" << defaultLayoutMode() << "| fallback=" << fallbackViewMode()
             << "| automatic=" << enabled();
    saveCache();
    ++m_generation;
    delete m_scanContext; m_scanContext = new QObject(this);
    for (auto it = m_libraries.begin(); it != m_libraries.end(); ++it) {
        it->scanning = false;
        if (it->verifiedAt.isValid() && it->verifiedAt.secsTo(QDateTime::currentDateTimeUtc()) > 24 * 3600) {
            it->complete = false;
            it->nextOffset = 0;
            if (it->state == ArtworkPolicyUtils::State::LandscapeOnly)
                it->state = ArtworkPolicyUtils::State::LandscapeProvisional;
        }
        Q_EMIT policyChanged(it.key());
    }
    Q_EMIT configurationChanged();
    if (m_core && m_core->serverManager()->activeProfile().isValid())
        QCoro::connect(discover(m_generation), this, []() {});
}
QCoro::Task<void> LibraryArtworkManager::discover(quint64 generation)
{
    QPointer<LibraryArtworkManager> guard(this);
    try {
        const auto views = co_await m_core->mediaService()->getUserViews();
        if (!guard || guard->m_generation != generation) co_return;
        const auto previousIds = guard->m_libraryIds;
        guard->m_libraryIds.clear();
        for (const auto& view : views) guard->m_libraryIds.insert(view.id);
        for (const auto& previous : previousIds) {
            if (guard->m_libraryIds.contains(previous)) continue;
            guard->m_libraries.remove(previous);
            for (auto owner = guard->m_owners.begin(); owner != guard->m_owners.end();)
                if (owner.value() == previous) owner = guard->m_owners.erase(owner);
                else ++owner;
            Q_EMIT guard->policyChanged(previous);
        }
        for (const auto& view : views)
            if (!previousIds.contains(view.id)) Q_EMIT guard->policyChanged(view.id);
        if (!guard->enabled() || !ConfigStore::instance()->get<bool>(
            ConfigKeys::forServer(guard->serverId(), ConfigKeys::ScanLibraryArtwork), false)) co_return;
        if (!guard->scanAuthorized()) {
            qInfo() << "[LibraryArtworkManager] Background scan requires authorization for the current endpoint";
            Q_EMIT guard->scanAuthorizationRequired();
            co_return;
        }
        
        for (auto view : views) {
            if (view.collectionType == "music" || view.collectionType == "playlists" ||
                view.collectionType == "livetv" || guard->hasManualOverride(view.id)) continue;
            co_await guard->scan(view.id, generation);
            if (!guard || guard->m_generation != generation) co_return;
        }
    } catch (...) { qWarning() << "[LibraryArtworkManager] Library discovery failed"; }
}
void LibraryArtworkManager::observeItems(QList<MediaItem> items)
{
    QSet<QString> changed;
    for (const auto& item : items) {
        QString id = libraryForItem(item);
        if (id.isEmpty()) continue;
        m_owners.insert(item.id, id);
        if (!enabled() || (item.type != "Movie" && item.type != "Series" && item.type != "Video" && item.type != "BoxSet")) continue;
        const auto image = item.images.bestPoster();
        const QString identity = image.first + '|' + image.second;
        auto& library = m_libraries[id];
        const auto current = library.evidence.constFind(item.id);
        if (current == library.evidence.cend() || current->identity != identity) {
            library.evidence.insert(item.id, {identity, {}, !image.first.isEmpty(), false});
            library.complete = false;
            library.rangeComplete = false;
            changed.insert(id);
        }
    }
    for (const auto& id : changed) updatePolicy(id);
}
void LibraryArtworkManager::requestEvidence(QList<MediaItem> items)
{
    if (!enabled()) return;
    observeItems(items);
    QSet<QString> libraries;
    for (const auto& item : items) {
        const QString id = libraryForItem(item);
        if (id.isEmpty() || !m_libraries[id].evidence.contains(item.id)) continue;
        m_libraries[id].evidence[item.id].requested = true;
        libraries.insert(id);
    }
    for (const auto& id : libraries) updatePolicy(id);
}

void LibraryArtworkManager::observeImage(MediaItem item, QString type, QString tag, QSize size)
{
    if (!enabled()) return;
    observeItems({item});
    const QString id = libraryForItem(item);
    if (id.isEmpty() || !m_libraries[id].evidence.contains(item.id)) return;
    const auto primary = item.images.bestPoster();
    
    if (tag != primary.first || type != primary.second) return;
    auto& evidence = m_libraries[id].evidence[item.id];
    if (!evidence.resolved || evidence.size != size)
        qDebug() << "[LibraryArtworkManager] Artwork dimensions observed"
                 << "| libraryId=" << id << "| itemId=" << item.id
                 << "| imageType=" << type << "| size=" << size << "| valid=" << size.isValid();
    evidence.requested = true;
    evidence.size = size; evidence.resolved = size.isValid();
    updatePolicy(id);
}
void LibraryArtworkManager::updatePolicy(QString id)
{
    auto& library = m_libraries[id];
    if (library.rangeComplete) {
        library.complete = true;
        for (const auto& evidence : library.evidence)
            if (evidence.hasImage && !evidence.resolved) { library.complete = false; break; }
    }
    auto state = ArtworkPolicyUtils::classify(library.evidence, library.complete);
    
    
    if (state == ArtworkPolicyUtils::State::Unknown &&
        ArtworkPolicyUtils::isLandscape(library.state)) state = ArtworkPolicyUtils::State::LandscapeProvisional;
    if (state != library.state) {
        library.state = state;
        qDebug() << "[LibraryArtworkManager] Policy changed | state=" << int(state)
                 << "| libraryId=" << id << "| evidence=" << library.evidence.size() << "| complete=" << library.complete;
        Q_EMIT policyChanged(id);
    }
}
void LibraryArtworkManager::invalidate(QString itemId)
{
    const QString id = m_owners.value(itemId);
    if (id.isEmpty()) return;
    m_libraries[id].evidence.remove(itemId);
    m_libraries[id].complete = false;
    m_libraries[id].nextOffset = 0;
    m_libraries[id].coveredItems.clear();
    m_libraries[id].rangeComplete = false;
    m_libraries[id].state = ArtworkPolicyUtils::State::Unknown;
    ++m_generation;
    delete m_scanContext; m_scanContext = new QObject(this);
    updatePolicy(id);
    Q_EMIT policyChanged(id);
}
void LibraryArtworkManager::followDefault(QString id)
{
    
    
    ConfigStore::instance()->set(ConfigKeys::forLibrary(serverId(), id, ConfigKeys::LibraryViewMode), QString());
}
QCoro::Task<QString> LibraryArtworkManager::resolveLibrary(MediaItem item)
{
    QString id = libraryForItem(item);
    if (!id.isEmpty() || !m_core) co_return id;
    const quint64 generation = m_generation;
    const QString original = item.id;
    QPointer<LibraryArtworkManager> guard(this);
    QSet<QString> seen;
    try {
        if (m_libraryIds.isEmpty()) {
            const auto views = co_await m_core->mediaService()->getUserViews();
            if (!guard || generation != guard->m_generation) co_return QString();
            for (const auto& view : views) guard->m_libraryIds.insert(view.id);
            id = guard->libraryForItem(item);
            if (!id.isEmpty()) { guard->m_owners.insert(original, id); co_return id; }
        }
        for (int depth = 0; depth < 32 && !item.id.isEmpty() && !seen.contains(item.id); ++depth) {
            seen.insert(item.id);
            item = co_await m_core->mediaService()->getItemDetail(item.parentId.isEmpty() ? item.id : item.parentId);
            if (!guard || generation != guard->m_generation) co_return QString();
            id = guard->libraryForItem(item);
            if (!id.isEmpty()) { guard->m_owners.insert(original, id); co_return id; }
        }
    } catch (...) { qWarning() << "[LibraryArtworkManager] Parent library resolution failed"; }
    co_return QString();
}
QCoro::Task<void> LibraryArtworkManager::scan(QString id, quint64 generation)
{
    if (m_libraries[id].complete || m_libraries[id].state == ArtworkPolicyUtils::State::HasPortrait) co_return;
    m_libraries[id].scanning = true;
    QPointer<LibraryArtworkManager> guard(this);
    int offset = m_libraries[id].nextOffset, checked = offset;
    QSet<QString> seen;
    qDebug() << "[LibraryArtworkManager] Background scan started";
    try {
        for (;;) {
            auto page = co_await m_core->mediaService()->getLibraryItemsPage(id, "SortName", "Ascending", "", "Movie,Series,Video,BoxSet", offset, 50, true, false, guard->m_scanContext);
            if (!guard || generation != guard->m_generation) co_return;
            if (page.items.isEmpty()) break;
            int added = 0;
            for (auto item : page.items) {
                if (seen.contains(item.id)) continue;
                seen.insert(item.id); ++added;
                item.owningLibraryId = id;
                guard->observeItems({item});
                const auto image = item.images.bestPoster();
                if (!image.first.isEmpty() && !guard->m_libraries[id].evidence.value(item.id).resolved) {
                    const auto pix = co_await guard->m_core->mediaService()->fetchImage(
                        item.getPrimaryImageId(), image.second, image.first, 96, -1,
                        ImageRequestPriority::Low, guard->m_scanContext);
                    if (!guard || generation != guard->m_generation) co_return;
                    guard->observeImage(item, image.second, image.first, pix.size());
                }
                Q_EMIT guard->scanProgress(id, ++checked, page.totalRecordCount);
                if (guard->m_libraries[id].state == ArtworkPolicyUtils::State::HasPortrait) {
                    guard->m_libraries[id].scanning = false; co_return;
                }
            }
            offset += page.items.size();
            if (!added) { qWarning() << "[LibraryArtworkManager] Repeated scan page rejected"; break; }
            guard->m_libraries[id].nextOffset = offset;
            if (page.hasTotalRecordCount && offset >= page.totalRecordCount) {
                guard->m_libraries[id].complete = true; break;
            }
            if (!page.hasTotalRecordCount && page.items.size() < 50) {
                guard->m_libraries[id].complete = true; break;
            }
        }
        guard->m_libraries[id].scanning = false;
        for (const auto& evidence : guard->m_libraries[id].evidence) {
            if (evidence.hasImage && !evidence.resolved) {
                guard->m_libraries[id].complete = false;
                guard->m_libraries[id].nextOffset = 0;
                break;
            }
        }
        if (guard->m_libraries[id].complete)
            guard->m_libraries[id].verifiedAt = QDateTime::currentDateTimeUtc();
        guard->updatePolicy(id);
        qDebug() << "[LibraryArtworkManager] Background scan finished | checked=" << checked;
    } catch (...) {
        if (!guard || generation != guard->m_generation) {
            qDebug() << "[LibraryArtworkManager] Scan cancelled by context change";
            co_return;
        }
        guard->m_libraries[id].scanning = false;
        qWarning() << "[LibraryArtworkManager] Scan interrupted; evidence retained";
    }
}
