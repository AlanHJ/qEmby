#include "playerview.h"
#include "../../components/playerdanmakuidentifydialog.h"
#include "../../components/playerdanmakusettingsdialog.h"
#include "../../components/playersubtitlesettingsdialog.h"
#include "../../components/playermediaswitcherpanel.h"
#include "../../components/modernscrollpanel.h"
#include "../../components/playerosdlayer.h"
#include "../../components/playerlongpresshandler.h"
#include "../../components/playerstatisticsoverlay.h"
#include "../../utils/mediaitemutils.h"
#include "../../utils/mediasourcepreferenceutils.h"
#include "../../utils/playerpreferenceutils.h"
#include "../../utils/subtitlestyleutils.h"
#include <fileutils.h>
#include "qembycore.h"
#include <QApplication>
#include <QAbstractAnimation>
#include <QCursor>
#include <QDebug>
#include <QDir>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QListWidget>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPalette>
#include <QPointer> 
#include <QEasingCurve>
#include <QSettings>
#include <QSignalBlocker>
#include <QStyle>
#include <QTime>
#include <QUrl>
#include <QUrlQuery>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWindow>
#include <cmath>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <models/media/playerlaunchcontext.h>
#include <models/media/playbackinfo.h>
#include <models/profile/serverprofile.h>
#include <services/danmaku/danmakuservice.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>

namespace
{
QString trimOrDash(QString value)
{
    value = value.trimmed();
    return value.isEmpty() ? QStringLiteral("-") : value;
}

QString firstNonEmpty(const QStringList &values)
{
    for (QString value : values)
    {
        value = value.trimmed();
        if (!value.isEmpty())
        {
            return value;
        }
    }
    return {};
}

QString joinNonEmpty(const QStringList &values, const QString &separator)
{
    QStringList filtered;
    for (QString value : values)
    {
        value = value.trimmed();
        if (!value.isEmpty())
        {
            filtered.append(value);
        }
    }
    return filtered.join(separator);
}

QString stripTrailingZeros(QString text)
{
    if (!text.contains(QLatin1Char('.')))
    {
        return text;
    }

    while (text.endsWith(QLatin1Char('0')))
    {
        text.chop(1);
    }
    if (text.endsWith(QLatin1Char('.')))
    {
        text.chop(1);
    }
    return text;
}

QString formatBitrateValue(qint64 bitsPerSecond)
{
    if (bitsPerSecond <= 0)
    {
        return {};
    }

    if (bitsPerSecond < 1000000)
    {
        return QStringLiteral("%1 kbps")
            .arg(stripTrailingZeros(QString::number(bitsPerSecond / 1000.0, 'f', 1)));
    }

    return QStringLiteral("%1 Mbps")
        .arg(stripTrailingZeros(QString::number(bitsPerSecond / 1000000.0, 'f', 2)));
}

QString formatDataRateValue(qint64 bytesPerSecond)
{
    if (bytesPerSecond <= 0)
    {
        return QStringLiteral("0.0 KB/s");
    }

    if (bytesPerSecond < 1024 * 1024)
    {
        return QStringLiteral("%1 KB/s")
            .arg(stripTrailingZeros(QString::number(bytesPerSecond / 1024.0, 'f', 1)));
    }

    return QStringLiteral("%1 MB/s")
        .arg(stripTrailingZeros(QString::number(bytesPerSecond / 1048576.0, 'f', 1)));
}

QString formatFrameRateValue(double fps)
{
    if (!std::isfinite(fps) || fps <= 0.0)
    {
        return {};
    }

    return QStringLiteral("%1 fps")
        .arg(stripTrailingZeros(QString::number(fps, 'f', fps >= 100.0 ? 1 : 3)));
}

QString formatDurationValue(double seconds)
{
    if (!std::isfinite(seconds) || seconds <= 0.0)
    {
        return {};
    }

    return QStringLiteral("%1 s")
        .arg(stripTrailingZeros(QString::number(seconds, 'f', seconds >= 100.0 ? 0 : 1)));
}

QString formatAvSyncValue(double seconds)
{
    if (!std::isfinite(seconds))
    {
        return {};
    }

    const double milliseconds = seconds * 1000.0;
    return QStringLiteral("%1%2 ms")
        .arg(milliseconds >= 0.0 ? QStringLiteral("+") : QString())
        .arg(stripTrailingZeros(QString::number(milliseconds, 'f', 1)));
}

QString formatLevelValue(int level)
{
    if (level <= 0)
    {
        return {};
    }

    if (level % 10 == 0)
    {
        return QString::number(level / 10);
    }

    return QStringLiteral("%1.%2").arg(level / 10).arg(level % 10);
}

QString formatDimensionValue(int width, int height)
{
    if (width <= 0 || height <= 0)
    {
        return {};
    }

    return QStringLiteral("%1 x %2").arg(width).arg(height);
}

QString formatAspectValue(double width, double height, QString explicitAspect = {})
{
    explicitAspect = explicitAspect.trimmed();
    if (!explicitAspect.isEmpty())
    {
        return explicitAspect;
    }

    if (!std::isfinite(width) || !std::isfinite(height) || width <= 0.0 || height <= 0.0)
    {
        return {};
    }

    const double ratio = width / height;
    struct KnownRatio
    {
        double value;
        const char *label;
    };

    static const KnownRatio knownRatios[] = {
        {4.0 / 3.0, "4:3"},
        {16.0 / 9.0, "16:9"},
        {21.0 / 9.0, "21:9"},
        {1.0, "1:1"}};

    for (const KnownRatio &known : knownRatios)
    {
        if (std::fabs(ratio - known.value) < 0.03)
        {
            return QString::fromLatin1(known.label);
        }
    }

    return QStringLiteral("%1:1")
        .arg(stripTrailingZeros(QString::number(ratio, 'f', 2)));
}

qint64 variantToLongLong(const QVariant &value)
{
    bool ok = false;
    const qint64 result = value.toLongLong(&ok);
    return ok ? result : 0;
}

double variantToDouble(const QVariant &value)
{
    bool ok = false;
    const double result = value.toDouble(&ok);
    return ok ? result : 0.0;
}

const MediaStreamInfo *findFirstStreamByType(const MediaSourceInfo &source, const QString &type)
{
    for (const MediaStreamInfo &stream : source.mediaStreams)
    {
        if (stream.type.compare(type, Qt::CaseInsensitive) == 0)
        {
            return &stream;
        }
    }
    return nullptr;
}

QVariantMap findSelectedTrackByType(const QVariantList &tracks, const QString &type)
{
    for (const QVariant &trackValue : tracks)
    {
        const QVariantMap track = trackValue.toMap();
        if (track.value(QStringLiteral("type")).toString() == type &&
            track.value(QStringLiteral("selected")).toBool())
        {
            return track;
        }
    }
    return {};
}

bool hasValidAnimationTarget(const QPropertyAnimation *animation)
{
    return animation && animation->targetObject();
}

bool fadeGroupHasValidTargets(const QParallelAnimationGroup *group)
{
    if (!group)
    {
        return false;
    }

    for (int i = 0; i < group->animationCount(); ++i)
    {
        const auto *animation =
            qobject_cast<QPropertyAnimation *>(group->animationAt(i));
        if (!hasValidAnimationTarget(animation))
        {
            return false;
        }
    }

    return true;
}

void stopPropertyAnimationSafely(QPropertyAnimation *animation)
{
    if (!hasValidAnimationTarget(animation) ||
        animation->state() == QAbstractAnimation::Stopped)
    {
        return;
    }

    animation->stop();
}

void stopFadeGroupSafely(QParallelAnimationGroup *group)
{
    if (!group || group->state() == QAbstractAnimation::Stopped ||
        !fadeGroupHasValidTargets(group))
    {
        return;
    }

    group->stop();
}

void detachPropertyAnimationTarget(QPropertyAnimation *animation)
{
    if (!animation)
    {
        return;
    }

    if (animation->state() != QAbstractAnimation::Stopped)
    {
        animation->stop();
    }

    animation->setTargetObject(nullptr);
}

void detachFadeGroupTargets(QParallelAnimationGroup *group)
{
    if (!group)
    {
        return;
    }

    if (group->state() != QAbstractAnimation::Stopped)
    {
        group->stop();
    }

    for (int i = 0; i < group->animationCount(); ++i)
    {
        auto *animation =
            qobject_cast<QPropertyAnimation *>(group->animationAt(i));
        detachPropertyAnimationTarget(animation);
    }
}
} 

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

    
    m_longPressHandler = new PlayerLongPressHandler(this);
    connect(m_longPressHandler, &PlayerLongPressHandler::seekRequested, this,
            &PlayerView::seekRelative);
    connect(m_longPressHandler, &PlayerLongPressHandler::toastRequested, this,
            &PlayerView::showToast);

    
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

                if (m_showStatisticsOverlay && m_statisticsOverlay)
                {
                    updateStatisticsDisplay();
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

                if (areControlsFullyVisible() &&
                    this->cursor().shape() != Qt::BlankCursor &&
                    (!m_mpvWidget ||
                     m_mpvWidget->cursor().shape() != Qt::BlankCursor))
                {
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
                    handlePointerActivity(currentPos);
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
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &value)
            {
                Q_UNUSED(value);
                if (key == ConfigKeys::PlayerDanmakuEnabled)
                {
                    updateDanmakuButtonState();
                }
                if (key == ConfigKeys::PlayerMediaSwitcherMode)
                {
                    applyMediaSwitcherMode();
                }
                if (SubtitleStyleUtils::isSubtitleStyleKey(key))
                {
                    applySubtitleStyleSettings();
                }
            });
    applyMediaSwitcherMode();
    applySubtitleStyleSettings();
    this->installEventFilter(this);
}


PlayerView::~PlayerView()
{
    beginViewTeardown();
    stopAndReport();
}

void PlayerView::prepareForStackLeave()
{
    if (m_isViewTearingDown && m_hasReportedStop)
    {
        return;
    }

    qDebug() << "[PlayerView] Prepare for stack leave"
             << "| mediaId=" << m_currentMediaId
             << "| isTearingDown=" << m_isViewTearingDown
             << "| hasReportedStop=" << m_hasReportedStop;

    beginViewTeardown();
    stopAndReport();
}

void PlayerView::clearMediaSwitcherCache()
{
    m_switcherCacheMediaId.clear();
    m_switcherCacheReady = false;
    m_switcherResumeItems.clear();
    m_switcherSeriesSeasons.clear();
    m_switcherSeasonEpisodes.clear();
}

bool PlayerView::useHudMediaSwitcher() const
{
    return ConfigStore::instance()->get<QString>(
               ConfigKeys::PlayerMediaSwitcherMode, "sidebar") ==
           QStringLiteral("hud");
}

void PlayerView::applyMediaSwitcherMode()
{
    const bool hudMode = useHudMediaSwitcher();

    if (m_mediaSwitchBtn)
    {
        m_mediaSwitchBtn->setVisible(hudMode);
    }
    if (m_rightTrigger)
    {
        m_rightTrigger->setVisible(!hudMode);
    }
    if (hudMode)
    {
        hideRightSidebar();
    }
    if (!hudMode)
    {
        hideHudMediaSwitcher();
    }

    updateMediaSwitcherButton();
}

