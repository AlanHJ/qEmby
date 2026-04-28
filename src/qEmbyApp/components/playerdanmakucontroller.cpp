#include "playerdanmakucontroller.h"

#include "mpvwidget.h"
#include "nativedanmakuoverlay.h"
#include "../utils/danmakurendererutils.h"
#include "../utils/subtitlestyleutils.h"
#include <config/config_keys.h>
#include <config/configstore.h>
#include <qembycore.h>
#include <services/danmaku/danmakuservice.h>
#include <services/manager/servermanager.h>

#include <QCoreApplication>
#include <QDir>
#include <QDebug>
#include <QTimer>
#include <exception>

namespace {

constexpr auto kDanmakuTrackTitle = "[qEmby] Danmaku";

bool trackSelected(const QVariantMap &trackMap)
{
    return trackMap.value(QStringLiteral("selected")).toBool();
}

QString contextTitle(const DanmakuMediaContext &context)
{
    const QString displayTitle = context.displayTitle().trimmed();
    return displayTitle.isEmpty() ? context.title.trimmed() : displayTitle;
}

QString dandanCredentialErrorMessage()
{
    return QCoreApplication::translate(
        "DandanplayProvider",
        "DandanPlay Open API now requires App ID and App Secret. Configure them in Danmaku Server settings.");
}

bool isDandanCredentialError(const QString &errorMessage)
{
    const QString trimmed = errorMessage.trimmed();
    return trimmed == dandanCredentialErrorMessage() ||
           trimmed == QStringLiteral(
               "DandanPlay Open API now requires App ID and App Secret. Configure them in Danmaku Server settings.");
}

QString danmakuEnabledConfigKey(const DanmakuMediaContext &context)
{
    const QString serverId = context.serverId.trimmed();
    if (serverId.isEmpty()) {
        return QString::fromLatin1(ConfigKeys::PlayerDanmakuEnabled);
    }
    return ConfigKeys::forServer(serverId, ConfigKeys::PlayerDanmakuEnabled);
}

bool readDanmakuEnabled(const DanmakuMediaContext &context)
{
    auto *store = ConfigStore::instance();
    const bool globalEnabled = store->get<bool>(ConfigKeys::PlayerDanmakuEnabled,
                                                false);
    return store->get<bool>(danmakuEnabledConfigKey(context), globalEnabled);
}

QString configuredVideoSyncMode()
{
    const QString mode = ConfigStore::instance()
                             ->get<QString>(ConfigKeys::PlayerVideoSync,
                                            QStringLiteral("display-resample"))
                             .trimmed();
    return mode.isEmpty() ? QStringLiteral("display-resample") : mode;
}

bool shouldInterpolationBeEnabled()
{
    const QString vsyncMode = configuredVideoSyncMode();
    return vsyncMode == QLatin1String("display-resample");
}

} 

PlayerDanmakuController::PlayerDanmakuController(QEmbyCore *core,
                                                 MpvWidget *mpvWidget,
                                                 NativeDanmakuOverlay *nativeDanmakuOverlay,
                                                 QObject *parent)
    : QObject(parent),
      m_core(core),
      m_mpvWidget(mpvWidget),
      m_nativeDanmakuOverlay(nativeDanmakuOverlay)
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return;
    }

    if (m_nativeDanmakuOverlay && m_core && m_core->danmakuService()) {
        m_nativeDanmakuOverlay->setRenderOptions(
            m_core->danmakuService()->renderOptions());
        m_nativeDanmakuOverlay->setDanmakuVisible(false);
    }

    connect(m_mpvWidget->controller(), &MpvController::fileLoaded, this,
            [this]() {
                m_fileLoaded = true;
                syncSubtitleSelectionFromTrackList();
                updateDanmakuPresentation();
            });

    connect(m_mpvWidget->controller(), &MpvController::propertyChanged, this,
            [this](const QString &property, const QVariant &) {
                if (property == QLatin1String("track-list")) {
                    onTrackListChanged();
                }
            });

    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &) {
                if (key == QLatin1String(ConfigKeys::PlayerDanmakuRenderer)) {
                    qDebug().noquote()
                        << "[Danmaku][Player] Renderer changed"
                        << "| renderer:" << DanmakuRendererUtils::normalizeRendererId(
                               ConfigStore::instance()->get<QString>(
                                   ConfigKeys::PlayerDanmakuRenderer,
                                   DanmakuRendererUtils::defaultRendererId()));
                    updateDanmakuPresentation();
                    emit stateChanged();
                } else if (key ==
                           QLatin1String(ConfigKeys::PlayerDanmakuDualSubtitle)) {
                    applyTrackSelection();
                    emit stateChanged();
                }
            });
}

