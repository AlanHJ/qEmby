#include "playbackmanager.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <services/manager/servermanager.h>
#include <models/profile/serverprofile.h>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <models/media/playbackinfo.h>
#include "../components/playerwindow.h"
#include "../components/moderntoast.h"
#include "../utils/playerpreferenceutils.h"
#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QCoreApplication>
#include <QTimer>
#include <QDebug>


static constexpr long long TICKS_PER_SECOND = 10000000LL;

static constexpr int PROGRESS_REPORT_INTERVAL_MS = 10000;

PlaybackManager* PlaybackManager::instance()
{
    static PlaybackManager s_instance;
    return &s_instance;
}

PlaybackManager::PlaybackManager(QObject *parent)
    : QObject(parent)
{
}

void PlaybackManager::init(QEmbyCore* core)
{
    m_core = core;
}

void PlaybackManager::startPlayback(const QString& mediaId, const QString& title,
                                     const QString& streamUrl, long long startPositionTicks,
                                     const QVariant& extraData)
{
    if (!m_core) return;

    
    if (m_independentWindow) {
        m_independentWindow->close();
        
    }

    qDebug() << "[PlaybackManager] ===== startPlayback =====";
    qDebug() << "[PlaybackManager] mediaId:" << mediaId;
    qDebug() << "[PlaybackManager] title:" << title;
    qDebug() << "[PlaybackManager] streamUrl:" << streamUrl;
    qDebug() << "[PlaybackManager] startPositionTicks:" << startPositionTicks
             << "(" << ticksToTimeString(startPositionTicks) << ")";

    
    
    
    bool extEnabled = ConfigStore::instance()->get<bool>(ConfigKeys::ExtPlayerEnable, false);
    bool quickPlayExt = ConfigStore::instance()->get<bool>(ConfigKeys::ExtPlayerQuickPlay, false);
    if (extEnabled && quickPlayExt) {
        QString playerPath = ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerPath);
        if (!playerPath.isEmpty() && QFile::exists(playerPath)) {
            qDebug() << "[PlaybackManager] → Mode: External player (quick play)";
            launchExternalPlayer(mediaId, title, streamUrl, startPositionTicks, extraData);
            return;
        }
        
        qDebug() << "[PlaybackManager] External player path invalid, falling back";
        ModernToast::showMessage(tr("External player not found, falling back to internal player"), 2000);
    }

    
    
    
    bool independent = ConfigStore::instance()->get<bool>(ConfigKeys::PlayerIndependentWindow, false);
    if (independent) {
        qDebug() << "[PlaybackManager] → Mode: Independent window";
        launchIndependentWindow(mediaId, title, streamUrl, startPositionTicks, extraData);
        return;
    }

    
    
    
    qDebug() << "[PlaybackManager] → Mode: Embedded player";
    Q_EMIT requestEmbeddedPlay(mediaId, title, streamUrl, startPositionTicks, extraData);
}

void PlaybackManager::startInternalPlayback(const QString& mediaId, const QString& title,
                                             const QString& streamUrl, long long startPositionTicks,
                                             const QVariant& extraData)
{
    if (!m_core) return;

    
    if (m_independentWindow) {
        m_independentWindow->close();
    }

    qDebug() << "[PlaybackManager] ===== startInternalPlayback =====";
    qDebug() << "[PlaybackManager] mediaId:" << mediaId << "title:" << title;
    qDebug() << "[PlaybackManager] streamUrl:" << streamUrl;

    
    bool independent = ConfigStore::instance()->get<bool>(ConfigKeys::PlayerIndependentWindow, false);
    if (independent) {
        qDebug() << "[PlaybackManager] → Mode: Independent window (internal)";
        launchIndependentWindow(mediaId, title, streamUrl, startPositionTicks, extraData);
        return;
    }
    qDebug() << "[PlaybackManager] → Mode: Embedded player (internal)";
    Q_EMIT requestEmbeddedPlay(mediaId, title, streamUrl, startPositionTicks, extraData);
}

