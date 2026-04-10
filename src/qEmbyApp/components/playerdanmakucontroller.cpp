#include "playerdanmakucontroller.h"

#include "mpvwidget.h"
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

} 

PlayerDanmakuController::PlayerDanmakuController(QEmbyCore *core,
                                                 MpvWidget *mpvWidget,
                                                 QObject *parent)
    : QObject(parent), m_core(core), m_mpvWidget(mpvWidget)
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return;
    }

    connect(m_mpvWidget->controller(), &MpvController::fileLoaded, this,
            [this]() {
                m_fileLoaded = true;
                syncSubtitleSelectionFromTrackList();
                if (!m_assFilePath.isEmpty()) {
                    attachDanmakuTrack();
                }
            });

    connect(m_mpvWidget->controller(), &MpvController::propertyChanged, this,
            [this](const QString &property, const QVariant &) {
                if (property == QLatin1String("track-list")) {
                    onTrackListChanged();
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
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_activeTargetId.clear();
    m_commentCount = 0;
    m_selectedSubtitleTrackId = -1;
    m_fileLoaded = false;
    m_visible = true;
    m_loading = false;

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
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_activeTargetId.clear();
    m_commentCount = 0;
    m_selectedSubtitleTrackId = -1;
    m_fileLoaded = false;
    m_visible = true;
    m_loading = false;
    qDebug() << "[Danmaku][Player] Cleared playback context";
    emit stateChanged();
}

bool PlayerDanmakuController::isDanmakuEnabled() const
{
    return ConfigStore::instance()->get<bool>(ConfigKeys::PlayerDanmakuEnabled,
                                              false);
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
    return !m_assFilePath.isEmpty();
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
    ConfigStore::instance()->set(ConfigKeys::PlayerDanmakuEnabled, enabled);
    
    m_visible = enabled;
    qDebug().noquote()
        << "[Danmaku][Player] Toggle danmaku (global)"
        << "| enabled:" << enabled
        << "| hasAssFile:" << !m_assFilePath.isEmpty()
        << "| fileLoaded:" << m_fileLoaded;
    if (!enabled) {
        applyTrackSelection();
        emit toastRequested(tr("Danmaku Disabled"));
        emit stateChanged();
        return;
    }

    if (m_assFilePath.isEmpty()) {
        reload();
        return;
    }

    if (m_fileLoaded && m_danmakuTrackId <= 0) {
        attachDanmakuTrack();
    } else {
        applyTrackSelection();
    }
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
        << "| hasTrack:" << (m_danmakuTrackId > 0);
    applyTrackSelection();
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
        ConfigStore::instance()->set(ConfigKeys::PlayerDanmakuEnabled, true);
    }

    ++m_requestSerial;
    removeDanmakuTrack();
    m_assFilePath.clear();
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_commentCount = 0;
    m_loading = false;
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
        ConfigStore::instance()->set(ConfigKeys::PlayerDanmakuEnabled, true);
    }

    ++m_requestSerial;
    removeDanmakuTrack();
    m_assFilePath.clear();
    m_sourceTitle.clear();
    m_sourceProvider.clear();
    m_commentCount = 0;
    m_loading = false;
    qDebug().noquote()
        << "[Danmaku][Player] Load candidate requested"
        << "| mediaId:" << m_mediaContext.mediaId
        << "| provider:" << candidate.provider
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
    if (isDanmakuVisible() && m_danmakuTrackId > 0) {
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
}

void PlayerDanmakuController::applyTrackSelection()
{
    if (!m_mpvWidget || !m_mpvWidget->controller()) {
        return;
    }

    const bool dualSubtitle = ConfigStore::instance()->get<bool>(
        ConfigKeys::PlayerDanmakuDualSubtitle, true);

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
        safeThis->m_sourceTitle = result.sourceTitle;
        safeThis->m_sourceProvider = result.provider;
        safeThis->m_activeTargetId = result.matchResult.selected.targetId;
        safeThis->m_commentCount = result.commentCount;
        qDebug().noquote()
            << "[Danmaku][Player] Load task finished"
            << "| mediaId:" << mediaContext.mediaId
            << "| success:" << result.success
            << "| commentCount:" << result.commentCount
            << "| needManualMatch:" << result.needManualMatch
            << "| assPath:" << result.assFilePath;

        if (result.hasRenderableTrack()) {
            if (result.commentCount > 0) {
                emit safeThis->toastRequested(
                    tr("Danmaku Loaded (%1)").arg(result.commentCount));
            } else {
                emit safeThis->toastRequested(tr("Danmaku Loaded"));
            }
            if (safeThis->m_fileLoaded) {
                safeThis->attachDanmakuTrack();
            }
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
        safeThis->m_sourceTitle = result.sourceTitle;
        safeThis->m_sourceProvider = result.provider;
        safeThis->m_activeTargetId = candidate.targetId;
        safeThis->m_commentCount = result.commentCount;
        qDebug().noquote()
            << "[Danmaku][Player] Candidate load task finished"
            << "| mediaId:" << mediaContext.mediaId
            << "| success:" << result.success
            << "| commentCount:" << result.commentCount
            << "| provider:" << result.provider
            << "| assPath:" << result.assFilePath;

        if (result.hasRenderableTrack()) {
            if (saveAsManualMatch) {
                core->danmakuService()->saveManualMatch(mediaContext, candidate);
            }
            emit safeThis->toastRequested(
                result.commentCount > 0
                    ? tr("Danmaku Loaded (%1)").arg(result.commentCount)
                    : tr("Danmaku Loaded"));
            if (safeThis->m_fileLoaded) {
                safeThis->attachDanmakuTrack();
            }
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
        safeThis->m_commentCount = 0;
        const QString errorMessage = QString::fromUtf8(e.what()).trimmed();
        qWarning().noquote()
            << "[Danmaku][Player] Candidate load task failed"
            << "| mediaId:" << mediaContext.mediaId
            << "| provider:" << candidate.provider
            << "| targetId:" << candidate.targetId
            << "| error:" << e.what();
        emit safeThis->toastRequested(
            isDandanCredentialError(errorMessage)
                ? errorMessage
                : tr("Failed to load danmaku"));
        emit safeThis->stateChanged();
    }
}