void PlayerDanmakuController::setPlaybackContext(const PlayerLaunchContext &context)
{
    ++m_requestSerial;
    removeDanmakuTrack();
    m_launchContext = context;
    m_mediaContext = buildMediaContext(context);
    m_assFilePath.clear();
    m_commentPayload.clear();
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_sourceServerId.clear();
    m_sourceServerName.clear();
    m_activeTargetId.clear();
    m_activeEndpointId.clear();
    m_commentCount = 0;
    m_selectedSubtitleTrackId = -1;
    m_fileLoaded = false;
    m_visible = true;
    m_loading = false;
    m_nativePayloadDirty = false;
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->clearDanmaku();
    }

    qDebug().noquote()
        << "[Danmaku][Player] Set playback context"
        << "| media:" << contextTitle(m_mediaContext)
        << "| mediaId:" << m_mediaContext.mediaId
        << "| sourceId:" << m_mediaContext.mediaSourceId
        << "| enabled:" << isDanmakuEnabled();

    if (!isDanmakuEnabled()) {
        emit stateChanged();
        return;
    }

    const bool autoLoad = ConfigStore::instance()->get<bool>(
        ConfigKeys::forServer(m_mediaContext.serverId, ConfigKeys::DanmakuAutoLoad),
        true);
    if (!autoLoad) {
        qDebug().noquote()
            << "[Danmaku][Player] Auto load disabled for current server"
            << "| serverId:" << m_mediaContext.serverId;
        emit stateChanged();
        return;
    }

    launchTask(loadDanmakuTask(m_requestSerial));
}

void PlayerDanmakuController::clearPlaybackContext()
{
    ++m_requestSerial;
    removeDanmakuTrack();
    m_launchContext = {};
    m_mediaContext = {};
    m_assFilePath.clear();
    m_commentPayload.clear();
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_sourceServerId.clear();
    m_sourceServerName.clear();
    m_activeTargetId.clear();
    m_activeEndpointId.clear();
    m_commentCount = 0;
    m_selectedSubtitleTrackId = -1;
    m_fileLoaded = false;
    m_visible = true;
    m_loading = false;
    m_nativePayloadDirty = false;
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->clearDanmaku();
    }
    qDebug() << "[Danmaku][Player] Cleared playback context";
    emit stateChanged();
}

bool PlayerDanmakuController::isDanmakuEnabled() const
{
    return readDanmakuEnabled(m_mediaContext);
}

bool PlayerDanmakuController::isDanmakuVisible() const
{
    return isDanmakuEnabled() && m_visible;
}

bool PlayerDanmakuController::hasDanmakuTrack() const
{
    return m_danmakuTrackId > 0;
}

bool PlayerDanmakuController::hasPreparedDanmaku() const
{
    return !m_assFilePath.isEmpty() || !m_commentPayload.isEmpty();
}

bool PlayerDanmakuController::isLoading() const
{
    return m_loading;
}

bool PlayerDanmakuController::hasPlaybackContext() const
{
    return !m_mediaContext.mediaId.isEmpty();
}

QString PlayerDanmakuController::sourceTitle() const
{
    return m_sourceTitle;
}

QString PlayerDanmakuController::sourceProvider() const
{
    return m_sourceProvider;
}

QString PlayerDanmakuController::sourceServerId() const
{
    return m_sourceServerId;
}

QString PlayerDanmakuController::sourceServerName() const
{
    return m_sourceServerName;
}

int PlayerDanmakuController::commentCount() const
{
    return m_commentCount;
}

DanmakuMediaContext PlayerDanmakuController::mediaContext() const
{
    return m_mediaContext;
}

QString PlayerDanmakuController::activeTargetId() const
{
    return m_activeTargetId;
}

QString PlayerDanmakuController::activeEndpointId() const
{
    return m_activeEndpointId;
}

QList<QVariantMap> PlayerDanmakuController::contentSubtitleTracks() const
{
    QList<QVariantMap> tracks;
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return tracks;
    }

    const QVariantList trackList =
        m_mpvWidget->controller()->getProperty(QStringLiteral("track-list")).toList();
    for (const QVariant &value : trackList) {
        QVariantMap trackMap = value.toMap();
        if (trackMap.value(QStringLiteral("type")).toString() !=
            QLatin1String("sub")) {
            continue;
        }
        if (isDanmakuTrackMap(trackMap)) {
            continue;
        }
        if (m_selectedSubtitleTrackId > 0 &&
            trackMap.value(QStringLiteral("id")).toInt() == m_selectedSubtitleTrackId) {
            trackMap.insert(QStringLiteral("selected"), true);
        }
        tracks.append(trackMap);
    }
    return tracks;
}

