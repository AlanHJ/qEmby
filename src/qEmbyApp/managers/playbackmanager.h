#ifndef PLAYBACKMANAGER_H
#define PLAYBACKMANAGER_H

#include <QObject>
#include <QPointer>
#include <QVariant>
#include <QSet>
#include <qcorotask.h>
#include "externalplayeripc.h"

class QEmbyCore;
class QProcess;
class PlayerWindow;
class QTimer;
class MpvHttpStreamRelay;
struct MediaSourceInfo;




class PlaybackManager : public QObject
{
    Q_OBJECT
public:
    static PlaybackManager* instance();

    
    void init(QEmbyCore* core);
    quint64 beginVideoIntent();
    void cancelVideoIntent(quint64 generation);
    bool isCurrentVideoIntent(quint64 generation) const { return generation == m_playIntent; }
    quint64 videoIntent() const { return m_playIntent; }
    void registerVideoOwner(QObject* owner);
    bool hasUnconfirmedExternalPlayback() const { return m_externalUnconfirmed; }
    void acknowledgeExternalStop();

    
    void startPlayback(const QString& mediaId, const QString& title,
                       const QString& streamUrl, long long startPositionTicks,
                       const QVariant& extraData = QVariant());

    
    void startExternalPlayback(const QString& playerPath,
                               const QString& mediaId, const QString& title,
                               const QString& streamUrl, long long startPositionTicks,
                               const QVariant& extraData = QVariant());

    
    void startInternalPlayback(const QString& mediaId, const QString& title,
                               const QString& streamUrl, long long startPositionTicks,
                               const QVariant& extraData = QVariant());

    
    void stopExternalPlayer();

signals:
    void externalCompletionUnconfirmed(bool unconfirmed);
    
    void requestEmbeddedPlay(const QString& mediaId, const QString& title,
                             const QString& streamUrl, long long startPositionTicks,
                             const QVariant& extraData);

    
    void playbackFinished();

private:
    explicit PlaybackManager(QObject *parent = nullptr);
    ~PlaybackManager() = default;

    void launchExternalPlayer(const QString& mediaId, const QString& title,
                              const QString& streamUrl, long long startPositionTicks,
                              const QVariant& extraData,
                              const QString& playerPathOverride = QString());

    void launchIndependentWindow(const QString& mediaId, const QString& title,
                                  const QString& streamUrl, long long startPositionTicks,
                                  const QVariant& extraData);

    void closeIndependentWindow();

    
    
    static PlayerType identifyPlayerType(const QString& playerPath);

    
    static QStringList buildPlayerArgs(PlayerType type,
                                        long long startPositionTicks,
                                        int audioStreamIndex,
                                        int subtitleStreamIndex,
                                        bool subtitleDisabled,
                                        const MediaSourceInfo* sourceInfo);

    
    static QString ticksToTimeString(long long ticks);

    
    static double ticksToSeconds(long long ticks);

    
    static int computeRelativeStreamIndex(const MediaSourceInfo& source,
                                           const QString& streamType,
                                           int globalIndex);

    
    static QString generateIpcEndpoint(PlayerType type);

    
    void cleanupExternalPlayerState();

    
    QEmbyCore* m_core = nullptr;
    quint64 m_playIntent = 0;
    QSet<QObject*> m_videoOwners;
    bool m_videoPending = false;
    bool m_externalUnconfirmed = false;
    QPointer<QProcess> m_extProcess;             
    QPointer<PlayerWindow> m_independentWindow;  

    
    ExternalPlayerIPC* m_ipc = nullptr;          
    QTimer* m_progressTimer = nullptr;           
    QString m_currentMediaId;                    
    QString m_currentMediaSourceId;              
    QString m_currentPlaySessionId;              
    QString m_externalLiveStreamId;
    MpvHttpStreamRelay* m_externalRelay = nullptr;
};

#endif 
