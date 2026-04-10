#ifndef PLAYERVIEW_H
#define PLAYERVIEW_H

#include "../baseview.h"
#include "../../components/mpvwidget.h"
#include "../../components/modernslider.h"
#include "../../components/loadingoverlay.h" 
#include "../../components/playerdanmakucontroller.h"
#include <models/media/mediaitem.h> 

#include <QVariant>
#include <qcorotask.h>

#include <QWidget>
#include <QPushButton>
#include <QLabel>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QRect>
#include <QPoint>
#include <QMenu>
#include <QKeyEvent>
#include <QProgressBar>
#include <QHash>
#include <QPointer>

class QEmbyCore;
class PlayerOverlayDialog;
class PlayerMediaSwitcherPanel;
class PlayerOsdLayer;
class PlayerLongPressHandler;
class PlayerStatisticsOverlay;

class PlayerView : public BaseView {
    Q_OBJECT
public:
    explicit PlayerView(QEmbyCore *core, QWidget *parent = nullptr);
    ~PlayerView() override;
    void prepareForStackLeave() override;

    
    void playMedia(const QString &mediaId, const QString &title, const QString &streamUrl, long long startPositionTicks = 0, const QVariant& sourceInfoVar = QVariant());

    
    bool isMediaPlaying() const;
    void pausePlayback();
    void resumePlayback();
    void stopAndReport(); 

protected:
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private slots:
    void onPositionChanged(double position);
    void onDurationChanged(double duration);
    void onPlaybackStateChanged(bool isPaused);
    void onMpvPropertyChanged(const QString &property, const QVariant &value);
    
    void togglePlayPause();
    void onSliderMoved(int value);
    
    
    void seekRelative(double delta, bool silent = false);
    
    
    void toggleMute();
    void changeVolume(int delta, bool silent = false);
    void onVolumeSliderMoved(int value);

    void showSpeedMenu();
    void showAudioMenu();
    void showSubtitleMenu();
    void showDanmakuMenu();
    void showDanmakuIdentifyDialog();
    void loadLocalDanmakuFile();
    void openSubtitleSettingsDialog();
    void openDanmakuSettingsDialog();
    void showSettingsMenu(); 
    
    void cycleVideoScale();
    void toggleFullscreenWindow();

    void showControls();
    void hideControls();
    void reportProgressToServer();
    void onBackClicked();


    
    
    void updateLoadingState();
    void updateDanmakuButtonState();

private:
    void setupUi();
    void updateTitleElision();
    void updateOverlayLayout();
    void clearMediaSwitcherCache();
    void applyMediaSwitcherMode();
    void updateMediaSwitcherButton();
    bool shouldShowDanmakuHudControls() const;
    bool useHudMediaSwitcher() const;
    QString formatMediaSwitcherPlaybackTitle(const MediaItem &item) const;
    void populateRightSidebarFromCache();
    void showHudMediaSwitcher();
    void hideHudMediaSwitcher();
    void syncHudMediaSwitcherContent();
    
    
    void setupRightSidebar();
    
    
    QCoro::Task<void> showRightSidebar();
    QCoro::Task<void> ensureMediaSwitcherDataLoaded();
    QCoro::Task<void> switchFromMediaSwitcher(QString mediaId,
                                              QString title,
                                              long long startPositionTicks);
    
    void hideRightSidebar(bool immediate = false);
    void setEffectivePlaybackSpeed(double speed);
    void handlePointerActivity(const QPoint &globalPos);
    void setCursorHidden(bool hidden);
    bool areControlsFullyVisible() const;

    void stopTransientUiAnimations(bool immediate = false);
    void beginViewTeardown();

    QPushButton* createHudButton(const QString& iconPath, const QSize& size = QSize(24, 24));
    QString formatTime(double seconds, double totalSeconds) const;
    void applySubtitleStyleSettings();
    
    void showToast(const QString& msg);
    void updateStatisticsDisplay();
    void setScaleIcon(); 
    QString formatDanmakuProviderLabel(QString provider) const;
    QString buildDanmakuSummaryText() const;
    QString buildDanmakuTooltipText() const;
    void closeActivePlayerDialog();
    void trackPlayerDialog(PlayerOverlayDialog *dialog);

    void showCenteredPopup(QWidget* popup, QPushButton* btn); 
    QWidget* m_activePopup = nullptr; 
    QPointer<PlayerOverlayDialog> m_activePlayerDialog;

    
    static QCoro::Task<void> executeFetchLogo(QPointer<PlayerView> safeThis, QEmbyCore* core, QString mediaId);

