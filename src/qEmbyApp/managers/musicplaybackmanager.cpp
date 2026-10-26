#include "musicplaybackmanager.h"
#include "../components/mpvhttpstreamrelay.h"
#include "../utils/hlsplaylistutils.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <services/manager/servermanager.h>
#include <api/proxymanager.h>
#include <mpv/client.h>
#include <QCoreApplication>
#include <QDebug>
#include <QUrlQuery>
#include <stdexcept>
#include <QUuid>
#include <cmath>

MusicPlaybackManager* MusicPlaybackManager::instance()
{
    static MusicPlaybackManager manager;
    return &manager;
}

MusicPlaybackManager::MusicPlaybackManager()
{
    m_relay = new MpvHttpStreamRelay(this);
    m_timer.setInterval(250);
    connect(&m_timer, &QTimer::timeout, this, &MusicPlaybackManager::poll);
    connect(qApp, &QCoreApplication::aboutToQuit, this, &MusicPlaybackManager::stop);
}

MusicPlaybackManager::~MusicPlaybackManager() { releaseBackend(); }

void MusicPlaybackManager::init(QEmbyCore* core)
{
    if (m_core == core) return;
    stop();
    if (m_core) disconnect(m_core->serverManager(), nullptr, this, nullptr);
    m_core = core;
    if (core) connect(core->serverManager(), &ServerManager::activeServerChanged,
                      this, [this]() { stop(); m_videoActive = false; });
}

MediaItem MusicPlaybackManager::currentTrack() const
{
    return m_index >= 0 && m_index < m_queue.size() ? m_queue[m_index] : MediaItem{};
}

void MusicPlaybackManager::transitionTo(State state, const char* reason)
{
    if (m_state == state) return;
    qDebug() << "[MusicPlaybackManager] State changed"
             << "| from=" << int(m_state) << "| to=" << int(state)
             << "| reason=" << reason << "| generation=" << m_generation
             << "| index=" << m_index << "| queueSize=" << m_queue.size()
             << "| itemId=" << currentTrack().id;
    m_state = state;
}

void MusicPlaybackManager::failCurrentTrack()
{
    const auto queue = m_queue;
    const int index = m_index;
    stop();
    m_queue = queue;
    m_index = index;
    transitionTo(Error, "backend-error");
    Q_EMIT changed();
    Q_EMIT errorOccurred(tr("Unable to play this track."));
}

void MusicPlaybackManager::releaseBackend()
{
    m_timer.stop();
    
    
    if (m_mpv) { mpv_terminate_destroy(m_mpv); m_mpv = nullptr; }
    m_relay->stop();
}

void MusicPlaybackManager::stop()
{
    ++m_generation;
    delete m_requestContext;
    m_requestContext = nullptr;
    const auto track = currentTrack();
    const QString source = m_sourceId;
    const QString session = m_sessionId;
    const qint64 ticks = qint64(m_position * 10000000.0);
    transitionTo(Stopping, "stop-request");
    releaseBackend();
    m_queue.clear(); m_index = -1;
    m_sourceId.clear(); m_sessionId.clear();
    m_position = m_duration = 0;
    m_progressPending = false;
    transitionTo(Idle, "backend-released");
    Q_EMIT changed();
    if (m_core && !track.id.isEmpty() && !session.isEmpty())
        QCoro::connect(reportStop(track.id, source, session, ticks), this, []() {});
}

void MusicPlaybackManager::setVideoActive(bool active)
{
    m_videoActive = active;
    if (active) stop();
}

void MusicPlaybackManager::playQueue(QList<MediaItem> tracks, int index)
{
    if (m_videoActive) { Q_EMIT errorOccurred(tr("Stop video playback before playing music.")); return; }
    if (!m_core || tracks.isEmpty() || index < 0 || index >= tracks.size()) return;
    stop();
    m_queue = std::move(tracks);
    startIndex(index);
}

quint64 MusicPlaybackManager::beginQueueRequest()
{
    if (m_videoActive) { Q_EMIT errorOccurred(tr("Stop video playback before playing music.")); return 0; }
    stop();
    m_requestContext = new QObject(this);
    return m_generation;
}

void MusicPlaybackManager::startIndex(int index)
{
    if (index < 0 || index >= m_queue.size() || m_videoActive) return;
    const auto queue = m_queue;
    stop();
    m_queue = queue;
    m_index = index;
    m_requestContext = new QObject(this);
    transitionTo(Loading, "queue-selection");
    Q_EMIT changed();
    QCoro::connect(loadTrack(m_generation), this, []() {});
}