void PlayerDanmakuController::selectSubtitleTrack(const QVariant &data)
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return;
    }

    const QString valStr = data.toString();
    if (valStr == QLatin1String("no")) {
        m_selectedSubtitleTrackId = -1;
    } else {
        m_selectedSubtitleTrackId = data.toInt();
    }
    qDebug().noquote()
        << "[Danmaku][Player] Select subtitle track"
        << "| trackData:" << valStr
        << "| resolvedTrackId:" << m_selectedSubtitleTrackId;
    applyTrackSelection();
    emit stateChanged();
}

void PlayerDanmakuController::setDanmakuEnabled(bool enabled)
{
    const QString enabledKey = danmakuEnabledConfigKey(m_mediaContext);
    ConfigStore::instance()->set(enabledKey, enabled);
    
    m_visible = enabled;
    qDebug().noquote()
        << "[Danmaku][Player] Toggle danmaku"
        << "| configKey:" << enabledKey
        << "| serverId:" << m_mediaContext.serverId
        << "| enabled:" << enabled
        << "| hasAssFile:" << !m_assFilePath.isEmpty()
        << "| commentPayload:" << m_commentPayload.size()
        << "| fileLoaded:" << m_fileLoaded;
    if (!enabled) {
        updateDanmakuPresentation();
        emit toastRequested(tr("Danmaku Disabled"));
        emit stateChanged();
        return;
    }

    if (!hasPreparedDanmaku()) {
        reload();
        return;
    }

    updateDanmakuPresentation();
    emit toastRequested(tr("Danmaku Enabled"));
    emit stateChanged();
}

void PlayerDanmakuController::setDanmakuVisible(bool visible)
{
    if (m_visible == visible) {
        return;
    }
    m_visible = visible;
    qDebug().noquote()
        << "[Danmaku][Player] Toggle danmaku visibility"
        << "| visible:" << visible
        << "| hasTrack:" << (m_danmakuTrackId > 0)
        << "| nativeRenderer:" << shouldUseNativeRenderer();
    updateDanmakuPresentation();
    emit toastRequested(visible ? tr("Danmaku Shown") : tr("Danmaku Hidden"));
    emit stateChanged();
}

void PlayerDanmakuController::reload(const QString &manualKeyword)
{
    if (m_mediaContext.mediaId.isEmpty()) {
        return;
    }

    const QString trimmedKeyword = manualKeyword.trimmed();
    if (!trimmedKeyword.isEmpty() && !isDanmakuEnabled()) {
        ConfigStore::instance()->set(danmakuEnabledConfigKey(m_mediaContext), true);
    }

    ++m_requestSerial;
    removeDanmakuTrack();
    m_assFilePath.clear();
    m_commentPayload.clear();
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_sourceServerId.clear();
    m_sourceServerName.clear();
    m_activeTargetId.clear();
    m_activeEndpointId.clear();
    m_commentCount = 0;
    m_loading = false;
    m_nativePayloadDirty = false;
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->clearDanmaku();
    }
    qDebug().noquote()
        << "[Danmaku][Player] Reload requested"
        << "| mediaId:" << m_mediaContext.mediaId
        << "| manualKeyword:" << trimmedKeyword
        << "| enabled:" << isDanmakuEnabled();
    emit stateChanged();
    launchTask(loadDanmakuTask(m_requestSerial, trimmedKeyword));
}

void PlayerDanmakuController::loadFromCandidate(
    DanmakuMatchCandidate candidate, bool saveAsManualMatch)
{
    if (m_mediaContext.mediaId.isEmpty() || !candidate.isValid()) {
        return;
    }

    if (!isDanmakuEnabled()) {
        ConfigStore::instance()->set(danmakuEnabledConfigKey(m_mediaContext), true);
    }

    ++m_requestSerial;
    removeDanmakuTrack();
    m_assFilePath.clear();
    m_commentPayload.clear();
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_sourceServerId.clear();
    m_sourceServerName.clear();
    m_activeTargetId.clear();
    m_activeEndpointId.clear();
    m_commentCount = 0;
    m_loading = false;
    m_nativePayloadDirty = false;
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->clearDanmaku();
    }
    qDebug().noquote()
        << "[Danmaku][Player] Load candidate requested"
        << "| mediaId:" << m_mediaContext.mediaId
        << "| provider:" << candidate.provider
        << "| endpointId:" << candidate.endpointId
        << "| endpointName:" << candidate.endpointName
        << "| targetId:" << candidate.targetId
        << "| saveManualMatch:" << saveAsManualMatch;
    emit stateChanged();
    launchTask(loadDanmakuCandidateTask(m_requestSerial, candidate,
                                        saveAsManualMatch));
}

