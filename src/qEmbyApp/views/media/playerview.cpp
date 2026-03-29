#include "playerview.h"
#include "../../components/modernscrollpanel.h"
#include "qembycore.h"
#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPalette>
#include <QPointer> 
#include <QSettings>
#include <QTime>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>
#include <cmath>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <models/media/playbackinfo.h>
#include <models/profile/serverprofile.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>

PlayerView::PlayerView(QEmbyCore *core, QWidget *parent)
    : BaseView(core, parent), m_isPlaying(false), m_currentPosition(0.0), m_totalDuration(0.0), m_activePopup(nullptr)
{

    setProperty("isImmersive", true);
    setMouseTracking(true);
    setAutoFillBackground(true);
    setFocusPolicy(Qt::StrongFocus); 

    m_isRightSidebarVisible = false; 

    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &PlayerView::hideControls);

    m_reportTimer = new QTimer(this);
    m_reportTimer->setInterval(10000); 
    connect(m_reportTimer, &QTimer::timeout, this, &PlayerView::reportProgressToServer);

    
    m_seekTimer = new QTimer(this);
    connect(m_seekTimer, &QTimer::timeout, this,
            [this]()
            {
                m_seekTimer->setInterval(100); 
                bool isHudVisible = (m_topOpacity->opacity() > 0.0);
                seekRelative(m_seekDirection * m_seekRate, !isHudVisible); 
                m_seekRate *= 1.15;                                        
                if (m_seekRate > 30.0)
                {
                    m_seekRate = 30.0; 
                }
            });

    
    m_bufferTimer = new QTimer(this);
    m_bufferTimer->setInterval(500);
    connect(m_bufferTimer, &QTimer::timeout, this,
            [this]()
            {
                if (m_totalDuration > 0 && m_mpvWidget && m_mpvWidget->controller())
                {
                    double bufferedPos = m_currentPosition;

                    
                    QVariant stateVar = m_mpvWidget->controller()->getProperty("demuxer-cache-state");
                    if (stateVar.isValid() && stateVar.type() == QVariant::Map)
                    {
                        auto ranges = stateVar.toMap()["seekable-ranges"].toList();
                        if (!ranges.isEmpty())
                        {
                            
                            bufferedPos = ranges.last().toMap()["end"].toDouble();
                        }
                    }
                    else
                    {
                        
                        double cacheDuration =
                            m_mpvWidget->controller()->getProperty("demuxer-cache-duration").toDouble();
                        if (cacheDuration > 0)
                        {
                            bufferedPos = m_currentPosition + cacheDuration;
                        }
                    }

                    if (bufferedPos > m_totalDuration)
                    {
                        bufferedPos = m_totalDuration;
                    }

                    
                    m_progressSlider->setBufferValue(static_cast<int>(bufferedPos));
                }
            });

    
    m_mousePollTimer = new QTimer(this);
    m_mousePollTimer->setInterval(100);
    connect(m_mousePollTimer, &QTimer::timeout, this,
            [this]()
            {
                if (!m_isPlaying)
                {
                    if (m_topOpacity->opacity() < 1.0)
                    {
                        showControls();
                    }
                    return;
                }

                QPoint currentPos = QCursor::pos();
                if (currentPos == m_lastMousePos)
                {
                    return; 
                }

                m_lastMousePos = currentPos;

                
                if (this->window()->isMinimized() || !this->isVisible())
                {
                    return;
                }

                QPoint localPos = this->mapFromGlobal(currentPos);
                bool isMouseInside = this->rect().contains(localPos);

                
                if (isMouseInside)
                {
                    showControls();
                }
            });

    m_toastTimer = new QTimer(this);
    m_toastTimer->setSingleShot(true);
    connect(m_toastTimer, &QTimer::timeout, this, [this]() { m_toastLabel->hide(); });

    
    m_singleClickTimer = new QTimer(this);
    m_singleClickTimer->setSingleShot(true);
    m_singleClickTimer->setInterval(200);
    connect(m_singleClickTimer, &QTimer::timeout, this, &PlayerView::togglePlayPause);

    setupUi();
    this->installEventFilter(this);
}


PlayerView::~PlayerView()
{
    stopAndReport();
}


bool PlayerView::isMediaPlaying() const
{
    return m_isPlaying;
}


void PlayerView::pausePlayback()
{
    if (m_mpvWidget)
        m_mpvWidget->pause();
}



void PlayerView::resumePlayback()
{
    if (!m_mpvWidget || m_currentMediaId.isEmpty())
        return;

    
    QString newStreamUrl = m_originalStreamUrl;
    if (m_currentSourceInfoVar.isValid() && m_currentSourceInfoVar.canConvert<MediaSourceInfo>())
    {
        MediaSourceInfo sourceInfo = m_currentSourceInfoVar.value<MediaSourceInfo>();
        QString directUrl = m_core->mediaService()->getStreamUrl(m_currentMediaId, sourceInfo);
        if (!directUrl.isEmpty())
        {
            newStreamUrl = directUrl;
        }
    }

    
    m_pendingSeekSeconds = m_currentPosition;
    m_isBuffering = true;
    updateLoadingState();
    m_mpvWidget->loadMedia(newStreamUrl);
    m_mpvWidget->play(); 
}


QPushButton *PlayerView::createHudButton(const QString &iconPath, const QSize &size)
{
    auto *btn = new QPushButton(this);
    btn->setProperty("class", "player-hud-btn");
    btn->setIcon(QIcon(iconPath));
    btn->setIconSize(size);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFocusPolicy(Qt::NoFocus); 
    btn->setFixedSize(size.width() + 16, size.height() + 16);
    return btn;
}