void PlaybackManager::stopExternalPlayer()
{
    if (m_extProcess && m_extProcess->state() != QProcess::NotRunning) {
        m_extProcess->kill();
        m_extProcess->waitForFinished(3000);
    }
    cleanupExternalPlayerState();
}

void PlaybackManager::cleanupExternalPlayerState()
{
    if (m_progressTimer) {
        m_progressTimer->stop();
        m_progressTimer->deleteLater();
        m_progressTimer = nullptr;
    }
    if (m_ipc) {
        m_ipc->stop();
        m_ipc->deleteLater();
        m_ipc = nullptr;
    }
    m_currentMediaId.clear();
    m_currentMediaSourceId.clear();
    m_currentPlaySessionId.clear();
}




PlayerType PlaybackManager::identifyPlayerType(const QString& playerPath)
{
    QString baseName = QFileInfo(playerPath).baseName().toLower();

    if (baseName.contains("potplayer"))
        return PlayerType::PotPlayer;
    if (baseName == "vlc")
        return PlayerType::VLC;
    if (baseName.contains("mpc-hc") || baseName == "mpc-hc64")
        return PlayerType::MPC_HC;
    if (baseName.contains("mpc-be") || baseName == "mpc-be64")
        return PlayerType::MPC_BE;
    if (baseName == "mpv")
        return PlayerType::MPV;
    if (baseName == "iina")
        return PlayerType::IINA;
    if (baseName.contains("mpc-qt"))
        return PlayerType::MPC_Qt;

    return PlayerType::Unknown;
}




QString PlaybackManager::ticksToTimeString(long long ticks)
{
    long long totalSeconds = ticks / TICKS_PER_SECOND;
    int h = static_cast<int>(totalSeconds / 3600);
    int m = static_cast<int>((totalSeconds % 3600) / 60);
    int s = static_cast<int>(totalSeconds % 60);
    return QString("%1:%2:%3")
        .arg(h, 2, 10, QChar('0'))
        .arg(m, 2, 10, QChar('0'))
        .arg(s, 2, 10, QChar('0'));
}

double PlaybackManager::ticksToSeconds(long long ticks)
{
    return static_cast<double>(ticks) / TICKS_PER_SECOND;
}





int PlaybackManager::computeRelativeStreamIndex(const MediaSourceInfo& source,
                                                 const QString& streamType,
                                                 int globalIndex)
{
    int count = 0;
    for (const auto& stream : source.mediaStreams) {
        if (stream.type == streamType) {
            if (stream.index == globalIndex)
                return count;
            count++;
        }
    }
    return -1; 
}




QString PlaybackManager::generateIpcEndpoint(PlayerType type)
{
    qint64 pid = QCoreApplication::applicationPid();
    switch (type) {
    case PlayerType::MPV:
    case PlayerType::IINA:
#ifdef Q_OS_WIN
        return QString("\\\\.\\pipe\\qemby-mpv-%1").arg(pid);
#else
        return QString("/tmp/qemby-mpv-%1.sock").arg(pid);
#endif
    case PlayerType::VLC: {
        
        int port = 42900 + static_cast<int>(pid % 1000);
        return QString::number(port);
    }
    default:
        return {};
    }
}