void PlayerDanmakuController::loadLocalFile(QString filePath)
{
    if (!m_core || !m_core->danmakuService()) {
        return;
    }

    const DanmakuMatchCandidate candidate =
        m_core->danmakuService()->createLocalFileCandidate(filePath.trimmed());
    if (!candidate.isValid()) {
        emit toastRequested(tr("Unsupported danmaku file"));
        return;
    }

    loadFromCandidate(candidate, true);
}

void PlayerDanmakuController::launchTask(QCoro::Task<void> &&task)
{
    QCoro::connect(std::move(task), this, []() {});
}

DanmakuMediaContext PlayerDanmakuController::buildMediaContext(
    const PlayerLaunchContext &context) const
{
    DanmakuMediaContext mediaContext;
    if (!m_core || !m_core->serverManager()) {
        return mediaContext;
    }

    const MediaItem &item = context.mediaItem;
    const MediaSourceInfo &source = context.selectedSource;
    mediaContext.serverId = m_core->serverManager()->activeProfile().id;
    mediaContext.mediaId = item.id;
    mediaContext.mediaSourceId = source.id;
    mediaContext.itemType = item.type;
    mediaContext.title = item.name;
    mediaContext.originalTitle = item.originalTitle;
    mediaContext.seriesName = item.seriesName;
    mediaContext.productionYear = item.productionYear;
    mediaContext.seasonNumber = item.parentIndexNumber;
    mediaContext.episodeNumber = item.indexNumber;
    mediaContext.durationMs =
        (source.runTimeTicks > 0 ? source.runTimeTicks : item.runTimeTicks) / 10000;
    mediaContext.path = !source.path.isEmpty() ? source.path : item.path;
    mediaContext.providerIds = item.providerIds;
    return mediaContext;
}

bool PlayerDanmakuController::isDanmakuTrackMap(const QVariantMap &trackMap) const
{
    const QString title = trackMap.value(QStringLiteral("title")).toString();
    if (title == QLatin1String(kDanmakuTrackTitle)) {
        return true;
    }

    const QString externalFilename =
        trackMap.value(QStringLiteral("external-filename")).toString();
    return !m_assFilePath.isEmpty() &&
           QDir::fromNativeSeparators(externalFilename) ==
               QDir::fromNativeSeparators(m_assFilePath);
}

void PlayerDanmakuController::onTrackListChanged()
{
    syncSubtitleSelectionFromTrackList();
    refreshDanmakuTrackId(0);
    if ((isDanmakuVisible() && m_danmakuTrackId > 0) || shouldUseNativeRenderer()) {
        applyTrackSelection();
    }
}

void PlayerDanmakuController::syncSubtitleSelectionFromTrackList()
{
    const QList<QVariantMap> tracks = contentSubtitleTracks();
    const int previousTrackId = m_selectedSubtitleTrackId;
    m_selectedSubtitleTrackId = -1;
    for (const QVariantMap &trackMap : tracks) {
        if (trackSelected(trackMap)) {
            m_selectedSubtitleTrackId = trackMap.value(QStringLiteral("id")).toInt();
            break;
        }
    }

    if (m_selectedSubtitleTrackId <= 0 && previousTrackId > 0) {
        for (const QVariantMap &trackMap : tracks) {
            if (trackMap.value(QStringLiteral("id")).toInt() == previousTrackId) {
                m_selectedSubtitleTrackId = previousTrackId;
                break;
            }
        }
    }
}

void PlayerDanmakuController::attachDanmakuTrack()
{
    if (shouldUseNativeRenderer()) {
        applyTrackSelection();
        return;
    }

    if (!m_fileLoaded || m_assFilePath.isEmpty() || !m_mpvWidget ||
        !m_mpvWidget->controller()) {
        return;
    }

    removeDanmakuTrack();
    qDebug().noquote()
        << "[Danmaku][Player] Attach danmaku track"
        << "| path:" << m_assFilePath;
    m_mpvWidget->controller()->command(QVariantList{
        QStringLiteral("sub-add"),
        QDir::toNativeSeparators(m_assFilePath),
        QStringLiteral("auto"),
        QString::fromLatin1(kDanmakuTrackTitle),
        QStringLiteral("qdm")});
    refreshDanmakuTrackId(5);
}

void PlayerDanmakuController::refreshDanmakuTrackId(int remainingRetries)
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return;
    }

    const QVariantList trackList =
        m_mpvWidget->controller()->getProperty(QStringLiteral("track-list")).toList();
    for (const QVariant &value : trackList) {
        const QVariantMap trackMap = value.toMap();
        if (trackMap.value(QStringLiteral("type")).toString() !=
            QLatin1String("sub")) {
            continue;
        }
        if (!isDanmakuTrackMap(trackMap)) {
            continue;
        }

        bool ok = false;
        const int trackId =
            trackMap.value(QStringLiteral("id")).toInt(&ok);
        m_danmakuTrackId = ok ? trackId : -1;
        qDebug().noquote()
            << "[Danmaku][Player] Danmaku track discovered"
            << "| trackId:" << m_danmakuTrackId
            << "| title:" << trackMap.value(QStringLiteral("title")).toString();
        applyTrackSelection();
        emit stateChanged();
        return;
    }

    if (remainingRetries > 0) {
        QTimer::singleShot(80, this, [this, remainingRetries]() {
            refreshDanmakuTrackId(remainingRetries - 1);
        });
    }
}