void PlayerView::updateMediaSwitcherButton()
{
    if (!m_mediaSwitchBtn)
    {
        return;
    }

    m_mediaSwitchBtn->setProperty(
        "drawerActive",
        m_mediaSwitchDrawer && m_mediaSwitchDrawer->isVisible() &&
            useHudMediaSwitcher());
    m_mediaSwitchBtn->style()->unpolish(m_mediaSwitchBtn);
    m_mediaSwitchBtn->style()->polish(m_mediaSwitchBtn);
    m_mediaSwitchBtn->update();

    const QString tooltip =
        m_isSeriesMode ? tr("Switch season and episode")
                       : tr("Switch media");
    m_mediaSwitchBtn->setToolTip(tooltip);
}

QString PlayerView::formatMediaSwitcherPlaybackTitle(const MediaItem &item) const
{
    return MediaItemUtils::playbackTitle(item, m_seriesName);
}

void PlayerView::showHudMediaSwitcher()
{
    if (m_isViewTearingDown || !useHudMediaSwitcher() || !m_mediaSwitchDrawer)
    {
        return;
    }

    if (m_activePopup)
    {
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }
    closeActivePlayerDialog();

    if (m_switcherCacheReady && m_switcherCacheMediaId == m_currentMediaId)
    {
        m_mediaSwitchDrawer->show();
        syncHudMediaSwitcherContent();
        updateMediaSwitcherButton();
        updateOverlayLayout();
        showControls();
        return;
    }

    m_mediaSwitchDrawer->setLoadingState(
        m_isSeriesMode
            ? (m_seriesName.isEmpty() ? tr("Episodes") : m_seriesName)
            : tr("Continue Watching"),
        tr("Loading..."), m_isSeriesMode);
    m_mediaSwitchDrawer->show();
    updateMediaSwitcherButton();
    updateOverlayLayout();
    showControls();
    ensureMediaSwitcherDataLoaded();
}

void PlayerView::hideHudMediaSwitcher()
{
    if (m_isViewTearingDown)
    {
        if (m_mediaSwitchDrawer)
        {
            m_mediaSwitchDrawer->hide();
        }
        return;
    }

    if (!m_mediaSwitchDrawer || !m_mediaSwitchDrawer->isVisible())
    {
        return;
    }

    m_mediaSwitchDrawer->hide();
    updateMediaSwitcherButton();
    updateOverlayLayout();
}

