#ifndef PLAYERDANMAKUCONTROLLER_H
#define PLAYERDANMAKUCONTROLLER_H

#include <QObject>
#include <QPointer>
#include <QVariantMap>

#include <models/danmaku/danmakumodels.h>
#include <models/media/playerlaunchcontext.h>
#include <qcorotask.h>

class QEmbyCore;
class MpvWidget;
class NativeDanmakuOverlay;

class PlayerDanmakuController : public QObject
{
    Q_OBJECT
public:
    explicit PlayerDanmakuController(QEmbyCore *core,
                                     MpvWidget *mpvWidget,
                                     NativeDanmakuOverlay *nativeDanmakuOverlay,
                                     QObject *parent = nullptr);

    void setPlaybackContext(const PlayerLaunchContext &context);
    void clearPlaybackContext();

    bool isDanmakuEnabled() const;
    bool isDanmakuVisible() const;
    bool hasDanmakuTrack() const;
    bool hasPreparedDanmaku() const;
    bool isLoading() const;
    bool hasPlaybackContext() const;
    QString sourceTitle() const;
    QString sourceProvider() const;
    QString sourceServerId() const;
    QString sourceServerName() const;
    int commentCount() const;
    DanmakuMediaContext mediaContext() const;
    QString activeTargetId() const;
    QString activeEndpointId() const;

    QList<QVariantMap> contentSubtitleTracks() const;
    void selectSubtitleTrack(const QVariant &data);

    void setDanmakuEnabled(bool enabled);
    void setDanmakuVisible(bool visible);
    void reload(const QString &manualKeyword = QString());
    void loadFromCandidate(DanmakuMatchCandidate candidate,
                           bool saveAsManualMatch = true);
    void loadLocalFile(QString filePath);

signals:
    void stateChanged();
    void toastRequested(const QString &message);

private:
    void launchTask(QCoro::Task<void> &&task);
    DanmakuMediaContext buildMediaContext(const PlayerLaunchContext &context) const;
    bool isDanmakuTrackMap(const QVariantMap &trackMap) const;
    void onTrackListChanged();
    void syncSubtitleSelectionFromTrackList();
    void attachDanmakuTrack();
    void refreshDanmakuTrackId(int remainingRetries = 5);
    void removeDanmakuTrack();
    bool prefersNativeRenderer() const;
    bool shouldUseNativeRenderer() const;
    void updateDanmakuPresentation();
    void applyTrackSelection();
    void applyDanmakuMotionStabilityProfile();
    void clearDanmakuMotionStabilityProfile();

    QCoro::Task<void> loadDanmakuTask(quint64 requestId,
                                      QString manualKeyword = QString());
    QCoro::Task<void> loadDanmakuCandidateTask(
        quint64 requestId,
        DanmakuMatchCandidate candidate,
        bool saveAsManualMatch);

    QPointer<QEmbyCore> m_core;
    QPointer<MpvWidget> m_mpvWidget;
    QPointer<NativeDanmakuOverlay> m_nativeDanmakuOverlay;
    PlayerLaunchContext m_launchContext;
    DanmakuMediaContext m_mediaContext;
    QString m_assFilePath;
    QString m_sourceTitle;
    QString m_sourceProvider;
    QString m_sourceServerId;
    QString m_sourceServerName;
    QString m_activeTargetId;
    QString m_activeEndpointId;
    QList<DanmakuComment> m_commentPayload;
    int m_commentCount = 0;
    int m_danmakuTrackId = -1;
    int m_selectedSubtitleTrackId = -1;
    bool m_fileLoaded = false;
    bool m_visible = true;
    bool m_loading = false;
    bool m_motionStabilityProfileApplied = false;
    bool m_nativePayloadDirty = false;
    quint64 m_requestSerial = 0;
};

#endif 