void PlayerView::setupUi()
{
    m_mpvWidget = new MpvWidget(this);
    m_mpvWidget->setAutoFillBackground(true);
    m_mpvWidget->installEventFilter(this); 
    m_mpvWidget->setMouseTracking(true);

    
    m_loadingOverlay = new LoadingOverlay(this);
    m_loadingOverlay->setObjectName("playerLoadingOverlay");
    
    m_loadingOverlay->setImmersive(true);
    
    m_loadingOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    
    connect(m_mpvWidget->controller(), &MpvController::fileLoaded, this,
            [this]()
            {
                m_isBuffering = false;
                updateLoadingState();
            });
    

    
    m_osdContainer = new QWidget(this);
    m_osdContainer->setObjectName("osdContainer");
    m_osdContainer->setAttribute(Qt::WA_TransparentForMouseEvents); 
    m_osdContainer->hide();

    m_osdOpacity = new QGraphicsOpacityEffect(m_osdContainer);
    m_osdOpacity->setOpacity(0.0);
    m_osdContainer->setGraphicsEffect(m_osdOpacity);

    m_osdAnim = new QPropertyAnimation(m_osdOpacity, "opacity", this);
    m_osdAnim->setDuration(250);

    m_osdHideTimer = new QTimer(this);
    m_osdHideTimer->setSingleShot(true);
    connect(m_osdHideTimer, &QTimer::timeout, this, &PlayerView::hideMiniOsd);

    m_osdSeekLine = new QProgressBar(m_osdContainer);
    m_osdSeekLine->setObjectName("osdSeekLine");
    m_osdSeekLine->setTextVisible(false);
    m_osdSeekLine->setFixedHeight(3);

    m_osdSeekTimeLabel = new QLabel(m_osdContainer);
    m_osdSeekTimeLabel->setObjectName("osdSeekTimeLabel");
    m_osdSeekTimeLabel->setAlignment(Qt::AlignCenter);

    m_osdVolumeLabel = new QLabel(m_osdContainer);
    m_osdVolumeLabel->setObjectName("osdVolumeLabel");
    m_osdVolumeLabel->setAlignment(Qt::AlignCenter);

    

    setupRightSidebar();

    
    m_rightTrigger = new QWidget(this);
    m_rightTrigger->setFixedWidth(15);
    m_rightTrigger->setCursor(Qt::PointingHandCursor);
    m_rightTrigger->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_rightTrigger->installEventFilter(this);

    
    m_logoLabel = new QLabel(this);
    m_logoLabel->setObjectName("playerLogoLabel");
    m_logoLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_logoLabel->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_logoLabel->hide();

    
    m_networkSpeedLabel = new QLabel("0.0 KB/s", this); 
    m_networkSpeedLabel->setObjectName("playerNetworkSpeed");
    m_networkSpeedLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true); 
    m_networkSpeedLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);

    
    m_topHUD = new QWidget(this);
    m_topHUD->setObjectName("playerTopHUD");
    
    m_topHUD->setFixedHeight(32);
    m_topHUD->installEventFilter(this); 

    auto *topLayout = new QHBoxLayout(m_topHUD);
    topLayout->setContentsMargins(0, 0, 0, 0); 
    topLayout->setSpacing(0);

    
    m_backBtn = new QPushButton(m_topHUD);
    m_backBtn->setObjectName("hud-back-btn");
    m_backBtn->setIcon(QIcon(":/svg/player/back.svg"));
    m_backBtn->setIconSize(QSize(16, 16)); 
    m_backBtn->setFixedSize(38, 28);       
    m_backBtn->setFocusPolicy(Qt::NoFocus);
    m_backBtn->setToolTip(tr("Back"));
    connect(m_backBtn, &QPushButton::clicked, this, &PlayerView::onBackClicked);

    m_titleLabel = new QLabel(m_topHUD);
    m_titleLabel->setObjectName("playerTitleLabel");
    m_titleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_titleLabel->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
    m_titleLabel->installEventFilter(this);

    
    auto *sysLayout = new QHBoxLayout();
    sysLayout->setSpacing(0);
    sysLayout->setContentsMargins(0, 0, 0, 0);

    
    m_minBtn = new QPushButton(m_topHUD);
    m_minBtn->setObjectName("hud-min-btn");
    m_minBtn->setIcon(QIcon(":/svg/player/min.svg"));
    m_minBtn->setIconSize(QSize(12, 12)); 
    m_minBtn->setFixedSize(38, 28);
    m_minBtn->setFocusPolicy(Qt::NoFocus);
    m_minBtn->setToolTip(tr("Minimize"));

    m_maxBtn = new QPushButton(m_topHUD);
    m_maxBtn->setObjectName("hud-max-btn");
    m_maxBtn->setIcon(QIcon(":/svg/player/max.svg"));
    m_maxBtn->setIconSize(QSize(12, 12));
    m_maxBtn->setFixedSize(38, 28);
    m_maxBtn->setFocusPolicy(Qt::NoFocus);
    m_maxBtn->setToolTip(tr("Maximize"));

    m_closeBtn = new QPushButton(m_topHUD);
    m_closeBtn->setObjectName("hud-close-btn");
    m_closeBtn->setIcon(QIcon(":/svg/player/close.svg"));
    m_closeBtn->setIconSize(QSize(12, 12));
    m_closeBtn->setFixedSize(38, 28);
    m_closeBtn->setFocusPolicy(Qt::NoFocus);
    m_closeBtn->setToolTip(tr("Close"));

    connect(m_minBtn, &QPushButton::clicked, this, [this]() { window()->showMinimized(); });

    connect(m_maxBtn, &QPushButton::clicked, this,
            [this]()
            {
                
                if (window()->isMaximized())
                {
                    window()->showNormal();
                }
                else
                {
                    window()->showMaximized();
                }
            });

    
    connect(m_closeBtn, &QPushButton::clicked, this,
            [this]()
            {
                stopAndReport();
                window()->close();
            });

    sysLayout->addWidget(m_minBtn, 0, Qt::AlignTop);
    sysLayout->addWidget(m_maxBtn, 0, Qt::AlignTop);
    sysLayout->addWidget(m_closeBtn, 0, Qt::AlignTop);

    
    topLayout->addWidget(m_backBtn, 0, Qt::AlignTop);
    topLayout->addWidget(m_titleLabel, 1, Qt::AlignTop);
    topLayout->addLayout(sysLayout, 0);

    
    m_bottomHUD = new QWidget(this);
    m_bottomHUD->setObjectName("playerBottomHUD");
    m_bottomHUD->setFixedHeight(110);

    
    m_bottomHUD->installEventFilter(this);

    auto *bottomLayout = new QVBoxLayout(m_bottomHUD);
    bottomLayout->setContentsMargins(32, 10, 32, 16);
    bottomLayout->setSpacing(8);

    
    auto *progressLayout = new QHBoxLayout();
    progressLayout->setSpacing(16);

    m_currentTimeLabel = new QLabel("00:00", m_bottomHUD);
    m_currentTimeLabel->setObjectName("playerTimeLabel");

    m_progressSlider = new ModernSlider(Qt::Horizontal, m_bottomHUD);
    m_progressSlider->setObjectName("playerSlider");
    m_progressSlider->setFocusPolicy(Qt::NoFocus); 
    m_progressSlider->setCursor(Qt::PointingHandCursor);
    m_progressSlider->setSingleStep(1); 
    m_progressSlider->setPageStep(10);
    m_progressSlider->setRange(0, 1000);

    m_totalTimeLabel = new QLabel("00:00", m_bottomHUD);
    m_totalTimeLabel->setObjectName("playerTimeLabel");

    progressLayout->addWidget(m_currentTimeLabel);
    progressLayout->addWidget(m_progressSlider, 1);
    progressLayout->addWidget(m_totalTimeLabel);

    
    auto *controlLayout = new QHBoxLayout();
    controlLayout->setSpacing(12);

    m_rewindBtn = createHudButton(":/svg/player/rewind.svg");
    m_rewindBtn->setToolTip(tr("Rewind"));
    m_playPauseBtn = createHudButton(":/svg/player/pause.svg", QSize(36, 36));
    m_playPauseBtn->setToolTip(tr("Play/Pause"));
    m_forwardBtn = createHudButton(":/svg/player/forward.svg");
    m_forwardBtn->setToolTip(tr("Forward"));

    
    m_volumeBtn = createHudButton(":/svg/player/volume.svg");
    m_volumeBtn->setToolTip(tr("Mute/Unmute"));
    m_volumeBtn->installEventFilter(this); 

    m_volumeSlider = new ModernSlider(Qt::Horizontal, m_bottomHUD);
    m_volumeSlider->setObjectName("volumeSlider"); 
    m_volumeSlider->setFocusPolicy(Qt::NoFocus);
    m_volumeSlider->setCursor(Qt::PointingHandCursor);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(100);
    m_volumeSlider->setFixedWidth(80);
    m_volumeSlider->installEventFilter(this);

    m_speedBtn = new QPushButton("1.0X", m_bottomHUD);
    m_speedBtn->setObjectName("playerSpeedBtn");
    m_speedBtn->setCursor(Qt::PointingHandCursor);
    m_speedBtn->setFocusPolicy(Qt::NoFocus);
    m_speedBtn->setFixedHeight(40);
    m_speedBtn->setToolTip(tr("Playback Speed"));

    m_audioBtn = createHudButton(":/svg/player/audio.svg");
    m_audioBtn->setToolTip(tr("Audio Track"));
    m_subtitleBtn = createHudButton(":/svg/player/subtitle.svg");
    m_subtitleBtn->setToolTip(tr("Subtitles"));
    m_settingsBtn = createHudButton(":/svg/player/settings.svg");
    m_settingsBtn->setToolTip(tr("Settings"));

    
    m_scaleBtn = createHudButton(":/svg/player/scale-fit.svg");
    m_scaleBtn->setToolTip(tr("Aspect Ratio"));
    m_fullscreenBtn = createHudButton(":/svg/player/fullscreen.svg");
    m_fullscreenBtn->setToolTip(tr("Fullscreen"));

    m_toastLabel = new QLabel("", m_bottomHUD);
    m_toastLabel->setObjectName("playerToastLabel");
    m_toastLabel->setAlignment(Qt::AlignCenter);
    m_toastLabel->hide();

    
    controlLayout->addWidget(m_rewindBtn);
    controlLayout->addWidget(m_playPauseBtn);
    controlLayout->addWidget(m_forwardBtn);

    
    controlLayout->addSpacing(20);

    controlLayout->addWidget(m_volumeBtn);
    controlLayout->addWidget(m_volumeSlider);

    controlLayout->addStretch();
    controlLayout->addWidget(m_toastLabel);
    controlLayout->addStretch();

    controlLayout->addWidget(m_speedBtn);
    controlLayout->addWidget(m_audioBtn);
    controlLayout->addWidget(m_subtitleBtn);
    controlLayout->addWidget(m_settingsBtn);
    controlLayout->addWidget(m_scaleBtn);
    controlLayout->addWidget(m_fullscreenBtn);

    bottomLayout->addLayout(progressLayout);
    bottomLayout->addLayout(controlLayout);

    
    m_infoOverlay = new QLabel(this);
    m_infoOverlay->setObjectName("playerInfoOverlay");
    m_infoOverlay->hide();
    m_infoOverlay->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_infoOverlay->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    
    m_topOpacity = new QGraphicsOpacityEffect(m_topHUD);
    m_bottomOpacity = new QGraphicsOpacityEffect(m_bottomHUD);
    m_logoOpacity = new QGraphicsOpacityEffect(m_logoLabel);
    m_speedOpacity = new QGraphicsOpacityEffect(m_networkSpeedLabel); 

    m_topHUD->setGraphicsEffect(m_topOpacity);
    m_bottomHUD->setGraphicsEffect(m_bottomOpacity);
    m_logoLabel->setGraphicsEffect(m_logoOpacity);
    m_networkSpeedLabel->setGraphicsEffect(m_speedOpacity); 

    m_fadeGroup = new QParallelAnimationGroup(this);

    auto *topAnim = new QPropertyAnimation(m_topOpacity, "opacity", this);
    topAnim->setDuration(250);

    auto *bottomAnim = new QPropertyAnimation(m_bottomOpacity, "opacity", this);
    bottomAnim->setDuration(250);

    auto *logoAnim = new QPropertyAnimation(m_logoOpacity, "opacity", this);
    logoAnim->setDuration(250);

    auto *speedAnim = new QPropertyAnimation(m_speedOpacity, "opacity", this);
    speedAnim->setDuration(250);

    m_fadeGroup->addAnimation(topAnim);
    m_fadeGroup->addAnimation(bottomAnim);
    m_fadeGroup->addAnimation(logoAnim);
    m_fadeGroup->addAnimation(speedAnim); 

    
    connect(m_playPauseBtn, &QPushButton::clicked, this, &PlayerView::togglePlayPause);
    connect(m_progressSlider, &QSlider::sliderMoved, this, &PlayerView::onSliderMoved);

    connect(m_rewindBtn, &QPushButton::clicked, this,
            [this]()
            {
                double step = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSeekStep, "10").toDouble();
                seekRelative(-step);
            });

    connect(m_forwardBtn, &QPushButton::clicked, this,
            [this]()
            {
                double step = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSeekStep, "10").toDouble();
                seekRelative(step);
            });

    
    connect(m_volumeBtn, &QPushButton::clicked, this, &PlayerView::toggleMute);
    connect(m_volumeSlider, &QSlider::valueChanged, this, &PlayerView::onVolumeSliderMoved);

    connect(m_speedBtn, &QPushButton::clicked, this, &PlayerView::showSpeedMenu);
    connect(m_audioBtn, &QPushButton::clicked, this, &PlayerView::showAudioMenu);
    connect(m_subtitleBtn, &QPushButton::clicked, this, &PlayerView::showSubtitleMenu);
    connect(m_settingsBtn, &QPushButton::clicked, this, &PlayerView::showSettingsMenu);
    connect(m_scaleBtn, &QPushButton::clicked, this, &PlayerView::cycleVideoScale);             
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &PlayerView::toggleFullscreenWindow); 

    
    connect(m_mpvWidget, &MpvWidget::positionChanged, this, &PlayerView::onPositionChanged);
    connect(m_mpvWidget, &MpvWidget::durationChanged, this, &PlayerView::onDurationChanged);
    connect(m_mpvWidget, &MpvWidget::playbackStateChanged, this, &PlayerView::onPlaybackStateChanged);
    
    connect(m_mpvWidget->controller(), &MpvController::propertyChanged, this, &PlayerView::onMpvPropertyChanged);

    setScaleIcon();
    m_rightSidebar->raise();
}