void PlayerDanmakuController::removeDanmakuTrack()
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        m_danmakuTrackId = -1;
        m_motionStabilityProfileApplied = false;
        return;
    }

    if (m_danmakuTrackId > 0) {
        qDebug().noquote()
            << "[Danmaku][Player] Remove danmaku track"
            << "| trackId:" << m_danmakuTrackId;
        m_mpvWidget->controller()->command(
            QVariantList{QStringLiteral("sub-remove"), m_danmakuTrackId});
    }
    m_danmakuTrackId = -1;
    clearDanmakuMotionStabilityProfile();
}

bool PlayerDanmakuController::prefersNativeRenderer() const
{
    return m_nativeDanmakuOverlay &&
           DanmakuRendererUtils::isNativeRenderer(
               ConfigStore::instance()->get<QString>(
                   ConfigKeys::PlayerDanmakuRenderer,
                   DanmakuRendererUtils::defaultRendererId()));
}

bool PlayerDanmakuController::shouldUseNativeRenderer() const
{
    return prefersNativeRenderer() && !m_commentPayload.isEmpty();
}

void PlayerDanmakuController::updateDanmakuPresentation()
{
    const bool nativeRendererPreferred = prefersNativeRenderer();
    if (nativeRendererPreferred && m_nativeDanmakuOverlay && m_core &&
        m_core->danmakuService()) {
        m_nativeDanmakuOverlay->setRenderOptions(
            m_core->danmakuService()->renderOptions());
        if (m_nativePayloadDirty) {
            if (!m_commentPayload.isEmpty()) {
                m_nativeDanmakuOverlay->setComments(m_commentPayload);
            } else {
                m_nativeDanmakuOverlay->clearDanmaku();
            }
            m_nativePayloadDirty = false;
        }
    } else if (m_nativeDanmakuOverlay && m_nativePayloadDirty &&
               m_commentPayload.isEmpty()) {
        m_nativeDanmakuOverlay->clearDanmaku();
        m_nativePayloadDirty = false;
    }

    if (shouldUseNativeRenderer()) {
        if (m_danmakuTrackId > 0) {
            removeDanmakuTrack();
        } else {
            clearDanmakuMotionStabilityProfile();
        }
        applyTrackSelection();
        return;
    }

    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->setDanmakuVisible(false);
    }

    if (isDanmakuVisible() && m_fileLoaded && !m_assFilePath.isEmpty() &&
        m_danmakuTrackId <= 0) {
        attachDanmakuTrack();
        return;
    }

    applyTrackSelection();
}

void PlayerDanmakuController::applyDanmakuMotionStabilityProfile()
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return;
    }

    auto *controller = m_mpvWidget->controller();
    auto setOptionWithLog = [controller](const QString &name,
                                         const QVariant &value) {
        const int err = controller->setProperty(name, value);
        if (err < 0) {
            qWarning().noquote()
                << "[Danmaku][Player] Failed to set MPV option"
                << "| option:" << name
                << "| value:" << value
                << "| error:" << err;
        }
    };
    setOptionWithLog(QStringLiteral("video-sync"), QStringLiteral("audio"));
    setOptionWithLog(QStringLiteral("interpolation"), false);
    setOptionWithLog(QStringLiteral("blend-subtitles"), QStringLiteral("no"));
    setOptionWithLog(QStringLiteral("sub-hinting"), QStringLiteral("none"));
    setOptionWithLog(QStringLiteral("sub-ass-hinting"), QStringLiteral("none"));
    setOptionWithLog(QStringLiteral("sub-blur"), 0.0);
    setOptionWithLog(QStringLiteral("sub-gauss"), 0.0);

    if (!m_motionStabilityProfileApplied) {
        qDebug().noquote()
            << "[Danmaku][Player] Applied motion stability profile"
            << "| video-sync: audio"
            << "| interpolation: off"
            << "| blend-subtitles: no"
            << "| sub-hinting: none"
            << "| sub-ass-hinting: none"
            << "| sub-blur: 0"
            << "| sub-gauss: 0";
    }
    m_motionStabilityProfileApplied = true;
}