QStringList PlaybackManager::buildPlayerArgs(PlayerType type,
                                              long long startPositionTicks,
                                              int audioStreamIndex,
                                              int subtitleStreamIndex,
                                              bool subtitleDisabled,
                                              const MediaSourceInfo* sourceInfo)
{
    QStringList args;
    if (startPositionTicks <= 0 && audioStreamIndex < 0 && subtitleStreamIndex < 0)
        return args;

    QString timeStr = ticksToTimeString(startPositionTicks);
    double seconds = ticksToSeconds(startPositionTicks);

    
    int relAudio = -1;
    int relSub = -1;
    if (sourceInfo) {
        if (audioStreamIndex >= 0)
            relAudio = computeRelativeStreamIndex(*sourceInfo, "Audio", audioStreamIndex);
        if (subtitleStreamIndex >= 0)
            relSub = computeRelativeStreamIndex(*sourceInfo, "Subtitle", subtitleStreamIndex);
    }

    switch (type) {
    case PlayerType::MPV:
        if (startPositionTicks > 0)
            args << QString("--start=%1").arg(timeStr);
        if (relAudio >= 0)
            args << QString("--aid=%1").arg(relAudio + 1); 
        if (relSub >= 0)
            args << QString("--sid=%1").arg(relSub + 1);
        else if (subtitleDisabled)
            args << "--sid=no";
        break;

    case PlayerType::IINA:
        if (startPositionTicks > 0)
            args << QString("--mpv-start=%1").arg(timeStr);
        if (relAudio >= 0)
            args << QString("--mpv-aid=%1").arg(relAudio + 1);
        if (relSub >= 0)
            args << QString("--mpv-sid=%1").arg(relSub + 1);
        else if (subtitleDisabled)
            args << "--mpv-sid=no";
        break;

    case PlayerType::PotPlayer:
        if (startPositionTicks > 0)
            args << QString("/seek=%1").arg(timeStr);
        
        break;

    case PlayerType::VLC:
        if (startPositionTicks > 0)
            args << QString("--start-time=%1").arg(static_cast<int>(seconds));
        if (relAudio >= 0)
            args << QString("--audio-track=%1").arg(relAudio);
        if (relSub >= 0)
            args << QString("--sub-track=%1").arg(relSub);
        break;

    case PlayerType::MPC_HC:
        if (startPositionTicks > 0)
            args << "/startpos" << timeStr;
        break;

    case PlayerType::MPC_BE:
        if (startPositionTicks > 0)
            args << "/startpos" << timeStr;
        break;

    default:
        break; 
    }

    return args;
}



void PlaybackManager::startExternalPlayback(const QString& playerPath,
                                              const QString& mediaId, const QString& title,
                                              const QString& streamUrl, long long startPositionTicks,
                                              const QVariant& extraData)
{
    if (!m_core) return;
    if (playerPath.isEmpty() || !QFile::exists(playerPath)) {
        ModernToast::showMessage(tr("External player not found"), 2000);
        return;
    }
    launchExternalPlayer(mediaId, title, streamUrl, startPositionTicks, extraData, playerPath);
}




