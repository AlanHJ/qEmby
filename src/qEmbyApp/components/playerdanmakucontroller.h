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

class PlayerDanmakuController : public QObject
{
    Q_OBJECT
public:
    explicit PlayerDanmakuController(QEmbyCore *core,
                                     MpvWidget *mpvWidget,
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
    int commentCount() const;
    DanmakuMediaContext mediaContext() const;
    QString activeTargetId() const;

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
    void applyTrackSelection();

    QCoro::Task<void> loadDanmakuTask(quint64 requestId,
                                      QString manualKeyword = QString());
    QCoro::Task<void> loadDanmakuCandidateTask(
        quint64 requestId,
        DanmakuMatchCandidate candidate,
        bool saveAsManualMatch);

    QPointer<QEmbyCore> m_core;
    QPointer<MpvWidget> m_mpvWidget;
    PlayerLaunchContext m_launchContext;
    DanmakuMediaContext m_mediaContext;
    QString m_assFilePath;
    QString m_sourceTitle;
    QString m_sourceProvider;
    QString m_activeTargetId;
    int m_commentCount = 0;
    int m_danmakuTrackId = -1;
    int m_selectedSubtitleTrackId = -1;
    bool m_fileLoaded = false;
    bool m_visible = true;
    bool m_loading = false;
    quint64 m_requestSerial = 0;
};

#endif 