void PlayerDanmakuController::clearDanmakuMotionStabilityProfile()
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        m_motionStabilityProfileApplied = false;
        return;
    }

    auto *controller = m_mpvWidget->controller();
    auto setOptionWithLog = [controller](const QString &name,
                                         const QVariant &value) {
        const int err = controller->setProperty(name, value);
        if (err < 0) {
            qWarning().noquote()
                << "[Danmaku][Player] Failed to restore MPV option"
                << "| option:" << name
                << "| value:" << value
                << "| error:" << err;
        }
    };
    const QString vsyncMode = configuredVideoSyncMode();
    const bool interpolationEnabled = shouldInterpolationBeEnabled();
    setOptionWithLog(QStringLiteral("video-sync"), vsyncMode);
    setOptionWithLog(QStringLiteral("interpolation"), interpolationEnabled);
    setOptionWithLog(QStringLiteral("blend-subtitles"), QStringLiteral("video"));
    setOptionWithLog(QStringLiteral("sub-hinting"), QStringLiteral("none"));
    setOptionWithLog(QStringLiteral("sub-ass-hinting"), QStringLiteral("none"));
    setOptionWithLog(QStringLiteral("sub-blur"), 0.0);
    setOptionWithLog(QStringLiteral("sub-gauss"), 0.0);

    if (m_motionStabilityProfileApplied) {
        qDebug().noquote()
            << "[Danmaku][Player] Restored default motion profile"
            << "| video-sync:" << vsyncMode
            << "| interpolation:" << interpolationEnabled
            << "| blend-subtitles: video"
            << "| sub-hinting: none"
            << "| sub-ass-hinting: none"
            << "| sub-blur: 0"
            << "| sub-gauss: 0";
    }
    m_motionStabilityProfileApplied = false;
}

void PlayerDanmakuController::applyTrackSelection()
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return;
    }

    const bool dualSubtitle = ConfigStore::instance()->get<bool>(
        ConfigKeys::PlayerDanmakuDualSubtitle, true);
    const bool useNativeRenderer =
        shouldUseNativeRenderer() && isDanmakuVisible();

    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->setBottomSubtitleProtected(
            useNativeRenderer && dualSubtitle && m_selectedSubtitleTrackId > 0);
        m_nativeDanmakuOverlay->setDanmakuVisible(useNativeRenderer);
    }

    if (useNativeRenderer) {
        qDebug().noquote()
            << "[Danmaku][Player] Apply native renderer selection"
            << "| contentSubtitleTrackId:" << m_selectedSubtitleTrackId
            << "| dualSubtitle:" << dualSubtitle
            << "| commentCount:" << m_commentPayload.size();
        m_mpvWidget->controller()->setProperty(QStringLiteral("secondary-sid"),
                                               QStringLiteral("no"));
        if (dualSubtitle && m_selectedSubtitleTrackId > 0) {
            m_mpvWidget->controller()->setProperty(QStringLiteral("sid"),
                                                   m_selectedSubtitleTrackId);
        } else {
            m_mpvWidget->controller()->setProperty(QStringLiteral("sid"),
                                                   QStringLiteral("no"));
        }
        clearDanmakuMotionStabilityProfile();
        SubtitleStyleUtils::applyToController(m_mpvWidget->controller(), false);
        return;
    }

    if (isDanmakuVisible() && m_danmakuTrackId > 0) {
        qDebug().noquote()
            << "[Danmaku][Player] Apply dual subtitle selection"
            << "| danmakuTrackId:" << m_danmakuTrackId
            << "| contentSubtitleTrackId:" << m_selectedSubtitleTrackId
            << "| dualSubtitle:" << dualSubtitle;
        m_mpvWidget->controller()->setProperty(QStringLiteral("sid"),
                                               m_danmakuTrackId);
        if (dualSubtitle && m_selectedSubtitleTrackId > 0) {
            m_mpvWidget->controller()->setProperty(QStringLiteral("secondary-sub-pos"),
                                                   92);
            m_mpvWidget->controller()->setProperty(QStringLiteral("secondary-sid"),
                                                   m_selectedSubtitleTrackId);
        } else {
            m_mpvWidget->controller()->setProperty(QStringLiteral("secondary-sid"),
                                                   QStringLiteral("no"));
        }
        applyDanmakuMotionStabilityProfile();
        SubtitleStyleUtils::applyToController(m_mpvWidget->controller(), true);
        return;
    }

    qDebug().noquote()
        << "[Danmaku][Player] Apply regular subtitle selection"
        << "| contentSubtitleTrackId:" << m_selectedSubtitleTrackId;
    m_mpvWidget->controller()->setProperty(QStringLiteral("secondary-sid"),
                                           QStringLiteral("no"));
    if (m_selectedSubtitleTrackId > 0) {
        m_mpvWidget->controller()->setProperty(QStringLiteral("sid"),
                                               m_selectedSubtitleTrackId);
    } else {
        m_mpvWidget->controller()->setProperty(QStringLiteral("sid"),
                                               QStringLiteral("no"));
    }
    clearDanmakuMotionStabilityProfile();
    SubtitleStyleUtils::applyToController(m_mpvWidget->controller(), false);
}