void PlaybackManager::launchExternalPlayer(const QString& mediaId, const QString& title,
                                            const QString& streamUrl, long long startPositionTicks,
                                            const QVariant& extraData,
                                            const QString& playerPathOverride)
{
    
    stopExternalPlayer();

    QString playerPath = playerPathOverride.isEmpty()
        ? ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerPath)
        : playerPathOverride;
    QString argsStr = ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerArgs);

    
    PlayerType playerType = identifyPlayerType(playerPath);

    
    int selectedAudioIdx = -1;
    int selectedSubIdx = -1;
    bool subtitleDisabled = false; 
    QString mediaSourceId = mediaId;
    MediaSourceInfo sourceInfo;
    bool hasSourceInfo = false;

    if (extraData.canConvert<MediaSourceInfo>()) {
        sourceInfo = extraData.value<MediaSourceInfo>();
        hasSourceInfo = true;
        mediaSourceId = sourceInfo.id;

        
        bool hasSubtitleStreams = false;
        for (const auto& stream : sourceInfo.mediaStreams) {
            if (stream.type == "Audio" && stream.isDefault) {
                selectedAudioIdx = stream.index;
            } else if (stream.type == "Subtitle") {
                hasSubtitleStreams = true;
                if (stream.isDefault) {
                    selectedSubIdx = stream.index;
                }
            }
        }
        
        
        
        
    }

    if (hasSourceInfo && selectedSubIdx < 0) {
        subtitleDisabled = PlayerPreferenceUtils::isSubtitleDisabled(
            ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang,
                                                  "auto"));
    }

    
    QStringList args;
    
    if (!argsStr.trimmed().isEmpty()) {
        args = QProcess::splitCommand(argsStr);
    }

    
    QStringList playerArgs = buildPlayerArgs(
        playerType, startPositionTicks,
        selectedAudioIdx, selectedSubIdx,
        subtitleDisabled,
        hasSourceInfo ? &sourceInfo : nullptr);
    args << playerArgs;

    
    QString ipcEndpoint = generateIpcEndpoint(playerType);

    switch (playerType) {
    case PlayerType::MPV:
        if (!ipcEndpoint.isEmpty())
            args << QString("--input-ipc-server=%1").arg(ipcEndpoint);
        break;
    case PlayerType::IINA:
        if (!ipcEndpoint.isEmpty())
            args << QString("--mpv-input-ipc-server=%1").arg(ipcEndpoint);
        break;
    
    default:
        break;
    }

    
    switch (playerType) {
    case PlayerType::MPV:
        args << "--autofit=1280x720";
        break;
    case PlayerType::IINA:
        args << "--mpv-autofit=1280x720";
        break;
    default:
        break; 
    }

    
    if (!title.isEmpty()) {
        switch (playerType) {
        case PlayerType::MPV:
            args << QString("--force-media-title=%1").arg(title);
            break;
        case PlayerType::IINA:
            args << QString("--mpv-force-media-title=%1").arg(title);
            break;
        case PlayerType::VLC:
            args << QString("--meta-title=%1").arg(title);
            break;
        default:
            break; 
        }
    }

    
    QString actualStreamUrl = streamUrl;
    qDebug() << "[PlaybackManager] Original streamUrl:" << streamUrl;
    if (ConfigStore::instance()->get<bool>(ConfigKeys::ExtPlayerDirectStream, false) && hasSourceInfo) {
        qDebug() << "[PlaybackManager] DirectStream enabled, sourceInfo.path:" << sourceInfo.path;
        qDebug() << "[PlaybackManager] sourceInfo.directStreamUrl:" << sourceInfo.directStreamUrl;
        
        if (!sourceInfo.path.isEmpty()) {
            actualStreamUrl = sourceInfo.path;
            qDebug() << "[PlaybackManager] Using sourceInfo.path as preferred direct source:" << actualStreamUrl;
        } else if (!sourceInfo.directStreamUrl.isEmpty()) {
            
            if (sourceInfo.directStreamUrl.startsWith("http://", Qt::CaseInsensitive) ||
                sourceInfo.directStreamUrl.startsWith("https://", Qt::CaseInsensitive)) {
                actualStreamUrl = sourceInfo.directStreamUrl;
                qDebug() << "[PlaybackManager] Using directStreamUrl (absolute):" << actualStreamUrl;
            } else {
                auto profile = m_core->serverManager()->activeProfile();
                if (profile.isValid()) {
                    actualStreamUrl = profile.url + sourceInfo.directStreamUrl;
                    qDebug() << "[PlaybackManager] Using directStreamUrl (relative, joined):" << actualStreamUrl;
                }
            }
        } else {
            
            auto profile = m_core->serverManager()->activeProfile();
            if (profile.isValid()) {
                actualStreamUrl = QString("%1/Videos/%2/stream?static=true&mediaSourceId=%3&api_key=%4")
                    .arg(profile.url, mediaId, sourceInfo.id, profile.accessToken);
                qDebug() << "[PlaybackManager] Using fallback static stream URL:" << actualStreamUrl;
            }
        }
    }

    
    QString replaceRules = ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerUrlReplace);
    if (!replaceRules.isEmpty()) {
        qDebug() << "[PlaybackManager] URL replace rules found, processing...";
        QString beforeReplace = actualStreamUrl;
        const QStringList lines = replaceRules.split('\n', Qt::SkipEmptyParts);
        for (const QString &line : lines) {
            int sepIdx = line.indexOf("=>");
            if (sepIdx <= 0) continue;
            QString src = line.left(sepIdx).trimmed();
            QString dst = line.mid(sepIdx + 2).trimmed();
            if (!src.isEmpty() && actualStreamUrl.contains(src)) {
                qDebug() << "[PlaybackManager] URL replace rule matched:" << src << "=>" << dst;
                actualStreamUrl.replace(src, dst);
                qDebug() << "[PlaybackManager] After replace:" << actualStreamUrl;
                
                
                if (!actualStreamUrl.startsWith("http://", Qt::CaseInsensitive) &&
                    !actualStreamUrl.startsWith("https://", Qt::CaseInsensitive) &&
                    !actualStreamUrl.contains("://")) {
                    actualStreamUrl = QUrl::fromPercentEncoding(actualStreamUrl.toUtf8());
                    qDebug() << "[PlaybackManager] After URL decode (local path):" << actualStreamUrl;
                }
            }
        }
        if (actualStreamUrl != beforeReplace) {
            qDebug() << "[PlaybackManager] URL changed by replace rules:" << beforeReplace << "->" << actualStreamUrl;
        }
    }

    
    
    
    {
        bool isFinalLocalPath = !actualStreamUrl.startsWith("http://", Qt::CaseInsensitive) &&
                                !actualStreamUrl.startsWith("https://", Qt::CaseInsensitive) &&
                                !actualStreamUrl.contains("://");
        if (isFinalLocalPath && !QFileInfo::exists(actualStreamUrl)) {
            qWarning() << "[PlaybackManager] Final local path does NOT exist:" << actualStreamUrl
                       << "- falling back to original streamUrl";
            actualStreamUrl = streamUrl;
        } else if (isFinalLocalPath) {
            qDebug() << "[PlaybackManager] Final local path verified exists:" << actualStreamUrl;
        }
    }

    
    
    bool isLocalPath = !actualStreamUrl.startsWith("http://", Qt::CaseInsensitive) &&
                       !actualStreamUrl.startsWith("https://", Qt::CaseInsensitive) &&
                       !actualStreamUrl.contains("://");

    if (isLocalPath) {
        qDebug() << "[PlaybackManager] Detected local path, adapting format...";
        
#ifdef Q_OS_WIN
        actualStreamUrl.replace('/', '\\');
#endif
        switch (playerType) {
        case PlayerType::MPV:
        case PlayerType::IINA:
        case PlayerType::MPC_Qt:
            
            actualStreamUrl = QUrl::fromLocalFile(actualStreamUrl).toString();
            qDebug() << "[PlaybackManager] Converted to file:// URI:" << actualStreamUrl;
            break;
        default:
            
            break;
        }
    }

    qDebug() << "[PlaybackManager] Final playback URL:" << actualStreamUrl;

    
    args << actualStreamUrl;

    
    m_currentMediaId = mediaId;
    m_currentMediaSourceId = mediaSourceId;