void PlayerView::updateLoadingState()
{
    if (m_isBuffering || m_isSeeking)
    {
        m_loadingOverlay->start();

        
        
        m_topHUD->raise();
        m_bottomHUD->raise();
        m_logoLabel->raise();         
        m_networkSpeedLabel->raise(); 
        m_rightSidebar->raise();
        m_rightTrigger->raise();
        if (m_activePopup)
            m_activePopup->raise();
    }
    else
    {
        m_loadingOverlay->stop();
    }
}


void PlayerView::showCenteredPopup(QWidget *popup, QPushButton *btn)
{
    if (m_activePopup)
    {
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    m_activePopup = popup;
    popup->adjustSize();

    QPoint btnPos = btn->mapTo(this, QPoint(btn->width() / 2, 0));
    int mx = btnPos.x() - popup->sizeHint().width() / 2;
    int my = btnPos.y() - popup->sizeHint().height() - 10;

    
    if (mx < 10)
    {
        mx = 10;
    }
    if (my < 10)
    {
        my = 10;
    }

    popup->move(mx, my);
    popup->show();
    popup->raise();

    showControls(); 
}


void PlayerView::setupRightSidebar()
{
    m_rightSidebar = new QWidget(this);
    m_rightSidebar->setObjectName("playerRightSidebar"); 

    m_rightSidebar->setFixedWidth(220);
    
    m_rightSidebar->setGeometry(20000, 0, 220, 100);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(35);
    shadow->setColor(QColor(0, 0, 0, 150));
    shadow->setOffset(-5, 0);
    m_rightSidebar->setGraphicsEffect(shadow);

    auto *layout = new QVBoxLayout(m_rightSidebar);
    layout->setContentsMargins(0, 15, 0, 15);
    layout->setSpacing(10);

    m_sidebarTitleLabel = new QLabel(tr("Continue Watching"), m_rightSidebar);
    m_sidebarTitleLabel->setObjectName("playerSidebarTitle"); 
    layout->addWidget(m_sidebarTitleLabel);

    m_resumeList = new QListWidget(m_rightSidebar);
    m_resumeList->setObjectName("playerResumeList"); 
    m_resumeList->setFocusPolicy(Qt::NoFocus);

    m_resumeList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_resumeList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_resumeList->setTextElideMode(Qt::ElideRight);

    layout->addWidget(m_resumeList);

    
    connect(m_resumeList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem *item) -> QCoro::Task<void>
            {
                
                QPointer<PlayerView> guard(this);
                if (item->flags() == Qt::NoItemFlags)
                {
                    co_return;
                }

                QString mediaId = item->data(Qt::UserRole).toString();
                
                if (mediaId == m_currentMediaId)
                {
                    hideRightSidebar();
                    co_return;
                }

                
                
                if (m_isSeriesMode && !m_seriesName.isEmpty())
                {
                    
                    QString rawText = item->text().trimmed();
                    int spacePos = rawText.indexOf("  ");
                    QString seInfo = (spacePos > 0) ? rawText.left(spacePos) : rawText;
                    m_sidebarPendingTitle = QString("%1 - %2 - %3").arg(m_seriesName, seInfo, item->toolTip());
                }
                else
                {
                    m_sidebarPendingTitle = item->toolTip();
                }
                m_sidebarPendingTicks = item->data(Qt::UserRole + 1).toLongLong();
                m_sidebarPendingItemId = mediaId;

                try
                {
                    MediaItem detail = co_await m_core->mediaService()->getItemDetail(mediaId);

                    if (!guard)
                        co_return; 

                    if (m_sidebarPendingItemId.isEmpty() || detail.id != m_sidebarPendingItemId)
                    {
                        co_return;
                    }

                    QString mediaSourceId = detail.id;
                    MediaSourceInfo selectedSource;

                    if (!detail.mediaSources.isEmpty())
                    {
                        selectedSource = detail.mediaSources.first();
                        mediaSourceId = selectedSource.id;
                    }

                    QString streamUrl = m_core->mediaService()->getStreamUrl(detail.id, mediaSourceId);

                    hideRightSidebar();
                    stopAndReport();
                    m_toastLabel->hide();

                    
                    QVariant sourceInfoVar = QVariant::fromValue(selectedSource);

                    
                    QTimer::singleShot(250, this,
                                       [this, id = detail.id, title = m_sidebarPendingTitle, url = streamUrl,
                                        ticks = m_sidebarPendingTicks, sourceInfoVar]()
                                       { playMedia(id, title, url, ticks, sourceInfoVar); });

                    m_sidebarPendingItemId.clear();
                }
                catch (const std::exception &e)
                {
                    if (!guard)
                        co_return; 
                    qDebug() << "Sidebar detail fetch failed: " << e.what();
                    m_sidebarPendingItemId.clear();
                }
            });

    m_rightSidebarAnim = new QPropertyAnimation(m_rightSidebar, "pos", this);
    m_rightSidebarAnim->setDuration(350);
    m_rightSidebarAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_rightSidebar->installEventFilter(this);
}


QCoro::Task<void> PlayerView::showRightSidebar()
{
    
    QPointer<PlayerView> guard(this);

    m_isRightSidebarVisible = true;
    int sidebarY = m_topHUD->height();

    if (m_rightSidebarAnim->state() == QAbstractAnimation::Running)
    {
        m_rightSidebarAnim->stop();
    }
    m_rightSidebarAnim->setStartValue(m_rightSidebar->pos());
    m_rightSidebarAnim->setEndValue(QPoint(width() - m_rightSidebar->width(), sidebarY));
    m_rightSidebarAnim->start();

    showControls();

    
    
    
    if (m_sidebarCachedForMediaId == m_currentMediaId && m_resumeList->count() > 0)
    {
        co_return; 
    }

    
    
    
    m_resumeList->clear();
    auto *loadingItem = new QListWidgetItem(tr("⏳  Loading..."));
    loadingItem->setFlags(Qt::NoItemFlags);
    loadingItem->setForeground(QColor(140, 140, 140));
    m_resumeList->addItem(loadingItem);

    
    
    
    if (m_isSeriesMode && !m_seriesId.isEmpty())
    {
        
        m_sidebarTitleLabel->setText(m_seriesName.isEmpty() ? tr("Episodes") : m_seriesName);

        try
        {
            auto seasons = co_await m_core->mediaService()->getSeasons(m_seriesId);
            if (!guard)
                co_return;

            m_resumeList->clear();
            QListWidgetItem *scrollTarget = nullptr;

            for (const auto &season : seasons)
            {
                
                auto *seasonHeader = new QListWidgetItem(season.name);
                seasonHeader->setFlags(Qt::NoItemFlags);
                QFont headerFont = m_resumeList->font();
                headerFont.setBold(true);
                headerFont.setPixelSize(13);
                seasonHeader->setFont(headerFont);
                seasonHeader->setForeground(QColor(140, 140, 140));
                m_resumeList->addItem(seasonHeader);

                
                auto episodes = co_await m_core->mediaService()->getEpisodes(m_seriesId, season.id);
                if (!guard)
                    co_return;

                for (const auto &ep : episodes)
                {
                    QString label = QString("  S%1E%2  %3")
                                        .arg(ep.parentIndexNumber, 2, 10, QChar('0'))
                                        .arg(ep.indexNumber, 2, 10, QChar('0'))
                                        .arg(ep.name);
                    auto *epItem = new QListWidgetItem(label);
                    epItem->setData(Qt::UserRole, ep.id);
                    epItem->setData(Qt::UserRole + 1, ep.userData.playbackPositionTicks);
                    epItem->setToolTip(ep.name);

                    
                    if (ep.id == m_currentMediaId)
                    {
                        QFont boldFont = m_resumeList->font();
                        boldFont.setBold(true);
                        epItem->setFont(boldFont);
                        epItem->setForeground(QColor(100, 180, 255));
                        scrollTarget = epItem;
                    }

                    m_resumeList->addItem(epItem);
                }
            }

            
            if (guard && scrollTarget)
            {
                m_resumeList->setCurrentItem(scrollTarget);
                m_resumeList->scrollToItem(scrollTarget, QAbstractItemView::PositionAtCenter);
            }

            if (guard && m_resumeList->count() == 0)
            {
                auto *empty = new QListWidgetItem(tr("No episodes found."));
                empty->setFlags(Qt::NoItemFlags);
                m_resumeList->addItem(empty);
            }

            
            if (guard)
                m_sidebarCachedForMediaId = m_currentMediaId;
        }
        catch (const std::exception &e)
        {
            if (!guard)
                co_return;
            qDebug() << "Sidebar series episodes fetch failed: " << e.what();
        }
    }
    else
    {
        
        m_sidebarTitleLabel->setText(tr("Continue Watching"));

        try
        {
            QList<MediaItem> items = co_await m_core->mediaService()->getResumeItems(30);

            if (!guard)
                co_return;

            m_resumeList->clear();
            for (const auto &item : items)
            {
                QString iconStr = item.type == "Episode" ? "📺 " : "🎬 ";
                QListWidgetItem *listItem = new QListWidgetItem(iconStr + item.name);
                listItem->setData(Qt::UserRole, item.id);
                listItem->setData(Qt::UserRole + 1, item.userData.playbackPositionTicks);
                listItem->setToolTip(item.name);
                m_resumeList->addItem(listItem);
            }
            if (items.isEmpty())
            {
                m_resumeList->addItem(new QListWidgetItem(tr("No active media found.")));
                m_resumeList->item(0)->setFlags(Qt::NoItemFlags);
            }

            
            if (guard)
                m_sidebarCachedForMediaId = m_currentMediaId;
        }
        catch (const std::exception &e)
        {
            if (!guard)
                co_return;
            qDebug() << "Sidebar resume items fetch failed: " << e.what();
        }
    }
}