QCoro::Task<void> PlayerDanmakuController::loadDanmakuTask(quint64 requestId,
                                                           QString manualKeyword)
{
    QPointer<PlayerDanmakuController> safeThis(this);
    QPointer<QEmbyCore> core(m_core);
    const DanmakuMediaContext mediaContext = m_mediaContext;
    const QString trimmedKeyword = manualKeyword.trimmed();
    if (!core || !core->danmakuService() || mediaContext.mediaId.isEmpty()) {
        co_return;
    }

    m_loading = true;
    emit stateChanged();
    qDebug().noquote()
        << "[Danmaku][Player] Load task start"
        << "| media:" << contextTitle(mediaContext)
        << "| mediaId:" << mediaContext.mediaId
        << "| manualKeyword:" << trimmedKeyword;

    try {
        DanmakuLoadResult result =
            co_await core->danmakuService()->prepareDanmaku(mediaContext,
                                                            trimmedKeyword);
        if (!safeThis || !safeThis->m_core || requestId != safeThis->m_requestSerial) {
            co_return;
        }

        safeThis->m_loading = false;
        safeThis->m_assFilePath = result.assFilePath;
        safeThis->m_commentPayload = result.comments;
        safeThis->m_nativePayloadDirty = !result.comments.isEmpty();
        safeThis->m_sourceTitle = result.sourceTitle;
        safeThis->m_sourceProvider = result.provider;
        safeThis->m_sourceServerId = result.sourceServerId;
        safeThis->m_sourceServerName = result.sourceServerName;
        safeThis->m_activeTargetId = result.matchResult.selected.targetId;
        safeThis->m_activeEndpointId =
            result.sourceServerId.trimmed().isEmpty()
                ? result.matchResult.selected.endpointId
                : result.sourceServerId;
        safeThis->m_commentCount = result.commentCount;
        qDebug().noquote()
            << "[Danmaku][Player] Load task finished"
            << "| mediaId:" << mediaContext.mediaId
            << "| success:" << result.success
            << "| commentCount:" << result.commentCount
            << "| endpointId:" << result.sourceServerId
            << "| endpointName:" << result.sourceServerName
            << "| needManualMatch:" << result.needManualMatch
            << "| assPath:" << result.assFilePath
            << "| nativePreferred:" << safeThis->prefersNativeRenderer()
            << "| payloadCount:" << result.comments.size();

        if (result.hasRenderableContent()) {
            const QString sourceServerName = result.sourceServerName.trimmed();
            if (!sourceServerName.isEmpty() && result.commentCount > 0) {
                emit safeThis->toastRequested(
                    tr("Danmaku Loaded from %1 (%2)")
                        .arg(sourceServerName)
                        .arg(result.commentCount));
            } else if (!sourceServerName.isEmpty()) {
                emit safeThis->toastRequested(
                    tr("Danmaku Loaded from %1").arg(sourceServerName));
            } else if (result.commentCount > 0) {
                emit safeThis->toastRequested(
                    tr("Danmaku Loaded (%1)").arg(result.commentCount));
            } else {
                emit safeThis->toastRequested(tr("Danmaku Loaded"));
            }
            safeThis->updateDanmakuPresentation();
        } else if (result.needManualMatch) {
            emit safeThis->toastRequested(tr("No matching danmaku found"));
        } else {
            emit safeThis->toastRequested(tr("No danmaku available"));
        }
        emit safeThis->stateChanged();
    } catch (const std::exception &e) {
        if (!safeThis || requestId != safeThis->m_requestSerial) {
            co_return;
        }
        safeThis->m_loading = false;
        const QString errorMessage = QString::fromUtf8(e.what()).trimmed();
        safeThis->m_sourceProvider.clear();
        safeThis->m_sourceServerId.clear();
        safeThis->m_sourceServerName.clear();
        safeThis->m_commentCount = 0;
        qWarning().noquote()
            << "[Danmaku][Player] Load task failed"
            << "| mediaId:" << mediaContext.mediaId
            << "| manualKeyword:" << trimmedKeyword
            << "| error:" << e.what();
        emit safeThis->toastRequested(
            isDandanCredentialError(errorMessage)
                ? errorMessage
                : tr("Failed to load danmaku"));
        emit safeThis->stateChanged();
    }
}