void PlayerView::syncHudMediaSwitcherContent()
{
    if (!m_mediaSwitchDrawer)
    {
        return;
    }

    if (m_isSeriesMode && !m_seriesId.isEmpty())
    {
        m_mediaSwitchDrawer->setSeriesData(m_seriesName,
                                           m_switcherSeriesSeasons,
                                           m_switcherSeasonEpisodes,
                                           m_currentMediaId);
    }
    else
    {
        m_mediaSwitchDrawer->setMovieItems(m_switcherResumeItems,
                                           m_currentMediaId);
    }

    updateMediaSwitcherButton();
    updateOverlayLayout();
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
    if (m_currentSourceInfoVar.isValid())
    {
        MediaSourceInfo sourceInfo;
        if (m_currentSourceInfoVar.canConvert<PlayerLaunchContext>())
        {
            sourceInfo = m_currentSourceInfoVar.value<PlayerLaunchContext>().selectedSource;
        }
        else if (m_currentSourceInfoVar.canConvert<MediaSourceInfo>())
        {
            sourceInfo = m_currentSourceInfoVar.value<MediaSourceInfo>();
        }

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
    m_danmakuController = new PlayerDanmakuController(m_core, m_mpvWidget, this);
    connect(m_danmakuController, &PlayerDanmakuController::toastRequested, this,
            [this](const QString &message) { showToast(message); });
    connect(m_danmakuController, &PlayerDanmakuController::stateChanged, this,
            &PlayerView::updateDanmakuButtonState);

    
    m_loadingOverlay = new LoadingOverlay(this);
    m_loadingOverlay->setObjectName("playerLoadingOverlay");
    
    m_loadingOverlay->setImmersive(true);
    
    m_loadingOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    
    connect(m_mpvWidget->controller(), &MpvController::fileLoaded, this,
            [this]()
            {
                m_isBuffering = false;
                updateLoadingState();
                applySubtitleStyleSettings();
            });
    

    
    m_osdLayer = new PlayerOsdLayer(this);

    

    

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
                beginViewTeardown();
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
    m_bottomHUD->setFixedHeight(m_bottomHudBaseHeight);

    
    m_bottomHUD->installEventFilter(this);

    auto *bottomLayout = new QVBoxLayout(m_bottomHUD);
    bottomLayout->setContentsMargins(32, 10, 32, 16);
    bottomLayout->setSpacing(8);

    m_mediaSwitchDrawer = new PlayerMediaSwitcherPanel(m_core, m_bottomHUD);
    m_mediaSwitchDrawer->hide();
    connect(m_mediaSwitchDrawer, &PlayerMediaSwitcherPanel::playRequested, this,
            [this](QString mediaId, QString title,
                   long long startPositionTicks) -> QCoro::Task<void>
            {
                co_await switchFromMediaSwitcher(mediaId, title,
                                                 startPositionTicks);
            });
    bottomLayout->addWidget(m_mediaSwitchDrawer);

    
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

    m_rewindBtn = createHudButton(":/svg/player/fast-rewind.svg");
    m_rewindBtn->setToolTip(tr("Rewind"));
    m_playPauseBtn = createHudButton(":/svg/player/pause.svg", QSize(36, 36));
    m_playPauseBtn->setToolTip(tr("Play/Pause"));
    m_forwardBtn = createHudButton(":/svg/player/fast-forward.svg");
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

    m_mediaSwitchBtn = createHudButton(":/svg/player/media-switcher.svg");
    m_mediaSwitchBtn->setObjectName("playerMediaSwitchBtn");
    m_mediaSwitchBtn->setToolTip(tr("Media Switcher"));

    m_audioBtn = createHudButton(":/svg/player/audio.svg");
    m_audioBtn->setToolTip(tr("Audio Track"));
    m_subtitleBtn = createHudButton(":/svg/player/subtitle.svg");
    m_subtitleBtn->setToolTip(tr("Subtitles"));
    m_danmakuBtn = createHudButton(":/svg/player/danmaku.svg");
    m_danmakuBtn->setToolTip(tr("Danmaku"));
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

    controlLayout->addWidget(m_mediaSwitchBtn);
    controlLayout->addWidget(m_speedBtn);
    controlLayout->addWidget(m_audioBtn);
    controlLayout->addWidget(m_subtitleBtn);
    controlLayout->addWidget(m_danmakuBtn);
    controlLayout->addWidget(m_settingsBtn);
    controlLayout->addWidget(m_scaleBtn);
    controlLayout->addWidget(m_fullscreenBtn);

    bottomLayout->addLayout(progressLayout);
    bottomLayout->addLayout(controlLayout);

    
    m_statisticsOverlay = new PlayerStatisticsOverlay(this);
    m_statisticsOverlay->setObjectName("playerStatisticsOverlay");
    m_statisticsOverlay->hide();

    
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
    connect(m_mediaSwitchBtn, &QPushButton::clicked, this,
            [this]()
            {
                if (m_mediaSwitchDrawer && m_mediaSwitchDrawer->isVisible())
                {
                    hideHudMediaSwitcher();
                    return;
                }
                showHudMediaSwitcher();
            });
    connect(m_audioBtn, &QPushButton::clicked, this, &PlayerView::showAudioMenu);
    connect(m_subtitleBtn, &QPushButton::clicked, this, &PlayerView::showSubtitleMenu);
    connect(m_danmakuBtn, &QPushButton::clicked, this, &PlayerView::showDanmakuMenu);
    connect(m_settingsBtn, &QPushButton::clicked, this, &PlayerView::showSettingsMenu);
    connect(m_scaleBtn, &QPushButton::clicked, this, &PlayerView::cycleVideoScale);             
    connect(m_fullscreenBtn, &QPushButton::clicked, this, &PlayerView::toggleFullscreenWindow); 

    
    connect(m_mpvWidget, &MpvWidget::positionChanged, this, &PlayerView::onPositionChanged);
    connect(m_mpvWidget, &MpvWidget::durationChanged, this, &PlayerView::onDurationChanged);
    connect(m_mpvWidget, &MpvWidget::playbackStateChanged, this, &PlayerView::onPlaybackStateChanged);
    
    connect(m_mpvWidget->controller(), &MpvController::propertyChanged, this, &PlayerView::onMpvPropertyChanged);

    setScaleIcon();
    updateDanmakuButtonState();
    updateMediaSwitcherButton();
    m_rightSidebar->raise();
}


void PlayerView::updateLoadingState()
{
    if (m_isViewTearingDown)
    {
        if (m_loadingOverlay)
        {
            m_loadingOverlay->forceStop();
        }
        return;
    }

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

QString PlayerView::formatDanmakuProviderLabel(QString provider) const
{
    provider = provider.trimmed();
    if (provider == QLatin1String("local-file"))
    {
        return tr("Local File");
    }
    if (provider == QLatin1String("dandanplay"))
    {
        return tr("DandanPlay");
    }
    return provider.isEmpty() ? tr("Unknown Source") : provider;
}

QString PlayerView::buildDanmakuSummaryText() const
{
    if (!m_danmakuController)
    {
        return tr("No danmaku loaded");
    }

    if (m_danmakuController->isLoading())
    {
        return tr("Loading danmaku...");
    }

    if (!m_danmakuController->hasPreparedDanmaku())
    {
        return tr("No danmaku loaded");
    }

    const QString sourceTitle = m_danmakuController->sourceTitle().trimmed().isEmpty()
                                    ? tr("Unknown Danmaku")
                                    : m_danmakuController->sourceTitle().trimmed();
    const QString countText =
        m_danmakuController->commentCount() > 0
            ? tr("%1 comments").arg(m_danmakuController->commentCount())
            : tr("Comment count unavailable");
    return tr("Current: %1 | %2 | %3")
        .arg(sourceTitle, countText,
             formatDanmakuProviderLabel(m_danmakuController->sourceProvider()));
}

QString PlayerView::buildDanmakuTooltipText() const
{
    if (!m_danmakuController)
    {
        return tr("Danmaku");
    }

    if (m_danmakuController->isLoading())
    {
        return tr("Danmaku (Loading...)");
    }

    if (!m_danmakuController->isDanmakuVisible())
    {
        return tr("Danmaku (Hidden)");
    }

    if (!m_danmakuController->hasPreparedDanmaku())
    {
        return tr("Danmaku");
    }

    const QString sourceTitle = m_danmakuController->sourceTitle().trimmed().isEmpty()
                                    ? tr("Unknown Danmaku")
                                    : m_danmakuController->sourceTitle().trimmed();
    const QString countText =
        m_danmakuController->commentCount() > 0
            ? tr("%1 comments").arg(m_danmakuController->commentCount())
            : tr("Comment count unavailable");
    return tr("Danmaku\nSource: %1\nProvider: %2\nComments: %3")
        .arg(sourceTitle,
             formatDanmakuProviderLabel(m_danmakuController->sourceProvider()),
             countText);
}

void PlayerView::closeActivePlayerDialog()
{
    if (m_activePlayerDialog)
    {
        m_activePlayerDialog->close();
        m_activePlayerDialog = nullptr;
    }
}

void PlayerView::trackPlayerDialog(PlayerOverlayDialog *dialog)
{
    if (!dialog)
    {
        return;
    }

    closeActivePlayerDialog();
    m_activePlayerDialog = dialog;

    connect(dialog, &PlayerOverlayDialog::finished, this,
            [this, dialog](int)
            {
                if (m_activePlayerDialog == dialog)
                {
                    m_activePlayerDialog = nullptr;
                    if (!m_isViewTearingDown)
                    {
                        showControls();
                    }
                }
            });

    showControls();
    dialog->open();
}

bool PlayerView::shouldShowDanmakuHudControls() const
{
    return ConfigStore::instance()->get<bool>(ConfigKeys::PlayerDanmakuEnabled,
                                              false);
}

void PlayerView::updateDanmakuButtonState()
{
    if (m_isViewTearingDown || !m_danmakuBtn)
    {
        return;
    }

    const bool showDanmakuControls = shouldShowDanmakuHudControls();
    m_danmakuBtn->setVisible(showDanmakuControls);
    if (!showDanmakuControls || !m_danmakuController)
    {
        return;
    }

    const bool visible = m_danmakuController->isDanmakuVisible();
    const bool loading = m_danmakuController->isLoading();
    const bool loaded = m_danmakuController->hasDanmakuTrack() ||
                        m_danmakuController->hasPreparedDanmaku();

    m_danmakuBtn->setEnabled(!loading);
    if (loading)
    {
        m_danmakuBtn->setToolTip(tr("Danmaku (Loading...)"));
    }
    else if (!visible)
    {
        m_danmakuBtn->setToolTip(tr("Danmaku (Hidden)"));
    }
    else if (loaded && !m_danmakuController->sourceTitle().isEmpty())
    {
        m_danmakuBtn->setToolTip(buildDanmakuTooltipText());
    }
    else if (loaded)
    {
        m_danmakuBtn->setToolTip(buildDanmakuTooltipText());
    }
    else
    {
        m_danmakuBtn->setToolTip(tr("Danmaku"));
    }
}


void PlayerView::showCenteredPopup(QWidget *popup, QPushButton *btn)
{
    if (m_isViewTearingDown)
    {
        if (popup)
        {
            popup->deleteLater();
        }
        return;
    }

    hideHudMediaSwitcher();

    if (m_activePopup)
    {
        m_activePopup->hide();
        m_activePopup->close();
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    m_activePopup = popup;
    connect(popup, &QObject::destroyed, this, [this, popup]() {
        if (m_activePopup == popup)
        {
            m_activePopup = nullptr;
        }
    });
    popup->adjustSize();

    QPoint btnPos = btn->mapTo(this, QPoint(btn->width() / 2, 0));
    int mx = btnPos.x() - popup->sizeHint().width() / 2;
    int my = btnPos.y() - popup->sizeHint().height() - 10;

    
    if (mx < 10)
    {
        mx = 10;
    }
    const int maxX = width() - popup->sizeHint().width() - 10;
    if (mx > maxX)
    {
        mx = qMax(10, maxX);
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

void PlayerView::populateRightSidebarFromCache()
{
    if (!m_resumeList || !m_sidebarTitleLabel)
    {
        return;
    }

    m_resumeList->clear();

    if (m_isSeriesMode && !m_seriesId.isEmpty())
    {
        m_sidebarTitleLabel->setText(
            m_seriesName.isEmpty() ? tr("Episodes") : m_seriesName);

        QListWidgetItem *scrollTarget = nullptr;
        for (const MediaItem &season : m_switcherSeriesSeasons)
        {
            auto *seasonHeader = new QListWidgetItem(season.name);
            seasonHeader->setFlags(Qt::NoItemFlags);
            QFont headerFont = m_resumeList->font();
            headerFont.setBold(true);
            headerFont.setPixelSize(13);
            seasonHeader->setFont(headerFont);
            seasonHeader->setForeground(QColor(140, 140, 140));
            m_resumeList->addItem(seasonHeader);

            const auto episodes = m_switcherSeasonEpisodes.value(season.id);
            for (const MediaItem &episode : episodes)
            {
                QString label =
                    tr("  S%1E%2  %3")
                        .arg(episode.parentIndexNumber, 2, 10, QChar('0'))
                        .arg(episode.indexNumber, 2, 10, QChar('0'))
                        .arg(episode.name);
                auto *epItem = new QListWidgetItem(label);
                epItem->setData(Qt::UserRole, episode.id);
                epItem->setData(Qt::UserRole + 1,
                                episode.userData.playbackPositionTicks);
                epItem->setData(Qt::UserRole + 2,
                                formatMediaSwitcherPlaybackTitle(episode));
                epItem->setToolTip(formatMediaSwitcherPlaybackTitle(episode));

                if (episode.id == m_currentMediaId)
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

        if (m_resumeList->count() == 0)
        {
            auto *empty = new QListWidgetItem(tr("No episodes found."));
            empty->setFlags(Qt::NoItemFlags);
            m_resumeList->addItem(empty);
            return;
        }

        if (scrollTarget)
        {
            m_resumeList->setCurrentItem(scrollTarget);
            m_resumeList->scrollToItem(scrollTarget,
                                       QAbstractItemView::PositionAtCenter);
        }
        return;
    }

    m_sidebarTitleLabel->setText(tr("Continue Watching"));
    for (const MediaItem &item : m_switcherResumeItems)
    {
        const QString displayTitle = formatMediaSwitcherPlaybackTitle(item);
        auto *listItem = new QListWidgetItem(displayTitle);
        listItem->setData(Qt::UserRole, item.id);
        listItem->setData(Qt::UserRole + 1, item.userData.playbackPositionTicks);
        listItem->setData(Qt::UserRole + 2, displayTitle);
        listItem->setToolTip(displayTitle);
        m_resumeList->addItem(listItem);
    }

    if (m_switcherResumeItems.isEmpty())
    {
        auto *empty = new QListWidgetItem(tr("No active media found."));
        empty->setFlags(Qt::NoItemFlags);
        m_resumeList->addItem(empty);
    }
}

QCoro::Task<void> PlayerView::ensureMediaSwitcherDataLoaded()
{
    if (m_currentMediaId.isEmpty())
    {
        co_return;
    }

    if (m_switcherCacheReady && m_switcherCacheMediaId == m_currentMediaId)
    {
        qDebug().noquote()
            << "[PlayerView] Reuse cached media switcher data"
            << "| mediaId:" << m_currentMediaId
            << "| seriesMode:" << m_isSeriesMode;
        if (useHudMediaSwitcher() && m_mediaSwitchDrawer &&
            m_mediaSwitchDrawer->isVisible())
        {
            syncHudMediaSwitcherContent();
        }
        if (!useHudMediaSwitcher() && m_isRightSidebarVisible)
        {
            populateRightSidebarFromCache();
        }
        co_return;
    }

    QPointer<PlayerView> guard(this);
    const QString currentMediaId = m_currentMediaId;
    const bool seriesMode = m_isSeriesMode && !m_seriesId.isEmpty();
    const QString seriesId = m_seriesId;

    clearMediaSwitcherCache();

    qDebug().noquote()
        << "[PlayerView] Loading media switcher data"
        << "| mediaId:" << currentMediaId
        << "| seriesMode:" << seriesMode
        << "| seriesId:" << seriesId;

    try
    {
        if (seriesMode)
        {
            const QList<MediaItem> seasons =
                co_await m_core->mediaService()->getSeasons(seriesId);
            if (!guard || guard->m_currentMediaId != currentMediaId)
            {
                co_return;
            }

            guard->m_switcherSeriesSeasons = seasons;
            for (const MediaItem &season : seasons)
            {
                const QList<MediaItem> episodes =
                    co_await m_core->mediaService()->getEpisodes(seriesId,
                                                                 season.id);
                if (!guard || guard->m_currentMediaId != currentMediaId)
                {
                    co_return;
                }

                guard->m_switcherSeasonEpisodes.insert(season.id, episodes);
            }
        }
        else
        {
            const QList<MediaItem> items =
                co_await m_core->mediaService()->getResumeItems(30);
            if (!guard || guard->m_currentMediaId != currentMediaId)
            {
                co_return;
            }

            guard->m_switcherResumeItems = items;
        }

        if (!guard || guard->m_currentMediaId != currentMediaId)
        {
            co_return;
        }

        guard->m_switcherCacheMediaId = currentMediaId;
        guard->m_switcherCacheReady = true;
        qDebug().noquote()
            << "[PlayerView] Media switcher data ready"
            << "| mediaId:" << currentMediaId
            << "| resumeItems:" << guard->m_switcherResumeItems.size()
            << "| seasons:" << guard->m_switcherSeriesSeasons.size();
        if (guard->useHudMediaSwitcher() && guard->m_mediaSwitchDrawer &&
            guard->m_mediaSwitchDrawer->isVisible())
        {
            guard->syncHudMediaSwitcherContent();
        }
        if (!guard->useHudMediaSwitcher() && guard->m_isRightSidebarVisible)
        {
            guard->populateRightSidebarFromCache();
        }
    }
    catch (const std::exception &e)
    {
        if (!guard || guard->m_currentMediaId != currentMediaId)
        {
            co_return;
        }

        guard->clearMediaSwitcherCache();
        qDebug() << "[PlayerView] Media switcher data fetch failed:"
                 << e.what();
        if (guard->useHudMediaSwitcher() && guard->m_mediaSwitchDrawer &&
            guard->m_mediaSwitchDrawer->isVisible())
        {
            guard->syncHudMediaSwitcherContent();
        }
        if (!guard->useHudMediaSwitcher() && guard->m_isRightSidebarVisible)
        {
            guard->populateRightSidebarFromCache();
        }
    }
}

QCoro::Task<void> PlayerView::switchFromMediaSwitcher(
    QString mediaId, QString title, long long startPositionTicks)
{
    QPointer<PlayerView> guard(this);
    if (mediaId.isEmpty())
    {
        co_return;
    }

    if (mediaId == m_currentMediaId)
    {
        hideRightSidebar();
        hideHudMediaSwitcher();
        if (m_activePopup)
        {
            m_activePopup->deleteLater();
            m_activePopup = nullptr;
        }
        co_return;
    }

    m_switcherPendingItemId = mediaId;
    m_switcherPendingTitle = title;
    m_switcherPendingTicks = startPositionTicks;

    try
    {
        MediaItem detail = co_await m_core->mediaService()->getItemDetail(mediaId);

        if (!guard)
        {
            co_return;
        }
        if (m_switcherPendingItemId.isEmpty() || detail.id != m_switcherPendingItemId)
        {
            co_return;
        }

        if (detail.mediaSources.isEmpty())
        {
            PlaybackInfo playbackInfo =
                co_await m_core->mediaService()->getPlaybackInfo(detail.id);

            if (!guard)
            {
                co_return;
            }
            if (m_switcherPendingItemId.isEmpty() ||
                detail.id != m_switcherPendingItemId)
            {
                co_return;
            }

            detail.mediaSources = playbackInfo.mediaSources;
        }

        MediaSourceInfo selectedSource;
        QString mediaSourceId = detail.id;
        if (!detail.mediaSources.isEmpty())
        {
            int sourceIdx =
                MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
                    detail.mediaSources,
                    ConfigStore::instance()
                        ->get<QString>(ConfigKeys::PlayerPreferredVersion)
                        .trimmed());
            if (sourceIdx < 0 || sourceIdx >= detail.mediaSources.size())
            {
                sourceIdx = 0;
            }

            selectedSource = detail.mediaSources.at(sourceIdx);
            PlayerPreferenceUtils::applyPreferredStreamRules(
                selectedSource,
                ConfigStore::instance()->get<QString>(
                    ConfigKeys::PlayerAudioLang, "auto"),
                ConfigStore::instance()->get<QString>(
                    ConfigKeys::PlayerSubLang, "auto"));
            mediaSourceId = selectedSource.id;
        }

        QString streamUrl = selectedSource.id.isEmpty()
                                ? m_core->mediaService()->getStreamUrl(
                                      detail.id, mediaSourceId)
                                : m_core->mediaService()->getStreamUrl(
                                      detail.id, selectedSource);
        QString resolvedTitle =
            MediaItemUtils::playbackTitle(detail, m_seriesName);
        if (resolvedTitle.isEmpty())
        {
            resolvedTitle = m_switcherPendingTitle.trimmed();
        }

        qDebug().noquote()
            << "[PlayerView] Switching playback item"
            << "| mediaId:" << detail.id
            << "| sourceId:" << mediaSourceId
            << "| title:" << resolvedTitle
            << "| startTicks:" << m_switcherPendingTicks;

        hideRightSidebar();
        hideHudMediaSwitcher();
        if (m_activePopup)
        {
            m_activePopup->deleteLater();
            m_activePopup = nullptr;
        }

        stopAndReport();
        m_toastLabel->hide();

        PlayerLaunchContext launchContext;
        launchContext.mediaItem = detail;
        launchContext.selectedSource = selectedSource;
        const QVariant sourceInfoVar = QVariant::fromValue(launchContext);

        QTimer::singleShot(250, this,
                           [this, id = detail.id, resolvedTitle, streamUrl,
                            startTicks = m_switcherPendingTicks, sourceInfoVar]()
                           {
                               playMedia(id, resolvedTitle, streamUrl,
                                         startTicks, sourceInfoVar);
                           });

        m_switcherPendingItemId.clear();
        m_switcherPendingTitle.clear();
        m_switcherPendingTicks = 0;
    }
    catch (const std::exception &e)
    {
        if (!guard)
        {
            co_return;
        }
        qDebug() << "[PlayerView] Media switch failed:" << e.what();
        m_switcherPendingItemId.clear();
        m_switcherPendingTitle.clear();
        m_switcherPendingTicks = 0;
    }
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
                if (!item || item->flags() == Qt::NoItemFlags)
                {
                    co_return;
                }

                co_await switchFromMediaSwitcher(
                    item->data(Qt::UserRole).toString(),
                    item->data(Qt::UserRole + 2).toString(),
                    item->data(Qt::UserRole + 1).toLongLong());
            });

    m_rightSidebarAnim = new QPropertyAnimation(m_rightSidebar, "pos", this);
    m_rightSidebarAnim->setDuration(350);
    m_rightSidebarAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_rightSidebar->installEventFilter(this);
}

QCoro::Task<void> PlayerView::showRightSidebar()
{
    if (m_isViewTearingDown || useHudMediaSwitcher() || m_hasReportedStop)
    {
        co_return;
    }

    m_isRightSidebarVisible = true;
    const int sidebarY = m_topHUD->height();

    if (!m_rightSidebar || !m_rightSidebarAnim || !m_rightSidebarAnim->targetObject())
    {
        if (m_rightSidebar)
        {
            m_rightSidebar->move(width() - m_rightSidebar->width(), sidebarY);
        }
    }
    else
    {
        if (m_rightSidebarAnim->state() == QAbstractAnimation::Running)
        {
            stopPropertyAnimationSafely(m_rightSidebarAnim);
        }
        m_rightSidebarAnim->setStartValue(m_rightSidebar->pos());
        m_rightSidebarAnim->setEndValue(
            QPoint(width() - m_rightSidebar->width(), sidebarY));
        m_rightSidebarAnim->start();
    }

    showControls();

    if (m_switcherCacheReady && m_switcherCacheMediaId == m_currentMediaId)
    {
        populateRightSidebarFromCache();
        co_return;
    }

    m_sidebarTitleLabel->setText(
        m_isSeriesMode ? (m_seriesName.isEmpty() ? tr("Episodes") : m_seriesName)
                       : tr("Continue Watching"));
    m_resumeList->clear();
    auto *loadingItem = new QListWidgetItem(tr("Loading..."));
    loadingItem->setFlags(Qt::NoItemFlags);
    loadingItem->setForeground(QColor(140, 140, 140));
    m_resumeList->addItem(loadingItem);

    co_await ensureMediaSwitcherDataLoaded();
    if (!m_switcherCacheReady || m_switcherCacheMediaId != m_currentMediaId)
    {
        co_return;
    }

    populateRightSidebarFromCache();
}

void PlayerView::hideRightSidebar(bool immediate)
{
    immediate = immediate || m_isViewTearingDown;
    m_isRightSidebarVisible = false;
    if (!m_rightSidebar)
    {
        return;
    }

    const int sidebarY = m_topHUD ? m_topHUD->height() : 0;
    const QPoint hiddenPos(width() + 30, sidebarY);

    if (immediate || m_hasReportedStop || !m_rightSidebarAnim || !m_rightSidebarAnim->targetObject())
    {
        if (m_rightSidebarAnim)
        {
            stopPropertyAnimationSafely(m_rightSidebarAnim);
        }
        m_rightSidebar->move(hiddenPos);
        return;
    }

    if (m_rightSidebarAnim->state() == QAbstractAnimation::Running)
    {
        stopPropertyAnimationSafely(m_rightSidebarAnim);
    }
    m_rightSidebarAnim->setStartValue(m_rightSidebar->pos());
    m_rightSidebarAnim->setEndValue(hiddenPos);
    m_rightSidebarAnim->start();
}



void PlayerView::stopAndReport()
{
    
    if (m_hasReportedStop)
    {
        return;
    }
    m_hasReportedStop = true;
    m_longPressHandler->setTeardown(true);
    stopTransientUiAnimations(m_isViewTearingDown);

    
    if (m_reportTimer)
        m_reportTimer->stop();
    if (m_mousePollTimer)
        m_mousePollTimer->stop();
    m_longPressHandler->stopKeyLongPress(false);
    m_longPressHandler->stopMouseEdgeLongPress();
    if (m_bufferTimer)
        m_bufferTimer->stop();
    if (m_hideTimer)
        m_hideTimer->stop();
    if (m_osdLayer)
        m_osdLayer->forceHide();
    if (m_toastTimer)
        m_toastTimer->stop();
    if (m_singleClickTimer)
        m_singleClickTimer->stop();

    hideRightSidebar(true);
    hideHudMediaSwitcher();
    if (m_activePopup)
    {
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }
    closeActivePlayerDialog();

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
        if (m_danmakuController)
        {
            m_danmakuController->clearPlaybackContext();
        }
        
        disconnect(m_mpvWidget, &MpvWidget::positionChanged, this, &PlayerView::onPositionChanged);
        m_mpvWidget->controller()->command(QVariantList{"stop"});
    }
}

void PlayerView::beginViewTeardown()
{
    if (m_isViewTearingDown)
    {
        return;
    }

    m_isViewTearingDown = true;
    qDebug() << "[PlayerView] Begin teardown: stop timers, disconnect late signals, detach animations";

    if (m_reportTimer)
        m_reportTimer->stop();
    if (m_mousePollTimer)
        m_mousePollTimer->stop();
    if (m_bufferTimer)
        m_bufferTimer->stop();
    if (m_hideTimer)
        m_hideTimer->stop();
    if (m_toastTimer)
        m_toastTimer->stop();
    if (m_singleClickTimer)
        m_singleClickTimer->stop();

    if (m_longPressHandler)
    {
        m_longPressHandler->setTeardown(true);
        m_longPressHandler->stopKeyLongPress(false);
        m_longPressHandler->stopMouseEdgeLongPress();
    }

    disconnect(ConfigStore::instance(), nullptr, this, nullptr);
    if (m_danmakuController)
    {
        disconnect(m_danmakuController, nullptr, this, nullptr);
    }
    if (m_mpvWidget)
    {
        disconnect(m_mpvWidget, nullptr, this, nullptr);
        if (m_mpvWidget->controller())
        {
            disconnect(m_mpvWidget->controller(), nullptr, this, nullptr);
        }
    }

    stopTransientUiAnimations(true);
    detachFadeGroupTargets(m_fadeGroup);
    detachPropertyAnimationTarget(m_rightSidebarAnim);

    m_isRightSidebarVisible = false;
    if (m_mediaSwitchDrawer)
    {
        m_mediaSwitchDrawer->hide();
    }
    if (m_topHUD)
    {
        m_topHUD->hide();
    }
    if (m_bottomHUD)
    {
        m_bottomHUD->hide();
    }
    if (m_rightTrigger)
    {
        m_rightTrigger->hide();
    }
    if (m_rightSidebar)
    {
        m_rightSidebar->hide();
    }
    if (m_logoLabel)
    {
        m_logoLabel->hide();
    }
    if (m_networkSpeedLabel)
    {
        m_networkSpeedLabel->hide();
    }
    if (m_statisticsOverlay)
    {
        m_statisticsOverlay->hide();
    }
    if (m_toastLabel)
    {
        m_toastLabel->hide();
    }
    if (m_activePopup)
    {
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }
    closeActivePlayerDialog();

    setCursorHidden(false);
}

void PlayerView::keyPressEvent(QKeyEvent *event)
{
    if (m_isViewTearingDown)
    {
        event->ignore();
        return;
    }

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
            m_longPressHandler->startKeyLongPress(-1, !isHudVisible);
        }
        event->accept();
    }
    else if (event->key() == Qt::Key_Right)
    {
        if (!event->isAutoRepeat())
        {
            m_longPressHandler->startKeyLongPress(1, !isHudVisible);
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
    if (m_isViewTearingDown)
    {
        event->ignore();
        return;
    }

    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right)
    {
        if (!event->isAutoRepeat())
        {
            m_longPressHandler->stopKeyLongPress();
        }
        event->accept();
    }
    else
    {
        BaseView::keyReleaseEvent(event);
    }
}



void PlayerView::setEffectivePlaybackSpeed(double speed)
{
    if (!m_mpvWidget || !m_mpvWidget->controller())
    {
        return;
    }

    m_mpvWidget->controller()->setProperty("speed", speed);
    if (m_speedBtn)
    {
        m_speedBtn->setText(PlayerLongPressHandler::formatSpeedText(speed));
    }
}

void PlayerView::stopTransientUiAnimations(bool immediate)
{
    if (m_longPressHandler)
    {
        m_longPressHandler->stopAllAnimations();
    }

    if (m_loadingOverlay)
    {
        if (immediate)
        {
            m_loadingOverlay->forceStop();
        }
        else
        {
            m_loadingOverlay->stop();
        }
    }
    if (m_rightSidebarAnim)
    {
        stopPropertyAnimationSafely(m_rightSidebarAnim);
    }
    if (m_osdLayer)
    {
        m_osdLayer->stopAnimations();
    }
    if (m_fadeGroup)
    {
        stopFadeGroupSafely(m_fadeGroup);
    }
}

void PlayerView::updateOverlayLayout()
{
    if (m_isViewTearingDown)
    {
        return;
    }

    if (!m_mpvWidget || !m_topHUD || !m_bottomHUD)
    {
        return;
    }

    const int drawerHeight =
        (m_mediaSwitchDrawer && m_mediaSwitchDrawer->isVisible() &&
         useHudMediaSwitcher())
            ? m_mediaSwitchDrawer->preferredDrawerHeight()
            : 0;
    const int targetBottomHudHeight =
        m_bottomHudBaseHeight + (drawerHeight > 0 ? drawerHeight + 8 : 0);

    m_mpvWidget->setGeometry(0, 0, width(), height());
    m_loadingOverlay->setGeometry(0, 0, width(), height());
    m_topHUD->setGeometry(0, 0, width(), m_topHUD->height());
    m_bottomHUD->setFixedHeight(targetBottomHudHeight);
    m_bottomHUD->setGeometry(0, height() - m_bottomHUD->height(), width(),
                             m_bottomHUD->height());

    if (m_mediaSwitchDrawer)
    {
        m_mediaSwitchDrawer->setVisible(drawerHeight > 0 && useHudMediaSwitcher());
    }

    if (m_logoLabel)
    {
        m_logoLabel->move(24, m_topHUD->height());
    }

    if (m_networkSpeedLabel)
    {
        m_networkSpeedLabel->setGeometry(width() - 210, m_topHUD->height(), 200,
                                         30);
    }

    if (m_statisticsOverlay)
    {
        int statisticsY = m_topHUD->height() + 10;
        if (m_logoLabel && m_logoLabel->isVisible())
        {
            statisticsY = qMax(statisticsY, m_logoLabel->y() + m_logoLabel->height() + 8);
        }

        const int statisticsWidth = qMax(280, qMin(width() - 24, 620));
        m_statisticsOverlay->setFixedWidth(statisticsWidth);
        m_statisticsOverlay->setFixedHeight(m_statisticsOverlay->sizeHint().height());
        m_statisticsOverlay->move(12, statisticsY);
    }

    const int sidebarY = m_topHUD->height();
    const int sidebarH = height() - m_topHUD->height() - m_bottomHUD->height();

    m_rightTrigger->setGeometry(width() - 15, sidebarY, 15, sidebarH);

    if (!m_isRightSidebarVisible)
    {
        m_rightSidebar->setGeometry(width() + 30, sidebarY,
                                    m_rightSidebar->width(), sidebarH);
    }
    else
    {
        m_rightSidebar->setGeometry(width() - m_rightSidebar->width(), sidebarY,
                                    m_rightSidebar->width(), sidebarH);
    }

    if (m_osdLayer)
    {
        m_osdLayer->updateGeometry(width(), height());
    }

    if (m_longPressHandler)
    {
        m_longPressHandler->updateGeometry(width(), height(),
                                            m_topHUD ? m_topHUD->height() : 0);
    }

    if (m_activePopup)
    {
        m_activePopup->raise();
    }

    updateTitleElision();
}

void PlayerView::resizeEvent(QResizeEvent *event)
{
    BaseView::resizeEvent(event);

    if (m_isViewTearingDown)
    {
        return;
    }

    
    
    
    
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
    updateOverlayLayout();
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
    if (m_isViewTearingDown)
    {
        return BaseView::eventFilter(watched, event);
    }

    if (watched == m_mpvWidget || watched == this || watched == m_topHUD || watched == m_titleLabel ||
        watched == m_bottomHUD)
    {
        
        
        if (m_activePlayerDialog && m_activePlayerDialog->isVisible())
        {
            const auto type = event->type();
            if (type == QEvent::MouseButtonPress ||
                type == QEvent::MouseButtonRelease ||
                type == QEvent::MouseButtonDblClick ||
                type == QEvent::MouseMove ||
                type == QEvent::Wheel)
            {
                return true;
            }
        }

        if (event->type() == QEvent::MouseButtonPress)
        {
            auto *me = static_cast<QMouseEvent *>(event);
            const QPoint clickPos =
                this->mapFromGlobal(me->globalPosition().toPoint());
            const bool isOnHud =
                m_topHUD->geometry().contains(clickPos) ||
                m_bottomHUD->geometry().contains(clickPos);

            
            
            
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
                    m_activePopup->hide();
                    m_activePopup->close();
                    m_activePopup->deleteLater();
                    m_activePopup = nullptr;
                    return true;
                }
            }

            showControls();
            if (me->button() == Qt::LeftButton && watched != m_bottomHUD)
            {
                m_singleClickTimer->stop();

                if (!isOnHud)
                {
                    m_longPressHandler->startMouseEdgeLongPress(clickPos.x() < width() / 2 ? -1 : 1);
                }

                
                m_dragPos = me->globalPosition().toPoint();
                m_didDrag = false; 
            }
        }
        else if (event->type() == QEvent::MouseMove)
        {
            auto *me = static_cast<QMouseEvent *>(event);
            m_lastMousePos = me->globalPosition().toPoint();
            handlePointerActivity(m_lastMousePos);
            const QPoint localPos =
                this->mapFromGlobal(me->globalPosition().toPoint());
            const bool isOnHud =
                m_topHUD->geometry().contains(localPos) ||
                m_bottomHUD->geometry().contains(localPos);
            const bool movedEnough =
                (me->globalPosition().toPoint() - m_dragPos).manhattanLength() >
                QApplication::startDragDistance();

            if (m_longPressHandler->mouseEdgeLongPressDirection() != 0 &&
                (movedEnough || isOnHud))
            {
                m_longPressHandler->stopMouseEdgeLongPress();
            }

            if ((me->buttons() & Qt::LeftButton) && watched != m_bottomHUD)
            {

                
                if (!window()->isFullScreen())
                {
                    
                    if (movedEnough)
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
            const bool consumedByMouseEdge =
                me->button() == Qt::LeftButton && m_longPressHandler->stopMouseEdgeLongPress();
            if (consumedByMouseEdge)
            {
                m_singleClickTimer->stop();
                m_didDrag = true;
                return true;
            }

            
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
            if (me->button() == Qt::LeftButton)
            {
                m_longPressHandler->stopMouseEdgeLongPress();
            }
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
        
        if (m_activePlayerDialog && m_activePlayerDialog->isVisible())
        {
            return true;
        }
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

void PlayerView::handlePointerActivity(const QPoint &globalPos)
{
    if (m_isViewTearingDown || m_hasReportedStop)
    {
        return;
    }

    const QPoint localPos = mapFromGlobal(globalPos);
    if (!rect().contains(localPos))
    {
        return;
    }

    setCursorHidden(false);

    if (m_osdLayer && m_osdLayer->isVisible())
    {
        m_osdLayer->forceHide();
    }

    if (m_isPlaying)
    {
        m_hideTimer->start(3000);
    }
    else
    {
        m_hideTimer->stop();
    }

    if (areControlsFullyVisible())
    {
        return;
    }

    showControls();
}

void PlayerView::setCursorHidden(bool hidden)
{
    const auto targetShape =
        hidden ? Qt::BlankCursor : Qt::ArrowCursor;

    if (cursor().shape() != targetShape)
    {
        setCursor(targetShape);
    }

    if (m_mpvWidget && m_mpvWidget->cursor().shape() != targetShape)
    {
        m_mpvWidget->setCursor(targetShape);
    }
}

bool PlayerView::areControlsFullyVisible() const
{
    return m_topOpacity && m_topOpacity->opacity() >= 1.0 &&
           (!m_fadeGroup ||
            m_fadeGroup->state() != QAbstractAnimation::Running);
}



void PlayerView::showControls()
{
    if (m_isViewTearingDown || m_hasReportedStop)
    {
        return;
    }

    setCursorHidden(false);

    
    if (m_osdLayer && m_osdLayer->isVisible())
    {
        m_osdLayer->forceHide();
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
        stopFadeGroupSafely(m_fadeGroup);
    }

    bool canAnimate = true;
    for (int i = 0; i < m_fadeGroup->animationCount(); ++i)
    {
        auto *anim = qobject_cast<QPropertyAnimation *>(m_fadeGroup->animationAt(i));
        if (!anim || !anim->targetObject())
        {
            canAnimate = false;
            break;
        }
        anim->setStartValue(anim->targetObject()->property("opacity")); 
        anim->setEndValue(1.0);
    }
    if (!canAnimate)
    {
        if (m_topOpacity)
            m_topOpacity->setOpacity(1.0);
        if (m_bottomOpacity)
            m_bottomOpacity->setOpacity(1.0);
        if (m_logoOpacity)
            m_logoOpacity->setOpacity(1.0);
        if (m_speedOpacity)
            m_speedOpacity->setOpacity(1.0);
        return;
    }
    m_fadeGroup->setDirection(QAbstractAnimation::Forward);
    m_fadeGroup->start();
}


void PlayerView::hideControls()
{
    if (m_isViewTearingDown || m_hasReportedStop)
    {
        return;
    }

    if (!m_isPlaying)
    {
        return;
    }

    QPoint localPos = this->mapFromGlobal(QCursor::pos());
    bool isAppActive = this->window()->isActiveWindow();
    bool isMouseInside = this->rect().contains(localPos);
    
    bool isMouseInsidePopup = m_activePopup && m_activePopup->geometry().contains(localPos);
    const bool hasActivePlayerDialog =
        m_activePlayerDialog && m_activePlayerDialog->isVisible();

    
    if (hasActivePlayerDialog)
    {
        m_hideTimer->start(1000);
        return;
    }

    if (isAppActive && isMouseInside &&
        (m_topHUD->geometry().contains(localPos) || m_bottomHUD->geometry().contains(localPos) || isMouseInsidePopup ||
         (m_isRightSidebarVisible && m_rightSidebar->geometry().contains(localPos))))
    {
        m_hideTimer->start(1000);
        return;
    }

    hideRightSidebar();
    hideHudMediaSwitcher();

    if (m_activePopup)
    {
        m_activePopup->hide();
        m_activePopup->close();
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    if (m_topOpacity->opacity() <= 0.0)
    {
        return;
    }

    if (m_fadeGroup->state() == QAbstractAnimation::Running)
    {
        stopFadeGroupSafely(m_fadeGroup);
    }

    bool canAnimate = true;
    for (int i = 0; i < m_fadeGroup->animationCount(); ++i)
    {
        auto *anim = qobject_cast<QPropertyAnimation *>(m_fadeGroup->animationAt(i));
        if (!anim || !anim->targetObject())
        {
            canAnimate = false;
            break;
        }
        anim->setStartValue(anim->targetObject()->property("opacity")); 
        anim->setEndValue(0.0);
    }
    if (!canAnimate)
    {
        if (m_topOpacity)
            m_topOpacity->setOpacity(0.0);
        if (m_bottomOpacity)
            m_bottomOpacity->setOpacity(0.0);
        if (m_logoOpacity)
            m_logoOpacity->setOpacity(0.0);
        if (m_speedOpacity)
            m_speedOpacity->setOpacity(0.0);
        return;
    }
    m_fadeGroup->setDirection(QAbstractAnimation::Forward);
    m_fadeGroup->start();

    setCursorHidden(true);
}

void PlayerView::onMpvPropertyChanged(const QString &property, const QVariant &value)
{
    if (m_isViewTearingDown)
    {
        return;
    }

    if (property == "cache-speed")
    {
        qint64 speedBytes = value.toLongLong();
        m_networkSpeedLabel->setText(formatDataRateValue(speedBytes));
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

    if (m_showStatisticsOverlay &&
        (property == QLatin1String("cache-speed") || property == QLatin1String("track-list") ||
         property == QLatin1String("pause")))
    {
        updateStatisticsDisplay();
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
        if (m_osdLayer && m_topOpacity->opacity() <= 0.0)
        {
            m_osdLayer->showVolume(tr("Volume: %1%").arg(static_cast<int>(m_currentVolume)));
        }
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
    const bool showDanmakuControls = shouldShowDanmakuHudControls();

    
    panel->addItem(tr("Network Speed"), "network_speed", m_showNetworkSpeed);
    panel->addItem(tr("Statistics"), "statistics", m_showStatisticsOverlay);
    panel->addItem(tr("Subtitle Settings"), "subtitle_settings", false);
    if (showDanmakuControls)
    {
        panel->addItem(tr("Danmaku Settings"), "danmaku_settings", false);
    }

    
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
                auto dismissPopup = [this]()
                {
                    if (m_activePopup)
                    {
                        m_activePopup->hide();
                        m_activePopup->close();
                        m_activePopup->deleteLater();
                        m_activePopup = nullptr;
                    }
                };

                if (action == "network_speed")
                {
                    m_showNetworkSpeed = !m_showNetworkSpeed;
                    m_networkSpeedLabel->setVisible(m_showNetworkSpeed);
                    showToast(m_showNetworkSpeed ? tr("Network Speed Enabled") : tr("Network Speed Disabled"));
                }
                else if (action == "statistics")
                {
                    m_showStatisticsOverlay = !m_showStatisticsOverlay;
                    if (m_showStatisticsOverlay)
                    {
                        updateStatisticsDisplay();
                        m_statisticsOverlay->show();
                        updateOverlayLayout();
                    }
                    else
                    {
                        m_statisticsOverlay->hide();
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
                else if (action == "subtitle_settings")
                {
                    dismissPopup();
                    openSubtitleSettingsDialog();
                    return;
                }
                else if (action == "danmaku_settings")
                {
                    dismissPopup();
                    openDanmakuSettingsDialog();
                    return;
                }

                
                dismissPopup();
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
                setEffectivePlaybackSpeed(s);
                showToast(tr("Speed: %1X").arg(PlayerLongPressHandler::formatSpeedText(s).chopped(1)));

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
    const QList<QVariantMap> tracks =
        m_danmakuController ? m_danmakuController->contentSubtitleTracks()
                            : QList<QVariantMap>{};
    bool anySubSelected = false;

    for (const QVariantMap &map : tracks)
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

    panel->addItem(tr("Disable Subtitles"), "no", !anySubSelected);
    panel->addItem(tr("Subtitle Settings"), QStringLiteral("settings"), false);

    int maxHeight = this->height() - m_bottomHUD->height() - 40;
    panel->finalizeLayout(maxHeight < 150 ? 150 : maxHeight, 240);

    connect(panel, &ModernScrollPanel::itemTriggered, this,
            [this](const QVariant &data, const QString &text)
            {
                auto dismissPopup = [this]()
                {
                    if (m_activePopup)
                    {
                        m_activePopup->hide();
                        m_activePopup->close();
                        m_activePopup->deleteLater();
                        m_activePopup = nullptr;
                    }
                };

                const QString action = data.toString();
                if (action == QLatin1String("settings"))
                {
                    dismissPopup();
                    openSubtitleSettingsDialog();
                    return;
                }

                if (m_danmakuController)
                {
                    m_danmakuController->selectSubtitleTrack(data);
                }

                showToast(action == "no" ? tr("Subtitles Disabled")
                                          : tr("Subtitle: %1").arg(text));
                dismissPopup();
            });

    showCenteredPopup(panel, m_subtitleBtn);
}

void PlayerView::showDanmakuMenu()
{
    if (!shouldShowDanmakuHudControls())
    {
        return;
    }

    auto *panel = new ModernScrollPanel(this);

    const bool visible =
        m_danmakuController && m_danmakuController->isDanmakuVisible();
    panel->addItem(buildDanmakuSummaryText(), QStringLiteral("info"), false);
    panel->addItem(visible ? tr("Hide Danmaku") : tr("Show Danmaku"),
                   QStringLiteral("toggle"), visible);
    panel->addItem(tr("Reload Danmaku"), QStringLiteral("reload"), false);
    panel->addItem(tr("Load Local Danmaku File"),
                   QStringLiteral("load_local"), false);
    panel->addItem(tr("Search Danmaku"), QStringLiteral("search"), false);
    panel->addItem(tr("Danmaku Settings"),
                   QStringLiteral("quick_settings"), false);

    int maxHeight = this->height() - m_bottomHUD->height() - 40;
    panel->finalizeLayout(maxHeight < 160 ? 160 : maxHeight, 360);

    connect(panel, &ModernScrollPanel::itemTriggered, this,
            [this](const QVariant &data, const QString &text)
            {
                Q_UNUSED(text);
                if (!m_danmakuController)
                {
                    return;
                }

                auto dismissPopup = [this]()
                {
                    if (m_activePopup)
                    {
                        m_activePopup->hide();
                        m_activePopup->close();
                        m_activePopup->deleteLater();
                        m_activePopup = nullptr;
                    }
                };

                const QString action = data.toString();
                if (action == QLatin1String("info"))
                {
                    showToast(buildDanmakuSummaryText());
                }
                else if (action == QLatin1String("toggle"))
                {
                    m_danmakuController->setDanmakuVisible(
                        !m_danmakuController->isDanmakuVisible());
                }
                else if (action == QLatin1String("reload"))
                {
                    m_danmakuController->reload();
                }
                else if (action == QLatin1String("load_local"))
                {
                    dismissPopup();
                    loadLocalDanmakuFile();
                    return;
                }
                else if (action == QLatin1String("search"))
                {
                    dismissPopup();
                    showDanmakuIdentifyDialog();
                    return;
                }
                else if (action == QLatin1String("quick_settings"))
                {
                    dismissPopup();
                    openDanmakuSettingsDialog();
                    return;
                }

                dismissPopup();
            });

    showCenteredPopup(panel, m_danmakuBtn);
}

void PlayerView::showDanmakuIdentifyDialog()
{
    if (!m_danmakuController || !m_danmakuController->hasPlaybackContext()) {
        showToast(tr("Danmaku is unavailable for the current media"));
        return;
    }

    if (m_activePopup) {
        m_activePopup->hide();
        m_activePopup->close();
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    const QString activeTargetId =
        m_danmakuController ? m_danmakuController->activeTargetId() : QString();
    auto *dialog = new PlayerDanmakuIdentifyDialog(
        m_core, m_danmakuController->mediaContext(), QString(),
        activeTargetId, this);
    connect(dialog, &PlayerDanmakuIdentifyDialog::finished, this,
            [this, dialog](int result)
            {
                if (result != PlayerOverlayDialog::Accepted ||
                    !dialog->selectedCandidate().isValid() ||
                    !m_danmakuController)
                {
                    return;
                }

                m_danmakuController->loadFromCandidate(
                    dialog->selectedCandidate(), true);
            });
    trackPlayerDialog(dialog);
}

void PlayerView::loadLocalDanmakuFile()
{
    if (!m_danmakuController || !m_danmakuController->hasPlaybackContext())
    {
        showToast(tr("Danmaku is unavailable for the current media"));
        return;
    }

    if (m_activePopup)
    {
        m_activePopup->hide();
        m_activePopup->close();
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }
    closeActivePlayerDialog();

    const QString serverId = m_danmakuController->mediaContext().serverId;
    const QString startDir = DanmakuService::ensureLocalDanmakuDirectory(serverId)
                                 ? DanmakuService::localDanmakuDirectoryPath(serverId)
                                 : QDir::homePath();
    const QString filePath = QFileDialog::getOpenFileName(
        this, tr("Load Local Danmaku File"), startDir,
        tr("Danmaku Files (*.ass *.json *.xml);;ASS Files (*.ass);;JSON Files (*.json);;XML Files (*.xml)"));
    if (filePath.trimmed().isEmpty())
    {
        return;
    }

    qDebug().noquote()
        << "[Danmaku][PlayerView] Load local danmaku file"
        << "| mediaId:" << m_danmakuController->mediaContext().mediaId
        << "| path:" << filePath;
    m_danmakuController->loadLocalFile(filePath);
}

void PlayerView::openSubtitleSettingsDialog()
{
    if (m_activePopup)
    {
        m_activePopup->hide();
        m_activePopup->close();
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    auto *dialog = new PlayerSubtitleSettingsDialog(this);
    trackPlayerDialog(dialog);
}

void PlayerView::openDanmakuSettingsDialog()
{
    if (!shouldShowDanmakuHudControls())
    {
        return;
    }

    if (m_activePopup)
    {
        m_activePopup->hide();
        m_activePopup->close();
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }

    auto *dialog = new PlayerDanmakuSettingsDialog(this);
    connect(dialog, &PlayerDanmakuSettingsDialog::danmakuEnabledToggled, this,
            [this](bool enabled)
            {
                if (!m_danmakuController)
                {
                    return;
                }
                m_danmakuController->setDanmakuEnabled(enabled);
            });
    connect(dialog, &PlayerDanmakuSettingsDialog::liveReloadRequested, this,
            [this]()
            {
                if (!m_danmakuController ||
                    !m_danmakuController->hasPlaybackContext() ||
                    !ConfigStore::instance()->get<bool>(
                        ConfigKeys::PlayerDanmakuEnabled, false))
                {
                    return;
                }
                m_danmakuController->reload();
            });
    connect(dialog, &PlayerDanmakuSettingsDialog::finished, this,
            [this, dialog](int)
            {
                if (!m_danmakuController)
                {
                    return;
                }

                if (dialog->requiresReload() &&
                    m_danmakuController->hasPlaybackContext() &&
                    ConfigStore::instance()->get<bool>(
                        ConfigKeys::PlayerDanmakuEnabled, false))
                {
                    m_danmakuController->reload();
                }
            });
    trackPlayerDialog(dialog);
}

void PlayerView::applySubtitleStyleSettings()
{
    if (!m_mpvWidget || !m_mpvWidget->controller())
    {
        return;
    }

    const bool protectDanmakuPrimary =
        m_danmakuController && m_danmakuController->isDanmakuEnabled() &&
        m_danmakuController->hasDanmakuTrack();
    SubtitleStyleUtils::applyToController(m_mpvWidget->controller(),
                                          protectDanmakuPrimary);
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
        if (m_osdLayer && m_topOpacity->opacity() <= 0.0)
        {
            m_osdLayer->showSeek(m_currentPosition, m_totalDuration, formatTime(m_currentPosition, m_totalDuration));
        }
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
    if (m_isViewTearingDown)
    {
        return;
    }

    m_toastLabel->setText(msg);
    m_toastLabel->show();
    m_toastTimer->start(2500);
}

void PlayerView::updateStatisticsDisplay()
{
    if (!m_statisticsOverlay || !m_mpvWidget || !m_mpvWidget->controller())
    {
        return;
    }

    auto *ctrl = m_mpvWidget->controller();
    const QVariantList tracks = ctrl->getProperty("track-list").toList();
    const QVariantMap selectedVideoTrack = findSelectedTrackByType(tracks, QStringLiteral("video"));
    const QVariantMap selectedAudioTrack = findSelectedTrackByType(tracks, QStringLiteral("audio"));
    const QVariantMap selectedSubtitleTrack = findSelectedTrackByType(tracks, QStringLiteral("sub"));

    const MediaSourceInfo &source = m_currentMediaSourceInfo;
    const MediaStreamInfo *videoStream = findFirstStreamByType(source, QStringLiteral("Video"));
    const MediaStreamInfo *audioStream = findFirstStreamByType(source, QStringLiteral("Audio"));

    auto addLine = [](QStringList &lines, const QString &label, const QString &value)
    {
        lines << (label + QStringLiteral(": ") + trimOrDash(value));
    };

    QStringList lines;

    QString sourcePath = firstNonEmpty({source.path, m_currentMediaItem.path});
    sourcePath = QUrl::fromPercentEncoding(sourcePath.toUtf8()).trimmed();

    QString fileDisplay = sourcePath;
    if (!fileDisplay.isEmpty())
    {
        const QFileInfo fileInfo(fileDisplay);
        if (!fileInfo.fileName().isEmpty())
        {
            fileDisplay = fileInfo.fileName();
        }
    }
    if (fileDisplay.isEmpty())
    {
        fileDisplay = firstNonEmpty({source.name, m_currentMediaItem.name, m_fullTitle});
    }
    addLine(lines, tr("File"), fileDisplay);

    QStringList sessionParts;
    if (!m_currentMediaId.isEmpty())
    {
        sessionParts << tr("Item ID") + QStringLiteral("=") + m_currentMediaId;
    }
    if (!m_currentMediaSourceId.isEmpty())
    {
        sessionParts << tr("Media Source ID") + QStringLiteral("=") + m_currentMediaSourceId;
    }
    if (!m_currentPlaySessionId.isEmpty())
    {
        sessionParts << tr("Play Session ID") + QStringLiteral("=") + m_currentPlaySessionId;
    }
    if (!sessionParts.isEmpty())
    {
        addLine(lines, tr("Session"), sessionParts.join(QStringLiteral("  ")));
    }

    const QVariantList chapterList = ctrl->getProperty("chapter-list").toList();
    const int chapterIndex = ctrl->getProperty("chapter").toInt();
    if (!chapterList.isEmpty() && chapterIndex >= 0 && chapterIndex < chapterList.size())
    {
        const QVariantMap chapterInfo = chapterList.at(chapterIndex).toMap();
        QString chapterTitle = chapterInfo.value(QStringLiteral("title")).toString().trimmed();
        if (chapterTitle.isEmpty())
        {
            chapterTitle = tr("Chapter") + QStringLiteral(" ") + QString::number(chapterIndex + 1);
        }
        chapterTitle +=
            QStringLiteral(" (%1 / %2)").arg(chapterIndex + 1).arg(chapterList.size());
        addLine(lines, tr("Chapter"), chapterTitle);
    }

    const qint64 fileSize = source.size > 0 ? source.size : m_currentMediaItem.size;
    const QString container =
        firstNonEmpty({source.container.toUpper(), m_currentMediaItem.container.toUpper()});
    addLine(lines, tr("Size"), joinNonEmpty({fileSize > 0 ? FileUtils::formatSize(fileSize) : QString(),
                                             container},
                                            QStringLiteral("  ")));

    const QVariantMap cacheState = ctrl->getProperty("demuxer-cache-state").toMap();
    const qint64 cacheBytes = qMax(qMax(variantToLongLong(cacheState.value(QStringLiteral("fw-bytes"))),
                                        variantToLongLong(cacheState.value(QStringLiteral("file-cache-bytes")))),
                                   variantToLongLong(cacheState.value(QStringLiteral("total-bytes"))));
    double cacheDuration = ctrl->getProperty("demuxer-cache-duration").toDouble();
    if (cacheDuration <= 0.0)
    {
        cacheDuration = variantToDouble(cacheState.value(QStringLiteral("cache-duration")));
    }
    const qint64 cacheSpeed = ctrl->getProperty("cache-speed").toLongLong();
    addLine(lines, tr("Cache"),
            joinNonEmpty({cacheBytes > 0 ? FileUtils::formatSize(cacheBytes) : QString(),
                          formatDurationValue(cacheDuration),
                          formatDataRateValue(cacheSpeed)},
                         QStringLiteral("  ")));

    const QString voName = ctrl->getProperty("current-vo").toString().trimmed();
    QString decoderName = ctrl->getProperty("hwdec-current").toString().trimmed();
    if (decoderName.isEmpty() || decoderName.compare(QStringLiteral("no"), Qt::CaseInsensitive) == 0)
    {
        decoderName = tr("Software (CPU)");
    }
    else
    {
        decoderName = decoderName.toUpper();
    }

    QStringList playbackParts;
    if (!voName.isEmpty())
    {
        playbackParts << voName;
    }
    if (!decoderName.isEmpty())
    {
        playbackParts << decoderName;
    }
    playbackParts << tr("Speed") + QStringLiteral(" ") +
                         stripTrailingZeros(QString::number(m_currentSpeed, 'f', 2)) +
                         QStringLiteral("X");

    const double avsync = ctrl->getProperty("avsync").toDouble();
    const QString avsyncText = formatAvSyncValue(avsync);
    if (!avsyncText.isEmpty())
    {
        playbackParts << tr("A-V") + QStringLiteral(" ") + avsyncText;
    }
    addLine(lines, tr("Playback"), playbackParts.join(QStringLiteral("  ")));

    double displayFps = ctrl->getProperty("display-fps").toDouble();
    if (displayFps <= 0.0)
    {
        displayFps = ctrl->getProperty("estimated-display-fps").toDouble();
    }
    double videoFps = ctrl->getProperty("container-fps").toDouble();
    if (videoFps <= 0.0)
    {
        videoFps = selectedVideoTrack.value(QStringLiteral("demux-fps")).toDouble();
    }
    if (videoFps <= 0.0 && videoStream)
    {
        videoFps = videoStream->realFrameRate;
    }

    QStringList refreshParts;
    if (displayFps > 0.0)
    {
        refreshParts << QStringLiteral("%1 Hz")
                            .arg(stripTrailingZeros(QString::number(displayFps, 'f', 2)));
    }
    const QString frameRateText = formatFrameRateValue(videoFps);
    if (!frameRateText.isEmpty())
    {
        refreshParts << frameRateText;
    }
    addLine(lines, tr("Refresh Rate"), refreshParts.join(QStringLiteral("  ")));

    const QString outputDrops = ctrl->getProperty("frame-drop-count").toString().trimmed();
    const QString decoderDrops = ctrl->getProperty("decoder-frame-drop-count").toString().trimmed();
    QStringList dropParts;
    if (!outputDrops.isEmpty())
    {
        dropParts << tr("Output") + QStringLiteral(" ") + outputDrops;
    }
    if (!decoderDrops.isEmpty())
    {
        dropParts << tr("Decoder") + QStringLiteral(" ") + decoderDrops;
    }
    addLine(lines, tr("Dropped Frames"), dropParts.join(QStringLiteral("  ")));

    lines << QString();

    const QString videoCodec =
        firstNonEmpty({ctrl->getProperty("video-codec").toString().toUpper(),
                       selectedVideoTrack.value(QStringLiteral("codec")).toString().toUpper(),
                       videoStream ? videoStream->codec.toUpper() : QString()});
    const QString videoFormat = ctrl->getProperty("video-format").toString().toUpper().trimmed();
    const QString videoProfile = videoStream ? videoStream->profile.trimmed() : QString();
    const QString videoLevel = videoStream ? formatLevelValue(videoStream->level) : QString();

    QStringList videoCodecParts;
    if (!videoCodec.isEmpty())
    {
        videoCodecParts << videoCodec;
    }
    if (!videoFormat.isEmpty())
    {
        videoCodecParts << videoFormat;
    }
    if (!videoProfile.isEmpty())
    {
        videoCodecParts << videoProfile;
    }
    if (!videoLevel.isEmpty())
    {
        videoCodecParts << QStringLiteral("L%1").arg(videoLevel);
    }
    addLine(lines, tr("Video"), videoCodecParts.join(QStringLiteral(" / ")));

    addLine(lines, tr("Frame Rate"), frameRateText);

    int sourceWidth = ctrl->getProperty("width").toInt();
    int sourceHeight = ctrl->getProperty("height").toInt();
    if ((sourceWidth <= 0 || sourceHeight <= 0) && videoStream)
    {
        sourceWidth = videoStream->width;
        sourceHeight = videoStream->height;
    }
    const double displayWidthRaw = ctrl->getProperty("video-params/dw").toDouble();
    const double displayHeightRaw = ctrl->getProperty("video-params/dh").toDouble();
    const int displayWidth = displayWidthRaw > 0.0 ? qRound(displayWidthRaw) : sourceWidth;
    const int displayHeight = displayHeightRaw > 0.0 ? qRound(displayHeightRaw) : sourceHeight;

    QString resolutionText = formatDimensionValue(sourceWidth, sourceHeight);
    const QString displayDimensions = formatDimensionValue(displayWidth, displayHeight);
    if (resolutionText.isEmpty())
    {
        resolutionText = displayDimensions;
    }
    else if (!displayDimensions.isEmpty() && displayDimensions != resolutionText)
    {
        resolutionText += QStringLiteral(" -> ") + displayDimensions;
    }
    const QString aspectText =
        formatAspectValue(displayWidth, displayHeight, videoStream ? videoStream->aspectRatio : QString());
    if (!aspectText.isEmpty())
    {
        resolutionText += QStringLiteral(" (%1)").arg(aspectText);
    }
    addLine(lines, tr("Resolution"), resolutionText);

    const QString pixelFormat =
        firstNonEmpty({ctrl->getProperty("video-params/pixelformat").toString(),
                       videoStream ? videoStream->pixelFormat : QString()});
    const QString colorLevels = ctrl->getProperty("video-params/colorlevels").toString().trimmed();
    const int bitDepth = videoStream ? videoStream->bitDepth : 0;
    addLine(lines, tr("Format"),
            joinNonEmpty({pixelFormat,
                          colorLevels,
                          bitDepth > 0 ? tr("%1-bit").arg(bitDepth) : QString()},
                         QStringLiteral("  ")));

    const QString colorMatrix = ctrl->getProperty("video-params/colormatrix").toString().trimmed();
    const QString colorPrimaries = ctrl->getProperty("video-params/primaries").toString().trimmed();
    const QString colorTransfer = ctrl->getProperty("video-params/gamma").toString().trimmed();
    addLine(lines, tr("Color"),
            joinNonEmpty({colorMatrix, colorPrimaries, colorTransfer},
                         QStringLiteral("  ")));

    qint64 videoBitrate = ctrl->getProperty("video-bitrate").toLongLong();
    if (videoBitrate <= 0 && videoStream)
    {
        videoBitrate = videoStream->bitRate;
    }
    if (videoBitrate <= 0)
    {
        videoBitrate = m_currentMediaItem.bitrate;
    }
    addLine(lines, tr("Bitrate"), formatBitrateValue(videoBitrate));

    lines << QString();

    const QString audioCodec =
        firstNonEmpty({ctrl->getProperty("audio-codec").toString().toUpper(),
                       selectedAudioTrack.value(QStringLiteral("codec")).toString().toUpper(),
                       audioStream ? audioStream->codec.toUpper() : QString()});
    const QString currentAo = ctrl->getProperty("current-ao").toString().trimmed();
    addLine(lines, tr("Audio"),
            joinNonEmpty({audioCodec,
                          !currentAo.isEmpty() ? tr("AO %1").arg(currentAo) : QString()},
                         QStringLiteral("  ")));

    addLine(lines, tr("Device"), ctrl->getProperty("audio-device").toString());

    const int channelCount =
        qMax(ctrl->getProperty("audio-params/channel-count").toInt(),
             audioStream ? audioStream->channels : 0);
    const QString channelLayout =
        firstNonEmpty({ctrl->getProperty("audio-params/channels").toString(),
                       selectedAudioTrack.value(QStringLiteral("demux-channels")).toString(),
                       audioStream ? audioStream->channelLayout : QString()});
    addLine(lines, tr("Channels"),
            joinNonEmpty({channelCount > 0 ? tr("%1 ch").arg(channelCount) : QString(),
                          channelLayout},
                         QStringLiteral("  ")));

    int sampleRate = ctrl->getProperty("audio-params/samplerate").toInt();
    if (sampleRate <= 0)
    {
        sampleRate = selectedAudioTrack.value(QStringLiteral("demux-samplerate")).toInt();
    }
    if (sampleRate <= 0 && audioStream)
    {
        sampleRate = audioStream->sampleRate;
    }
    addLine(lines, tr("Sample Rate"),
            sampleRate > 0 ? QStringLiteral("%1 Hz").arg(sampleRate) : QString());

    qint64 audioBitrate = ctrl->getProperty("audio-bitrate").toLongLong();
    if (audioBitrate <= 0 && audioStream)
    {
        audioBitrate = audioStream->bitRate;
    }
    addLine(lines, tr("Bitrate"), formatBitrateValue(audioBitrate));

    if (!selectedSubtitleTrack.isEmpty())
    {
        const QString subtitleLabel =
            firstNonEmpty({selectedSubtitleTrack.value(QStringLiteral("title")).toString(),
                           selectedSubtitleTrack.value(QStringLiteral("lang")).toString()});
        if (!subtitleLabel.isEmpty())
        {
            addLine(lines, tr("Subtitle"), subtitleLabel);
        }
    }

    m_statisticsOverlay->setLines(lines);
    if (m_showStatisticsOverlay)
    {
        updateOverlayLayout();
    }
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
                safeThis->updateOverlayLayout();
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
    PlayerLaunchContext launchContext;
    MediaSourceInfo resolvedSourceInfo;
    MediaItem resolvedItem;
    if (sourceInfoVar.isValid() && sourceInfoVar.canConvert<PlayerLaunchContext>())
    {
        launchContext = sourceInfoVar.value<PlayerLaunchContext>();
        resolvedItem = launchContext.mediaItem;
        resolvedSourceInfo = launchContext.selectedSource;
    }
    else if (sourceInfoVar.isValid() && sourceInfoVar.canConvert<MediaSourceInfo>())
    {
        resolvedSourceInfo = sourceInfoVar.value<MediaSourceInfo>();
    }

    connect(m_mpvWidget, &MpvWidget::positionChanged, this,
            &PlayerView::onPositionChanged, Qt::UniqueConnection);

    m_hasReportedStop = false;
    m_currentMediaId = mediaId;
    m_currentMediaSourceId = mediaId;
    m_currentPlaySessionId.clear();
    m_currentMediaItem = resolvedItem;
    m_currentMediaSourceInfo = resolvedSourceInfo;
    hideRightSidebar();
    hideHudMediaSwitcher();
    if (m_activePopup)
    {
        m_activePopup->deleteLater();
        m_activePopup = nullptr;
    }
    closeActivePlayerDialog();

    
    m_logoLabel->clear();
    m_logoLabel->hide();

    
    m_originalStreamUrl = streamUrl;
    m_currentSourceInfoVar = sourceInfoVar;

    QString actualStreamUrl = streamUrl;
    if (!resolvedSourceInfo.id.isEmpty())
    {
        QString directUrl = m_core->mediaService()->getStreamUrl(mediaId, resolvedSourceInfo);
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

    if (m_currentMediaSourceInfo.id.isEmpty() && !m_currentMediaItem.mediaSources.isEmpty())
    {
        for (const MediaSourceInfo &candidate : m_currentMediaItem.mediaSources)
        {
            if (!m_currentMediaSourceId.isEmpty() &&
                candidate.id.compare(m_currentMediaSourceId, Qt::CaseInsensitive) == 0)
            {
                m_currentMediaSourceInfo = candidate;
                break;
            }
        }

        if (m_currentMediaSourceInfo.id.isEmpty())
        {
            m_currentMediaSourceInfo = m_currentMediaItem.mediaSources.first();
        }
    }

    m_pendingSeekSeconds = startPositionTicks / 10000000.0;

    m_fullTitle = title;
    updateTitleElision();

    m_currentPosition = m_pendingSeekSeconds;
    m_totalDuration = 0.0;

    if (m_statisticsOverlay)
    {
        if (m_showStatisticsOverlay)
        {
            updateStatisticsDisplay();
            m_statisticsOverlay->show();
        }
        else
        {
            m_statisticsOverlay->hide();
        }
    }

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
    clearMediaSwitcherCache();
    updateMediaSwitcherButton();

    if (resolvedItem.type == "Episode" && !resolvedItem.seriesId.isEmpty())
    {
        m_isSeriesMode = true;
        m_seriesId = resolvedItem.seriesId;
        m_seriesName = resolvedItem.seriesName;
        updateMediaSwitcherButton();
        
        ensureMediaSwitcherDataLoaded();
    }
    else
    {
        
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
                    safeThis->clearMediaSwitcherCache();
                    safeThis->updateMediaSwitcherButton();
                    
                    safeThis->ensureMediaSwitcherDataLoaded();
                    if (safeThis->useHudMediaSwitcher() &&
                        safeThis->m_mediaSwitchDrawer &&
                        safeThis->m_mediaSwitchDrawer->isVisible())
                    {
                        safeThis->showHudMediaSwitcher();
                    }
                    else if (safeThis->m_isRightSidebarVisible)
                    {
                        safeThis->showRightSidebar();
                    }
                }
            }
            catch (...)
            {
            }
        };
        detectSeriesMode(QPointer<PlayerView>(this), m_core, mediaId);
        
        ensureMediaSwitcherDataLoaded();
    }

    
    
    
    executeFetchLogo(QPointer<PlayerView>(this), m_core, mediaId);

    
    m_mpvWidget->loadMedia(actualStreamUrl);

    if (!resolvedItem.id.isEmpty() || !resolvedSourceInfo.id.isEmpty())
    {
        if (resolvedItem.id.isEmpty())
        {
            resolvedItem.id = mediaId;
            resolvedItem.name = title;
        }
        PlayerLaunchContext danmakuContext;
        danmakuContext.mediaItem = resolvedItem;
        danmakuContext.selectedSource = resolvedSourceInfo;
        qDebug().noquote()
            << "[Danmaku][PlayerView] Prepared playback context"
            << "| mediaId:" << danmakuContext.mediaItem.id
            << "| sourceId:" << danmakuContext.selectedSource.id
            << "| title:" << danmakuContext.mediaItem.name;
        m_danmakuController->setPlaybackContext(danmakuContext);
    }
    else
    {
        qDebug().noquote()
            << "[Danmaku][PlayerView] No playback context available for danmaku"
            << "| mediaId:" << mediaId
            << "| sourceInfoValid:" << sourceInfoVar.isValid();
        m_danmakuController->clearPlaybackContext();
    }

    
    
    
    m_isBuffering = true;
    m_isSeeking = false;
    updateLoadingState();

    
    
    
    m_targetAudioStreamIndex = -2; 
    m_targetSubStreamIndex = -2;

    QVariantList pendingSubtitles;

    if (!resolvedSourceInfo.id.isEmpty())
    {
        MediaSourceInfo sourceInfo = resolvedSourceInfo;

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
    if (m_isViewTearingDown || m_currentMediaId.isEmpty() || m_hasReportedStop ||
        m_currentPlaySessionId.isEmpty())
    {
        return;
    }
    long long currentTicks = static_cast<long long>(m_currentPosition * 10000000.0);
    
    m_core->mediaService()->reportPlaybackProgress(m_currentMediaId, m_currentMediaSourceId, currentTicks, !m_isPlaying,
                                                   m_currentPlaySessionId);

    if (m_showStatisticsOverlay)
    {
        updateStatisticsDisplay();
    }
}

void PlayerView::onBackClicked()
{
    if (m_isViewTearingDown)
    {
        return;
    }

    
    
    prepareForStackLeave();
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
    if (m_isViewTearingDown)
    {
        return;
    }

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

    
    if (m_osdLayer && m_osdLayer->isSeekLineVisible())
    {
        m_osdLayer->updateSeekPosition(static_cast<int>(position), formatTime(position, m_totalDuration));
    }
}

void PlayerView::onDurationChanged(double duration)
{
    if (m_isViewTearingDown)
    {
        return;
    }

    if (std::isnan(duration) || std::isinf(duration) || duration < 0)
    {
        duration = 0.0;
    }
    m_totalDuration = duration;
    m_progressSlider->setMaximum(static_cast<int>(duration));
    m_totalTimeLabel->setText(formatTime(duration, duration));

    

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
    if (m_isViewTearingDown)
    {
        return;
    }

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