void PlayerView::hideRightSidebar()
{
    m_isRightSidebarVisible = false;
    int sidebarY = m_topHUD->height();

    if (m_rightSidebarAnim->state() == QAbstractAnimation::Running)
    {
        m_rightSidebarAnim->stop();
    }
    m_rightSidebarAnim->setStartValue(m_rightSidebar->pos());
    m_rightSidebarAnim->setEndValue(QPoint(width() + 30, sidebarY));
    m_rightSidebarAnim->start();
}



void PlayerView::stopAndReport()
{
    
    if (m_hasReportedStop)
    {
        return;
    }
    m_hasReportedStop = true;

    
    if (m_reportTimer)
        m_reportTimer->stop();
    if (m_mousePollTimer)
        m_mousePollTimer->stop();
    if (m_seekTimer)
        m_seekTimer->stop();
    if (m_bufferTimer)
        m_bufferTimer->stop();
    if (m_hideTimer)
        m_hideTimer->stop();
    if (m_osdHideTimer)
        m_osdHideTimer->stop();
    if (m_toastTimer)
        m_toastTimer->stop();
    if (m_singleClickTimer)
        m_singleClickTimer->stop();

    if (!m_currentMediaId.isEmpty() && m_core && m_core->mediaService())
    {
        long long currentTicks = static_cast<long long>(m_currentPosition * 10000000.0);

        auto *service = m_core->mediaService();
        QString mediaId = m_currentMediaId;
        QString sourceId = m_currentMediaSourceId;
        
        QString sessionId = m_currentPlaySessionId;
        m_currentPlaySessionId.clear(); 

        
        
        auto taskRoutine = [](MediaService *s, QString mId, QString sId, long long ticks,
                              QString sessId) -> QCoro::Task<void>
        {
            try
            {
                co_await s->reportPlaybackProgress(mId, sId, ticks, true, sessId);
            }
            catch (const std::exception &e)
            {
                qDebug() << "Progress sync failed (ignored):" << e.what();
            }
            try
            {
                co_await s->reportPlaybackStopped(mId, sId, ticks, sessId);
            }
            catch (const std::exception &e)
            {
                qDebug() << "Stop sync failed:" << e.what();
            }
        };

        
        
        auto *lingeringTask = new QCoro::Task<void>(taskRoutine(service, mediaId, sourceId, currentTicks, sessionId));

        
        
        QTimer::singleShot(10000, m_core, [lingeringTask]() { delete lingeringTask; });
    }

    if (m_mpvWidget)
    {
        
        disconnect(m_mpvWidget, &MpvWidget::positionChanged, this, &PlayerView::onPositionChanged);
        m_mpvWidget->controller()->command(QVariantList{"stop"});
    }
}

void PlayerView::keyPressEvent(QKeyEvent *event)
{
    bool isHudVisible = (m_topOpacity->opacity() > 0.0);

    if (event->key() == Qt::Key_Escape)
    {
        if (window()->isFullScreen())
        {
            window()->showNormal();
        }
        showControls();
        event->accept();
    }
    else if (event->key() == Qt::Key_Space)
    {
        if (!event->isAutoRepeat())
        {
            togglePlayPause();
        }
        showControls();
        event->accept();
    }
    else if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)
    {
        if (!event->isAutoRepeat())
        {
            toggleFullscreenWindow();
        }
        showControls();
        event->accept();
    }
    else if (event->key() == Qt::Key_Left)
    {
        if (!event->isAutoRepeat())
        {
            double step = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSeekStep, "10").toDouble();
            m_seekDirection = -1;
            seekRelative(-step, !isHudVisible); 
            m_seekRate = 2.0;
            if (ConfigStore::instance()->get<bool>(ConfigKeys::PlayerLongPressSeek, true))
                m_seekTimer->start(500);
        }
        event->accept();
    }
    else if (event->key() == Qt::Key_Right)
    {
        if (!event->isAutoRepeat())
        {
            double step = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSeekStep, "10").toDouble();
            m_seekDirection = 1;
            seekRelative(step, !isHudVisible);
            m_seekRate = 2.0;
            if (ConfigStore::instance()->get<bool>(ConfigKeys::PlayerLongPressSeek, true))
                m_seekTimer->start(500);
        }
        event->accept();
    }
    else if (event->key() == Qt::Key_Up)
    {
        changeVolume(5, !isHudVisible);
        event->accept();
    }
    else if (event->key() == Qt::Key_Down)
    {
        changeVolume(-5, !isHudVisible);
        event->accept();
    }
    else
    {
        showControls();
        BaseView::keyPressEvent(event);
    }
}

void PlayerView::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
    {
        if (!event->isAutoRepeat())
        {
            m_seekTimer->stop();
        }
        event->accept();
    }
    else
    {
        BaseView::keyReleaseEvent(event);
    }
}

void PlayerView::resizeEvent(QResizeEvent *event)
{
    BaseView::resizeEvent(event);

    
    
    
    
    QTimer::singleShot(
        0, this,
        [this]()
        {
            if (window() && m_maxBtn)
            {
                bool isMax = window()->isMaximized();
                if (!m_maxBtn->property("isMax").isValid() || m_maxBtn->property("isMax").toBool() != isMax)
                {
                    m_maxBtn->setIcon(QIcon(isMax ? ":/svg/player/restore.svg" : ":/svg/player/max.svg"));
                    m_maxBtn->setProperty("isMax", isMax);
                }
            }
        });

    m_mpvWidget->setGeometry(0, 0, width(), height());
    m_loadingOverlay->setGeometry(0, 0, width(), height()); 
    m_topHUD->setGeometry(0, 0, width(), m_topHUD->height());
    m_bottomHUD->setGeometry(0, height() - m_bottomHUD->height(), width(), m_bottomHUD->height());

    
    
    
    if (m_logoLabel)
    {
        
        m_logoLabel->move(24, m_topHUD->height());
    }

    
    
    if (m_networkSpeedLabel)
    {
        m_networkSpeedLabel->setGeometry(width() - 210, m_topHUD->height(), 200, 30);
    }

    
    m_infoOverlay->move(24, m_topHUD->height() + 60);

    
    int sidebarY = m_topHUD->height();
    int sidebarH = height() - m_topHUD->height() - m_bottomHUD->height();

    m_rightTrigger->setGeometry(width() - 15, sidebarY, 15, sidebarH);

    if (!m_isRightSidebarVisible)
    {
        m_rightSidebar->setGeometry(width() + 30, sidebarY, m_rightSidebar->width(), sidebarH);
    }
    else
    {
        m_rightSidebar->setGeometry(width() - m_rightSidebar->width(), sidebarY, m_rightSidebar->width(), sidebarH);
    }

    
    m_osdContainer->setGeometry(0, 0, width(), height());
    m_osdSeekLine->setGeometry(0, height() - 3, width(), 3);
    m_osdSeekTimeLabel->setGeometry((width() - 200) / 2, height() - 45, 200, 36);
    m_osdVolumeLabel->setGeometry((width() - 160) / 2, height() - 45, 160, 36);

    
    if (m_activePopup)
    {
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    updateTitleElision();
}


void PlayerView::updateTitleElision()
{
    if (m_fullTitle.isEmpty() || !m_titleLabel)
    {
        return;
    }

    
    int safeWidth = this->width() - 320;
    if (safeWidth < 50)
    {
        safeWidth = 50;
    }

    
    QFontMetrics fm(m_titleLabel->font());
    QString elided = fm.elidedText(m_fullTitle, Qt::ElideRight, safeWidth);

    m_titleLabel->setText(elided);
    m_titleLabel->setToolTip(m_fullTitle); 
}

bool PlayerView::eventFilter(QObject *watched, QEvent *event)
{

    if (watched == m_mpvWidget || watched == this || watched == m_topHUD || watched == m_titleLabel ||
        watched == m_bottomHUD)
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            auto *me = static_cast<QMouseEvent *>(event);

            
            
            
            if (me->button() == Qt::BackButton || me->button() == Qt::XButton1)
            {
                onBackClicked();
                return true; 
            }

            
            if (m_activePopup)
            {
                QPoint localPos = this->mapFromGlobal(me->globalPosition().toPoint());
                if (!m_activePopup->geometry().contains(localPos))
                {
                    m_activePopup->deleteLater();
                    m_activePopup = nullptr;
                    return true;
                }
            }

            showControls();
            if (me->button() == Qt::LeftButton && watched != m_bottomHUD)
            {
                
                m_dragPos = me->globalPosition().toPoint();
                m_didDrag = false; 
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            showControls(); 
            auto *me = static_cast<QMouseEvent *>(event);
            if ((me->buttons() & Qt::LeftButton) && watched != m_bottomHUD)
            {

                
                if (!window()->isFullScreen())
                {
                    
                    if ((me->globalPosition().toPoint() - m_dragPos).manhattanLength() >
                        QApplication::startDragDistance())
                    {
                        m_didDrag = true; 
                        if (window() && window()->windowHandle())
                        {
                            
                            window()->windowHandle()->startSystemMove();
                        }
                    }
                }
                return true;
            }
        }
        else if (event->type() == QEvent::MouseButtonRelease)
        {
            auto *me = static_cast<QMouseEvent *>(event);
            
            QPoint clickPos = this->mapFromGlobal(me->globalPosition().toPoint());
            bool isOnHud = m_topHUD->geometry().contains(clickPos) || m_bottomHUD->geometry().contains(clickPos);
            if (me->button() == Qt::LeftButton && !isOnHud && !m_didDrag &&
                ConfigStore::instance()->get<bool>(ConfigKeys::PlayerClickToPause, true))
            {
                m_singleClickTimer->start(); 
            }
        }
        else if (event->type() == QEvent::MouseButtonDblClick)
        {
            auto *me = static_cast<QMouseEvent *>(event);
            QPoint clickPos = this->mapFromGlobal(me->globalPosition().toPoint());
            bool isOnHud = m_topHUD->geometry().contains(clickPos) || m_bottomHUD->geometry().contains(clickPos);
            if (me->button() == Qt::LeftButton && !isOnHud)
            {
                m_singleClickTimer->stop(); 
                m_didDrag = true;           
                toggleFullscreenWindow();
                return true;
            }
        }
    }

    
    if (watched == m_rightTrigger && event->type() == QEvent::Enter)
    {
        showRightSidebar();
        return true;
    }
    
    if (watched == m_rightSidebar && event->type() == QEvent::Leave)
    {
        QPoint globalPos = QCursor::pos();
        QPoint localPos = m_rightSidebar->mapFromGlobal(globalPos);
        if (!m_rightSidebar->rect().contains(localPos))
        {
            hideRightSidebar();
        }
        return false;
    }

    if (event->type() == QEvent::Wheel)
    {
        if (watched == m_volumeBtn || watched == m_volumeSlider || watched == m_bottomHUD || watched == m_mpvWidget ||
            watched == this)
        {
            showControls();
            auto *we = static_cast<QWheelEvent *>(event);
            changeVolume(we->angleDelta().y() > 0 ? 5 : -5);
            return true;
        }
    }

    return BaseView::eventFilter(watched, event);
}