QCoro::Task<void> PlayerDanmakuController::loadDanmakuCandidateTask(
    quint64 requestId, DanmakuMatchCandidate candidate, bool saveAsManualMatch)
{
    QPointer<PlayerDanmakuController> safeThis(this);
    QPointer<QEmbyCore> core(m_core);
    const DanmakuMediaContext mediaContext = m_mediaContext;
    if (!core || !core->danmakuService() || mediaContext.mediaId.isEmpty() ||
        !candidate.isValid()) {
        co_return;
    }

    m_loading = true;
    emit stateChanged();
    qDebug().noquote()
        << "[Danmaku][Player] Candidate load task start"
        << "| media:" << contextTitle(mediaContext)
        << "| mediaId:" << mediaContext.mediaId
        << "| provider:" << candidate.provider
        << "| endpointId:" << candidate.endpointId
        << "| endpointName:" << candidate.endpointName
        << "| targetId:" << candidate.targetId
        << "| saveManualMatch:" << saveAsManualMatch;

    try {
        DanmakuLoadResult result =
            co_await core->danmakuService()->prepareDanmakuForCandidate(
                mediaContext, candidate);
        if (!safeThis || !safeThis->m_core ||
            requestId != safeThis->m_requestSerial) {
            co_return;
        }

        safeThis->m_loading = false;
        safeThis->m_assFilePath = result.assFilePath;
        safeThis->m_commentPayload = result.comments;
        safeThis->m_nativePayloadDirty = !result.comments.isEmpty();
        safeThis->m_sourceTitle = result.sourceTitle;
        safeThis->m_sourceProvider = result.provider;
        safeThis->m_sourceServerId = result.sourceServerId;
        safeThis->m_sourceServerName = result.sourceServerName;
        safeThis->m_activeTargetId = candidate.targetId;
        safeThis->m_activeEndpointId =
            result.sourceServerId.trimmed().isEmpty()
                ? candidate.endpointId
                : result.sourceServerId;
        safeThis->m_commentCount = result.commentCount;
        qDebug().noquote()
            << "[Danmaku][Player] Candidate load task finished"
            << "| mediaId:" << mediaContext.mediaId
            << "| success:" << result.success
            << "| commentCount:" << result.commentCount
            << "| provider:" << result.provider
            << "| endpointId:" << result.sourceServerId
            << "| endpointName:" << result.sourceServerName
            << "| assPath:" << result.assFilePath
            << "| nativePreferred:" << safeThis->prefersNativeRenderer()
            << "| payloadCount:" << result.comments.size();

        if (result.hasRenderableContent()) {
            if (saveAsManualMatch) {
                DanmakuMatchCandidate savedCandidate = candidate;
                if (savedCandidate.endpointId.trimmed().isEmpty()) {
                    savedCandidate.endpointId = result.sourceServerId;
                }
                if (savedCandidate.endpointName.trimmed().isEmpty()) {
                    savedCandidate.endpointName = result.sourceServerName;
                }
                core->danmakuService()->saveManualMatch(mediaContext,
                                                        savedCandidate);
            }
            const QString sourceServerName = result.sourceServerName.trimmed();
            if (!sourceServerName.isEmpty() && result.commentCount > 0) {
                emit safeThis->toastRequested(
                    tr("Danmaku Loaded from %1 (%2)")
                        .arg(sourceServerName)
                        .arg(result.commentCount));
            } else if (!sourceServerName.isEmpty()) {
                emit safeThis->toastRequested(
                    tr("Danmaku Loaded from %1").arg(sourceServerName));
            } else {
                emit safeThis->toastRequested(
                    result.commentCount > 0
                        ? tr("Danmaku Loaded (%1)").arg(result.commentCount)
                        : tr("Danmaku Loaded"));
            }
            safeThis->updateDanmakuPresentation();
        } else {
            emit safeThis->toastRequested(tr("No danmaku available"));
        }
        emit safeThis->stateChanged();
    } catch (const std::exception &e) {
        if (!safeThis || requestId != safeThis->m_requestSerial) {
            co_return;
        }

        safeThis->m_loading = false;
        safeThis->m_sourceProvider.clear();
        safeThis->m_sourceServerId.clear();
        safeThis->m_sourceServerName.clear();
        safeThis->m_commentCount = 0;
        const QString errorMessage = QString::fromUtf8(e.what()).trimmed();
        qWarning().noquote()
            << "[Danmaku][Player] Candidate load task failed"
            << "| mediaId:" << mediaContext.mediaId
            << "| provider:" << candidate.provider
            << "| endpointId:" << candidate.endpointId
            << "| endpointName:" << candidate.endpointName
            << "| targetId:" << candidate.targetId
            << "| error:" << e.what();
        emit safeThis->toastRequested(
            isDandanCredentialError(errorMessage)
                ? errorMessage
                : tr("Failed to load danmaku"));
        emit safeThis->stateChanged();
    }
}