#ifdef Q_OS_WIN
    if (playerType == PlayerType::MPC_HC) {
        
        
        auto* mpcIpc = new MpcSlaveIPC(this);
        mpcIpc->start(startPositionTicks);
        QString hwnd = mpcIpc->hwndString();
        if (!hwnd.isEmpty()) {
            args.insert(args.size() - 1, "/slave"); 
            args.insert(args.size() - 1, hwnd);
        }
        m_ipc = mpcIpc;
    } else
#endif
    {
        m_ipc = ExternalPlayerIPC::create(playerType, ipcEndpoint, this);
        m_ipc->start(startPositionTicks);
    }

    
    auto *process = new QProcess(this);
    process->setProgram(playerPath);
    process->setArguments(args);

    
    
    QCoro::connect(m_core->mediaService()->reportPlaybackStart(mediaId, mediaSourceId, startPositionTicks),
                   this, [this](const QString &sessionId) {
        m_currentPlaySessionId = sessionId;
    });

    
    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, [this]() {
        if (!m_core || !m_ipc || m_currentMediaId.isEmpty() || m_currentPlaySessionId.isEmpty()) return;
        long long currentTicks = m_ipc->currentPositionTicks();
        m_core->mediaService()->reportPlaybackProgress(
            m_currentMediaId, m_currentMediaSourceId, currentTicks, false, m_currentPlaySessionId);
    });

    
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode, QProcess::ExitStatus status) {
        qDebug() << "[PlaybackManager] External player exited, code:" << exitCode
                 << "status:" << (status == QProcess::NormalExit ? "NormalExit" : "CrashExit");

        
        long long finalTicks = 0;
        if (m_ipc) {
            finalTicks = m_ipc->currentPositionTicks();
        }
        qDebug() << "[PlaybackManager] Final position:" << finalTicks
                 << "(" << ticksToTimeString(finalTicks) << ")";

        
        if (!m_currentMediaId.isEmpty() && !m_currentPlaySessionId.isEmpty()) {
            qDebug() << "[PlaybackManager] Reporting playback stopped, sessionId:" << m_currentPlaySessionId;
            m_core->mediaService()->reportPlaybackStopped(
                m_currentMediaId, m_currentMediaSourceId, finalTicks, m_currentPlaySessionId);
        }

        cleanupExternalPlayerState();

        if (m_extProcess == process) {
            m_extProcess = nullptr;
        }
        process->deleteLater();

        
        Q_EMIT playbackFinished();
    });

    
    connect(process, &QProcess::errorOccurred, this, [this, process](QProcess::ProcessError error) {
        qDebug() << "[PlaybackManager] External player error:" << error;
        if (error == QProcess::FailedToStart) {
            ModernToast::showMessage(tr("Failed to launch external player"), 2000);
        }
        cleanupExternalPlayerState();
        if (m_extProcess == process) {
            m_extProcess = nullptr;
        }
        process->deleteLater();
    });

    m_extProcess = process;
    process->start();
    m_progressTimer->start(PROGRESS_REPORT_INTERVAL_MS);

    
    QString playerName = QFileInfo(playerPath).baseName();
    ModernToast::showMessage(tr("Launching %1...").arg(playerName), 1500);

    qDebug() << "[PlaybackManager] Launching external player:"
             << playerPath
             << "Type:" << static_cast<int>(playerType)
             << "Args:" << args
             << "IPC:" << (m_ipc ? (m_ipc->isPrecise() ? "precise" : "estimation") : "none");
}