void PlayerView::showMiniOsd(const QString &type)
{
    if (m_topOpacity->opacity() > 0.0)
    {
        return; 
    }

    if (!m_osdContainer->isVisible() || m_osdOpacity->opacity() == 0.0)
    {
        m_osdContainer->show();
        m_osdAnim->stop();
        m_osdAnim->setStartValue(m_osdOpacity->opacity());
        m_osdAnim->setEndValue(1.0);
        m_osdAnim->start();
    }

    if (type == "seek")
    {
        m_osdSeekLine->show();
        m_osdSeekTimeLabel->show();
        m_osdVolumeLabel->hide();

        m_osdSeekLine->setMaximum(static_cast<int>(m_totalDuration));
        m_osdSeekLine->setValue(static_cast<int>(m_currentPosition));
        m_osdSeekTimeLabel->setText(formatTime(m_currentPosition, m_totalDuration));
    }
    else if (type == "volume")
    {
        m_osdSeekLine->hide();
        m_osdSeekTimeLabel->hide();
        m_osdVolumeLabel->show();

        m_osdVolumeLabel->setText(tr("Volume: %1%").arg(static_cast<int>(m_currentVolume)));
    }

    m_osdHideTimer->start(1500); 
}

void PlayerView::hideMiniOsd()
{
    if (m_osdOpacity->opacity() <= 0.0)
    {
        return;
    }
    m_osdAnim->stop();
    m_osdAnim->setStartValue(m_osdOpacity->opacity());
    m_osdAnim->setEndValue(0.0);
    m_osdAnim->start();
}

void PlayerView::showControls()
{
    this->setCursor(Qt::ArrowCursor);
    if (m_mpvWidget)
    {
        m_mpvWidget->setCursor(Qt::ArrowCursor);
    }

    
    if (m_osdContainer->isVisible())
    {
        m_osdHideTimer->stop();
        m_osdOpacity->setOpacity(0.0);
        m_osdContainer->hide();
    }

    if (m_isPlaying)
    {
        m_hideTimer->start(3000);
    }
    else
    {
        m_hideTimer->stop(); 
    }

    if (m_topOpacity->opacity() >= 1.0 && m_fadeGroup->state() != QAbstractAnimation::Running)
    {
        return;
    }

    if (m_fadeGroup->state() == QAbstractAnimation::Running)
    {
        m_fadeGroup->stop();
    }

    for (int i = 0; i < m_fadeGroup->animationCount(); ++i)
    {
        auto *anim = qobject_cast<QPropertyAnimation *>(m_fadeGroup->animationAt(i));
        anim->setStartValue(anim->targetObject()->property("opacity")); 
        anim->setEndValue(1.0);
    }
    m_fadeGroup->setDirection(QAbstractAnimation::Forward);
    m_fadeGroup->start();
}


