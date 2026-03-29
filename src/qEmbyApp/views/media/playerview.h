#ifndef PLAYERVIEW_H
#define PLAYERVIEW_H

#include "../baseview.h"
#include "../../components/mpvwidget.h"
#include "../../components/modernslider.h"
#include "../../components/loadingoverlay.h" 
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

class QEmbyCore;

class PlayerView : public BaseView {
    Q_OBJECT
public:
    explicit PlayerView(QEmbyCore *core, QWidget *parent = nullptr);
    ~PlayerView() override;

    
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
    void showSettingsMenu(); 
    
    void cycleVideoScale();
    void toggleFullscreenWindow();

    void showControls();
    void hideControls();
    void reportProgressToServer();
    void onBackClicked();

    
    void showMiniOsd(const QString& type);
    void hideMiniOsd();
    
    
    void updateLoadingState();

private:
    void setupUi();
    void updateTitleElision();
    
    
    void setupRightSidebar();
    
    
    QCoro::Task<void> showRightSidebar();
    
    void hideRightSidebar();

    QPushButton* createHudButton(const QString& iconPath, const QSize& size = QSize(24, 24));
    QString formatTime(double seconds, double totalSeconds) const;
    
    void showToast(const QString& msg);
    void updateInfoDisplay();
    void setScaleIcon(); 

    void showCenteredPopup(QWidget* popup, QPushButton* btn); 
    QWidget* m_activePopup = nullptr; 

    
    static QCoro::Task<void> executeFetchLogo(QPointer<PlayerView> safeThis, QEmbyCore* core, QString mediaId);

    MpvWidget *m_mpvWidget;

    
    QWidget *m_topHUD;
    QWidget *m_bottomHUD;
    QLabel  *m_infoOverlay; 
    LoadingOverlay *m_loadingOverlay; 

    
    QLabel *m_logoLabel;
    QGraphicsOpacityEffect *m_logoOpacity;

    
    QGraphicsOpacityEffect *m_speedOpacity;

    
    QWidget *m_osdContainer;
    QProgressBar *m_osdSeekLine;
    QLabel *m_osdSeekTimeLabel;
    QLabel *m_osdVolumeLabel;
    QGraphicsOpacityEffect *m_osdOpacity;
    QPropertyAnimation *m_osdAnim;
    QTimer *m_osdHideTimer;

    
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

    QPushButton *m_speedBtn;
    QPushButton *m_audioBtn;
    QPushButton *m_subtitleBtn;
    QPushButton *m_settingsBtn;
    QPushButton *m_scaleBtn;      
    QPushButton *m_fullscreenBtn; 

    QGraphicsOpacityEffect *m_topOpacity;
    QGraphicsOpacityEffect *m_bottomOpacity;
    QParallelAnimationGroup *m_fadeGroup;
    QTimer *m_hideTimer; 
    QTimer *m_reportTimer;
    QTimer *m_mousePollTimer;
    
    
    QTimer *m_seekTimer;
    
    
    QTimer *m_bufferTimer;

    int m_seekDirection = 0;
    double m_seekRate = 2.0;

    QString m_currentMediaId;
    QString m_currentMediaSourceId; 
    QString m_currentPlaySessionId; 
    
    
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
    bool m_showInfoOverlay = false; 
    bool m_hasSetVideoSize = false; 
    bool m_hasReportedStop = false; 
    bool m_isRightSidebarVisible = false; 
    
    
    QString m_sidebarPendingItemId;
    QString m_sidebarPendingTitle;
    long long m_sidebarPendingTicks = 0;
    
    
    bool m_isSeriesMode = false;
    QString m_seriesId;
    QString m_seriesName;

    
    QString m_sidebarCachedForMediaId;

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