QCoro::Task<void> MusicPlaybackManager::loadTrack(quint64 generation)
{
    QPointer<MusicPlaybackManager> guard(this);
    QPointer<MediaService> service(m_core->mediaService());
    const auto profile = m_core->serverManager()->activeProfile();
    const MediaItem track = currentTrack();
    try {
        const auto info = co_await service->getPlaybackInfo(track.id, false, m_requestContext);
        if (!guard || !service || generation != guard->m_generation) co_return;
        if (info.mediaSources.isEmpty()) throw std::runtime_error("no audio source");
        auto source = info.mediaSources.first();
        source.playbackKind = PlaybackKind::Audio;
        const QString stream = service->getStreamUrl(track.id, source);
        if (HlsPlaylistUtils::sameOrigin(QUrl(profile.url), QUrl(stream)))
            source.requiredHttpHeaders.insert(QStringLiteral("X-Emby-Token"), profile.accessToken);
        if (stream.isEmpty()) throw std::runtime_error("no audio URL");
        guard->m_mpv = mpv_create();
        if (!guard->m_mpv) throw std::runtime_error("audio backend unavailable");
        mpv_set_option_string(guard->m_mpv, "vid", "no");
        mpv_set_option_string(guard->m_mpv, "vo", "null");
        mpv_set_option_string(guard->m_mpv, "terminal", "no");
        mpv_set_option_string(guard->m_mpv, "config", "no");
        mpv_set_option_string(guard->m_mpv, "idle", "yes");
        if (mpv_initialize(guard->m_mpv) < 0) throw std::runtime_error("audio init failed");
        const auto local = guard->m_relay->prepare(QUrl(stream), profile.id,
                              ProxyManager::instance()->resolveForServer(profile), source.requiredHttpHeaders,
                              QUrl(profile.url), profile.ignoreSslVerification);
        if (local.isEmpty()) throw std::runtime_error("audio relay unavailable");
        const QByteArray url = local.toEncoded();
        const char* command[] = {"loadfile", url.constData(), nullptr};
        if (mpv_command(guard->m_mpv, command) < 0) throw std::runtime_error("audio load failed");
        guard->m_sourceId = source.id;
        guard->m_sessionId = info.playSessionId.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : info.playSessionId;
        guard->setVolume(guard->m_volume);
        guard->m_reportTicks = 0;
        guard->m_timer.start();
    } catch (const std::exception&) {
        if (!guard || generation != guard->m_generation) co_return;
        guard->releaseBackend();
        guard->transitionTo(Error, "source-resolution-error");
        qWarning() << "[MusicPlaybackManager] Audio source failed | generation=" << generation;
        Q_EMIT guard->changed();
        Q_EMIT guard->errorOccurred(tr("Unable to play this track."));
    }
}

void MusicPlaybackManager::poll()
{
    if (!m_mpv) return;
    for (;;) {
        const auto* event = mpv_wait_event(m_mpv, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        if (event->event_id == MPV_EVENT_FILE_LOADED) {
            transitionTo(Playing, "file-loaded");
            QCoro::connect(reportStart(m_generation), this, []() {});
        } else if (event->event_id == MPV_EVENT_END_FILE) {
            const auto* ended = static_cast<mpv_event_end_file*>(event->data);
            if (ended && ended->reason == MPV_END_FILE_REASON_EOF) {
                if (m_index + 1 < m_queue.size()) startIndex(m_index + 1);
                else stop();
                return;
            }
            if (ended && ended->reason == MPV_END_FILE_REASON_ERROR) {
                failCurrentTrack();
                return;
            }
        }
    }
    mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &m_position);
    mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &m_duration);
    if (!std::isfinite(m_position) || m_position < 0) m_position = 0;
    if (!std::isfinite(m_duration) || m_duration < 0) m_duration = 0;
    if (++m_reportTicks >= 40 && !m_progressPending) {
        m_reportTicks = 0;
        QCoro::connect(reportProgress(), this, []() {});
    }
    Q_EMIT changed();
}

void MusicPlaybackManager::togglePause()
{
    if (!m_mpv || (m_state != Playing && m_state != Paused)) return;
    int paused = m_state == Playing;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused);
    transitionTo(paused ? Paused : Playing, "pause-toggle");
    Q_EMIT changed();
    if (!m_progressPending) QCoro::connect(reportProgress(), this, []() {});
}
void MusicPlaybackManager::next() { if (m_index + 1 < m_queue.size()) startIndex(m_index + 1); }
void MusicPlaybackManager::previous() { if (m_index > 0) startIndex(m_index - 1); }
void MusicPlaybackManager::seek(double seconds)
{
    if (m_mpv) { double value = qBound(0.0, seconds, m_duration); mpv_set_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &value); }
}
void MusicPlaybackManager::setVolume(int volume)
{
    m_volume = qBound(0, volume, 100);
    if (m_mpv) { double value = m_volume; mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &value); }
}

QCoro::Task<void> MusicPlaybackManager::reportStart(quint64 generation)
{
    QPointer<MusicPlaybackManager> guard(this);
    QPointer<MediaService> service(m_core->mediaService());
    const QString item = currentTrack().id;
    const QString source = m_sourceId;
    const QString sessionId = m_sessionId;
    try {
        const auto session = co_await service->reportPlaybackStart(item, source, 0, sessionId, true);
        if (guard && service && generation != guard->m_generation)
            co_await service->reportPlaybackStopped(item, source, 0, session);
    } catch (...) { qWarning() << "[MusicPlaybackManager] Start report failed"; }
}
QCoro::Task<void> MusicPlaybackManager::reportProgress()
{
    if (m_sessionId.isEmpty() || !m_core) co_return;
    QPointer<MusicPlaybackManager> guard(this);
    const quint64 generation = m_generation;
    m_progressPending = true;
    try {
        co_await m_core->mediaService()->reportPlaybackProgress(currentTrack().id, m_sourceId,
                    qint64(m_position * 10000000.0), m_state == Paused, m_sessionId);
    } catch (...) { qWarning() << "[MusicPlaybackManager] Progress report failed"; }
    if (guard && guard->m_generation == generation) guard->m_progressPending = false;
}
QCoro::Task<void> MusicPlaybackManager::reportStop(QString item, QString source, QString session, qint64 ticks)
{
    if (!m_core) co_return;
    try { co_await m_core->mediaService()->reportPlaybackStopped(item, source, ticks, session); }
    catch (...) { qWarning() << "[MusicPlaybackManager] Stop report failed"; }
}
