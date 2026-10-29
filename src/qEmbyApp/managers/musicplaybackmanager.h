#pragma once

#include <QObject>
#include <QPointer>
#include <QTimer>
#include <models/media/mediaitem.h>
#include <qcorotask.h>

class QEmbyCore;
class MpvHttpStreamRelay;
struct mpv_handle;

class MusicPlaybackManager : public QObject {
    Q_OBJECT
public:
    enum State { Idle, Loading, Playing, Paused, Stopping, Error };
    Q_ENUM(State)
    static MusicPlaybackManager* instance();
    void init(QEmbyCore* core);
    void playQueue(QList<MediaItem> tracks, int index = 0);
    quint64 beginQueueRequest();
    QObject* queueRequestContext() const { return m_requestContext; }
    bool isCurrentQueueRequest(quint64 generation) const { return generation && generation == m_generation && !m_videoActive; }
    void stop();
    void togglePause();
    void next();
    void previous();
    void seek(double seconds);
    void setVolume(int volume);
    void setVideoActive(bool active);
    State state() const { return m_state; }
    MediaItem currentTrack() const;
    double position() const { return m_position; }
    double duration() const { return m_duration; }
    int volume() const { return m_volume; }
signals:
    void changed();
    void errorOccurred(const QString& message);
private:
    explicit MusicPlaybackManager();
    ~MusicPlaybackManager() override;
    QCoro::Task<void> loadTrack(quint64 generation);
    QCoro::Task<void> reportStart(quint64 generation);
    QCoro::Task<void> reportProgress();
    QCoro::Task<void> reportStop(QString item, QString source, QString session,
                                 qint64 ticks);
    void poll();
    void releaseBackend();
    void transitionTo(State state, const char* reason);
    void failCurrentTrack();
    void startIndex(int index);
    QPointer<QEmbyCore> m_core;
    MpvHttpStreamRelay* m_relay = nullptr;
    mpv_handle* m_mpv = nullptr;
    QTimer m_timer;
    QList<MediaItem> m_queue;
    int m_index = -1;
    State m_state = Idle;
    quint64 m_generation = 0;
    QString m_sourceId;
    QString m_sessionId;
    double m_position = 0;
    double m_duration = 0;
    int m_volume = 80;
    int m_reportTicks = 0;
    bool m_videoActive = false;
    bool m_progressPending = false;
    QObject* m_requestContext = nullptr;
};