void PlayerView::hideControls()
{
    if (!m_isPlaying)
    {
        return;
    }

    QPoint localPos = this->mapFromGlobal(QCursor::pos());
    bool isAppActive = this->window()->isActiveWindow();
    bool isMouseInside = this->rect().contains(localPos);
    
    bool isMouseInsidePopup = m_activePopup && m_activePopup->geometry().contains(localPos);

    
    if (isAppActive && isMouseInside &&
        (m_topHUD->geometry().contains(localPos) || m_bottomHUD->geometry().contains(localPos) || isMouseInsidePopup ||
         (m_isRightSidebarVisible && m_rightSidebar->geometry().contains(localPos))))
    {
        m_hideTimer->start(1000);
        return;
    }

    hideRightSidebar();

    if (m_activePopup)
    {
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    if (m_topOpacity->opacity() <= 0.0)
    {
        return;
    }

    if (m_fadeGroup->state() == QAbstractAnimation::Running)
    {
        m_fadeGroup->stop();
    }

    for (int i = 0; i < m_fadeGroup->animationCount(); ++i)
    {
        auto *anim = qobject_cast<QPropertyAnimation *>(m_fadeGroup->animationAt(i));
        anim->setStartValue(anim->targetObject()->property("opacity")); 
        anim->setEndValue(0.0);
    }
    m_fadeGroup->setDirection(QAbstractAnimation::Forward);
    m_fadeGroup->start();

    this->setCursor(Qt::BlankCursor);
    if (m_mpvWidget)
    {
        m_mpvWidget->setCursor(Qt::BlankCursor);
    }
}

void PlayerView::onMpvPropertyChanged(const QString &property, const QVariant &value)
{
    if (property == "cache-speed")
    {
        qint64 speedBytes = value.toLongLong();
        if (speedBytes == 0)
        {
            m_networkSpeedLabel->setText("0.0 KB/s");
        }
        else if (speedBytes < 1024 * 1024)
        {
            m_networkSpeedLabel->setText(QString::number(speedBytes / 1024.0, 'f', 1) + " KB/s");
        }
        else
        {
            m_networkSpeedLabel->setText(QString::number(speedBytes / 1048576.0, 'f', 1) + " MB/s");
        }
    }
    else if (property == "mute")
    {
        m_isMuted = value.toBool();
        m_volumeBtn->setIcon(QIcon(m_isMuted ? ":/svg/player/volume-mute.svg" : ":/svg/player/volume.svg"));

        m_volumeSlider->blockSignals(true);
        if (m_isMuted)
        {
            m_volumeSlider->setValue(0);
        }
        else
        {
            m_volumeSlider->setValue(static_cast<int>(m_currentVolume));
        }
        m_volumeSlider->blockSignals(false);
    }
    else if (property == "volume")
    {
        m_currentVolume = value.toDouble();
        if (!m_isMuted)
        { 
            m_volumeSlider->blockSignals(true);
            m_volumeSlider->setValue(static_cast<int>(m_currentVolume));
            m_volumeSlider->blockSignals(false);
        }
    }
    else if (property == "paused-for-cache")
    {
        
        m_isBuffering = value.toBool();
        updateLoadingState();
    }
    else if (property == "seeking")
    {
        
        m_isSeeking = value.toBool();
        updateLoadingState();
    }
}

void PlayerView::toggleMute()
{
    m_isMuted = !m_isMuted;
    m_mpvWidget->controller()->setProperty("mute", m_isMuted);
    showToast(m_isMuted ? tr("Muted") : tr("Volume: %1%").arg(static_cast<int>(m_currentVolume)));
    showControls();

    
    ConfigStore::instance()->set(ConfigKeys::PlayerVolumeMuted, m_isMuted);
}


void PlayerView::changeVolume(int delta, bool silent)
{
    if (m_isMuted)
    {
        return;
    }

    m_currentVolume += delta;
    if (m_currentVolume < 0)
    {
        m_currentVolume = 0;
    }
    if (m_currentVolume > 100)
    {
        m_currentVolume = 100;
    }

    m_mpvWidget->controller()->setProperty("volume", m_currentVolume);

    if (!silent)
    {
        showToast(tr("Volume: %1%").arg(static_cast<int>(m_currentVolume)));
        showControls();
    }
    else
    {
        showMiniOsd("volume");
    }

    
    ConfigStore::instance()->set(ConfigKeys::PlayerVolumeLevel, m_currentVolume);
}

void PlayerView::onVolumeSliderMoved(int value)
{
    m_currentVolume = value;
    if (m_isMuted && m_currentVolume > 0)
    {
        m_isMuted = false;
        m_mpvWidget->controller()->setProperty("mute", false);
    }
    m_mpvWidget->controller()->setProperty("volume", m_currentVolume);
    showToast(tr("Volume: %1%").arg(static_cast<int>(m_currentVolume)));
    showControls();

    
    ConfigStore::instance()->set(ConfigKeys::PlayerVolumeLevel, m_currentVolume);
    ConfigStore::instance()->set(ConfigKeys::PlayerVolumeMuted, m_isMuted);
}





void PlayerView::showSettingsMenu()
{
    auto *panel = new ModernScrollPanel(this);

    
    panel->addItem(tr("Network Speed"), "network_speed", m_showNetworkSpeed);
    panel->addItem(tr("Tech Info Panel"), "tech_info", m_showInfoOverlay);

    
    ServerProfile profile = m_core->serverManager()->activeProfile();
    bool defaultStrmDirect = (profile.type == ServerProfile::Jellyfin);
    QSettings settings("qEmby", "Player");
    bool strmDirect = settings.value("EnableStrmDirectPlay", defaultStrmDirect).toBool();

    panel->addItem(tr("STRM Direct Play"), "strm_direct", strmDirect);

    int maxHeight = this->height() - m_bottomHUD->height() - 40;
    
    panel->finalizeLayout(maxHeight < 150 ? 150 : maxHeight, 300);

    
    connect(panel, &ModernScrollPanel::itemTriggered, this,
            [this](const QVariant &data, const QString &text)
            {
                Q_UNUSED(text);
                QString action = data.toString();

                if (action == "network_speed")
                {
                    m_showNetworkSpeed = !m_showNetworkSpeed;
                    m_networkSpeedLabel->setVisible(m_showNetworkSpeed);
                    showToast(m_showNetworkSpeed ? tr("Network Speed Enabled") : tr("Network Speed Disabled"));
                }
                else if (action == "tech_info")
                {
                    m_showInfoOverlay = !m_showInfoOverlay;
                    if (m_showInfoOverlay)
                    {
                        updateInfoDisplay();
                        m_infoOverlay->show();
                    }
                    else
                    {
                        m_infoOverlay->hide();
                    }
                }
                else if (action == "strm_direct")
                {
                    ServerProfile profile = m_core->serverManager()->activeProfile();
                    bool defaultStrmDirect = (profile.type == ServerProfile::Jellyfin);
                    QSettings settings("qEmby", "Player");
                    bool currentStrm = settings.value("EnableStrmDirectPlay", defaultStrmDirect).toBool();
                    bool newStrm = !currentStrm;

                    settings.setValue("EnableStrmDirectPlay", newStrm);
                    showToast(newStrm ? tr("STRM Direct Play: ON (Reloading...)")
                                      : tr("STRM Direct Play: OFF (Reloading...)"));

                    
                    QString mediaId = m_currentMediaId;
                    QString title = m_fullTitle;
                    QString origUrl = m_originalStreamUrl;
                    QVariant sourceInfo = m_currentSourceInfoVar;
                    long long currentTicks = static_cast<long long>(m_currentPosition * 10000000.0);

                    
                    stopAndReport();

                    
                    QTimer::singleShot(150, this, [this, mediaId, title, origUrl, currentTicks, sourceInfo]()
                                       { playMedia(mediaId, title, origUrl, currentTicks, sourceInfo); });
                }

                
                if (m_activePopup)
                {
                    m_activePopup->deleteLater();
                    m_activePopup = nullptr;
                }
            });

    showCenteredPopup(panel, m_settingsBtn);
}





void PlayerView::showSpeedMenu()
{
    auto *panel = new ModernScrollPanel(this);

    QList<double> speeds = {0.5, 1.0, 1.25, 1.5, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};

    for (double s : speeds)
    {
        bool isSelected = qFuzzyCompare(m_currentSpeed, s);
        panel->addItem(QString("%1X").arg(s), s, isSelected);
    }

    int maxHeight = this->height() - m_bottomHUD->height() - 40;
    panel->finalizeLayout(maxHeight < 150 ? 150 : maxHeight, 130);

    connect(panel, &ModernScrollPanel::itemTriggered, this,
            [this](const QVariant &data, const QString &text)
            {
                Q_UNUSED(text);
                double s = data.toDouble();
                m_currentSpeed = s;
                m_mpvWidget->controller()->setProperty("speed", s);
                m_speedBtn->setText(QString("%1X").arg(s));
                showToast(tr("Speed: %1X").arg(s));

                if (m_activePopup)
                {
                    m_activePopup->deleteLater();
                    m_activePopup = nullptr;
                }
            });

    showCenteredPopup(panel, m_speedBtn);
}

void PlayerView::showAudioMenu()
{
    auto *panel = new ModernScrollPanel(this);

    auto tracks = m_mpvWidget->controller()->getProperty("track-list").toList();
    bool anyAudioSelected = false;

    for (const QVariant &v : tracks)
    {
        auto map = v.toMap();
        if (map["type"].toString() == "audio")
        {
            int id = map["id"].toInt();
            QString title = map["title"].toString();
            QString lang = map["lang"].toString();
            bool selected = map["selected"].toBool();
            if (selected)
            {
                anyAudioSelected = true;
            }

            QString text = title.isEmpty() ? (lang.isEmpty() ? tr("Track %1").arg(id) : lang) : title;
            panel->addItem(text, id, selected);
        }
    }

    panel->addItem(tr("Disable Audio"), "no", !anyAudioSelected);

    int maxHeight = this->height() - m_bottomHUD->height() - 40;
    panel->finalizeLayout(maxHeight < 150 ? 150 : maxHeight, 200);

    connect(panel, &ModernScrollPanel::itemTriggered, this,
            [this](const QVariant &data, const QString &text)
            {
                QString valStr = data.toString();
                if (valStr == "no")
                {
                    m_mpvWidget->controller()->setProperty("aid", "no");
                    showToast(tr("Audio Disabled"));
                }
                else
                {
                    int id = data.toInt();
                    m_mpvWidget->controller()->setProperty("aid", id);
                    showToast(tr("Audio: %1").arg(text));
                }

                if (m_activePopup)
                {
                    m_activePopup->deleteLater();
                    m_activePopup = nullptr;
                }
            });

    showCenteredPopup(panel, m_audioBtn);
}

void PlayerView::showSubtitleMenu()
{
    auto *panel = new ModernScrollPanel(this);

    auto tracks = m_mpvWidget->controller()->getProperty("track-list").toList();
    bool anySubSelected = false;

    for (const QVariant &v : tracks)
    {
        auto map = v.toMap();
        if (map["type"].toString() == "sub")
        {
            int id = map["id"].toInt();
            QString title = map["title"].toString();
            QString lang = map["lang"].toString();
            bool selected = map["selected"].toBool();
            if (selected)
            {
                anySubSelected = true;
            }

            QString text = title.isEmpty() ? (lang.isEmpty() ? tr("Subtitle %1").arg(id) : lang) : title;
            panel->addItem(text, id, selected);
        }
    }

    panel->addItem(tr("Disable Subtitles"), "no", !anySubSelected);

    int maxHeight = this->height() - m_bottomHUD->height() - 40;
    panel->finalizeLayout(maxHeight < 150 ? 150 : maxHeight, 200);

    connect(panel, &ModernScrollPanel::itemTriggered, this,
            [this](const QVariant &data, const QString &text)
            {
                QString valStr = data.toString();
                if (valStr == "no")
                {
                    m_mpvWidget->controller()->setProperty("sid", "no");
                    showToast(tr("Subtitles Disabled"));
                }
                else
                {
                    int id = data.toInt();
                    m_mpvWidget->controller()->setProperty("sid", id);
                    showToast(tr("Subtitle: %1").arg(text));
                }

                if (m_activePopup)
                {
                    m_activePopup->deleteLater();
                    m_activePopup = nullptr;
                }
            });

    showCenteredPopup(panel, m_subtitleBtn);
}



void PlayerView::seekRelative(double delta, bool silent)
{
    m_mpvWidget->controller()->command(QVariantList{"seek", delta, "relative"});

    if (!silent)
    {
        showToast(delta > 0 ? tr("Forward %1s").arg(std::abs(delta)) : tr("Rewind %1s").arg(std::abs(delta)));
        showControls();
    }
    else
    {
        showMiniOsd("seek");
    }
}

void PlayerView::cycleVideoScale()
{
    m_videoScaleMode = (m_videoScaleMode + 1) % 4;
    auto *ctrl = m_mpvWidget->controller();
    QString modeStr;

    switch (m_videoScaleMode)
    {
    case 0:
        ctrl->setProperty("keepaspect", true);
        ctrl->setProperty("panscan", 0.0);
        ctrl->setProperty("video-unscaled", false);
        modeStr = tr("Scale: Fit");
        break;
    case 1:
        ctrl->setProperty("keepaspect", true);
        ctrl->setProperty("panscan", 1.0);
        ctrl->setProperty("video-unscaled", false);
        modeStr = tr("Scale: Crop");
        break;
    case 2:
        ctrl->setProperty("keepaspect", false);
        ctrl->setProperty("panscan", 0.0);
        ctrl->setProperty("video-unscaled", false);
        modeStr = tr("Scale: Stretch");
        break;
    case 3:
        ctrl->setProperty("keepaspect", true);
        ctrl->setProperty("panscan", 0.0);
        ctrl->setProperty("video-unscaled", true);
        modeStr = tr("Scale: Original");
        break;
    }
    setScaleIcon();
    showToast(modeStr);

    
    ConfigStore::instance()->set(ConfigKeys::PlayerDefaultScale, m_videoScaleMode);
}

void PlayerView::setScaleIcon()
{
    switch (m_videoScaleMode)
    {
    case 0:
        m_scaleBtn->setIcon(QIcon(":/svg/player/scale-fit.svg"));
        break;
    case 1:
        m_scaleBtn->setIcon(QIcon(":/svg/player/scale-crop.svg"));
        break;
    case 2:
        m_scaleBtn->setIcon(QIcon(":/svg/player/scale-stretch.svg"));
        break;
    case 3:
        m_scaleBtn->setIcon(QIcon(":/svg/player/scale-original.svg"));
        break;
    }
}

void PlayerView::showToast(const QString &msg)
{
    m_toastLabel->setText(msg);
    m_toastLabel->show();
    m_toastTimer->start(2500);
}


void PlayerView::updateInfoDisplay()
{
    if (!m_mpvWidget || !m_mpvWidget->controller())
    {
        return;
    }
    auto *ctrl = m_mpvWidget->controller();

    QString vCodec = ctrl->getProperty("video-codec").toString().toUpper();
    QString vFormat = ctrl->getProperty("video-format").toString().toUpper();
    QString aCodec = ctrl->getProperty("audio-codec").toString().toUpper();
    QString hwdec = ctrl->getProperty("hwdec-current").toString().toUpper();

    QString w = ctrl->getProperty("width").toString();
    QString h = ctrl->getProperty("height").toString();
    QString fps = ctrl->getProperty("container-fps").toString();
    QString drops = ctrl->getProperty("frame-drop-count").toString();

    QString aCh = ctrl->getProperty("audio-params/channel-count").toString();
    QString aSr = ctrl->getProperty("audio-params/samplerate").toString();

    
    qint64 vBitrateRaw = ctrl->getProperty("video-bitrate").toLongLong();
    qint64 aBitrateRaw = ctrl->getProperty("audio-bitrate").toLongLong();
    QString vBitrate = vBitrateRaw > 0 ? QString::number(vBitrateRaw / 1000) + " kbps" : "Variable / Unknown";
    QString aBitrate = aBitrateRaw > 0 ? QString::number(aBitrateRaw / 1000) + " kbps" : "Variable / Unknown";

    
    QString text = QString("[Video Info]\n"
                           "Codec: %1 (%2)\n"
                           "Resolution: %3 x %4\n"
                           "Frame Rate: %5 fps\n"
                           "Bitrate: %6\n"
                           "HW Decoder: %7\n"
                           "Dropped Frames: %8\n\n"
                           "[Audio Info]\n"
                           "Codec: %9\n"
                           "Channels: %10 ch\n"
                           "Sample Rate: %11 Hz\n"
                           "Bitrate: %12")
                       .arg(vCodec.isEmpty() ? "UNKNOWN" : vCodec)
                       .arg(vFormat.isEmpty() ? "-" : vFormat)
                       .arg(w.isEmpty() ? "0" : w)
                       .arg(h.isEmpty() ? "0" : h)
                       .arg(fps.isEmpty() ? "0" : fps)
                       .arg(vBitrate)
                       .arg(hwdec.isEmpty() || hwdec == "NO" ? "Software (CPU)" : hwdec)
                       .arg(drops.isEmpty() ? "0" : drops)
                       .arg(aCodec.isEmpty() ? "UNKNOWN" : aCodec)
                       .arg(aCh.isEmpty() ? "0" : aCh)
                       .arg(aSr.isEmpty() ? "0" : aSr)
                       .arg(aBitrate);

    m_infoOverlay->setText(text);
    m_infoOverlay->adjustSize();
}

void PlayerView::toggleFullscreenWindow()
{
    if (window()->isFullScreen())
    {
        window()->showNormal();
    }
    else
    {
        window()->showFullScreen();
    }
}


QCoro::Task<void> PlayerView::executeFetchLogo(QPointer<PlayerView> safeThis, QEmbyCore *core, QString mediaId)
{
    try
    {
        
        MediaItem detail = co_await core->mediaService()->getItemDetail(mediaId);

        
        if (!safeThis || safeThis->m_currentMediaId != mediaId)
        {
            co_return;
        }

        if (!detail.images.logoTag.isEmpty())
        {
            QPixmap pix = co_await core->mediaService()->fetchImage(mediaId, "Logo", detail.images.logoTag, 400);

            if (safeThis && safeThis->m_currentMediaId == mediaId && !pix.isNull())
            {
                
                QPixmap scaledPix = pix.scaled(140, 55, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                safeThis->m_logoLabel->setPixmap(scaledPix);
                safeThis->m_logoLabel->adjustSize();

                
                
                safeThis->m_logoOpacity->setOpacity(safeThis->m_topOpacity->opacity());
                safeThis->m_logoLabel->show();
            }
        }
    }
    catch (...)
    {
        
    }
}

void PlayerView::playMedia(const QString &mediaId, const QString &title, const QString &streamUrl,
                           long long startPositionTicks, const QVariant &sourceInfoVar)
{
    m_hasReportedStop = false;
    m_currentMediaId = mediaId;
    m_currentMediaSourceId = mediaId;

    
    m_logoLabel->clear();
    m_logoLabel->hide();

    
    m_originalStreamUrl = streamUrl;
    m_currentSourceInfoVar = sourceInfoVar;

    QString actualStreamUrl = streamUrl;
    if (sourceInfoVar.isValid() && sourceInfoVar.canConvert<MediaSourceInfo>())
    {
        MediaSourceInfo sourceInfo = sourceInfoVar.value<MediaSourceInfo>();
        QString directUrl = m_core->mediaService()->getStreamUrl(mediaId, sourceInfo);
        if (!directUrl.isEmpty())
        {
            actualStreamUrl = directUrl; 
        }
    }

    QUrl streamQUrl(actualStreamUrl);
    QUrl origQUrl(streamUrl); 

    QUrlQuery query(streamQUrl);
    if (!query.hasQueryItem("mediaSourceId") && QUrlQuery(origQUrl).hasQueryItem("mediaSourceId"))
    {
        query = QUrlQuery(origQUrl); 
    }

    if (query.hasQueryItem("mediaSourceId"))
    {
        m_currentMediaSourceId = query.queryItemValue("mediaSourceId");
    }
    else if (query.hasQueryItem("MediaSourceId"))
    {
        m_currentMediaSourceId = query.queryItemValue("MediaSourceId");
    }

    m_pendingSeekSeconds = startPositionTicks / 10000000.0;

    m_fullTitle = title;
    updateTitleElision();

    m_currentPosition = m_pendingSeekSeconds;
    m_totalDuration = 0.0;

    QWidget *win = window();
    if (win)
    {
        m_wasMaximized = win->isMaximized();
        m_originalGeometry = win->geometry();
        m_hasSetVideoSize = false;

        
        if (m_wasMaximized)
        {
            win->showFullScreen();
        }
    }


    
    
    
    m_isSeriesMode = false;
    m_seriesId.clear();
    m_seriesName.clear();
    m_sidebarCachedForMediaId.clear(); 

    
    auto detectSeriesMode = [](QPointer<PlayerView> safeThis, QEmbyCore *core, QString mId) -> QCoro::Task<void>
    {
        try
        {
            MediaItem detail = co_await core->mediaService()->getItemDetail(mId);
            if (!safeThis || safeThis->m_currentMediaId != mId)
                co_return;
            if (detail.type == "Episode" && !detail.seriesId.isEmpty())
            {
                safeThis->m_isSeriesMode = true;
                safeThis->m_seriesId = detail.seriesId;
                safeThis->m_seriesName = detail.seriesName;
            }
        }
        catch (...)
        {
        }
    };
    detectSeriesMode(QPointer<PlayerView>(this), m_core, mediaId);

    
    
    
    executeFetchLogo(QPointer<PlayerView>(this), m_core, mediaId);

    
    m_mpvWidget->loadMedia(actualStreamUrl);

    
    
    
    m_isBuffering = true;
    m_isSeeking = false;
    updateLoadingState();

    
    
    
    m_targetAudioStreamIndex = -2; 
    m_targetSubStreamIndex = -2;

    QVariantList pendingSubtitles;

    if (sourceInfoVar.isValid() && sourceInfoVar.canConvert<MediaSourceInfo>())
    {
        MediaSourceInfo sourceInfo = sourceInfoVar.value<MediaSourceInfo>();

        m_targetAudioStreamIndex = -2; 
        m_targetSubStreamIndex = -2;
        bool hasExternalDefault = false;
        bool foundDefaultAudio = false;

        for (const auto &stream : sourceInfo.mediaStreams)
        {
            if (stream.type == "Audio" && stream.isDefault)
            {
                m_targetAudioStreamIndex = stream.index;
                foundDefaultAudio = true;
            }
            else if (stream.type == "Subtitle" && stream.isDefault)
            {
                if (stream.isExternal)
                {
                    hasExternalDefault = true;
                }
                else
                {
                    m_targetSubStreamIndex = stream.index;
                }
            }
        }

        
        

        
        if (hasExternalDefault)
        {
            m_targetSubStreamIndex = -2;
        }

        QString tokenQuery;
        if (query.hasQueryItem("api_key"))
        {
            tokenQuery = "api_key=" + query.queryItemValue("api_key");
        }
        else if (query.hasQueryItem("X-Emby-Token"))
        {
            tokenQuery = "X-Emby-Token=" + query.queryItemValue("X-Emby-Token");
        }
        else if (query.hasQueryItem("api_token"))
        {
            tokenQuery = "api_token=" + query.queryItemValue("api_token");
        }

        
        
        QString baseUrl;
        int videosIdx = streamUrl.indexOf("/videos/", 0, Qt::CaseInsensitive);
        if (videosIdx != -1)
        {
            baseUrl = streamUrl.left(videosIdx);
        }
        else
        {
            baseUrl = origQUrl.scheme() + "://" + origQUrl.authority();
        }

        for (const auto &stream : sourceInfo.mediaStreams)
        {
            
            
            if (stream.type == "Subtitle" && stream.isExternal)
            {
                QString subUrl = stream.deliveryUrl;

                
                if (subUrl.isEmpty())
                {
                    QString codec = stream.codec.toLower();
                    if (codec.isEmpty() || codec == "subrip")
                    {
                        codec = "srt"; 
                    }

                    
                    subUrl = QString("/Videos/%1/%2/Subtitles/%3/Stream.%4")
                                 .arg(mediaId)
                                 .arg(sourceInfo.id)
                                 .arg(stream.index)
                                 .arg(codec);
                }

                
                if (!subUrl.startsWith("http"))
                {
                    if (!subUrl.startsWith("/"))
                    {
                        subUrl = "/" + subUrl;
                    }
                    subUrl = baseUrl + subUrl;

                    
                    if (!tokenQuery.isEmpty())
                    {
                        subUrl += (subUrl.contains("?") ? "&" : "?") + tokenQuery;
                    }
                }

                QString subTitle = stream.displayTitle.isEmpty() ? stream.language : stream.displayTitle;
                if (subTitle.isEmpty())
                {
                    subTitle = tr("External Sub %1").arg(stream.index);
                }

                
                
                QString flag = stream.isDefault ? "select" : "auto";

                
                QVariantMap subMap;
                subMap["url"] = subUrl;
                subMap["flag"] = flag;
                subMap["title"] = subTitle;
                subMap["lang"] = stream.language;
                pendingSubtitles.append(subMap);
            }
        }
    }

    
    setProperty("pendingSubtitles", pendingSubtitles);

    m_mpvWidget->play();

    m_isPlaying = true;
    m_playPauseBtn->setIcon(QIcon(":/svg/player/pause.svg"));

    
    m_currentVolume = ConfigStore::instance()->get<double>(ConfigKeys::PlayerVolumeLevel, 100.0);
    m_isMuted = ConfigStore::instance()->get<bool>(ConfigKeys::PlayerVolumeMuted, false);

    m_mpvWidget->controller()->setProperty("volume", m_currentVolume);
    m_mpvWidget->controller()->setProperty("mute", m_isMuted);

    
    
    m_volumeSlider->blockSignals(true);
    m_volumeSlider->setValue(m_isMuted ? 0 : static_cast<int>(m_currentVolume));
    m_volumeSlider->blockSignals(false);
    m_volumeBtn->setIcon(QIcon(m_isMuted ? ":/svg/player/volume-mute.svg" : ":/svg/player/volume.svg"));

    
    m_videoScaleMode = ConfigStore::instance()->get<int>(ConfigKeys::PlayerDefaultScale, 1);
    setScaleIcon();

    switch (m_videoScaleMode)
    {
    case 0:
        m_mpvWidget->controller()->setProperty("keepaspect", true);
        m_mpvWidget->controller()->setProperty("panscan", 0.0);
        m_mpvWidget->controller()->setProperty("video-unscaled", false);
        break;
    case 1:
        m_mpvWidget->controller()->setProperty("keepaspect", true);
        m_mpvWidget->controller()->setProperty("panscan", 1.0);
        m_mpvWidget->controller()->setProperty("video-unscaled", false);
        break;
    case 2:
        m_mpvWidget->controller()->setProperty("keepaspect", false);
        m_mpvWidget->controller()->setProperty("panscan", 0.0);
        m_mpvWidget->controller()->setProperty("video-unscaled", false);
        break;
    case 3:
        m_mpvWidget->controller()->setProperty("keepaspect", true);
        m_mpvWidget->controller()->setProperty("panscan", 0.0);
        m_mpvWidget->controller()->setProperty("video-unscaled", true);
        break;
    }

    
    auto startSessionTask = [](QPointer<PlayerView> safeThis, MediaService *s, QString mId, QString sId,
                               long long ticks) -> QCoro::Task<void>
    {
        QString sessionId = co_await s->reportPlaybackStart(mId, sId, ticks);
        if (safeThis && safeThis->m_currentMediaId == mId)
        {
            safeThis->m_currentPlaySessionId = sessionId;
        }
    };
    startSessionTask(QPointer<PlayerView>(this), m_core->mediaService(), m_currentMediaId, m_currentMediaSourceId,
                     startPositionTicks);

    m_reportTimer->start();
    m_mousePollTimer->start();
    m_bufferTimer->start();
    showControls();
}

void PlayerView::reportProgressToServer()
{
    if (m_currentMediaId.isEmpty() || m_hasReportedStop || m_currentPlaySessionId.isEmpty())
    {
        return;
    }
    long long currentTicks = static_cast<long long>(m_currentPosition * 10000000.0);
    
    m_core->mediaService()->reportPlaybackProgress(m_currentMediaId, m_currentMediaSourceId, currentTicks, !m_isPlaying,
                                                   m_currentPlaySessionId);

    if (m_showInfoOverlay)
    {
        updateInfoDisplay(); 
    }
}

void PlayerView::onBackClicked()
{
    
    
    stopAndReport();
    Q_EMIT navigateBack();

    
    
    QWidget *win = window();
    bool isEmbedded = qobject_cast<QMainWindow *>(win) != nullptr;
    bool wasMaximized = m_wasMaximized;
    QRect originalGeo = m_originalGeometry;
    if (win)
    {
        QPointer<QWidget> safeWin(win);
        QTimer::singleShot(0, this,
                           [safeWin, isEmbedded, wasMaximized, originalGeo]()
                           {
                               if (!safeWin)
                                   return;

                               if (isEmbedded)
                               {
                                   
                                   
                                   if (safeWin->isFullScreen())
                                   {
                                       safeWin->showMaximized();
                                   }
                                   
                               }
                               else
                               {
                                   
                                   if (safeWin->isFullScreen())
                                   {
                                       safeWin->showNormal();
                                   }
                                   if (wasMaximized)
                                   {
                                       safeWin->showMaximized();
                                   }
                                   else if (originalGeo.isValid())
                                   {
                                       safeWin->setGeometry(originalGeo);
                                   }
                               }
                           });
    }
}

void PlayerView::onPositionChanged(double position)
{
    if (std::isnan(position) || std::isinf(position) || position < 0)
    {
        position = 0.0;
    }
    m_currentPosition = position;

    if (!m_progressSlider->isSliderDown())
    {
        m_progressSlider->blockSignals(true);
        m_progressSlider->setValue(static_cast<int>(position));
        m_progressSlider->blockSignals(false);
    }

    m_currentTimeLabel->setText(formatTime(position, m_totalDuration));

    
    if (m_osdContainer->isVisible() && !m_osdSeekLine->isHidden())
    {
        m_osdSeekLine->setValue(static_cast<int>(position));
        m_osdSeekTimeLabel->setText(formatTime(position, m_totalDuration));
    }
}

void PlayerView::onDurationChanged(double duration)
{
    if (std::isnan(duration) || std::isinf(duration) || duration < 0)
    {
        duration = 0.0;
    }
    m_totalDuration = duration;
    m_progressSlider->setMaximum(static_cast<int>(duration));
    m_totalTimeLabel->setText(formatTime(duration, duration));

    
    m_osdSeekLine->setMaximum(static_cast<int>(duration));

    if (!m_hasSetVideoSize && duration > 0)
    {
        m_hasSetVideoSize = true;

        
        double dw = m_mpvWidget->controller()->getProperty("video-params/dw").toDouble();
        double dh = m_mpvWidget->controller()->getProperty("video-params/dh").toDouble();
        if (dw <= 0 || dh <= 0)
        {
            dw = m_mpvWidget->controller()->getProperty("width").toDouble();
            dh = m_mpvWidget->controller()->getProperty("height").toDouble();
        }

        QWidget *win = window();
        
        
        if (win && !m_wasMaximized && !win->isFullScreen() && !qobject_cast<QMainWindow *>(win))
        {
            double aspect = (dw > 0 && dh > 0) ? (dw / dh) : (16.0 / 9.0);

            
            int playerW = this->width();
            if (playerW < 100)
            {
                playerW = win->width(); 
            }

            
            int targetPlayerH = qRound(playerW / aspect);

            
            if (playerW % 2 != 0)
            {
                playerW++;
            }
            if (targetPlayerH % 2 != 0)
            {
                targetPlayerH++;
            }

            
            int deltaH = targetPlayerH - this->height();

            
            int targetWinW = win->width() + (playerW - this->width());
            int targetWinH = win->height() + deltaH;

            QRect currentGeo = win->geometry();
            win->setGeometry(currentGeo.center().x() - targetWinW / 2, currentGeo.center().y() - targetWinH / 2,
                             targetWinW, targetWinH);
        }

        
        
        
        if (m_targetAudioStreamIndex != -2 || m_targetSubStreamIndex != -2)
        {
            QVariantList tracks = m_mpvWidget->controller()->getProperty("track-list").toList();

            
            if (m_targetAudioStreamIndex == -1)
            {
                m_mpvWidget->controller()->setProperty("aid", "no");
            }
            if (m_targetSubStreamIndex == -1)
            {
                m_mpvWidget->controller()->setProperty("sid", "no");
            }

            for (const QVariant &v : tracks)
            {
                QVariantMap map = v.toMap();
                QString type = map["type"].toString();
                int ffIndex = map["ff-index"].toInt();
                int mpvId = map["id"].toInt();

                if (type == "audio" && m_targetAudioStreamIndex >= 0 && ffIndex == m_targetAudioStreamIndex)
                {
                    m_mpvWidget->controller()->setProperty("aid", mpvId);
                }
                if (type == "sub" && m_targetSubStreamIndex >= 0 && ffIndex == m_targetSubStreamIndex)
                {
                    m_mpvWidget->controller()->setProperty("sid", mpvId);
                }
            }
        }

        
        QVariantList pendingSubs = property("pendingSubtitles").toList();
        for (const QVariant &v : pendingSubs)
        {
            QVariantMap map = v.toMap();
            m_mpvWidget->controller()->command(QVariantList{"sub-add", map["url"].toString(), map["flag"].toString(),
                                                            map["title"].toString(), map["lang"].toString()});
        }
        setProperty("pendingSubtitles", QVariantList()); 
    }

    if (m_pendingSeekSeconds > 0 && duration > 0)
    {
        m_mpvWidget->seek(m_pendingSeekSeconds);
        m_pendingSeekSeconds = 0.0;
    }
}

void PlayerView::onPlaybackStateChanged(bool isPaused)
{
    m_isPlaying = !isPaused;
    m_playPauseBtn->setIcon(QIcon(m_isPlaying ? ":/svg/player/pause.svg" : ":/svg/player/play.svg"));

    
    showControls();

    if (!m_hasReportedStop && !m_currentMediaId.isEmpty() && !m_currentPlaySessionId.isEmpty())
    {
        long long currentTicks = static_cast<long long>(m_currentPosition * 10000000.0);
        m_core->mediaService()->reportPlaybackProgress(m_currentMediaId, m_currentMediaSourceId, currentTicks, isPaused,
                                                       m_currentPlaySessionId);
    }
}

void PlayerView::togglePlayPause()
{
    if (m_isPlaying)
    {
        m_mpvWidget->pause();
    }
    else
    {
        m_mpvWidget->play();
    }

    showControls();
}

void PlayerView::onSliderMoved(int value)
{
    m_mpvWidget->seek(static_cast<double>(value));

    if (!m_hasReportedStop && !m_currentMediaId.isEmpty() && !m_currentPlaySessionId.isEmpty())
    {
        long long currentTicks = static_cast<long long>(value * 10000000.0);
        m_core->mediaService()->reportPlaybackProgress(m_currentMediaId, m_currentMediaSourceId, currentTicks,
                                                       !m_isPlaying, m_currentPlaySessionId);
    }

    showControls();
}

QString PlayerView::formatTime(double seconds, double totalSeconds) const
{
    QTime t(0, 0, 0);
    t = t.addSecs(static_cast<int>(seconds));
    QTime totalT(0, 0, 0);
    totalT = totalT.addSecs(static_cast<int>(totalSeconds));

    if (totalT.hour() > 0)
    {
        return t.toString("hh:mm:ss");
    }
    else
    {
        return t.toString("mm:ss");
    }
}