    MpvWidget *m_mpvWidget;

    
    QWidget *m_topHUD;
    QWidget *m_bottomHUD;
    PlayerStatisticsOverlay *m_statisticsOverlay;
    LoadingOverlay *m_loadingOverlay; 

    
    QLabel *m_logoLabel;
    QGraphicsOpacityEffect *m_logoOpacity;

    
    QGraphicsOpacityEffect *m_speedOpacity;

    
    PlayerOsdLayer *m_osdLayer = nullptr;
    
    PlayerLongPressHandler *m_longPressHandler = nullptr;

    
    QWidget *m_rightSidebar;
    QWidget *m_rightTrigger;
    QPropertyAnimation *m_rightSidebarAnim;
    
    
    QLabel *m_sidebarTitleLabel;
    class QListWidget *m_resumeList;

    QPushButton *m_backBtn;
    QLabel *m_titleLabel;
    QPushButton *m_minBtn;
    QPushButton *m_maxBtn;
    QPushButton *m_closeBtn;
    QLabel *m_networkSpeedLabel;

    
    QLabel *m_currentTimeLabel;
    ModernSlider *m_progressSlider;
    QLabel *m_totalTimeLabel;

    
    QPushButton *m_playPauseBtn;
    QPushButton *m_rewindBtn;
    QPushButton *m_forwardBtn;
    
    
    QPushButton *m_volumeBtn;
    ModernSlider *m_volumeSlider;

    QLabel *m_toastLabel;
    QTimer *m_toastTimer;

    QPushButton *m_speedBtn = nullptr;
    QPushButton *m_mediaSwitchBtn = nullptr;
    QPushButton *m_audioBtn = nullptr;
    QPushButton *m_subtitleBtn = nullptr;
    QPushButton *m_danmakuBtn = nullptr;
    QPushButton *m_settingsBtn = nullptr;
    QPushButton *m_scaleBtn = nullptr;      
    QPushButton *m_fullscreenBtn = nullptr; 
    PlayerMediaSwitcherPanel *m_mediaSwitchDrawer = nullptr;
    int m_bottomHudBaseHeight = 110;

    PlayerDanmakuController *m_danmakuController = nullptr;

    QGraphicsOpacityEffect *m_topOpacity;
    QGraphicsOpacityEffect *m_bottomOpacity;
    QParallelAnimationGroup *m_fadeGroup;
    QTimer *m_hideTimer; 
    QTimer *m_reportTimer;
    QTimer *m_mousePollTimer;
    
    
    QTimer *m_bufferTimer;

    QString m_currentMediaId;
    QString m_currentMediaSourceId; 
    QString m_currentPlaySessionId; 
    MediaItem m_currentMediaItem;
    MediaSourceInfo m_currentMediaSourceInfo;
    
    
    QString m_originalStreamUrl; 
    QVariant m_currentSourceInfoVar; 

    bool m_isPlaying;
    bool m_isBuffering = false; 
    bool m_isSeeking = false;   
    
    double m_currentPosition;
    double m_totalDuration = 0.0; 
    double m_pendingSeekSeconds = 0.0;
    
    double m_currentSpeed = 1.0;
    int m_videoScaleMode = 1;
    
    
    double m_currentVolume = 100.0;
    bool m_isMuted = false;

    bool m_showNetworkSpeed = true; 
    bool m_showStatisticsOverlay = false; 
    bool m_hasSetVideoSize = false; 
    bool m_hasReportedStop = false; 
    bool m_isViewTearingDown = false;
    bool m_isRightSidebarVisible = false; 
    
    
    QString m_switcherPendingItemId;
    QString m_switcherPendingTitle;
    long long m_switcherPendingTicks = 0;
    
    
    bool m_isSeriesMode = false;
    QString m_seriesId;
    QString m_seriesName;

    
    QString m_switcherCacheMediaId;
    bool m_switcherCacheReady = false;
    QList<MediaItem> m_switcherResumeItems;
    QList<MediaItem> m_switcherSeriesSeasons;
    QHash<QString, QList<MediaItem>> m_switcherSeasonEpisodes;

    QString m_fullTitle;
    
    QRect m_originalGeometry;
    bool m_wasMaximized = false;
    QPoint m_dragPos;
    QPoint m_lastMousePos; 
    bool m_didDrag = false; 
    QTimer *m_singleClickTimer = nullptr; 

    
    int m_targetAudioStreamIndex = -2;
    int m_targetSubStreamIndex = -2;
};

#endif 