void PlaybackManager::launchIndependentWindow(const QString& mediaId, const QString& title,
                                                const QString& streamUrl, long long startPositionTicks,
                                                const QVariant& extraData)
{
    qDebug() << "[PlaybackManager] ===== launchIndependentWindow =====";
    qDebug() << "[PlaybackManager] mediaId:" << mediaId << "title:" << title;
    qDebug() << "[PlaybackManager] streamUrl:" << streamUrl;
    qDebug() << "[PlaybackManager] startPositionTicks:" << startPositionTicks
             << "(" << ticksToTimeString(startPositionTicks) << ")";

    
    if (m_independentWindow) {
        qDebug() << "[PlaybackManager] Reusing existing independent window";
        m_independentWindow->playMedia(mediaId, title, streamUrl, startPositionTicks, extraData);
        m_independentWindow->raise();
        m_independentWindow->activateWindow();
        return;
    }

    qDebug() << "[PlaybackManager] Creating new independent window";
    
    auto* playerWindow = new PlayerWindow(m_core);
    playerWindow->setAttribute(Qt::WA_DeleteOnClose, true);
    playerWindow->setMinimumSize(960, 540);
    playerWindow->resize(1280, 720);

    
    connect(playerWindow, &QObject::destroyed, this, [this]() {
        m_independentWindow = nullptr;
        qDebug() << "[PlaybackManager] Independent window destroyed";
        Q_EMIT playbackFinished();
    });

    m_independentWindow = playerWindow;

    playerWindow->show();
    playerWindow->playMedia(mediaId, title, streamUrl, startPositionTicks, extraData);
}
