#include "detailview.h"
#include "../../components/detailactionwidget.h"
#include "../../components/detailbottominfowidget.h"
#include "../../components/detailcontentwidget.h"
#include "../../components/detailtagbutton.h"
#include "../../components/flowlayout.h"
#include "../../components/horizontallistviewgallery.h"
#include "../../components/mediaactionmenu.h"
#include "../../components/mediasectionwidget.h"
#include "../../components/modernmenubutton.h"
#include "../../components/moderntoast.h"
#include "../../managers/playbackmanager.h"
#include "../../managers/thememanager.h"
#include "../../utils/detailcacheutils.h"
#include "../../utils/mediaitemutils.h"
#include "../../utils/mediasourcepreferenceutils.h"
#include "../../utils/numberextractor.h"
#include "../../utils/playerpreferenceutils.h"
#include "../../utils/smoothscrollcontroller.h"
#include "overviewdialog.h"
#include <config/config_keys.h>
#include <config/configstore.h>
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>

#include <QClipboard>
#include <QDate>
#include <QDebug>
#include <QEvent>
#include <QFuture>
#include <QFutureWatcher>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHash>
#include <QIcon>
#include <QImage>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QSignalBlocker>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QtConcurrent/QtConcurrentRun>

#include <qcorofuture.h>

#include <optional>

namespace
{

QString activeServerId(QEmbyCore *core)
{
    return core && core->serverManager() ? core->serverManager()->activeProfile().id : QString();
}

int preferredSourceIndex(QEmbyCore *core, const MediaItem &item)
{
    return MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
        item.mediaSources,
        ConfigStore::instance()->get<QString>(ConfigKeys::PlayerPreferredVersion).trimmed(),
        MediaSourcePreferenceUtils::rememberedMediaSourceId(activeServerId(core), item.id));
}

QWidget *createTransparentReserveWidget(QWidget *parent, const char *objectName, int height)
{
    auto *widget = new QWidget(parent);
    widget->setObjectName(QString::fromLatin1(objectName));
    widget->setFixedHeight(height);
    widget->hide();
    return widget;
}

int reserveHeightFor(QWidget *widget, int fallbackHeight)
{
    if (!widget)
        return fallbackHeight;

    const int hintHeight = widget->sizeHint().height();
    return qMax(fallbackHeight, hintHeight);
}

void showReserveWidget(QWidget *reserve, int height = -1)
{
    if (!reserve)
        return;
    if (height > 0 && reserve->height() != height)
        reserve->setFixedHeight(height);
    reserve->show();
}

void hideReserveWidget(QWidget *reserve)
{
    if (reserve)
        reserve->hide();
}

void prepareReservedSection(QWidget *reserve, MediaSectionWidget *section, int height = -1)
{
    showReserveWidget(reserve, height);
    if (section)
        section->clear();
}

void applyReservedSectionItems(QWidget *reserve, MediaSectionWidget *section, const QList<MediaItem> &items)
{
    if (!section)
    {
        hideReserveWidget(reserve);
        return;
    }

    if (items.isEmpty())
    {
        section->clear();
        hideReserveWidget(reserve);
        return;
    }

    hideReserveWidget(reserve);
    section->setItems(items);
}





QString computeSourcesFingerprint(const QList<MediaSourceInfo> &sources)
{
    if (sources.isEmpty())
        return QStringLiteral("<empty>");
    QStringList parts;
    parts.reserve(sources.size());
    for (const auto &src : sources)
    {
        QStringList streamParts;
        streamParts.reserve(src.mediaStreams.size());
        for (const auto &s : src.mediaStreams)
        {
            streamParts << QStringLiteral("%1:%2:%3:%4:%5:%6:%7:%8:%9:%10:%11")
                               .arg(s.type)
                               .arg(s.index)
                               .arg(s.codec)
                               .arg(s.language)
                               .arg(s.title)
                               .arg(s.displayTitle)
                               .arg(s.profile)
                               .arg(s.width)
                               .arg(s.height)
                               .arg(s.bitRate)
                               .arg(s.realFrameRate);
        }
        parts << QStringLiteral("%1#%2#%3#%4#%5#%6#%7#%8#%9")
                     .arg(src.id)
                     .arg(src.name)
                     .arg(src.path)
                     .arg(src.container)
                     .arg(src.size)
                     .arg(src.dateCreated.toString(Qt::ISODateWithMs))
                     .arg(src.dateModified.toString(Qt::ISODateWithMs))
                     .arg(src.mediaStreams.size())
                     .arg(streamParts.join(QLatin1Char('|')));
    }
    return parts.join(QLatin1Char(';'));
}




QString computeBottomInfoFingerprint(const MediaItem &item, const QList<MediaSourceInfo> &sources)
{
    QStringList studioParts;
    studioParts.reserve(item.studios.size());
    for (const auto &s : item.studios)
        studioParts << s.name;

    QStringList urlParts;
    urlParts.reserve(item.externalUrls.size());
    for (const auto &u : item.externalUrls)
        urlParts << (u.name + QLatin1Char('=') + u.url);

    return QStringLiteral("%1|tags=%2|studios=%3|urls=%4|src=%5")
        .arg(item.id, item.tags.join(QLatin1Char(',')), studioParts.join(QLatin1Char(',')),
             urlParts.join(QLatin1Char(',')), computeSourcesFingerprint(sources));
}

QString formatDetailReleaseDate(const MediaItem &item)
{
    QString dateText = item.premiereDate.trimmed();
    if (!dateText.isEmpty())
    {
        const int timeSeparator = dateText.indexOf(QLatin1Char('T'));
        if (timeSeparator > 0)
            dateText = dateText.left(timeSeparator);

        
        const int dateLen = dateText.length();
        if (dateLen > 0)
        {
            const QString dateOnly = dateText.left(qMin(dateLen, 10));
            const QDate date = QDate::fromString(dateOnly, Qt::ISODate);
            if (date.isValid())
                return date.toString(QStringLiteral("yyyy-MM-dd"));
            return dateOnly;
        }
    }

    if (item.productionYear > 0)
        return QString::number(item.productionYear);

    return QString();
}

QString formatEpisodeTag(const MediaItem &item)
{
    QString tag;
    if (item.parentIndexNumber >= 0)
    {
        tag += QStringLiteral("S%1").arg(item.parentIndexNumber, 2, 10, QLatin1Char('0'));
    }
    if (item.indexNumber >= 0)
    {
        tag += QStringLiteral("E%1").arg(item.indexNumber, 2, 10, QLatin1Char('0'));
    }
    return tag;
}

MediaItem resolveSeasonPlaybackEpisode(const QList<MediaItem> &episodes)
{
    for (const MediaItem &episode : episodes)
    {
        if (episode.userData.playbackPositionTicks > 0 && !episode.userData.played &&
            episode.userData.playedPercentage < 99.0)
        {
            return episode;
        }
    }

    for (const MediaItem &episode : episodes)
    {
        if (!episode.userData.played)
            return episode;
    }

    return episodes.isEmpty() ? MediaItem{} : episodes.first();
}

MediaItem withSeedContext(MediaItem cachedItem, const MediaItem &seedItem)
{
    if (seedItem.id.isEmpty() || seedItem.id != cachedItem.id)
        return cachedItem;

    if (cachedItem.name.trimmed().isEmpty())
        cachedItem.name = seedItem.name;
    if (cachedItem.type.trimmed().isEmpty())
        cachedItem.type = seedItem.type;
    if (cachedItem.mediaType.trimmed().isEmpty())
        cachedItem.mediaType = seedItem.mediaType;
    if (cachedItem.overview.trimmed().isEmpty())
        cachedItem.overview = seedItem.overview;
    if (cachedItem.genres.isEmpty())
        cachedItem.genres = seedItem.genres;
    if (cachedItem.taglines.isEmpty())
        cachedItem.taglines = seedItem.taglines;
    if (cachedItem.mediaSources.isEmpty())
        cachedItem.mediaSources = seedItem.mediaSources;
    if (cachedItem.images.primaryTag.isEmpty() && cachedItem.images.thumbTag.isEmpty() &&
        cachedItem.images.backdropTag.isEmpty() && cachedItem.images.logoTag.isEmpty())
    {
        cachedItem.images = seedItem.images;
    }

    cachedItem.playlistId = seedItem.playlistId;
    cachedItem.playlistItemId = seedItem.playlistItemId;
    cachedItem.hasResumeContext = seedItem.hasResumeContext;
    cachedItem.resumeItemId = seedItem.resumeItemId;
    cachedItem.resumeUserData = seedItem.resumeUserData;
    return cachedItem;
}

bool mediaItemListsEqual(const QList<MediaItem> &left, const QList<MediaItem> &right)
{
    if (left.size() != right.size())
        return false;

    for (int i = 0; i < left.size(); ++i)
    {
        if (DetailCacheUtils::fingerprint(left[i]) != DetailCacheUtils::fingerprint(right[i]))
        {
            return false;
        }
    }
    return true;
}

QCoro::Task<bool> mediaItemListsEqualAsync(QList<MediaItem> left, QList<MediaItem> right)
{
    auto future = QtConcurrent::run(
        [left = std::move(left), right = std::move(right)]() mutable
        { return mediaItemListsEqual(left, right); });
    co_return co_await future;
}

QString castFingerprint(const MediaItem &item)
{
    QStringList parts;
    parts.reserve(item.people.size());
    for (const auto &person : item.people)
    {
        parts << QStringLiteral("%1:%2:%3:%4").arg(person.id, person.name, person.type, person.role);
    }
    return parts.join(QLatin1Char('|'));
}

bool episodesBelongToSeason(const QList<MediaItem> &episodes, const MediaItem &season)
{
    if (episodes.isEmpty() || season.indexNumber < 0)
        return true;

    for (const MediaItem &episode : episodes)
    {
        if (episode.parentIndexNumber < 0)
            continue;
        if (episode.parentIndexNumber != season.indexNumber)
            return false;
    }
    return true;
}

bool cacheEntryHasAnySecondaryData(const DetailCacheUtils::DetailCacheEntry &entry)
{
    return entry.hasPlayableItem || entry.hasSeasons || entry.hasSeasonEpisodes || entry.hasSimilarItems ||
           entry.hasCollections || entry.hasAdditionalParts || !entry.item.people.isEmpty();
}

struct CachedDetailImages
{
    std::optional<QImage> poster;
    std::optional<QImage> backdrop;
    std::optional<QImage> logo;
};

void saveDetailCacheAsync(QString serverId, QString userId, MediaItem item, QString fingerprint)
{
    if (serverId.isEmpty() || userId.isEmpty() || item.id.isEmpty())
        return;

    (void)QtConcurrent::run(
        [serverId = std::move(serverId), userId = std::move(userId), item = std::move(item),
         fingerprint = std::move(fingerprint)]() mutable
        {
            QString errorString;
            const QString itemId = item.id;
            DetailCacheUtils::DetailCacheEntry entry;
            const auto existing = DetailCacheUtils::load(serverId, userId, itemId);
            if (existing.has_value())
                entry = *existing;
            entry.item = item;
            if (entry.hasPlayableItem && !entry.playableItemFromServer)
            {
                entry.hasPlayableItem = false;
                entry.playableItem = MediaItem{};
            }
            const bool saved = DetailCacheUtils::save(serverId, userId, entry, &errorString);
            if (saved)
            {
                qDebug() << "[DetailView] Detail cache saved"
                         << "itemId=" << itemId << "fingerprint=" << fingerprint.left(12);
            }
            else
            {
                qWarning() << "[DetailView] Detail cache save failed"
                           << "itemId=" << itemId << "error=" << errorString;
            }
        });
}

void saveDetailCacheEntryAsync(QString serverId, QString userId, DetailCacheUtils::DetailCacheEntry entry,
                               QString reason)
{
    if (serverId.isEmpty() || userId.isEmpty() || entry.item.id.isEmpty())
        return;

    (void)QtConcurrent::run(
        [serverId = std::move(serverId), userId = std::move(userId), entry = std::move(entry),
         reason = std::move(reason)]() mutable
        {
            QString errorString;
            const QString itemId = entry.item.id;
            const auto existing = DetailCacheUtils::load(serverId, userId, itemId);
            if (existing.has_value())
            {
                if (!entry.hasPlayableItem && existing->hasPlayableItem && existing->playableItemFromServer)
                {
                    entry.hasPlayableItem = true;
                    entry.playableItem = existing->playableItem;
                    entry.playableItemFromServer = true;
                }
                if (!entry.hasSeasons && existing->hasSeasons)
                {
                    entry.hasSeasons = true;
                    entry.seasons = existing->seasons;
                }
                if (!entry.hasSeasonEpisodes && existing->hasSeasonEpisodes)
                {
                    entry.hasSeasonEpisodes = true;
                    entry.seasonEpisodes = existing->seasonEpisodes;
                    entry.seasonIndex = existing->seasonIndex;
                    entry.seasonId = existing->seasonId;
                }
                if (!entry.hasSimilarItems && existing->hasSimilarItems)
                {
                    entry.hasSimilarItems = true;
                    entry.similarItems = existing->similarItems;
                }
                if (!entry.hasCollections && existing->hasCollections)
                {
                    entry.hasCollections = true;
                    entry.collections = existing->collections;
                }
                if (!entry.hasAdditionalParts && existing->hasAdditionalParts)
                {
                    entry.hasAdditionalParts = true;
                    entry.additionalParts = existing->additionalParts;
                }
            }
            if (entry.hasPlayableItem && !entry.playableItemFromServer)
            {
                entry.hasPlayableItem = false;
                entry.playableItem = MediaItem{};
            }
            const QString itemFp = DetailCacheUtils::fingerprint(entry.item);
            const QString sectionsFp = DetailCacheUtils::sectionsFingerprint(entry);
            const bool saved = DetailCacheUtils::save(serverId, userId, entry, &errorString);
            if (saved)
            {
                qDebug() << "[DetailView] Detail cache snapshot saved"
                         << "itemId=" << itemId << "reason=" << reason << "fingerprint=" << itemFp.left(12)
                         << "sections=" << sectionsFp.left(12);
            }
            else
            {
                qWarning() << "[DetailView] Detail cache snapshot save failed"
                           << "itemId=" << itemId << "reason=" << reason << "error=" << errorString;
            }
        });
}

void saveDetailImageAsync(QString serverId, QString userId, QString ownerItemId, QString role, QString imageItemId,
                          QString imageType, QString imageTag, int maxWidth, QImage image)
{
    if (serverId.isEmpty() || userId.isEmpty() || ownerItemId.isEmpty() || imageTag.isEmpty() || image.isNull())
        return;

    (void)QtConcurrent::run(
        [serverId = std::move(serverId), userId = std::move(userId), ownerItemId = std::move(ownerItemId),
         role = std::move(role), imageItemId = std::move(imageItemId), imageType = std::move(imageType),
         imageTag = std::move(imageTag), maxWidth, image = std::move(image)]() mutable
        {
            QString errorString;
            const bool saved = DetailCacheUtils::saveImage(serverId, userId, ownerItemId, role, imageItemId, imageType,
                                                           imageTag, maxWidth, image, &errorString);
            if (!saved)
            {
                qWarning() << "[DetailView] Detail image cache save failed"
                           << "itemId=" << ownerItemId << "role=" << role << "error=" << errorString;
            }
        });
}

} 

DetailView::DetailView(QEmbyCore *core, QWidget *parent) : BaseView(core, parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("detail-view");
    setProperty("showGlobalSearch", true);
    setupUi();
}

void DetailView::scrollToTop()
{
    if (m_mainScrollArea && m_mainScrollArea->verticalScrollBar())
    {
        if (m_vScrollController)
        {
            m_vScrollController->scrollTo(0, false);
        }
        else
        {
            m_mainScrollArea->verticalScrollBar()->setValue(0);
        }
    }
}

void DetailView::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    m_mainScrollArea = new QScrollArea(this);
    m_mainScrollArea->setWidgetResizable(true);
    m_mainScrollArea->setFrameShape(QFrame::NoFrame);
    m_mainScrollArea->setObjectName("detail-scrollarea");
    m_mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_mainScrollArea->setStyleSheet("QScrollArea { background: transparent; }");

    m_vScrollController = new SmoothScrollController(m_mainScrollArea->verticalScrollBar(), this);
    m_vScrollController->setDuration(170);

    m_contentWidget = new DetailContentWidget(m_mainScrollArea);
    m_contentWidget->setObjectName("detail-content-container");

    auto *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 10);
    contentLayout->setSpacing(0);

    QWidget *infoContainer = new QWidget(m_contentWidget);
    infoContainer->setObjectName("detail-info-container");
    infoContainer->setStyleSheet("QWidget#detail-info-container { background: transparent; }");

    m_infoLayout = new QGridLayout(infoContainer);
    m_infoLayout->setContentsMargins(40, 40, 40, 0);
    m_infoLayout->setHorizontalSpacing(40);
    m_infoLayout->setVerticalSpacing(0);
    m_infoLayout->setRowMinimumHeight(0, 110);

    m_logoLabel = new QLabel(infoContainer);
    m_logoLabel->setObjectName("detail-logo-label");
    m_logoLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
    m_infoLayout->addWidget(m_logoLabel, 0, 1, 1, 2, Qt::AlignRight | Qt::AlignTop);

    m_posterLabel = new QLabel(infoContainer);
    m_posterLabel->setFixedSize(250, 375);
    m_posterLabel->setObjectName("detail-poster-label");
    m_posterLabel->setAlignment(Qt::AlignCenter);

    
    
    
    m_posterShadow = new QGraphicsDropShadowEffect(this);
    m_posterShadow->setBlurRadius(20);
    m_posterShadow->setColor(QColor(0, 0, 0, 80));
    m_posterShadow->setOffset(0, 8);
    m_posterShadow->setEnabled(false);
    m_posterLabel->setGraphicsEffect(m_posterShadow);
    m_infoLayout->addWidget(m_posterLabel, 1, 0, 1, 1, Qt::AlignTop);

    m_textContainer = new QWidget(infoContainer);
    m_textContainer->setMaximumWidth(1200);
    m_textContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto *textLayout = new QVBoxLayout(m_textContainer);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(10);
    textLayout->setAlignment(Qt::AlignTop);

    m_titleLabel = new QLabel(m_textContainer);
    m_titleLabel->setObjectName("detail-title");
    m_titleLabel->setWordWrap(true);

    m_metaRowWidget = new QWidget(m_textContainer);
    m_metaRowWidget->setObjectName("detail-meta-row");
    auto *metaLayout = new QHBoxLayout(m_metaRowWidget);
    metaLayout->setContentsMargins(0, 0, 0, 0);
    metaLayout->setSpacing(6);

    m_ratingStarLabel = new QLabel(m_metaRowWidget);
    m_ratingStarLabel->setFixedWidth(16);
    m_ratingStarLabel->setAlignment(Qt::AlignCenter);
    m_ratingStarLabel->setPixmap(QIcon(":/svg/dark/star-filled.svg").pixmap(16, 16));
    m_ratingStarLabel->setContentsMargins(0, 2, 0, 0);
    m_ratingStarLabel->hide();

    m_metaLabel = new QLabel(m_metaRowWidget);
    m_metaLabel->setObjectName("detail-meta");
    m_metaLabel->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Preferred);

    m_numberButton = new QPushButton(m_metaRowWidget);
    m_numberButton->setObjectName("detail-number-chip");
    m_numberButton->setCursor(Qt::PointingHandCursor);
    m_numberButton->setFocusPolicy(Qt::NoFocus);
    m_numberButton->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    m_numberButton->hide();
    connect(m_numberButton, &QPushButton::clicked, this, &DetailView::copyDisplayedNumber);

    metaLayout->addWidget(m_ratingStarLabel);
    metaLayout->addWidget(m_metaLabel, 0, Qt::AlignLeft);
    metaLayout->addWidget(m_numberButton, 0, Qt::AlignLeft);
    metaLayout->addStretch(1);
    m_metaRowWidget->hide();

    m_tagsWidget = new QWidget(m_textContainer);
    m_tagsWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_tagsLayout = new FlowLayout(m_tagsWidget, 0, 8, 8);

    m_actionWidget = new DetailActionWidget(m_textContainer);
    connect(m_actionWidget, &DetailActionWidget::playRequested, this,
            [this]() -> QCoro::Task<void> { co_await executePlay(m_currentPlayableItem, 0); });
    connect(m_actionWidget, &DetailActionWidget::resumeRequested, this, [this]() -> QCoro::Task<void>
            { co_await executePlay(m_currentPlayableItem, m_currentPlayableItem.userData.playbackPositionTicks); });
    connect(m_actionWidget, &DetailActionWidget::favoriteRequested, this,
            [this]()
            {
                if (!m_currentMediaItem.id.isEmpty())
                    handleFavoriteRequested(m_currentMediaItem);
            });
    connect(m_actionWidget, &DetailActionWidget::playedToggleRequested, this,
            [this]()
            {
                const MediaItem &target =
                    m_currentPlayableItem.type == "Episode" ? m_currentPlayableItem : m_currentMediaItem;
                if (target.id.isEmpty())
                    return;
                if (target.userData.played)
                    handleMarkUnplayedRequested(target);
                else
                    handleMarkPlayedRequested(target);
            });
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
            [this]()
            {
                const MediaItem &target =
                    m_currentPlayableItem.type == "Episode" ? m_currentPlayableItem : m_currentMediaItem;
                if (!target.id.isEmpty())
                    m_actionWidget->setPlayedState(target.userData.played);
            });
    connect(m_actionWidget, &DetailActionWidget::sourceVersionChanged, this, &DetailView::onVersionChanged);
    connect(m_actionWidget, &DetailActionWidget::audioStreamChanged, this,
            [this](int streamIndex)
            {
                const MediaItem &item =
                    m_currentMediaItem.type == "Series" && !m_currentPlayableItem.id.isEmpty()
                        ? m_currentPlayableItem
                        : m_currentMediaItem;
                const int sourceIndex = m_actionWidget->currentSourceIndex();
                if (sourceIndex < 0 || sourceIndex >= item.mediaSources.size())
                    return;
                PlayerPreferenceUtils::rememberAudioStreamIndex(
                    activeServerId(m_core), item.id,
                    item.mediaSources[sourceIndex].id, streamIndex);
            });
    connect(m_actionWidget, &DetailActionWidget::subtitleStreamChanged, this,
            [this](int streamIndex)
            {
                const MediaItem &item =
                    m_currentMediaItem.type == "Series" && !m_currentPlayableItem.id.isEmpty()
                        ? m_currentPlayableItem
                        : m_currentMediaItem;
                const int sourceIndex = m_actionWidget->currentSourceIndex();
                if (sourceIndex < 0 || sourceIndex >= item.mediaSources.size())
                    return;
                PlayerPreferenceUtils::rememberSubtitleStreamIndex(
                    activeServerId(m_core), item.id,
                    item.mediaSources[sourceIndex].id, streamIndex);
            });
    connect(m_actionWidget, &DetailActionWidget::externalPlayRequested, this,
            [this](const QString &playerPath) -> QCoro::Task<void>
            { co_await executeExternalPlay(m_currentPlayableItem, playerPath); });

    m_taglineLabel = new QLabel(m_textContainer);
    m_taglineLabel->setObjectName("detail-tagline");

    m_overviewLabel = new QLabel(m_textContainer);
    m_overviewLabel->setObjectName("detail-overview");
    m_overviewLabel->setWordWrap(true);
    m_overviewLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    m_overviewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_overviewLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
    connect(m_overviewLabel, &QLabel::linkActivated, this, &DetailView::onOverviewMoreClicked);

    textLayout->addWidget(m_titleLabel);
    textLayout->addWidget(m_metaRowWidget);
    textLayout->addWidget(m_tagsWidget);
    textLayout->addWidget(m_actionWidget);
    textLayout->addSpacing(4);
    textLayout->addWidget(m_taglineLabel);
    textLayout->addWidget(m_overviewLabel);
    textLayout->addSpacing(10);

    m_infoLayout->addWidget(m_textContainer, 1, 1, 1, 1, Qt::AlignTop);
    m_infoLayout->setColumnStretch(0, 0);
    m_infoLayout->setColumnStretch(1, 10);
    m_infoLayout->setColumnStretch(2, 1);
    contentLayout->addWidget(infoContainer);

    
    
    
    m_seasonWidget = new MediaSectionWidget(tr("Seasons"), m_core, m_contentWidget);
    m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
    m_seasonWidget->setGalleryHeight(300);

    
    m_episodeWidget = new MediaSectionWidget(tr("Episodes"), m_core, m_contentWidget);
    m_episodeWidget->setCardStyle(MediaCardDelegate::LibraryTile);
    m_episodeWidget->gallery()->setHoverControls(MediaCardDelegate::HoverControlPlay |
                                                 MediaCardDelegate::HoverControlMore);
    
    
    {
        int imgH = 100;
        int imgW = qRound(imgH * 16.0 / 9.0);                      
        const QSize episodeTileSize(imgW + 16, 8 + imgH + 6 + 20); 
        m_episodeTileWidth = episodeTileSize.width();
        m_episodeWidget->setTileSize(episodeTileSize);
    }
    m_episodeWidget->setGalleryHeight(145);

    
    m_episodeHeaderControls = new QWidget(m_episodeWidget);
    m_episodeHeaderControls->setObjectName("detail-episode-header-controls");
    m_episodeHeaderControls->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    auto *episodeHeaderLayout = new QHBoxLayout(m_episodeHeaderControls);
    episodeHeaderLayout->setContentsMargins(0, 0, 0, 0);
    episodeHeaderLayout->setSpacing(6);
    episodeHeaderLayout->setSizeConstraint(QLayout::SetFixedSize);

    m_seasonSwitcher = new ModernMenuButton(m_episodeHeaderControls);
    m_seasonSwitcher->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_seasonSwitcher->hide(); 

    m_episodeJumpEdit = new QLineEdit(m_episodeHeaderControls);
    m_episodeJumpEdit->setObjectName("detail-episode-jump-edit");
    m_episodeJumpEdit->setPlaceholderText(tr("Ep #"));
    m_episodeJumpEdit->setToolTip(tr("Jump to episode number"));
    m_episodeJumpEdit->setAlignment(Qt::AlignCenter);
    m_episodeJumpEdit->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_episodeJumpEdit->setFixedWidth(64);
    m_episodeJumpEdit->setFixedHeight(m_seasonSwitcher->sizeHint().height());
    m_episodeJumpEdit->setMaxLength(4);
    m_episodeJumpValidator = new QIntValidator(1, 9999, m_episodeJumpEdit);
    m_episodeJumpEdit->setValidator(m_episodeJumpValidator);
    m_episodeJumpEdit->hide(); 

    episodeHeaderLayout->addWidget(m_seasonSwitcher, 0, Qt::AlignVCenter);
    episodeHeaderLayout->addWidget(m_episodeJumpEdit, 0, Qt::AlignVCenter);
    m_episodeHeaderControls->hide();
    m_episodeWidget->setHeaderWidget(m_episodeHeaderControls);
    m_episodeHeaderControls->hide();

    m_episodeSectionReserveWidget = createTransparentReserveWidget(m_contentWidget, "detail-episode-section-reserve",
                                                                   reserveHeightFor(m_episodeWidget, 190));
    m_seasonSectionReserveWidget = createTransparentReserveWidget(m_contentWidget, "detail-season-section-reserve",
                                                                  reserveHeightFor(m_seasonWidget, 320));

    connect(m_seasonSwitcher, &ModernMenuButton::currentIndexChanged, this,
            [this](int idx)
            {
                if (idx >= 0 && idx != m_currentSeasonIndex)
                {
                    markManualSeriesSelection(QStringLiteral("season-switcher"));
                    m_currentSeasonIndex = idx;
                    QCoro::connect(switchToSeason(idx, true), this, []() {});
                }
            });
    connect(m_episodeJumpEdit, &QLineEdit::editingFinished, this, &DetailView::submitEpisodeJump);

    m_castWidget = new MediaSectionWidget(tr("Cast & Crew"), m_core, m_contentWidget);
    m_castWidget->setCardStyle(MediaCardDelegate::Cast);
    m_castWidget->setGalleryHeight(280);

    m_castSectionReserveWidget = new QWidget(m_contentWidget);
    m_castSectionReserveWidget->setObjectName("detail-cast-section-reserve");
    m_castSectionReserveWidget->setFixedHeight(qMax(320, m_castWidget->sizeHint().height()));
    m_castSectionReserveWidget->hide();

    m_additionalPartsWidget = new MediaSectionWidget(tr("Additional Parts"), m_core, m_contentWidget);
    m_additionalPartsWidget->setCardStyle(MediaCardDelegate::Poster);
    m_additionalPartsWidget->setGalleryHeight(300);
    m_additionalPartsSectionReserveWidget = createTransparentReserveWidget(
        m_contentWidget, "detail-additional-section-reserve", reserveHeightFor(m_additionalPartsWidget, 350));

    m_collectionWidget = new MediaSectionWidget(tr("Collection"), m_core, m_contentWidget);
    m_collectionWidget->setCardStyle(MediaCardDelegate::Poster);
    m_collectionWidget->setGalleryHeight(300);
    m_collectionSectionReserveWidget = createTransparentReserveWidget(
        m_contentWidget, "detail-collection-section-reserve", reserveHeightFor(m_collectionWidget, 350));

    m_similarWidget = new MediaSectionWidget(tr("More Like This"), m_core, m_contentWidget);
    m_similarWidget->setCardStyle(MediaCardDelegate::Poster);
    m_similarWidget->setGalleryHeight(300);
    m_similarSectionReserveWidget = createTransparentReserveWidget(m_contentWidget, "detail-similar-section-reserve",
                                                                   reserveHeightFor(m_similarWidget, 350));

    m_seasonWidget->gallery()->listView()->viewport()->installEventFilter(this);
    m_episodeWidget->gallery()->listView()->viewport()->installEventFilter(this);
    m_castWidget->gallery()->listView()->viewport()->installEventFilter(this);
    m_additionalPartsWidget->gallery()->listView()->viewport()->installEventFilter(this);
    m_collectionWidget->gallery()->listView()->viewport()->installEventFilter(this);
    m_similarWidget->gallery()->listView()->viewport()->installEventFilter(this);

    auto connectGallerySignals = [this](MediaSectionWidget *widget)
    {
        connect(widget, &MediaSectionWidget::playRequested, this, &BaseView::handlePlayRequested);
        connect(widget, &MediaSectionWidget::favoriteRequested, this, &BaseView::handleFavoriteRequested);
        connect(widget, &MediaSectionWidget::moreMenuRequested, this, &BaseView::handleMoreMenuRequested);
    };

    connectGallerySignals(m_castWidget);
    connectGallerySignals(m_additionalPartsWidget);
    connectGallerySignals(m_collectionWidget);
    connectGallerySignals(m_similarWidget);

    connect(m_seasonWidget, &MediaSectionWidget::playRequested, this,
            [this](MediaItem item) -> QCoro::Task<void>
            {
                if (item.type == "Season")
                {
                    markManualSeriesSelection(QStringLiteral("season-play"));
                    co_await executePlaySeason(item);
                }
                else
                {
                    handlePlayRequested(item);
                }
            });
    connect(m_seasonWidget, &MediaSectionWidget::favoriteRequested, this, &BaseView::handleFavoriteRequested);
    connect(m_seasonWidget, &MediaSectionWidget::moreMenuRequested, this, &BaseView::handleMoreMenuRequested);

    connect(m_episodeWidget, &MediaSectionWidget::playRequested, this,
            [this](MediaItem item) -> QCoro::Task<void>
            {
                if (item.type == "Episode")
                {
                    markManualSeriesSelection(QStringLiteral("episode-play"));
                    co_await applySeriesPlayableItem(item, false);
                    if (m_currentPlayableItem.id == item.id)
                    {
                        co_await executePlay(m_currentPlayableItem,
                                             m_currentPlayableItem.userData.playbackPositionTicks);
                    }
                }
                else
                {
                    handlePlayRequested(item);
                }
            });
    connect(m_episodeWidget, &MediaSectionWidget::favoriteRequested, this, &BaseView::handleFavoriteRequested);
    connect(m_episodeWidget, &MediaSectionWidget::moreMenuRequested, this,
            [this](const MediaItem &item, const QPoint &globalPos)
            {
                MediaActionMenu menu(item, m_core, this,
                                     {CardContextMenuAction::MarkPlayed, CardContextMenuAction::MarkUnplayed});
                CardContextMenuRequest request = menu.execRequest(globalPos);
                dispatchCardContextMenuRequest(item, request);
            });

    auto commonClicked = [this](const MediaItem &item)
    {
        if (item.type == "BoxSet" || item.type == "Playlist" || item.type == "Folder")
            Q_EMIT navigateToFolder(item.id, item.name);
        else if (item.type == "Person")
            Q_EMIT navigateToPerson(item.id, item.name);
        else
            Q_EMIT navigateToDetail(item.id, item.name, item);
    };

    
    connect(m_seasonWidget, &MediaSectionWidget::itemClicked, this,
            [this](const MediaItem &item)
            {
                if (item.type == "Season")
                    Q_EMIT navigateToSeason(m_currentItemId, item.id, item.name);
                else
                    Q_EMIT navigateToDetail(item.id, item.name, item);
            });

    
    connect(m_episodeWidget, &MediaSectionWidget::itemClicked, this,
            [this, commonClicked](const MediaItem &item)
            {
                if (item.type == "Episode")
                {
                    markManualSeriesSelection(QStringLiteral("episode-click"));
                    QCoro::connect(applySeriesPlayableItem(item, true), this, []() {});
                }
                else
                {
                    commonClicked(item);
                }
            });

    connect(m_castWidget, &MediaSectionWidget::itemClicked, this,
            [this](const MediaItem &item) { Q_EMIT navigateToPerson(item.id, item.name); });
    connect(m_additionalPartsWidget, &MediaSectionWidget::itemClicked, this, commonClicked);
    connect(m_collectionWidget, &MediaSectionWidget::itemClicked, this,
            [this, commonClicked](const MediaItem &item)
            {
                if (!item.id.isEmpty())
                {
                    qDebug() << "[DetailView] Navigate collection membership"
                             << "itemId=" << item.id << "name=" << item.name << "type=" << item.type
                             << "collectionType=" << item.collectionType;
                    Q_EMIT navigateToFolder(item.id, item.name);
                    return;
                }
                commonClicked(item);
            });
    connect(m_similarWidget, &MediaSectionWidget::itemClicked, this, commonClicked);

    contentLayout->addWidget(m_episodeSectionReserveWidget);
    contentLayout->addWidget(m_episodeWidget);
    contentLayout->addWidget(m_seasonSectionReserveWidget);
    contentLayout->addWidget(m_seasonWidget);
    contentLayout->addWidget(m_castSectionReserveWidget);
    contentLayout->addWidget(m_castWidget);
    contentLayout->addWidget(m_additionalPartsSectionReserveWidget);
    contentLayout->addWidget(m_additionalPartsWidget);
    contentLayout->addWidget(m_collectionSectionReserveWidget);
    contentLayout->addWidget(m_collectionWidget);
    contentLayout->addWidget(m_similarSectionReserveWidget);
    contentLayout->addWidget(m_similarWidget);

    m_bottomInfoWidget = new DetailBottomInfoWidget(m_contentWidget);
    connect(m_bottomInfoWidget, &DetailBottomInfoWidget::filterClicked, this,
            [this](const QString &type, const QString &value) { Q_EMIT navigateToFilteredView(type, value); });
    m_bottomInfoReserveWidget = createTransparentReserveWidget(m_contentWidget, "detail-bottom-info-reserve", 220);
    contentLayout->addWidget(m_bottomInfoReserveWidget);
    contentLayout->addWidget(m_bottomInfoWidget);

    contentLayout->addStretch();
    m_mainScrollArea->setWidget(m_contentWidget);
    mainLayout->addWidget(m_mainScrollArea);
    m_mainScrollArea->viewport()->installEventFilter(this);
}

void DetailView::loadItem(QString itemId, MediaItem seedItem)
{
    QPointer<DetailView> safeThis(this);
    setDeferredPresentationBatchActive(false);
    m_currentItemId = itemId;
    
    
    m_skipNextSilentRefresh = true;
    m_pendingDeferredFetchId = itemId;
    m_pendingFetchedItem = MediaItem{};
    m_pendingFetchReady = false;
    m_pendingAnimationGuardDone = false;
    m_pendingFetchedItemFromCache = false;
    m_deferBottomInfoReveal = false;
    m_currentPlayableItem = MediaItem{};
    m_currentSeasonEpisodes.clear();
    updateEpisodeJumpControl(0);
    
    m_appliedSourcesFingerprint.clear();
    m_appliedBottomInfoFingerprint.clear();
    m_detailCacheServerId.clear();
    m_detailCacheUserId.clear();
    m_cachedDetailFingerprint.clear();
    m_cachedSectionsFingerprint.clear();
    m_cachedSeriesPlayableItem = MediaItem{};
    m_cachedSeriesSeasons.clear();
    m_cachedSeasonEpisodes.clear();
    m_cachedSimilarItems.clear();
    m_cachedCollections.clear();
    m_cachedAdditionalParts.clear();
    m_cachedSeasonId.clear();
    m_manualSelectedSeasonId.clear();
    m_manualSelectedEpisodeId.clear();
    m_cachedSeasonIndex = -1;
    m_manualSelectedSeasonIndex = -1;
    m_hasManualSeriesSelection = false;
    m_hasCachedSeriesPlayableItem = false;
    m_hasCachedSeriesSeasons = false;
    m_hasCachedSeasonEpisodes = false;
    m_hasCachedSimilarItems = false;
    m_hasCachedCollections = false;
    m_hasCachedAdditionalParts = false;
    m_hasCachedCastData = false;
    m_appliedCachedCastToUi = false;
    m_appliedCachedSeriesSeasonsToUi = false;
    m_appliedCachedSeasonEpisodesToUi = false;
    m_appliedCachedSimilarItemsToUi = false;
    m_appliedCachedCollectionsToUi = false;
    m_appliedCachedAdditionalPartsToUi = false;
    m_appliedCastFingerprint.clear();
    m_appliedSeriesPlayableFingerprint.clear();

    if (m_core && m_core->serverManager())
    {
        const ServerProfile profile = m_core->serverManager()->activeProfile();
        m_detailCacheServerId = profile.id;
        m_detailCacheUserId = profile.userId;
    }

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    const bool hasSeed = !seedItem.id.isEmpty();
    
    
    
    m_deferBottomInfoReveal = hasSeed;

    if (hasSeed)
    {
        m_currentPlayableItem = seedItem;
        applySeedToUi(seedItem);
        if (seedItem.type == "Series")
        {
            prepareReservedSection(m_episodeSectionReserveWidget, m_episodeWidget,
                                   reserveHeightFor(m_episodeWidget, 190));
            prepareReservedSection(m_seasonSectionReserveWidget, m_seasonWidget, reserveHeightFor(m_seasonWidget, 320));
        }
        else
        {
            hideReserveWidget(m_episodeSectionReserveWidget);
            hideReserveWidget(m_seasonSectionReserveWidget);
            m_seasonWidget->clear();
            m_episodeWidget->clear();
        }
    }
    else
    {
        
        m_titleLabel->setText(tr("Loading..."));
        m_logoLabel->clear();
        m_logoLabel->hide();
        m_metaLabel->clear();
        updateDisplayNumber(QString());
        m_metaRowWidget->hide();
        m_overviewLabel->clear();
        m_taglineLabel->hide();

        m_currentBackdropPix = QPixmap();
        m_currentPosterPix = QPixmap();
        updateBackdrop();
        m_posterLabel->setPixmap(QPixmap());

        m_actionWidget->clear();
        m_bottomInfoWidget->clear();
        clearLayout(m_tagsLayout);

        m_seasonWidget->clear();
        m_episodeWidget->clear();
        hideReserveWidget(m_episodeSectionReserveWidget);
        hideReserveWidget(m_seasonSectionReserveWidget);

        m_seriesSeasons.clear();
        m_currentSeasonIndex = 0;
        if (m_seasonSwitcher)
        {
            QSignalBlocker blocker(m_seasonSwitcher);
            m_seasonSwitcher->clear();
            m_seasonSwitcher->hide();
        }
        updateEpisodeHeaderControlsVisibility();
    }

    
    
    prepareReservedSection(m_castSectionReserveWidget, m_castWidget, reserveHeightFor(m_castWidget, 320));
    prepareReservedSection(m_additionalPartsSectionReserveWidget, m_additionalPartsWidget,
                           reserveHeightFor(m_additionalPartsWidget, 350));
    prepareReservedSection(m_collectionSectionReserveWidget, m_collectionWidget,
                           reserveHeightFor(m_collectionWidget, 350));
    prepareReservedSection(m_similarSectionReserveWidget, m_similarWidget, reserveHeightFor(m_similarWidget, 350));

    scrollToTop();

    
    
    if (!itemId.isEmpty())
    {
        QCoro::connect(loadDetailCacheAndStartPrefetch(itemId, seedItem, m_detailCacheServerId, m_detailCacheUserId),
                       this, []() {});
    }

    
    
    QTimer::singleShot(380, this,
                       [safeThis, itemId]()
                       {
                           if (!safeThis || safeThis->m_pendingDeferredFetchId != itemId)
                               return;
                           if (safeThis->m_posterShadow)
                               safeThis->m_posterShadow->setEnabled(true);
                           safeThis->m_pendingAnimationGuardDone = true;
                           safeThis->maybeFlushDeferredUpdate(itemId);
                       });

    return;
}

QCoro::Task<void> DetailView::loadDetailCacheAndStartPrefetch(QString itemId, MediaItem seedItem, QString cacheServerId,
                                                              QString cacheUserId)
{
    QPointer<DetailView> safeThis(this);
    QString targetId = std::move(itemId);
    MediaItem seed = std::move(seedItem);
    QString serverId = std::move(cacheServerId);
    QString userId = std::move(cacheUserId);
    QString cachedFingerprint;

    if (!serverId.isEmpty() && !userId.isEmpty() && !targetId.isEmpty())
    {
        auto cacheFuture = QtConcurrent::run(
            [serverId, userId, targetId]() mutable
            { return DetailCacheUtils::load(serverId, userId, targetId); });
        const auto cachedEntry = co_await cacheFuture;
        if (!safeThis || safeThis->m_currentItemId != targetId)
            co_return;

        if (cachedEntry.has_value())
        {
            MediaItem cachedItem = withSeedContext(cachedEntry->item, seed);
            cachedFingerprint = cachedEntry->fingerprint;

            safeThis->m_cachedDetailFingerprint = cachedEntry->fingerprint;
            safeThis->m_cachedSectionsFingerprint = cachedEntry->sectionsFingerprint;
            safeThis->m_cachedSeriesPlayableItem = withSeedContext(cachedEntry->playableItem, MediaItem{});
            safeThis->m_cachedSeriesSeasons = cachedEntry->seasons;
            safeThis->m_cachedSeasonEpisodes = cachedEntry->seasonEpisodes;
            safeThis->m_cachedSimilarItems = cachedEntry->similarItems;
            safeThis->m_cachedCollections = cachedEntry->collections;
            safeThis->m_cachedAdditionalParts = cachedEntry->additionalParts;
            safeThis->m_cachedSeasonId = cachedEntry->seasonId;
            safeThis->m_cachedSeasonIndex = cachedEntry->seasonIndex;
            safeThis->m_manualSelectedSeasonId.clear();
            safeThis->m_manualSelectedSeasonIndex = -1;
            safeThis->m_manualSelectedEpisodeId.clear();
            safeThis->m_hasCachedSeriesPlayableItem =
                cachedEntry->hasPlayableItem && cachedEntry->playableItemFromServer;
            if (!safeThis->m_hasCachedSeriesPlayableItem)
                safeThis->m_cachedSeriesPlayableItem = MediaItem{};
            safeThis->m_hasCachedSeriesSeasons = cachedEntry->hasSeasons;
            safeThis->m_hasCachedSeasonEpisodes = cachedEntry->hasSeasonEpisodes;
            safeThis->m_hasCachedSimilarItems = cachedEntry->hasSimilarItems;
            safeThis->m_hasCachedCollections = cachedEntry->hasCollections;
            safeThis->m_hasCachedAdditionalParts = cachedEntry->hasAdditionalParts;
            safeThis->m_hasCachedCastData = !cachedItem.people.isEmpty();
            const bool hasCachedSecondaryData = cacheEntryHasAnySecondaryData(*cachedEntry);

            if (!cachedItem.name.trimmed().isEmpty())
            {
                safeThis->m_pendingFetchedItem = cachedItem;
                safeThis->m_pendingFetchReady = true;
                safeThis->m_pendingFetchedItemFromCache = true;

                
                
                const bool hasSeedUi = safeThis->m_currentMediaItem.id == targetId &&
                                       !safeThis->m_currentMediaItem.name.trimmed().isEmpty();
                if (!hasSeedUi)
                {
                    safeThis->applySeedToUi(cachedItem);
                }
                safeThis->maybeFlushDeferredUpdate(targetId);

                qDebug() << "[DetailView] Detail cache hit"
                         << "itemId=" << targetId << "fingerprint=" << cachedFingerprint.left(12)
                         << "hasSecondaryCache=" << hasCachedSecondaryData << "deferredSeedApply=" << hasSeedUi;
            }
            else
            {
                cachedFingerprint.clear();
                safeThis->m_cachedDetailFingerprint.clear();
                safeThis->m_cachedSectionsFingerprint.clear();
                safeThis->m_cachedSeriesPlayableItem = MediaItem{};
                safeThis->m_cachedSeriesSeasons.clear();
                safeThis->m_cachedSeasonEpisodes.clear();
                safeThis->m_cachedSimilarItems.clear();
                safeThis->m_cachedCollections.clear();
                safeThis->m_cachedAdditionalParts.clear();
                safeThis->m_cachedSeasonId.clear();
                safeThis->m_manualSelectedSeasonId.clear();
                safeThis->m_manualSelectedEpisodeId.clear();
                safeThis->m_cachedSeasonIndex = -1;
                safeThis->m_manualSelectedSeasonIndex = -1;
                safeThis->m_hasCachedSeriesPlayableItem = false;
                safeThis->m_hasCachedSeriesSeasons = false;
                safeThis->m_hasCachedSeasonEpisodes = false;
                safeThis->m_hasCachedSimilarItems = false;
                safeThis->m_hasCachedCollections = false;
                safeThis->m_hasCachedAdditionalParts = false;
                safeThis->m_hasCachedCastData = false;
                qWarning() << "[DetailView] Ignore incomplete detail cache"
                           << "itemId=" << targetId << "file=" << cachedEntry->filePath;
            }
        }
        else
        {
            qDebug() << "[DetailView] Detail cache miss"
                     << "itemId=" << targetId;
        }
    }

    if (!safeThis || safeThis->m_currentItemId != targetId)
        co_return;

    if (safeThis->m_core && safeThis->m_core->mediaService())
    {
        QCoro::connect(safeThis->prefetchItemDetail(targetId, serverId, userId, cachedFingerprint), safeThis.data(),
                       []() {});
    }
}

QCoro::Task<void> DetailView::prefetchItemDetail(QString itemId, QString cacheServerId, QString cacheUserId,
                                                 QString cachedFingerprint)
{
    QPointer<DetailView> safeThis(this);
    QPointer<MediaService> mediaService(
        m_core ? m_core->mediaService() : nullptr);
    QString targetId = std::move(itemId);
    QString serverId = std::move(cacheServerId);
    QString userId = std::move(cacheUserId);
    QString previousFingerprint = std::move(cachedFingerprint);

    try
    {
        if (!mediaService)
            co_return;
        MediaItem item = co_await mediaService->getItemDetail(targetId);
        auto fingerprintFuture = QtConcurrent::run(
            [item]() mutable { return DetailCacheUtils::fingerprint(item); });
        const QString fetchedFingerprint = co_await fingerprintFuture;
        const bool differsFromCache = previousFingerprint.isEmpty() || fetchedFingerprint != previousFingerprint;

        if (differsFromCache)
        {
            saveDetailCacheAsync(serverId, userId, item, fetchedFingerprint);
        }
        else
        {
            qDebug() << "[DetailView] Detail cache unchanged"
                     << "itemId=" << targetId << "fingerprint=" << fetchedFingerprint.left(12);
        }

        if (!safeThis || (safeThis->m_pendingDeferredFetchId != targetId && safeThis->m_currentItemId != targetId))
        {
            
            co_return;
        }

        safeThis->m_cachedDetailFingerprint = fetchedFingerprint;

        if (safeThis->m_pendingDeferredFetchId == targetId)
        {
            if (!differsFromCache && safeThis->m_pendingFetchReady && safeThis->m_pendingFetchedItemFromCache)
            {
                safeThis->maybeFlushDeferredUpdate(targetId);
                co_return;
            }

            
            
            safeThis->m_pendingFetchedItem = std::move(item);
            safeThis->m_pendingFetchReady = true;
            safeThis->m_pendingFetchedItemFromCache = false;
            safeThis->maybeFlushDeferredUpdate(targetId);
            co_return;
        }

        
        if (safeThis->m_currentItemId == targetId && differsFromCache)
        {
            qDebug() << "[DetailView] Applying refreshed detail after cache"
                     << "itemId=" << targetId;
            QCoro::connect(safeThis->executeDeferredUpdate(std::move(item), true), safeThis.data(), []() {});
        }
    }
    catch (...)
    {
        if (safeThis && safeThis->m_pendingDeferredFetchId == targetId)
        {
            if (!safeThis->m_pendingFetchReady && safeThis->m_currentMediaItem.id != targetId)
            {
                safeThis->m_pendingDeferredFetchId.clear();
                const QString errorText = DetailView::tr("Error Loading Item");
                QMetaObject::invokeMethod(
                    safeThis.data(),
                    [safeThis, targetId, errorText]()
                    {
                        if (safeThis && safeThis->m_currentItemId == targetId)
                            safeThis->m_titleLabel->setText(errorText);
                    },
                    Qt::QueuedConnection);
            }
            else
            {
                qDebug() << "[DetailView] Detail refresh failed; using existing data"
                         << "itemId=" << targetId;
            }
        }
    }
}

void DetailView::maybeFlushDeferredUpdate(const QString &itemId)
{
    if (m_pendingDeferredFetchId != itemId)
        return;
    if (!m_pendingFetchReady || !m_pendingAnimationGuardDone)
        return;

    
    m_pendingDeferredFetchId.clear();

    MediaItem item = m_pendingFetchedItem;
    const bool fromCache = m_pendingFetchedItemFromCache;
    m_pendingFetchedItem = MediaItem{};
    m_pendingFetchReady = false;
    m_pendingAnimationGuardDone = false;
    m_pendingFetchedItemFromCache = false;

    if (item.type != "Series" || m_currentPlayableItem.type != "Episode" || m_currentPlayableItem.id.isEmpty())
    {
        m_currentPlayableItem = item;
    }
    qDebug() << "[DetailView] Deferred detail update"
             << "itemId=" << itemId << "fromCache=" << fromCache;
    QCoro::connect(executeDeferredUpdate(std::move(item)), this, []() {});
}

QCoro::Task<void> DetailView::executeDeferredUpdate(MediaItem item, bool isSilentRefresh)
{
    QPointer<DetailView> safeThis(this);
    const QString targetId = item.id;
    const QString detectedType = item.type;
    const bool batchPresentation = isVisible();

    if (batchPresentation)
    {
        
        
        
        setDeferredPresentationBatchActive(true);
        QTimer::singleShot(160, this,
                           [safeThis]()
                           {
                               if (safeThis && safeThis->m_deferredPresentationBatchActive)
                                   safeThis->setDeferredPresentationBatchActive(false);
                           });
    }

    
    
    
    co_await updateUi(item, isSilentRefresh);
    if (!safeThis)
        co_return;
    safeThis->persistDetailCacheSnapshot(QStringLiteral("detail-ui"));

    if (isSilentRefresh)
    {
        safeThis->setDeferredPresentationBatchActive(false);
        co_return;
    }

    if (batchPresentation)
    {
        QTimer::singleShot(0, safeThis.data(),
                           [safeThis, targetId, detectedType]()
                           {
                               if (!safeThis)
                                   return;
                               safeThis->finishDeferredPresentationBatch(targetId, detectedType);
                           });
    }
    else
    {
        safeThis->startSecondaryFetches(targetId, detectedType);
    }
}

void DetailView::setDeferredPresentationBatchActive(bool active)
{
    m_deferredPresentationBatchActive = active;
}

void DetailView::finishDeferredPresentationBatch(const QString &targetId, const QString &detectedType)
{
    if (m_currentItemId != targetId)
    {
        setDeferredPresentationBatchActive(false);
        return;
    }

    updateTagLayoutHeight();
    updateEpisodeJumpControl(m_currentSeasonEpisodes.size());

    if (m_contentWidget && m_contentWidget->layout())
        m_contentWidget->layout()->activate();
    if (layout())
        layout()->activate();

    setDeferredPresentationBatchActive(false);

    startSecondaryFetches(targetId, detectedType);
}

void DetailView::startSecondaryFetches(const QString &targetId, const QString &detectedType)
{
    if (targetId.isEmpty() || !m_core || !m_core->mediaService())
        return;

    const auto startNetworkFetches = [this, targetId, detectedType]()
    {
        qDebug() << "[DetailView][network] Start secondary fetches"
                 << "itemId=" << targetId << "type=" << detectedType;
        QCoro::connect(executeFetchSecondaries(QPointer<DetailView>(this), m_core, targetId, detectedType), this,
                       []() {});
    };

    if (detectedType == "Series")
    {
        const bool hasCachedSeriesSecondaryUi = m_hasCachedCastData || m_hasCachedSeriesPlayableItem ||
                                                m_hasCachedSeriesSeasons || m_hasCachedSeasonEpisodes ||
                                                m_hasCachedSimilarItems || m_hasCachedCollections ||
                                                m_hasCachedAdditionalParts;
        startNetworkFetches();
        if (hasCachedSeriesSecondaryUi)
        {
            applyCachedSecondaryData(targetId, detectedType);
        }
        else
        {
            qDebug() << "[DetailView][cache-ui] Skip cached secondary data"
                     << "itemId=" << targetId << "type=" << detectedType
                     << "reason=" << "no-cached-series-secondary-data";
        }
        return;
    }

    applyCachedSecondaryData(targetId, detectedType);
    startNetworkFetches();
}

void DetailView::applyCachedSecondaryData(const QString &targetId, const QString &detectedType, bool revealBottomInfo)
{
    if (targetId != m_currentItemId)
        return;

    qDebug() << "[DetailView][cache-ui] Applying cached secondary data"
             << "itemId=" << targetId << "type=" << detectedType << "sections=" << m_cachedSectionsFingerprint.left(12);

    if (detectedType == "Series")
    {
        if (!m_hasManualSeriesSelection && m_hasCachedSeriesPlayableItem &&
            m_cachedSeriesPlayableItem.type == "Episode" && !m_cachedSeriesPlayableItem.id.isEmpty())
        {
            if (m_currentPlayableItem.id != m_cachedSeriesPlayableItem.id)
                applySeriesPlayableItemToUi(m_cachedSeriesPlayableItem);
        }

        if (m_hasCachedSeriesSeasons && !m_appliedCachedSeriesSeasonsToUi)
        {
            m_seriesSeasons = m_cachedSeriesSeasons;
            int targetSeasonIdx = 0;
            const int cachedEpisodeSeasonIndex = m_cachedSeasonIndex;
            const QString cachedEpisodeSeasonId = m_cachedSeasonId;
            bool selectedSeasonResolved = false;
            if (m_hasManualSeriesSelection && m_currentSeasonIndex >= 0 &&
                m_currentSeasonIndex < m_seriesSeasons.size())
            {
                targetSeasonIdx = m_currentSeasonIndex;
                selectedSeasonResolved = true;
            }
            const int playableParentIdx = m_currentPlayableItem.parentIndexNumber;
            if (!selectedSeasonResolved && !m_hasManualSeriesSelection && playableParentIdx >= 0)
            {
                for (int i = 0; i < m_seriesSeasons.size(); ++i)
                {
                    if (m_seriesSeasons[i].indexNumber == playableParentIdx)
                    {
                        targetSeasonIdx = i;
                        break;
                    }
                }
            }
            if (!m_seriesSeasons.isEmpty())
            {
                targetSeasonIdx = qBound(0, targetSeasonIdx, m_seriesSeasons.size() - 1);
                m_currentSeasonIndex = targetSeasonIdx;
                updateSeasonSwitcher(targetSeasonIdx);
            }

            if (m_hasCachedSeasonEpisodes && cachedEpisodeSeasonIndex == targetSeasonIdx &&
                cachedEpisodeSeasonId ==
                    (m_seriesSeasons.isEmpty() ? QString() : m_seriesSeasons[targetSeasonIdx].id) &&
                !m_seriesSeasons.isEmpty() &&
                episodesBelongToSeason(m_cachedSeasonEpisodes, m_seriesSeasons[targetSeasonIdx]))
            {
                m_currentSeasonEpisodes = m_cachedSeasonEpisodes;
                if (!m_appliedCachedSeasonEpisodesToUi && m_episodeWidget)
                {
                    applyReservedSectionItems(m_episodeSectionReserveWidget, m_episodeWidget, m_currentSeasonEpisodes);
                    updateEpisodeJumpControl(m_currentSeasonEpisodes.size());
                    const QString highlightedEpisodeId = m_currentPlayableItem.id;
                    if (!highlightedEpisodeId.isEmpty())
                    {
                        m_episodeWidget->gallery()->setHighlightedItemId(highlightedEpisodeId);
                        m_episodeWidget->gallery()->scrollToItemId(highlightedEpisodeId);
                    }
                }
                m_appliedCachedSeasonEpisodesToUi = true;
            }

            if (m_seasonWidget)
            {
                m_seasonWidget->setTitle(tr("Seasons"));
                m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
                m_seasonWidget->setGalleryHeight(300);
                applyReservedSectionItems(m_seasonSectionReserveWidget, m_seasonWidget, m_seriesSeasons);
                m_appliedCachedSeriesSeasonsToUi = true;
            }
        }
    }

    
    if (m_hasCachedCastData && !m_appliedCachedCastToUi)
    {
        applyCastSection(m_currentMediaItem);
        m_appliedCachedCastToUi = true;
    }
    if (revealBottomInfo && m_deferBottomInfoReveal)
    {
        m_deferBottomInfoReveal = false;
        hideReserveWidget(m_bottomInfoReserveWidget);
        if (m_bottomInfoWidget)
            m_bottomInfoWidget->show();
    }

    
    
    
    if (m_hasCachedAdditionalParts && !m_appliedCachedAdditionalPartsToUi)
    {
        applyReservedSectionItems(m_additionalPartsSectionReserveWidget, m_additionalPartsWidget,
                                  m_cachedAdditionalParts);
        m_appliedCachedAdditionalPartsToUi = true;
    }
    else if (!m_hasCachedAdditionalParts && (m_hasCachedCollections || m_hasCachedSimilarItems))
    {
        applyReservedSectionItems(m_additionalPartsSectionReserveWidget, m_additionalPartsWidget, {});
    }

    if (m_hasCachedCollections && !m_appliedCachedCollectionsToUi)
    {
        applyCollectionGalleryLayout(m_cachedCollections);
        applyReservedSectionItems(m_collectionSectionReserveWidget, m_collectionWidget, m_cachedCollections);
        m_appliedCachedCollectionsToUi = true;
    }
    else if (!m_hasCachedCollections && m_hasCachedSimilarItems)
    {
        applyReservedSectionItems(m_collectionSectionReserveWidget, m_collectionWidget, {});
    }

    if (m_hasCachedSimilarItems && !m_appliedCachedSimilarItemsToUi)
    {
        applyReservedSectionItems(m_similarSectionReserveWidget, m_similarWidget, m_cachedSimilarItems);
        m_appliedCachedSimilarItemsToUi = true;
    }
}

void DetailView::persistDetailCacheSnapshot(const QString &reason)
{
    if (m_detailCacheServerId.isEmpty() || m_detailCacheUserId.isEmpty() || m_currentMediaItem.id.isEmpty())
        return;

    DetailCacheUtils::DetailCacheEntry entry;
    entry.item = m_currentMediaItem;
    const bool isSeries = m_currentMediaItem.type == "Series";
    entry.hasPlayableItem = isSeries && m_currentPlayableItem.type == "Episode" && !m_currentPlayableItem.id.isEmpty();
    if (entry.hasPlayableItem)
        entry.playableItem = m_currentPlayableItem;
    entry.hasSeasons = isSeries && m_hasCachedSeriesSeasons;
    entry.seasons = m_cachedSeriesSeasons;
    entry.hasSeasonEpisodes = isSeries && m_hasCachedSeasonEpisodes;
    entry.seasonEpisodes = m_cachedSeasonEpisodes;
    entry.seasonIndex = isSeries ? m_cachedSeasonIndex : -1;
    entry.seasonId = isSeries ? m_cachedSeasonId : QString();
    entry.playableItemFromServer = entry.hasPlayableItem;
    entry.hasSimilarItems = m_hasCachedSimilarItems;
    entry.similarItems = m_cachedSimilarItems;
    entry.hasCollections = m_hasCachedCollections;
    entry.collections = m_cachedCollections;
    entry.hasAdditionalParts = m_hasCachedAdditionalParts;
    entry.additionalParts = m_cachedAdditionalParts;

    saveDetailCacheEntryAsync(m_detailCacheServerId, m_detailCacheUserId, entry, reason);
}

void DetailView::updateMetaRow(const MediaItem &item, const QString &leadingMeta)
{
    QStringList metas;
    if (item.communityRating > 0)
    {
        m_ratingStarLabel->show();
        metas << QString::number(item.communityRating, 'f', 1);
    }
    else
    {
        m_ratingStarLabel->hide();
    }
    if (!leadingMeta.isEmpty())
        metas << leadingMeta;
    const QString releaseDate = formatDetailReleaseDate(item);
    if (!releaseDate.isEmpty())
        metas << releaseDate;
    if (item.runTimeTicks > 0)
        metas << formatRunTime(item.runTimeTicks);
    if (!item.officialRating.isEmpty())
        metas << item.officialRating;

    m_metaLabel->setText(metas.join("  \u2022  "));
    m_metaRowWidget->show();
}






void DetailView::applySeedToUi(const MediaItem &seed)
{
    m_currentMediaItem = seed;
    m_titleLabel->setText(seed.name);

    
    updateMetaRow(seed, formatEpisodeTag(seed));

    
    const bool shouldShowNumber = shouldShowDisplayNumber(seed);
    const QString displayNumber = shouldShowNumber ? extractDisplayNumber(seed) : QString();
    updateDisplayNumber(displayNumber);

    
    QString text = seed.overview;
    text.replace(QRegularExpression("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
    text.replace("</p>", "\n\n");
    text.remove(QRegularExpression("<[^>]*>"));
    QTextDocument doc;
    doc.setHtml(text);
    QString cleanText = doc.toPlainText();
    cleanText.replace(QChar::LineSeparator, '\n');
    cleanText.remove(QRegularExpression("<[^>]*>"));
    m_cleanOverviewText = cleanText;
    m_lastOverviewWidth = -1;
    updateOverviewElidedText();

    
    if (!seed.taglines.isEmpty())
    {
        m_taglineLabel->setText(seed.taglines.first());
        m_taglineLabel->show();
    }
    else
    {
        m_taglineLabel->hide();
    }

    
    m_isFavorite = seed.isFavorite();
    m_actionWidget->setFavoriteState(m_isFavorite);
    m_actionWidget->setPlayedState(seed.userData.played);

    
    
    
    
    if (seed.type == "Series")
    {
        m_actionWidget->setSeriesLoadingMode();
    }
    else
    {
        m_actionWidget->setupNormalMode(seed);
    }

    
    
    
    
    
    if (!seed.mediaSources.isEmpty())
    {
        const int selectedSourceIndex = preferredSourceIndex(m_core, seed);
        m_actionWidget->setSources(seed.mediaSources, selectedSourceIndex);
        if (selectedSourceIndex < seed.mediaSources.size() &&
            !seed.mediaSources[selectedSourceIndex].mediaStreams.isEmpty())
        {
            applyRememberedStreams(seed, seed.mediaSources[selectedSourceIndex]);
        }
        m_appliedSourcesFingerprint = computeSourcesFingerprint(seed.mediaSources);
    }
    else
    {
        m_actionWidget->setSources(QList<MediaSourceInfo>{}, 0);
        m_actionWidget->setStreams(MediaSourceInfo{});
        m_appliedSourcesFingerprint = computeSourcesFingerprint(QList<MediaSourceInfo>{});
    }
    m_actionWidget->refreshExtPlayerButton();

    
    
    
    buildTagButtons(seed.genres);

    
    
    
    
    
    
    QList<MediaSourceInfo> seedDefaultSource;
    if (!seed.mediaSources.isEmpty())
    {
        const int selectedSourceIndex = m_actionWidget->currentSourceIndex();
        if (selectedSourceIndex < seed.mediaSources.size())
            seedDefaultSource.append(seed.mediaSources[selectedSourceIndex]);
    }
    m_bottomInfoWidget->setInfo(seed, seedDefaultSource);
    m_bottomInfoWidget->setVisible(!m_deferBottomInfoReveal);
    if (m_deferBottomInfoReveal)
    {
        showReserveWidget(m_bottomInfoReserveWidget, reserveHeightFor(m_bottomInfoWidget, 220));
    }
    else
    {
        hideReserveWidget(m_bottomInfoReserveWidget);
    }
    m_appliedBottomInfoFingerprint = computeBottomInfoFingerprint(seed, seedDefaultSource);

    
    
    m_currentBackdropPix = QPixmap();
    updateBackdrop();

    
    
    
    
    
    QCoro::connect(executeLoadImages(QPointer<DetailView>(this), m_core, seed, false), this, []() {});
}





void DetailView::buildTagButtons(const QStringList &genres)
{
    if (!m_tagsWidget || !m_tagsLayout)
        return;

    
    if (m_tagsLayout->count() == genres.size())
    {
        bool same = true;
        for (int i = 0; i < genres.size(); ++i)
        {
            QLayoutItem *layoutItem = m_tagsLayout->itemAt(i);
            QWidget *w = layoutItem ? layoutItem->widget() : nullptr;
            auto *btn = qobject_cast<QPushButton *>(w);
            if (!btn || btn->text() != genres[i])
            {
                same = false;
                break;
            }
        }
        if (same)
            return;
    }

    m_tagsWidget->setUpdatesEnabled(false);
    clearLayout(m_tagsLayout);
    for (const QString &genre : genres)
    {
        auto *tagBtn = new DetailTagButton(genre, m_tagsWidget);
        connect(tagBtn, &QPushButton::clicked, this,
                [this, genre]() { Q_EMIT navigateToFilteredView("Genre", genre); });
        m_tagsLayout->addWidget(tagBtn);
    }
    m_tagsWidget->setUpdatesEnabled(true);
    updateTagLayoutHeight();
    QTimer::singleShot(0, this, [this]() { updateTagLayoutHeight(); });
}

void DetailView::applyCastSection(const MediaItem &item)
{
    if (!m_castWidget)
        return;

    const QString fp = castFingerprint(item);
    if (!fp.isEmpty() && fp == m_appliedCastFingerprint)
        return;

    if (item.people.isEmpty())
    {
        applyReservedSectionItems(m_castSectionReserveWidget, m_castWidget, {});
        m_appliedCastFingerprint = fp;
        return;
    }

    QList<MediaItem> castItems;
    castItems.reserve(item.people.size());
    for (const auto &person : item.people)
    {
        MediaItem fakeItem;
        fakeItem.id = person.id;
        fakeItem.name = person.name;
        fakeItem.images.primaryTag = person.primaryImageTag;
        
        QStringList personParts;
        if (!person.type.isEmpty())
        {
            
            static const QHash<QString, const char *> typeMap = {
                {"Actor", QT_TR_NOOP("Actor")},         {"Director", QT_TR_NOOP("Director")},
                {"Writer", QT_TR_NOOP("Writer")},       {"Producer", QT_TR_NOOP("Producer")},
                {"Composer", QT_TR_NOOP("Composer")},   {"Conductor", QT_TR_NOOP("Conductor")},
                {"GuestStar", QT_TR_NOOP("GuestStar")}, {"Editor", QT_TR_NOOP("Editor")},
                {"Lyricist", QT_TR_NOOP("Lyricist")},
            };
            auto it = typeMap.constFind(person.type);
            personParts << (it != typeMap.constEnd() ? tr(it.value()) : person.type);
        }
        if (!person.role.isEmpty())
            personParts << person.role;
        fakeItem.overview = personParts.join(" · ");
        fakeItem.type = "Person";
        castItems.append(fakeItem);
    }

    applyReservedSectionItems(m_castSectionReserveWidget, m_castWidget, castItems);
    m_appliedCastFingerprint = fp;
}

void DetailView::applyCollectionGalleryLayout(const QList<MediaItem> &collections)
{
    int landscapeSignals = 0;
    int portraitSignals = 0;

    const auto hasLandscapeArtwork = [](const MediaItem &item)
    {
        const double primaryAspectRatio = item.images.primaryImageAspectRatio;
        if (primaryAspectRatio > 1.05)
            return true;
        if (primaryAspectRatio > 0.0)
            return false;

        if (!item.images.primaryTag.isEmpty())
            return false;

        return !item.images.thumbTag.isEmpty() || !item.images.backdropTag.isEmpty() ||
               !item.images.parentThumbTag.isEmpty() || !item.images.parentBackdropTag.isEmpty();
    };

    for (const MediaItem &collection : collections)
    {
        if (hasLandscapeArtwork(collection))
        {
            ++landscapeSignals;
        }
        else if (!collection.images.primaryTag.isEmpty() || collection.images.primaryImageAspectRatio > 0.0)
        {
            ++portraitSignals;
        }
    }

    const bool useLandscapeCards = landscapeSignals > 0 && landscapeSignals > portraitSignals;

    if (useLandscapeCards)
    {
        m_collectionWidget->setCardStyle(MediaCardDelegate::LibraryTile);
        m_collectionWidget->setGalleryHeight(230);
    }
    else
    {
        m_collectionWidget->setCardStyle(MediaCardDelegate::Poster);
        m_collectionWidget->setGalleryHeight(300);
    }

    qDebug() << "[DetailView] Collection gallery layout"
             << "itemCount=" << collections.size() << "landscapeSignals=" << landscapeSignals
             << "portraitSignals=" << portraitSignals << "style=" << (useLandscapeCards ? "LibraryTile" : "Poster");
}







QCoro::Task<void> DetailView::executeFetchSecondaries(QPointer<DetailView> safeThis, QEmbyCore *core, QString targetId,
                                                      QString itemType)
{
    if (!safeThis)
        co_return;

    
    
    if (itemType == "Series")
    {
        if (!safeThis->m_hasManualSeriesSelection && safeThis->m_hasCachedSeriesPlayableItem &&
            safeThis->m_cachedSeriesPlayableItem.type == "Episode" &&
            !safeThis->m_cachedSeriesPlayableItem.id.isEmpty())
        {
            safeThis->applySeriesPlayableItemToUi(safeThis->m_cachedSeriesPlayableItem);
            qDebug() << "[DetailView][cache-ui] Applied cached series playable item"
                     << "seriesId=" << targetId << "episodeId=" << safeThis->m_cachedSeriesPlayableItem.id;
        }
        qDebug() << "[DetailView][network] Fetch series next-up"
                 << "seriesId=" << targetId << "applyToUi=" << !safeThis->m_hasManualSeriesSelection;
        co_await safeThis->fetchSeriesNextUp(targetId, !safeThis->m_hasManualSeriesSelection);
        if (!safeThis || safeThis->m_currentItemId != targetId)
            co_return;

        
        qDebug() << "[DetailView][network] Fetch series seasons"
                 << "seriesId=" << targetId;
        auto seasons = co_await core->mediaService()->getSeasons(targetId);
        if (!safeThis || safeThis->m_currentItemId != targetId)
            co_return;

        bool seasonsUnchanged = false;
        if (safeThis->m_appliedCachedSeriesSeasonsToUi)
        {
            seasonsUnchanged = co_await mediaItemListsEqualAsync(safeThis->m_cachedSeriesSeasons, seasons);
            if (!safeThis || safeThis->m_currentItemId != targetId)
                co_return;
        }
        safeThis->m_seriesSeasons = seasons;
        safeThis->m_cachedSeriesSeasons = seasons;
        safeThis->m_hasCachedSeriesSeasons = true;

        
        
        int targetSeasonIdx = 0;
        bool selectedSeasonResolved = false;
        if (safeThis->m_hasManualSeriesSelection && !safeThis->m_manualSelectedSeasonId.isEmpty())
        {
            for (int i = 0; i < seasons.size(); ++i)
            {
                if (seasons[i].id == safeThis->m_manualSelectedSeasonId)
                {
                    targetSeasonIdx = i;
                    selectedSeasonResolved = true;
                    break;
                }
            }
        }
        if (!selectedSeasonResolved && safeThis->m_hasManualSeriesSelection &&
            safeThis->m_manualSelectedSeasonIndex >= 0 && safeThis->m_manualSelectedSeasonIndex < seasons.size())
        {
            targetSeasonIdx = safeThis->m_manualSelectedSeasonIndex;
            selectedSeasonResolved = true;
        }
        if (!selectedSeasonResolved && safeThis->m_hasManualSeriesSelection && safeThis->m_currentSeasonIndex >= 0 &&
            safeThis->m_currentSeasonIndex < seasons.size())
        {
            targetSeasonIdx = safeThis->m_currentSeasonIndex;
            selectedSeasonResolved = true;
        }
        int playableParentIdx = safeThis->m_currentPlayableItem.parentIndexNumber;
        if (!selectedSeasonResolved && !safeThis->m_hasManualSeriesSelection && playableParentIdx >= 0)
        {
            for (int i = 0; i < seasons.size(); ++i)
            {
                if (seasons[i].indexNumber == playableParentIdx)
                {
                    targetSeasonIdx = i;
                    break;
                }
            }
        }
        if (!seasons.isEmpty())
            targetSeasonIdx = qBound(0, targetSeasonIdx, seasons.size() - 1);
        safeThis->m_currentSeasonIndex = targetSeasonIdx;

        
        if (!seasons.isEmpty() && safeThis->m_episodeWidget)
        {
            safeThis->updateSeasonSwitcher(targetSeasonIdx);

            const QString playableItemId =
                safeThis->m_hasManualSeriesSelection && !safeThis->m_manualSelectedEpisodeId.isEmpty()
                    ? safeThis->m_manualSelectedEpisodeId
                    : safeThis->m_currentPlayableItem.id;
            co_await safeThis->loadEpisodesForSeason(targetSeasonIdx, playableItemId, !playableItemId.isEmpty(),
                                                     safeThis->m_hasManualSeriesSelection);
        }
        else if (safeThis->m_episodeWidget)
        {
            applyReservedSectionItems(safeThis->m_episodeSectionReserveWidget, safeThis->m_episodeWidget, {});
        }
        if (!safeThis || safeThis->m_currentItemId != targetId)
            co_return;

        
        if (safeThis->m_seasonWidget && !seasonsUnchanged)
        {
            safeThis->m_seasonWidget->setTitle(tr("Seasons"));
            safeThis->m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
            safeThis->m_seasonWidget->setGalleryHeight(300);
            applyReservedSectionItems(safeThis->m_seasonSectionReserveWidget, safeThis->m_seasonWidget, seasons);
        }
    }

    
    if (safeThis->m_currentItemId != targetId)
        co_return;
    qDebug() << "[DetailView][ui] Apply cast section"
             << "itemId=" << targetId << "afterSeriesSections=" << (itemType == "Series");
    safeThis->applyCastSection(safeThis->m_currentMediaItem);
    if (safeThis->m_deferBottomInfoReveal)
    {
        safeThis->m_deferBottomInfoReveal = false;
        hideReserveWidget(safeThis->m_bottomInfoReserveWidget);
        if (safeThis->m_bottomInfoWidget)
            safeThis->m_bottomInfoWidget->show();
    }
    if (!safeThis)
        co_return;

    
    
    
    try
    {
        qDebug() << "[DetailView][network] Fetch similar items"
                 << "itemId=" << targetId;
        QList<MediaItem> similar = co_await core->mediaService()->getSimilarItems(targetId, 15);
        if (!safeThis || safeThis->m_currentItemId != targetId)
            co_return;

        bool similarUnchanged = false;
        if (safeThis->m_appliedCachedSimilarItemsToUi)
        {
            similarUnchanged = co_await mediaItemListsEqualAsync(safeThis->m_cachedSimilarItems, similar);
            if (!safeThis || safeThis->m_currentItemId != targetId)
                co_return;
        }
        safeThis->m_cachedSimilarItems = similar;
        safeThis->m_hasCachedSimilarItems = true;
        if (!similarUnchanged)
        {
            applyReservedSectionItems(safeThis->m_similarSectionReserveWidget, safeThis->m_similarWidget, similar);
        }
    }
    catch (...)
    {
        if (safeThis && safeThis->m_currentItemId == targetId && !safeThis->m_appliedCachedSimilarItemsToUi)
        {
            applyReservedSectionItems(safeThis->m_similarSectionReserveWidget, safeThis->m_similarWidget, {});
        }
    }
    if (!safeThis)
        co_return;

    
    
    try
    {
        qDebug() << "[DetailView][network] Fetch item collections"
                 << "itemId=" << targetId;
        QList<MediaItem> collections = co_await core->mediaService()->getItemCollections(targetId);
        if (!safeThis || safeThis->m_currentItemId != targetId)
            co_return;

        bool collectionsUnchanged = false;
        if (safeThis->m_appliedCachedCollectionsToUi)
        {
            collectionsUnchanged = co_await mediaItemListsEqualAsync(safeThis->m_cachedCollections, collections);
            if (!safeThis || safeThis->m_currentItemId != targetId)
                co_return;
        }
        safeThis->m_cachedCollections = collections;
        safeThis->m_hasCachedCollections = true;
        if (!collectionsUnchanged)
        {
            safeThis->applyCollectionGalleryLayout(collections);
            applyReservedSectionItems(safeThis->m_collectionSectionReserveWidget, safeThis->m_collectionWidget,
                                      collections);
        }
    }
    catch (...)
    {
        if (safeThis && safeThis->m_currentItemId == targetId && !safeThis->m_appliedCachedCollectionsToUi)
        {
            applyReservedSectionItems(safeThis->m_collectionSectionReserveWidget, safeThis->m_collectionWidget, {});
        }
    }
    if (!safeThis)
        co_return;

    
    try
    {
        qDebug() << "[DetailView][network] Fetch additional parts"
                 << "itemId=" << targetId;
        QList<MediaItem> additionalParts = co_await core->mediaService()->getAdditionalParts(targetId);
        if (!safeThis || safeThis->m_currentItemId != targetId)
            co_return;

        bool additionalPartsUnchanged = false;
        if (safeThis->m_appliedCachedAdditionalPartsToUi)
        {
            additionalPartsUnchanged =
                co_await mediaItemListsEqualAsync(safeThis->m_cachedAdditionalParts, additionalParts);
            if (!safeThis || safeThis->m_currentItemId != targetId)
                co_return;
        }
        safeThis->m_cachedAdditionalParts = additionalParts;
        safeThis->m_hasCachedAdditionalParts = true;
        if (!additionalPartsUnchanged)
        {
            applyReservedSectionItems(safeThis->m_additionalPartsSectionReserveWidget,
                                      safeThis->m_additionalPartsWidget, additionalParts);
        }
    }
    catch (...)
    {
        if (safeThis && safeThis->m_currentItemId == targetId && !safeThis->m_appliedCachedAdditionalPartsToUi)
        {
            applyReservedSectionItems(safeThis->m_additionalPartsSectionReserveWidget,
                                      safeThis->m_additionalPartsWidget, {});
        }
    }

    if (safeThis && safeThis->m_currentItemId == targetId)
        safeThis->persistDetailCacheSnapshot(QStringLiteral("secondaries"));
}

QCoro::Task<void> DetailView::fetchSeriesNextUp(QString targetId, bool applyToUi)
{
    QPointer<DetailView> safeThis(this);
    QPointer<MediaService> mediaService(
        m_core ? m_core->mediaService() : nullptr);
    try
    {
        if (!mediaService)
            co_return;
        MediaItem playableItem;
        qDebug() << "[DetailView][network] Fetch next-up"
                 << "seriesId=" << targetId;
        auto nextUp = co_await mediaService->getNextUp(targetId);

        if (!safeThis || !mediaService || safeThis->m_currentItemId != targetId)
            co_return;

        if (!nextUp.isEmpty())
            playableItem = nextUp.first();
        else
        {
            qDebug() << "[DetailView][network] Fetch seasons for next-up fallback"
                     << "seriesId=" << targetId;
            auto seasons = co_await mediaService->getSeasons(targetId);
            if (!safeThis || !mediaService ||
                safeThis->m_currentItemId != targetId)
                co_return;
            if (!seasons.isEmpty())
            {
                qDebug() << "[DetailView][network] Fetch episodes for next-up fallback"
                         << "seriesId=" << targetId << "seasonId=" << seasons.first().id;
                auto episodes = co_await mediaService->getEpisodes(
                    targetId, seasons.first().id);
                if (!safeThis || safeThis->m_currentItemId != targetId)
                    co_return;
                if (!episodes.isEmpty())
                    playableItem = episodes.first();
            }
        }

        if (safeThis && safeThis->m_currentItemId == targetId && !playableItem.id.isEmpty())
        {
            safeThis->m_cachedSeriesPlayableItem = playableItem;
            safeThis->m_hasCachedSeriesPlayableItem = true;

            if (!applyToUi || safeThis->m_hasManualSeriesSelection)
            {
                qDebug() << "[DetailView][network] Updated server next-up cache without "
                            "changing manual selection"
                         << "seriesId=" << targetId << "episodeId=" << playableItem.id << "applyToUi=" << applyToUi
                         << "manualSelection=" << safeThis->m_hasManualSeriesSelection;
                safeThis->persistDetailCacheSnapshot(QStringLiteral("series-server-next-up"));
                co_return;
            }

            co_await safeThis->applySeriesPlayableItem(playableItem);
        }
    }
    catch (...)
    {
    }
}

void DetailView::applySeriesPlayableItemToUi(const MediaItem &playableItem, bool scrollToEpisode)
{
    if (playableItem.id.isEmpty() || m_currentMediaItem.type != "Series")
        return;

    const QString playableFingerprint = DetailCacheUtils::fingerprint(playableItem);
    const bool playableUnchanged =
        !m_appliedSeriesPlayableFingerprint.isEmpty() && playableFingerprint == m_appliedSeriesPlayableFingerprint;
    m_currentPlayableItem = playableItem;
    const QString episodeTag = formatEpisodeTag(playableItem);

    if (!playableUnchanged)
    {
        const QString actionTag = episodeTag.isEmpty() ? playableItem.name : episodeTag;
        m_actionWidget->setupSeriesMode(playableItem, actionTag);
        updateMetaRow(m_currentMediaItem, episodeTag);

        m_actionWidget->setSources(playableItem.mediaSources,
                                   preferredSourceIndex(m_core, playableItem));
        m_appliedSourcesFingerprint = computeSourcesFingerprint(playableItem.mediaSources);

        QList<MediaSourceInfo> selectedSources;
        if (!playableItem.mediaSources.isEmpty())
        {
            int sourceIndex = m_actionWidget->currentSourceIndex();
            if (sourceIndex >= playableItem.mediaSources.size())
                sourceIndex = 0;

            const MediaSourceInfo &selectedSource = playableItem.mediaSources[sourceIndex];
            applyRememberedStreams(playableItem, selectedSource);
            selectedSources.append(selectedSource);
        }
        else
        {
            m_actionWidget->setStreams(MediaSourceInfo{});
        }

        if (!selectedSources.isEmpty())
        {
            const QString bottomInfoFingerprint = computeBottomInfoFingerprint(playableItem, selectedSources);
            if (bottomInfoFingerprint != m_appliedBottomInfoFingerprint)
            {
                m_bottomInfoWidget->setInfo(playableItem, selectedSources);
                m_appliedBottomInfoFingerprint = bottomInfoFingerprint;
                m_bottomInfoWidget->setVisible(!m_deferBottomInfoReveal);
                if (m_deferBottomInfoReveal)
                {
                    showReserveWidget(m_bottomInfoReserveWidget, reserveHeightFor(m_bottomInfoWidget, 220));
                }
                else
                {
                    hideReserveWidget(m_bottomInfoReserveWidget);
                }
            }
            else
            {
                m_bottomInfoWidget->setVisible(!m_deferBottomInfoReveal);
            }
        }

        m_appliedSeriesPlayableFingerprint = playableFingerprint;
    }

    m_actionWidget->refreshExtPlayerButton();
    m_actionWidget->setPlayedState(playableItem.userData.played);
    if (m_episodeWidget)
    {
        m_episodeWidget->gallery()->setHighlightedItemId(playableItem.id);
        if (scrollToEpisode)
            m_episodeWidget->gallery()->scrollToItemId(playableItem.id);
    }

    qDebug() << "[DetailView] Apply series playable item"
             << "seriesId=" << m_currentItemId << "episodeId=" << playableItem.id << "episodeTag=" << episodeTag
             << "sourceCount=" << playableItem.mediaSources.size();
}

QCoro::Task<void> DetailView::applySeriesPlayableItem(MediaItem playableItem, bool scrollToEpisode)
{
    if (playableItem.id.isEmpty() || m_currentMediaItem.type != "Series")
        co_return;

    QPointer<DetailView> safeThis(this);
    const QString seriesId = m_currentItemId;
    const QString playableItemId = playableItem.id;

    applySeriesPlayableItemToUi(playableItem, scrollToEpisode);
    const bool persistServerSelection = !m_hasManualSeriesSelection;
    rememberSeriesSelection(playableItem, QStringLiteral("series-selection"), persistServerSelection);

    const bool needsPlaybackInfo =
        playableItem.mediaSources.isEmpty() || playableItem.mediaSources.first().mediaStreams.isEmpty();
    if (!needsPlaybackInfo)
        co_return;

    try
    {
        PlaybackInfo info = co_await m_core->mediaService()->getPlaybackInfo(playableItemId);
        if (!safeThis || safeThis->m_currentItemId != seriesId || safeThis->m_currentMediaItem.type != "Series" ||
            safeThis->m_currentPlayableItem.id != playableItemId)
            co_return;
        if (!info.mediaSources.isEmpty())
        {
            playableItem.mediaSources = info.mediaSources;
            safeThis->applySeriesPlayableItemToUi(playableItem, false);
            const bool persistPlaybackInfoSelection = !safeThis->m_hasManualSeriesSelection;
            safeThis->rememberSeriesSelection(playableItem, QStringLiteral("series-selection-playback-info"),
                                              persistPlaybackInfoSelection);
        }
    }
    catch (...)
    {
    }
}

void DetailView::applyRememberedStreams(const MediaItem &item,
                                        const MediaSourceInfo &source)
{
    const PlayerPreferenceUtils::RememberedStreamSelection remembered =
        PlayerPreferenceUtils::validatedRememberedStreamSelection(
            activeServerId(m_core), item.id, source);
    m_actionWidget->setStreams(source, remembered.audioIndex,
                               remembered.subtitleIndex);
}

QCoro::Task<void> DetailView::onVersionChanged(int index)
{
    const bool usePlayableItem = m_currentMediaItem.type == "Series" && !m_currentPlayableItem.id.isEmpty();
    MediaItem actionItem = usePlayableItem ? m_currentPlayableItem : m_currentMediaItem;
    if (index < 0 || index >= actionItem.mediaSources.size())
        co_return;
    MediaSourceInfo source = actionItem.mediaSources[index];
    QPointer<DetailView> safeThis(this);
    const QString expectedPageId = m_currentItemId;
    const QString actionItemId = actionItem.id;

    MediaSourcePreferenceUtils::rememberMediaSourceId(
        activeServerId(m_core), actionItemId, source.id);

    if (source.mediaStreams.isEmpty())
    {
        try
        {
            PlaybackInfo info = co_await m_core->mediaService()->getPlaybackInfo(actionItemId);
            if (!safeThis || safeThis->m_currentItemId != expectedPageId)
                co_return;
            if (!info.mediaSources.isEmpty())
            {
                if (usePlayableItem)
                {
                    if (safeThis->m_currentPlayableItem.id != actionItemId)
                        co_return;
                    safeThis->m_currentPlayableItem.mediaSources = info.mediaSources;
                    actionItem = safeThis->m_currentPlayableItem;
                }
                else
                {
                    safeThis->m_currentMediaItem.mediaSources = info.mediaSources;

                    
                    
                    safeThis->m_currentPlayableItem.mediaSources = info.mediaSources;
                    actionItem = safeThis->m_currentMediaItem;
                }

                int selectedIndex = MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
                    actionItem.mediaSources, QString(), source.id);
                if (selectedIndex < actionItem.mediaSources.size())
                {
                    source = actionItem.mediaSources[selectedIndex];
                }
            }
        }
        catch (...)
        {
            co_return;
        }
    }
    if (!safeThis || source.mediaStreams.isEmpty())
        co_return;

    safeThis->applyRememberedStreams(actionItem, source);
    QList<MediaSourceInfo> singleSource;
    singleSource.append(source);
    const MediaItem infoItem = usePlayableItem ? safeThis->m_currentPlayableItem : safeThis->m_currentMediaItem;
    safeThis->m_bottomInfoWidget->setInfo(infoItem, singleSource);
    hideReserveWidget(safeThis->m_bottomInfoReserveWidget);
}

QCoro::Task<void> DetailView::executePlay(MediaItem targetItem, long long startTicks)
{
    if (targetItem.id.isEmpty())
        co_return;
    try
    {
        MediaItem actualItem = targetItem;
        if (actualItem.mediaSources.isEmpty())
        {
            PlaybackInfo info = co_await m_core->mediaService()->getPlaybackInfo(actualItem.id);
            actualItem.mediaSources = info.mediaSources;
        }
        if (actualItem.mediaSources.isEmpty())
            co_return;

        int sourceIdx = 0;
        int selectedAudioIdx = -1;
        int selectedSubIdx = -1;
        const bool useUiSelection = isCurrentActionItem(actualItem.id);

        if (useUiSelection)
        {
            
            sourceIdx = m_actionWidget->currentSourceIndex();
            selectedAudioIdx = m_actionWidget->currentAudioIndex();
            selectedSubIdx = m_actionWidget->currentSubtitleIndex();
        }
        else
        {
            sourceIdx = MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
                actualItem.mediaSources,
                ConfigStore::instance()->get<QString>(ConfigKeys::PlayerPreferredVersion).trimmed(),
                MediaSourcePreferenceUtils::rememberedMediaSourceId(
                    activeServerId(m_core), actualItem.id));
        }

        if (sourceIdx >= actualItem.mediaSources.size())
            sourceIdx = 0;
        MediaSourceInfo modifiedSource = actualItem.mediaSources[sourceIdx];

        if (useUiSelection)
        {
            
            
            
            for (auto &stream : modifiedSource.mediaStreams)
            {
                if (stream.type == "Audio" && selectedAudioIdx >= 0)
                    stream.isDefault = (stream.index == selectedAudioIdx);
                else if (stream.type == "Subtitle" && selectedSubIdx >= 0)
                    stream.isDefault = (stream.index == selectedSubIdx);
            }
        }
        else
        {
            PlayerPreferenceUtils::applyRememberedOrPreferredStreamRules(
                modifiedSource, activeServerId(m_core), actualItem.id,
                ConfigStore::instance()->get<QString>(ConfigKeys::PlayerAudioLang, "auto"),
                ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto"));
        }

        QString streamUrl = m_core->mediaService()->getStreamUrl(actualItem.id, modifiedSource.id);

        const QString playTitle = MediaItemUtils::playbackTitle(actualItem, m_currentMediaItem.name);

        
        PlaybackManager::instance()->startInternalPlayback(actualItem.id, playTitle, streamUrl, startTicks,
                                                           QVariant::fromValue(modifiedSource));
    }
    catch (...)
    {
    }
}

QCoro::Task<void> DetailView::executePlaySeason(MediaItem seasonItem)
{
    if (seasonItem.id.isEmpty() || m_currentItemId.isEmpty() || !m_core || !m_core->mediaService())
        co_return;

    QPointer<DetailView> safeThis(this);
    const QString seriesId = m_currentItemId;
    const QString seasonId = seasonItem.id;

    try
    {
        QList<MediaItem> episodes = co_await m_core->mediaService()->getEpisodes(seriesId, seasonId);
        if (!safeThis || safeThis->m_currentItemId != seriesId)
            co_return;

        MediaItem targetEpisode = resolveSeasonPlaybackEpisode(episodes);
        if (targetEpisode.id.isEmpty())
            co_return;

        qDebug() << "[DetailView] Play season"
                 << "seriesId=" << seriesId << "seasonId=" << seasonId << "episodeId=" << targetEpisode.id
                 << "episodeTag=" << formatEpisodeTag(targetEpisode) << "episodeCount=" << episodes.size()
                 << "startTicks=" << targetEpisode.userData.playbackPositionTicks;

        co_await safeThis->applySeriesPlayableItem(targetEpisode, false);
        if (!safeThis || safeThis->m_currentItemId != seriesId)
            co_return;

        const MediaItem playbackItem =
            safeThis->m_currentPlayableItem.id == targetEpisode.id ? safeThis->m_currentPlayableItem : targetEpisode;
        co_await safeThis->executePlay(playbackItem, playbackItem.userData.playbackPositionTicks);
    }
    catch (...)
    {
    }
}

QCoro::Task<void> DetailView::executeExternalPlay(MediaItem targetItem, QString playerPath)
{
    if (targetItem.id.isEmpty() || playerPath.isEmpty())
        co_return;
    try
    {
        MediaItem actualItem = targetItem;
        if (actualItem.mediaSources.isEmpty())
        {
            PlaybackInfo info = co_await m_core->mediaService()->getPlaybackInfo(actualItem.id);
            actualItem.mediaSources = info.mediaSources;
        }
        if (actualItem.mediaSources.isEmpty())
            co_return;

        
        int sourceIdx = 0;
        const bool useUiSelection = isCurrentActionItem(actualItem.id);
        if (useUiSelection)
        {
            sourceIdx = m_actionWidget->currentSourceIndex();
        }
        else
        {
            sourceIdx = preferredSourceIndex(m_core, actualItem);
        }
        if (sourceIdx >= actualItem.mediaSources.size())
            sourceIdx = 0;
        MediaSourceInfo modifiedSource = actualItem.mediaSources[sourceIdx];

        if (useUiSelection)
        {
            int selectedAudioIdx = m_actionWidget->currentAudioIndex();
            int selectedSubIdx = m_actionWidget->currentSubtitleIndex();
            for (auto &stream : modifiedSource.mediaStreams)
            {
                if (stream.type == "Audio" && selectedAudioIdx >= 0)
                    stream.isDefault = (stream.index == selectedAudioIdx);
                else if (stream.type == "Subtitle" && selectedSubIdx >= 0)
                    stream.isDefault = (stream.index == selectedSubIdx);
            }
        }
        else
        {
            PlayerPreferenceUtils::applyRememberedOrPreferredStreamRules(
                modifiedSource, activeServerId(m_core), actualItem.id,
                ConfigStore::instance()->get<QString>(ConfigKeys::PlayerAudioLang, "auto"),
                ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto"));
        }

        QString streamUrl = m_core->mediaService()->getStreamUrl(actualItem.id, modifiedSource.id);

        const QString playTitle = MediaItemUtils::playbackTitle(actualItem, m_currentMediaItem.name);

        long long startTicks = actualItem.userData.playbackPositionTicks;
        PlaybackManager::instance()->startExternalPlayback(playerPath, actualItem.id, playTitle, streamUrl, startTicks,
                                                           QVariant::fromValue(modifiedSource));
    }
    catch (...)
    {
    }
}

void DetailView::updateBackdrop()
{
    if (m_contentWidget)
        static_cast<DetailContentWidget *>(m_contentWidget)->setBackdrop(m_currentBackdropPix);
}

void DetailView::updateTagLayoutHeight()
{
    if (!m_tagsWidget || !m_tagsLayout)
        return;

    int targetWidth = m_tagsWidget->width();
    if (targetWidth <= 0 && m_textContainer)
    {
        targetWidth = m_textContainer->contentsRect().width();
    }

    if (targetWidth <= 0)
    {
        m_tagsWidget->updateGeometry();
        QTimer::singleShot(50, this, [this]() { updateTagLayoutHeight(); });
        return;
    }

    m_tagsLayout->invalidate();
    const int targetHeight = m_tagsLayout->heightForWidth(targetWidth);
    if (m_tagsWidget->minimumHeight() != targetHeight)
    {
        m_tagsWidget->setMinimumHeight(targetHeight);
    }
    m_tagsWidget->updateGeometry();

    if (m_textContainer)
    {
        m_textContainer->updateGeometry();
    }
}

void DetailView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_overviewLabel && m_overviewLabel->width() > 10 && qAbs(m_overviewLabel->width() - m_lastOverviewWidth) > 5)
    {
        m_lastOverviewWidth = m_overviewLabel->width();
        updateOverviewElidedText();
    }
    updateTagLayoutHeight();
    updateEpisodeJumpControl(m_currentSeasonEpisodes.size());
}

bool DetailView::eventFilter(QObject *obj, QEvent *event)
{
    if (m_mainScrollArea && obj == m_mainScrollArea->viewport() && event->type() == QEvent::Resize)
    {
        QResizeEvent *re = static_cast<QResizeEvent *>(event);
        int newWidth = re->size().width();
        if (m_contentWidget && newWidth > 100 && m_contentWidget->maximumWidth() != newWidth)
        {
            m_contentWidget->setMaximumWidth(newWidth);
            QTimer::singleShot(0, this,
                               [this]()
                               {
                                   updateTagLayoutHeight();
                                   updateEpisodeJumpControl(m_currentSeasonEpisodes.size());
                               });
        }
    }
    if (m_episodeWidget && m_episodeWidget->gallery() && obj == m_episodeWidget->gallery()->listView()->viewport() &&
        event->type() == QEvent::Resize)
    {
        QTimer::singleShot(0, this, [this]() { updateEpisodeJumpControl(m_currentSeasonEpisodes.size()); });
    }
    if (event->type() == QEvent::Wheel)
    {
        bool isHorizontalViewport = obj->parent() && obj->parent()->property("isHorizontalListView").toBool();
        bool isMainViewport = (m_mainScrollArea && obj == m_mainScrollArea->viewport());
        if (isHorizontalViewport || isMainViewport)
        {
            QWheelEvent *we = static_cast<QWheelEvent *>(event);
            if (m_vScrollController)
            {
                m_vScrollController->scrollByWheelEvent(we, Qt::Vertical);
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void DetailView::showEvent(QShowEvent *event)
{
    BaseView::showEvent(event);

    
    
    if (m_skipNextSilentRefresh)
    {
        m_skipNextSilentRefresh = false;
        return;
    }

    
    
    
    m_pendingDeferredFetchId.clear();

    if (!m_currentItemId.isEmpty() && m_core && m_core->mediaService())
    {
        executeSilentRefresh(QPointer<DetailView>(this), m_core, m_currentItemId);
    }
}

void DetailView::clearLayout(QLayout *layout)
{
    if (!layout)
        return;
    QLayoutItem *child;
    while ((child = layout->takeAt(0)) != nullptr)
    {
        if (child->widget())
        {
            child->widget()->hide();
            child->widget()->setParent(nullptr);
            child->widget()->deleteLater();
        }
        else if (child->layout())
        {
            clearLayout(child->layout());
        }
        delete child;
    }
}

QString DetailView::formatRunTime(long long ticks)
{
    long long totalSeconds = ticks / 10000000;
    long long hours = totalSeconds / 3600;
    long long minutes = (totalSeconds % 3600) / 60;
    if (hours > 0)
        return QString(tr("%1 hr %2 min")).arg(hours).arg(minutes);
    return QString(tr("%1 min")).arg(minutes);
}

bool DetailView::shouldShowDisplayNumber(const MediaItem &item) const
{
    if (item.type.compare("Movie", Qt::CaseInsensitive) != 0)
    {
        return false;
    }

    static const QList<QRegularExpression> adultMarkers = {
        QRegularExpression(R"((^|[^0-9])18\+?([^0-9]|$))", QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(R"(\b(?:R[-\s]?18\+?|NC[-\s]?17|JAV|AV)\b)", QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(QStringLiteral("(成人|无码|有码|無碼|有碼)"))};

    const auto containsAdultMarker = [](const QString &value) -> bool
    {
        const QString trimmedValue = value.trimmed();
        if (trimmedValue.isEmpty())
        {
            return false;
        }

        for (const QRegularExpression &pattern : adultMarkers)
        {
            if (pattern.match(trimmedValue).hasMatch())
            {
                return true;
            }
        }

        return false;
    };

    if (containsAdultMarker(item.officialRating))
    {
        return true;
    }

    for (const QString &genre : item.genres)
    {
        if (containsAdultMarker(genre))
        {
            return true;
        }
    }

    for (const QString &tag : item.tags)
    {
        if (containsAdultMarker(tag))
        {
            return true;
        }
    }

    return false;
}

bool DetailView::isCurrentActionItem(const QString &itemId) const
{
    if (itemId.isEmpty())
        return false;
    if (itemId == m_currentMediaItem.id)
        return true;
    return m_currentMediaItem.type == "Series" && itemId == m_currentPlayableItem.id;
}

QStringList DetailView::buildNumberCandidates(const MediaItem &item) const
{
    QStringList candidates;
    const auto appendCandidate = [&candidates](const QString &value)
    {
        const QString trimmedValue = value.trimmed();
        if (!trimmedValue.isEmpty() && !candidates.contains(trimmedValue))
        {
            candidates.append(trimmedValue);
        }
    };

    for (const MediaSourceInfo &source : item.mediaSources)
    {
        appendCandidate(source.path);
    }

    appendCandidate(item.path);
    appendCandidate(item.name);
    appendCandidate(item.originalTitle);
    appendCandidate(item.sortName);

    for (const QString &tagline : item.taglines)
    {
        appendCandidate(tagline);
    }

    for (const QString &tag : item.tags)
    {
        appendCandidate(tag);
    }

    return candidates;
}

QString DetailView::extractDisplayNumber(const MediaItem &item) const
{
    static const NumberExtractor extractor(true);
    const QStringList candidates = buildNumberCandidates(item);
    const QString displayNumber = extractor.extractBestNumber(candidates);

    qDebug() << "[DetailView] adult number extraction itemId=" << item.id << "officialRating=" << item.officialRating
             << "candidateCount=" << candidates.size() << "selected=" << displayNumber;

    return displayNumber;
}

void DetailView::updateDisplayNumber(const QString &number)
{
    if (!m_numberButton || !m_metaRowWidget)
    {
        return;
    }

    const QString trimmedNumber = number.trimmed();
    const bool hasNumber = !trimmedNumber.isEmpty();
    m_numberButton->setVisible(hasNumber);
    m_numberButton->setText(trimmedNumber);
    m_numberButton->setToolTip(hasNumber ? tr("Click to copy number") : QString());
    m_metaRowWidget->setVisible(!m_metaLabel->text().isEmpty() || hasNumber);
}

void DetailView::copyDisplayedNumber()
{
    if (!m_numberButton)
    {
        return;
    }

    const QString displayNumber = m_numberButton->text().trimmed();
    if (displayNumber.isEmpty())
    {
        return;
    }

    if (QClipboard *clipboard = QGuiApplication::clipboard())
    {
        clipboard->setText(displayNumber);
    }

    qDebug() << "[DetailView] copied display number" << displayNumber << "for itemId" << m_currentItemId;
    ModernToast::showMessage(tr("Number copied: %1").arg(displayNumber), 2000);
}

void DetailView::markManualSeriesSelection(const QString &reason)
{
    if (m_currentMediaItem.type != "Series")
        return;

    if (!m_hasManualSeriesSelection)
    {
        qDebug() << "[DetailView] Manual series selection locked server auto-sync"
                 << "seriesId=" << m_currentItemId << "reason=" << reason;
    }
    m_hasManualSeriesSelection = true;
}

void DetailView::rememberSeriesSelection(const MediaItem &episode, const QString &reason, bool persist)
{
    if (m_currentMediaItem.type != "Series")
        return;

    int selectedSeasonIndex = -1;
    QString selectedSeasonId;
    bool hasSelectedSeason = false;
    if (episode.type == "Episode" && episode.parentIndexNumber >= 0)
    {
        for (int i = 0; i < m_seriesSeasons.size(); ++i)
        {
            if (m_seriesSeasons[i].indexNumber == episode.parentIndexNumber)
            {
                selectedSeasonIndex = i;
                selectedSeasonId = m_seriesSeasons[i].id;
                hasSelectedSeason = true;
                break;
            }
        }
    }
    else
    {
        selectedSeasonIndex = m_currentSeasonIndex;
        if (selectedSeasonIndex >= 0 && selectedSeasonIndex < m_seriesSeasons.size())
        {
            selectedSeasonId = m_seriesSeasons[selectedSeasonIndex].id;
            hasSelectedSeason = true;
        }
    }

    if (hasSelectedSeason)
    {
        if (persist)
        {
            m_cachedSeasonIndex = selectedSeasonIndex;
            m_cachedSeasonId = selectedSeasonId;
        }
        else
        {
            m_manualSelectedSeasonIndex = selectedSeasonIndex;
            m_manualSelectedSeasonId = selectedSeasonId;
        }
    }
    if (episode.type == "Episode" && !episode.id.isEmpty())
    {
        if (persist)
        {
            m_cachedSeriesPlayableItem = episode;
            m_hasCachedSeriesPlayableItem = true;
        }
        else
        {
            m_manualSelectedEpisodeId = episode.id;
        }
    }

    qDebug() << "[DetailView] Remember series selection"
             << "seriesId=" << m_currentItemId
             << "seasonIndex=" << (persist ? m_cachedSeasonIndex : m_manualSelectedSeasonIndex)
             << "seasonId=" << (persist ? m_cachedSeasonId : m_manualSelectedSeasonId)
             << "episodeId=" << (persist ? m_cachedSeriesPlayableItem.id : m_manualSelectedEpisodeId)
             << "persist=" << persist << "reason=" << reason;

    if (persist)
        persistDetailCacheSnapshot(reason);
}

void DetailView::updateOverviewElidedText()
{
    if (m_cleanOverviewText.isEmpty() || !m_overviewLabel)
        return;

    const int generation = ++m_overviewElideGeneration;
    QPointer<DetailView> safeThis(this);
    QTimer::singleShot(0, this,
                       [safeThis, generation]()
                       {
                           if (!safeThis || generation != safeThis->m_overviewElideGeneration)
                               return;

                           int labelWidth = safeThis->m_overviewLabel->contentsRect().width();
                           if (labelWidth <= 10 && safeThis->m_textContainer)
                               labelWidth = safeThis->m_textContainer->contentsRect().width();
                           if (labelWidth <= 10)
                               return;

                           QFontMetrics fm(safeThis->m_overviewLabel->font());
                           int maxHeight = fm.lineSpacing() * 5 + 10;
                           QTextDocument doc;
                           doc.setDefaultFont(safeThis->m_overviewLabel->font());
                           doc.setTextWidth(labelWidth);

                           QString fullHtml = safeThis->m_cleanOverviewText.toHtmlEscaped();
                           fullHtml.replace("\n", "<br>");
                           doc.setHtml(fullHtml);

                           if (doc.size().height() <= maxHeight)
                           {
                               if (safeThis->m_overviewLabel->text() != fullHtml)
                                   safeThis->m_overviewLabel->setText(fullHtml);
                               return;
                           }

                           int low = 0, high = safeThis->m_cleanOverviewText.length(), best = 0;
                           QString moreLink = safeThis->tr("... <a href='more' style='color:#3B82F6; "
                                                           "text-decoration:none; font-weight:bold;'>More</a>");
                           while (low <= high)
                           {
                               int mid = low + (high - low) / 2;
                               QString testHtml = safeThis->m_cleanOverviewText.left(mid).toHtmlEscaped();
                               testHtml.replace("\n", "<br>");
                               doc.setHtml(testHtml + moreLink);
                               if (doc.size().height() <= maxHeight)
                               {
                                   best = mid;
                                   low = mid + 1;
                               }
                               else
                               {
                                   high = mid - 1;
                               }
                           }

                           QString finalHtml = safeThis->m_cleanOverviewText.left(best).toHtmlEscaped();
                           finalHtml.replace("\n", "<br>");
                           const QString overviewHtml = finalHtml + moreLink;
                           if (safeThis->m_overviewLabel->text() != overviewHtml)
                               safeThis->m_overviewLabel->setText(overviewHtml);
                       });
}

QCoro::Task<void> DetailView::updateUi(MediaItem item, bool isSilentRefresh)
{
    m_currentMediaItem = item;
    if (!isSilentRefresh && item.type == "Series")
        m_deferBottomInfoReveal = true;
    m_titleLabel->setText(item.name);

    const QString metaEpisodeTag = item.type == "Series" && m_currentPlayableItem.type == "Episode"
                                       ? formatEpisodeTag(m_currentPlayableItem)
                                       : formatEpisodeTag(item);
    updateMetaRow(item, metaEpisodeTag);

    const bool shouldShowNumber = shouldShowDisplayNumber(item);
    const QString displayNumber = shouldShowNumber ? extractDisplayNumber(item) : QString();
    updateDisplayNumber(displayNumber);

    qDebug() << "[DetailView] updateUi itemId=" << item.id << "adultMovie=" << shouldShowNumber
             << "displayNumber=" << displayNumber;

    QString text = item.overview;
    text.replace(QRegularExpression("<br\\s*/?>", QRegularExpression::CaseInsensitiveOption), "\n");
    text.replace("</p>", "\n\n");
    text.remove(QRegularExpression("<[^>]*>"));
    QTextDocument doc;
    doc.setHtml(text);
    QString cleanText = doc.toPlainText();
    cleanText.replace(QChar::LineSeparator, '\n');
    cleanText.remove(QRegularExpression("<[^>]*>"));
    m_cleanOverviewText = cleanText;
    m_lastOverviewWidth = -1;
    updateOverviewElidedText();

    if (item.type == "Series")
    {
        const bool hasCurrentEpisode = m_currentPlayableItem.type == "Episode" && !m_currentPlayableItem.id.isEmpty();
        if (!isSilentRefresh && !hasCurrentEpisode)
            m_actionWidget->setSeriesLoadingMode();
    }
    else
    {
        
        
        m_currentPlayableItem = item;
        m_actionWidget->setupNormalMode(item);
    }

    
    
    const QString newSourcesFingerprint = computeSourcesFingerprint(item.mediaSources);
    const bool sourcesUnchanged =
        !m_appliedSourcesFingerprint.isEmpty() && newSourcesFingerprint == m_appliedSourcesFingerprint;
    const bool sourcesOwnedBySeriesEpisode =
        item.type == "Series" && m_currentPlayableItem.type == "Episode" && !m_currentPlayableItem.id.isEmpty();
    if (!sourcesOwnedBySeriesEpisode && !sourcesUnchanged)
    {
        m_actionWidget->setSources(item.mediaSources,
                                   preferredSourceIndex(m_core, item));
        m_appliedSourcesFingerprint = newSourcesFingerprint;
        if (item.mediaSources.isEmpty() || item.mediaSources.first().mediaStreams.isEmpty())
        {
            m_actionWidget->setStreams(MediaSourceInfo{});
        }
    }
    m_actionWidget->refreshExtPlayerButton();

    if (!item.taglines.isEmpty())
    {
        m_taglineLabel->setText(item.taglines.first());
        m_taglineLabel->show();
    }
    else
    {
        m_taglineLabel->hide();
    }

    m_isFavorite = item.isFavorite();
    m_actionWidget->setFavoriteState(m_isFavorite);
    m_actionWidget->setPlayedState(item.userData.played);

    
    
    buildTagButtons(item.genres);

    QCoro::connect(executeLoadImages(QPointer<DetailView>(this), m_core, item, true), this, []() {});

    QPointer<DetailView> safeThis(this);

    
    
    auto applyBottomInfo = [this, &item](const QList<MediaSourceInfo> &srcs)
    {
        const QString fp = computeBottomInfoFingerprint(item, srcs);
        if (fp == m_appliedBottomInfoFingerprint)
            return;
        m_bottomInfoWidget->setInfo(item, srcs);
        m_appliedBottomInfoFingerprint = fp;
        if (m_deferBottomInfoReveal)
        {
            showReserveWidget(m_bottomInfoReserveWidget, reserveHeightFor(m_bottomInfoWidget, 220));
        }
    };

    if (sourcesOwnedBySeriesEpisode)
    {
        applySeriesPlayableItemToUi(m_currentPlayableItem);
    }
    else if (!item.mediaSources.isEmpty() && !item.mediaSources.first().mediaStreams.isEmpty())
    {
        int defaultIndex = m_actionWidget->currentSourceIndex();
        QList<MediaSourceInfo> defaultSource;
        if (defaultIndex < item.mediaSources.size())
        {
            const MediaSourceInfo &dSrc = item.mediaSources[defaultIndex];
            
            
            if (!sourcesUnchanged)
                applyRememberedStreams(item, dSrc);
            defaultSource.append(dSrc);
        }
        applyBottomInfo(defaultSource);
    }
    else
    {
        
        
        applyBottomInfo({});
        if (item.mediaType == "Video" || item.mediaType == "Audio" || item.type == "Movie" || item.type == "Episode")
        {
            
            
            QCoro::connect(fetchAndApplyPlaybackInfo(item.id), this, []() {});
        }
    }

    if (!safeThis)
        co_return;
    safeThis->m_bottomInfoWidget->setVisible(!safeThis->m_deferBottomInfoReveal);
    if (safeThis->m_deferBottomInfoReveal)
    {
        showReserveWidget(safeThis->m_bottomInfoReserveWidget, reserveHeightFor(safeThis->m_bottomInfoWidget, 220));
    }
    else
    {
        hideReserveWidget(safeThis->m_bottomInfoReserveWidget);
    }
    if (!isSilentRefresh && safeThis)
        QMetaObject::invokeMethod(safeThis.data(), "scrollToTop", Qt::QueuedConnection);
}





QCoro::Task<void> DetailView::fetchAndApplyPlaybackInfo(QString itemId)
{
    QPointer<DetailView> safeThis(this);
    QPointer<MediaService> mediaService(
        m_core ? m_core->mediaService() : nullptr);
    try
    {
        if (!mediaService)
            co_return;
        PlaybackInfo info = co_await mediaService->getPlaybackInfo(itemId);
        if (!safeThis || safeThis->m_currentItemId != itemId)
            co_return;
        if (info.mediaSources.isEmpty())
            co_return;

        safeThis->m_currentMediaItem.mediaSources = info.mediaSources;

        const QString refreshedNumber = safeThis->shouldShowDisplayNumber(safeThis->m_currentMediaItem)
                                            ? safeThis->extractDisplayNumber(safeThis->m_currentMediaItem)
                                            : QString();
        safeThis->updateDisplayNumber(refreshedNumber);

        
        if (safeThis->m_currentMediaItem.type != "Series")
        {
            safeThis->m_currentPlayableItem.mediaSources = info.mediaSources;
        }

        int idx = MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
            info.mediaSources,
            ConfigStore::instance()->get<QString>(ConfigKeys::PlayerPreferredVersion).trimmed(),
            MediaSourcePreferenceUtils::rememberedMediaSourceId(
                activeServerId(safeThis->m_core), itemId));
        safeThis->m_actionWidget->setSources(info.mediaSources, idx);
        safeThis->m_appliedSourcesFingerprint = computeSourcesFingerprint(info.mediaSources);

        idx = safeThis->m_actionWidget->currentSourceIndex();
        if (idx >= safeThis->m_currentMediaItem.mediaSources.size())
            idx = 0;
        if (idx < safeThis->m_currentMediaItem.mediaSources.size())
        {
            const MediaSourceInfo &singleSource = safeThis->m_currentMediaItem.mediaSources[idx];
            safeThis->applyRememberedStreams(safeThis->m_currentMediaItem,
                                             singleSource);
            QList<MediaSourceInfo> sList;
            sList.append(singleSource);
            const QString fp = computeBottomInfoFingerprint(safeThis->m_currentMediaItem, sList);
            if (fp != safeThis->m_appliedBottomInfoFingerprint)
            {
                safeThis->m_bottomInfoWidget->setInfo(safeThis->m_currentMediaItem, sList);
                safeThis->m_appliedBottomInfoFingerprint = fp;
                if (safeThis->m_deferBottomInfoReveal)
                {
                    showReserveWidget(safeThis->m_bottomInfoReserveWidget,
                                      reserveHeightFor(safeThis->m_bottomInfoWidget, 220));
                }
            }
        }
    }
    catch (...)
    {
    }
}

void DetailView::onOverviewMoreClicked(const QString &link)
{
    if (link == "more")
    {
        OverviewDialog dialog(this);
        dialog.setMediaItem(m_currentMediaItem, m_currentPosterPix);
        dialog.exec();
    }
}

QCoro::Task<void> DetailView::executeSilentRefresh(QPointer<DetailView> safeThis, QEmbyCore *core, QString itemId)
{
    QString cacheServerId;
    QString cacheUserId;
    if (core && core->serverManager())
    {
        const ServerProfile profile = core->serverManager()->activeProfile();
        cacheServerId = profile.id;
        cacheUserId = profile.userId;
    }

    try
    {
        MediaItem item = co_await core->mediaService()->getItemDetail(itemId);
        if (!cacheServerId.isEmpty() && !cacheUserId.isEmpty())
        {
            auto fingerprintFuture = QtConcurrent::run(
                [item]() mutable { return DetailCacheUtils::fingerprint(item); });
            const QString fp = co_await fingerprintFuture;
            saveDetailCacheAsync(cacheServerId, cacheUserId, item, fp);
            if (safeThis && item.id == safeThis->m_currentItemId)
                safeThis->m_cachedDetailFingerprint = fp;
        }
        if (safeThis && item.id == safeThis->m_currentItemId)
        {
            co_await safeThis->updateUi(item, true);

            
            if (item.type == "Series")
            {
                co_await safeThis->fetchSeriesNextUp(itemId, !safeThis->m_hasManualSeriesSelection);
                if (!safeThis || safeThis->m_currentItemId != itemId)
                    co_return;

                auto seasons = co_await core->mediaService()->getSeasons(itemId);
                if (!safeThis || safeThis->m_currentItemId != itemId)
                    co_return;

                safeThis->m_seriesSeasons = seasons;
                safeThis->m_cachedSeriesSeasons = seasons;
                safeThis->m_hasCachedSeriesSeasons = true;

                
                
                bool selectedSeasonResolved = false;
                if (safeThis->m_hasManualSeriesSelection && !safeThis->m_manualSelectedSeasonId.isEmpty())
                {
                    for (int i = 0; i < seasons.size(); ++i)
                    {
                        if (seasons[i].id == safeThis->m_manualSelectedSeasonId)
                        {
                            safeThis->m_currentSeasonIndex = i;
                            selectedSeasonResolved = true;
                            break;
                        }
                    }
                }
                if (!selectedSeasonResolved && safeThis->m_hasManualSeriesSelection &&
                    safeThis->m_manualSelectedSeasonIndex >= 0 &&
                    safeThis->m_manualSelectedSeasonIndex < seasons.size())
                {
                    safeThis->m_currentSeasonIndex = safeThis->m_manualSelectedSeasonIndex;
                    selectedSeasonResolved = true;
                }
                if (!selectedSeasonResolved && safeThis->m_hasManualSeriesSelection &&
                    safeThis->m_currentSeasonIndex >= 0 && safeThis->m_currentSeasonIndex < seasons.size())
                {
                    selectedSeasonResolved = true;
                }
                int playableParentIdx = safeThis->m_currentPlayableItem.parentIndexNumber;
                if (!selectedSeasonResolved && !safeThis->m_hasManualSeriesSelection && playableParentIdx >= 0)
                {
                    for (int i = 0; i < seasons.size(); ++i)
                    {
                        if (seasons[i].indexNumber == playableParentIdx)
                        {
                            safeThis->m_currentSeasonIndex = i;
                            break;
                        }
                    }
                }

                if (!seasons.isEmpty() && safeThis->m_episodeWidget)
                {
                    const int idx = qBound(0, safeThis->m_currentSeasonIndex, seasons.size() - 1);
                    safeThis->updateSeasonSwitcher(idx);

                    const QString playableItemId =
                        safeThis->m_hasManualSeriesSelection && !safeThis->m_manualSelectedEpisodeId.isEmpty()
                            ? safeThis->m_manualSelectedEpisodeId
                            : safeThis->m_currentPlayableItem.id;
                    co_await safeThis->loadEpisodesForSeason(idx, playableItemId, !playableItemId.isEmpty(),
                                                             safeThis->m_hasManualSeriesSelection);
                }
                else if (safeThis->m_episodeWidget)
                {
                    applyReservedSectionItems(safeThis->m_episodeSectionReserveWidget, safeThis->m_episodeWidget, {});
                }
                if (!safeThis || safeThis->m_currentItemId != itemId)
                    co_return;

                if (safeThis->m_seasonWidget)
                {
                    safeThis->m_seasonWidget->setTitle(tr("Seasons"));
                    safeThis->m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
                    safeThis->m_seasonWidget->setGalleryHeight(300);
                    applyReservedSectionItems(safeThis->m_seasonSectionReserveWidget, safeThis->m_seasonWidget,
                                              seasons);
                }
            }

            safeThis->applyCastSection(item);
            if (safeThis->m_deferBottomInfoReveal)
            {
                safeThis->m_deferBottomInfoReveal = false;
                hideReserveWidget(safeThis->m_bottomInfoReserveWidget);
                if (safeThis->m_bottomInfoWidget)
                    safeThis->m_bottomInfoWidget->show();
            }
            safeThis->persistDetailCacheSnapshot(QStringLiteral("silent-refresh"));
        }
    }
    catch (...)
    {
    }
}

QCoro::Task<void> DetailView::executeLoadImages(QPointer<DetailView> safeThis, QEmbyCore *core, MediaItem item,
                                                bool allowPrimaryBackdropFallback)
{
    bool adaptive = ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages, true);
    QString cacheServerId;
    QString cacheUserId;
    if (safeThis)
    {
        cacheServerId = safeThis->m_detailCacheServerId;
        cacheUserId = safeThis->m_detailCacheUserId;
    }
    else if (core && core->serverManager())
    {
        const ServerProfile profile = core->serverManager()->activeProfile();
        cacheServerId = profile.id;
        cacheUserId = profile.userId;
    }

    
    
    QString posterType = QStringLiteral("Primary");
    QString posterTag = item.images.primaryTag;
    QString posterId = item.id;
    if (posterTag.isEmpty() && adaptive)
    {
        auto best = item.images.bestPoster();
        posterTag = best.first;
        posterType = best.second;
        if (item.images.isParentTag(posterTag))
        {
            posterId = item.images.parentImageItemId.isEmpty() ? item.seriesId : item.images.parentImageItemId;
        }
    }
    const int posterMaxWidth = posterType == QStringLiteral("Primary") ? 400 : 800;

    QString backdropType = QStringLiteral("Backdrop");
    QString backdropTag = item.images.backdropTag;
    QString backdropId = item.id;
    if (backdropTag.isEmpty() && adaptive)
    {
        if (allowPrimaryBackdropFallback)
        {
            auto best = item.images.bestBackdrop();
            backdropTag = best.first;
            backdropType = best.second;
        }
        else
        {
            if (!item.images.thumbTag.isEmpty())
            {
                backdropTag = item.images.thumbTag;
                backdropType = QStringLiteral("Thumb");
            }
            else if (!item.images.parentBackdropTag.isEmpty())
            {
                backdropTag = item.images.parentBackdropTag;
                backdropType = QStringLiteral("Backdrop");
            }
            else if (!item.images.parentThumbTag.isEmpty())
            {
                backdropTag = item.images.parentThumbTag;
                backdropType = QStringLiteral("Thumb");
            }
        }
        if (item.images.isParentTag(backdropTag))
        {
            backdropId = item.images.parentImageItemId.isEmpty() ? item.seriesId : item.images.parentImageItemId;
        }
    }

    const QString logoId = item.id;
    const QString logoType = QStringLiteral("Logo");
    const QString logoTag = item.images.logoTag;
    constexpr int logoMaxWidth = 400;

    auto cachedImagesFuture = QtConcurrent::run(
        [cacheServerId, cacheUserId, ownerItemId = item.id, posterId, posterType, posterTag, posterMaxWidth, backdropId,
         backdropType, backdropTag, logoId, logoType, logoTag, logoMaxWidth]() mutable
        {
            CachedDetailImages result;
            if (!posterTag.isEmpty())
            {
                result.poster =
                    DetailCacheUtils::loadImage(cacheServerId, cacheUserId, ownerItemId, QStringLiteral("poster"),
                                                posterId, posterType, posterTag, posterMaxWidth);
            }
            if (!backdropTag.isEmpty())
            {
                result.backdrop =
                    DetailCacheUtils::loadImage(cacheServerId, cacheUserId, ownerItemId, QStringLiteral("backdrop"),
                                                backdropId, backdropType, backdropTag, 1920);
            }
            if (!logoTag.isEmpty())
            {
                result.logo =
                    DetailCacheUtils::loadImage(cacheServerId, cacheUserId, ownerItemId, QStringLiteral("logo"), logoId,
                                                logoType, logoTag, logoMaxWidth);
            }
            return result;
        });
    const CachedDetailImages cachedImages = co_await cachedImagesFuture;
    if (!safeThis || safeThis->m_currentItemId != item.id)
        co_return;

    if (cachedImages.poster.has_value())
    {
        safeThis->m_currentPosterPix = QPixmap::fromImage(cachedImages.poster.value());
        safeThis->m_posterLabel->setPixmap(safeThis->m_currentPosterPix);
        qDebug() << "[DetailView] Detail poster image cache hit"
                 << "itemId=" << item.id << "imageId=" << posterId << "type=" << posterType;
    }

    if (cachedImages.backdrop.has_value())
    {
        safeThis->m_currentBackdropPix = QPixmap::fromImage(cachedImages.backdrop.value());
        safeThis->updateBackdrop();
        qDebug() << "[DetailView] Detail backdrop image cache hit"
                 << "itemId=" << item.id << "imageId=" << backdropId << "type=" << backdropType;
    }

    if (cachedImages.logo.has_value())
    {
        safeThis->m_logoLabel->setPixmap(QPixmap::fromImage(cachedImages.logo.value()));
        safeThis->m_logoLabel->show();
        qDebug() << "[DetailView] Detail logo image cache hit"
                 << "itemId=" << item.id;
    }

    
    if (!posterTag.isEmpty())
    {
        try
        {
            QPixmap pix = co_await core->mediaService()->fetchImage(posterId, posterType, posterTag, posterMaxWidth);
            if (safeThis && safeThis->m_currentItemId == item.id && !pix.isNull())
            {
                safeThis->m_currentPosterPix = pix;

                
                
                
                const QSize posterSize(250, 375);
                const QString currentItemId = item.id;
                QImage src = pix.toImage();

                auto *watcher = new QFutureWatcher<QImage>(safeThis.data());
                QObject::connect(watcher, &QFutureWatcher<QImage>::finished, safeThis.data(),
                                 [safeThis, watcher, currentItemId, cacheServerId, cacheUserId, posterId, posterType,
                                  posterTag, posterMaxWidth]()
                                 {
                                     if (safeThis && safeThis->m_currentItemId == currentItemId)
                                     {
                                         const QImage result = watcher->result();
                                         if (!result.isNull())
                                         {
                                             safeThis->m_posterLabel->setPixmap(QPixmap::fromImage(result));
                                             saveDetailImageAsync(cacheServerId, cacheUserId, currentItemId,
                                                                  QStringLiteral("poster"), posterId, posterType,
                                                                  posterTag, posterMaxWidth, result);
                                         }
                                     }
                                     watcher->deleteLater();
                                 });
                watcher->setFuture(QtConcurrent::run(
                    [src, posterSize]() -> QImage
                    {
                        QImage scaled =
                            src.scaled(posterSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                        const int cropX = (scaled.width() - posterSize.width()) / 2;
                        const int cropY = (scaled.height() - posterSize.height()) / 2;
                        QImage cropped = scaled.copy(cropX, cropY, posterSize.width(), posterSize.height());

                        
                        QImage rounded(posterSize, QImage::Format_ARGB32_Premultiplied);
                        rounded.fill(Qt::transparent);
                        QPainter p(&rounded);
                        p.setRenderHint(QPainter::Antialiasing);
                        QPainterPath path;
                        path.addRoundedRect(QRectF(0, 0, posterSize.width(), posterSize.height()), 12, 12);
                        p.setClipPath(path);
                        p.drawImage(0, 0, cropped);
                        p.end();
                        return rounded;
                    }));
            }
        }
        catch (...)
        {
        }
    }

    
    if (!backdropTag.isEmpty())
    {
        try
        {
            QPixmap pix = co_await core->mediaService()->fetchImage(backdropId, backdropType, backdropTag, 1920);
            if (safeThis && safeThis->m_currentItemId == item.id && !pix.isNull())
            {
                safeThis->m_currentBackdropPix = pix;
                safeThis->updateBackdrop();
                saveDetailImageAsync(cacheServerId, cacheUserId, item.id, QStringLiteral("backdrop"), backdropId,
                                     backdropType, backdropTag, 1920, pix.toImage());
            }
        }
        catch (...)
        {
        }
    }

    
    
    if (!logoTag.isEmpty())
    {
        try
        {
            QPixmap pix = co_await core->mediaService()->fetchImage(logoId, logoType, logoTag, logoMaxWidth);
            if (safeThis && safeThis->m_currentItemId == item.id && !pix.isNull())
            {
                const QString currentItemId = item.id;
                QImage src = pix.toImage();

                auto *watcher = new QFutureWatcher<QImage>(safeThis.data());
                QObject::connect(watcher, &QFutureWatcher<QImage>::finished, safeThis.data(),
                                 [safeThis, watcher, currentItemId, cacheServerId, cacheUserId, logoId, logoType,
                                  logoTag, logoMaxWidth]()
                                 {
                                     if (safeThis && safeThis->m_currentItemId == currentItemId)
                                     {
                                         const QImage result = watcher->result();
                                         if (!result.isNull())
                                         {
                                             safeThis->m_logoLabel->setPixmap(QPixmap::fromImage(result));
                                             safeThis->m_logoLabel->show();
                                             saveDetailImageAsync(cacheServerId, cacheUserId, currentItemId,
                                                                  QStringLiteral("logo"), logoId, logoType, logoTag,
                                                                  logoMaxWidth, result);
                                         }
                                     }
                                     watcher->deleteLater();
                                 });
                watcher->setFuture(QtConcurrent::run(
                    [src]() -> QImage { return src.scaled(250, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation); }));
            }
        }
        catch (...)
        {
        }
    }
}

void DetailView::onMediaItemUpdated(const MediaItem &item)
{
    
    if (m_currentItemId == item.id && m_core && !m_skipSilentRefresh)
    {
        QCoro::connect(executeSilentRefresh(QPointer<DetailView>(this), m_core, item.id), this, []() {});
    }

    
    if (item.type == "Season" && !m_currentSeasonEpisodes.isEmpty())
    {
        bool belongsToSeries = false;
        for (const MediaItem &season : m_seriesSeasons)
        {
            if (season.id == item.id)
            {
                belongsToSeries = true;
                break;
            }
        }
        if (belongsToSeries)
        {
            for (MediaItem &ep : m_currentSeasonEpisodes)
            {
                if (ep.parentIndexNumber == item.indexNumber)
                    ep.userData.played = item.userData.played;
            }
            m_episodeWidget->setItems(m_currentSeasonEpisodes);
        }
    }

    
    if (item.type == "Episode" && !m_currentSeasonEpisodes.isEmpty() && !m_seriesSeasons.isEmpty())
    {
        
        for (MediaItem &ep : m_currentSeasonEpisodes)
        {
            if (ep.id == item.id)
            {
                ep.userData.played = item.userData.played;
                break;
            }
        }
        
        const int seasonIndex = item.parentIndexNumber;
        if (seasonIndex >= 0)
        {
            for (MediaItem &season : m_seriesSeasons)
            {
                if (season.indexNumber == seasonIndex)
                {
                    int total = 0, played = 0;
                    for (const MediaItem &ep : m_currentSeasonEpisodes)
                    {
                        if (ep.parentIndexNumber == seasonIndex)
                        {
                            total++;
                            if (ep.userData.played)
                                played++;
                        }
                    }
                    const bool allPlayed = (total > 0 && played == total);
                    if (season.userData.played != allPlayed)
                    {
                        season.userData.played = allPlayed;
                        m_seasonWidget->updateItem(season);
                    }
                    break;
                }
            }
        }
    }

    
    if (m_currentPlayableItem.id == item.id)
    {
        m_currentPlayableItem.userData.played = item.userData.played;
        m_actionWidget->setPlayedState(item.userData.played);
    }
    if (m_currentMediaItem.id == item.id)
    {
        m_currentMediaItem.userData.played = item.userData.played;
        m_actionWidget->setPlayedState(item.userData.played);
    }

    if (m_seasonWidget)
        m_seasonWidget->updateItem(item);
    if (m_episodeWidget)
        m_episodeWidget->updateItem(item);
    if (m_castWidget)
        m_castWidget->updateItem(item);
    if (m_additionalPartsWidget)
        m_additionalPartsWidget->updateItem(item);
    if (m_collectionWidget)
        m_collectionWidget->updateItem(item);
    if (m_similarWidget)
        m_similarWidget->updateItem(item);
}

void DetailView::beginOptimisticPlayedUpdate()
{
    m_skipSilentRefresh = true;
}

void DetailView::endOptimisticPlayedUpdate()
{
    m_skipSilentRefresh = false;
}

void DetailView::updateSeasonSwitcher(int currentIndex)
{
    if (!m_seasonSwitcher)
        return;

    QSignalBlocker blocker(m_seasonSwitcher);
    m_seasonSwitcher->clear();

    for (const MediaItem &season : m_seriesSeasons)
        m_seasonSwitcher->addItem(season.name, "");

    if (!m_seriesSeasons.isEmpty())
    {
        const int idx = qBound(0, currentIndex, m_seriesSeasons.size() - 1);
        m_seasonSwitcher->setCurrentIndex(idx);
    }

    m_seasonSwitcher->setVisible(m_seriesSeasons.size() > 1);
    m_seasonSwitcher->adjustSize();
    if (m_episodeJumpEdit)
    {
        const int switcherHeight = qMax(m_seasonSwitcher->height(), m_seasonSwitcher->sizeHint().height());
        m_episodeJumpEdit->setFixedHeight(switcherHeight);
    }
    updateEpisodeHeaderControlsVisibility();
}

void DetailView::updateEpisodeHeaderControlsVisibility()
{
    if (!m_episodeHeaderControls)
        return;

    const bool showSeasonSwitcher = m_seasonSwitcher && !m_seasonSwitcher->isHidden();
    const bool showEpisodeJump = m_episodeJumpEdit && !m_episodeJumpEdit->isHidden();
    m_episodeHeaderControls->setVisible(showSeasonSwitcher || showEpisodeJump);
    m_episodeHeaderControls->adjustSize();
    m_episodeHeaderControls->updateGeometry();
}

void DetailView::updateEpisodeJumpControl(int episodeCount)
{
    if (!m_episodeJumpEdit)
    {
        return;
    }

    const int visibleCardCapacity = episodeGalleryVisibleCardCapacity();
    const bool showJump = episodeCount > 0 && visibleCardCapacity > 0 && episodeCount > visibleCardCapacity;
    if (showJump)
    {
        int explicitMaxEpisodeNumber = 0;
        for (const MediaItem &episode : m_currentSeasonEpisodes)
            explicitMaxEpisodeNumber = qMax(explicitMaxEpisodeNumber, episode.indexNumber);

        const int maxEpisodeNumber = explicitMaxEpisodeNumber > 0 ? explicitMaxEpisodeNumber : episodeCount;
        const QString rangeText = tr("1-%1").arg(maxEpisodeNumber);
        m_episodeJumpEdit->setPlaceholderText(rangeText);
        m_episodeJumpEdit->setToolTip(tr("Jump to episode number (1-%1)").arg(maxEpisodeNumber));
        if (m_episodeJumpValidator)
            m_episodeJumpValidator->setTop(maxEpisodeNumber);
        m_episodeJumpEdit->setMaxLength(qMax(1, QString::number(maxEpisodeNumber).length()));

        const int textWidth = m_episodeJumpEdit->fontMetrics().horizontalAdvance(rangeText);
        m_episodeJumpEdit->setFixedWidth(qBound(64, textWidth + 22, 92));
    }
    else
    {
        m_episodeJumpEdit->clear();
        m_episodeJumpEdit->setPlaceholderText(tr("Ep #"));
        m_episodeJumpEdit->setToolTip(tr("Jump to episode number"));
        if (m_episodeJumpValidator)
            m_episodeJumpValidator->setTop(9999);
        m_episodeJumpEdit->setMaxLength(4);
        m_episodeJumpEdit->setFixedWidth(64);
    }

    m_episodeJumpEdit->setVisible(showJump);
    updateEpisodeHeaderControlsVisibility();
}

int DetailView::episodeGalleryVisibleCardCapacity() const
{
    if (!m_episodeWidget || !m_episodeWidget->gallery())
        return 0;

    QListView *listView = m_episodeWidget->gallery()->listView();
    if (!listView || !listView->viewport())
        return 0;

    int cardWidth = m_episodeTileWidth;
    if (cardWidth <= 0 && listView->model() && listView->model()->rowCount() > 0)
    {
        const QSize itemSize = listView->sizeHintForIndex(listView->model()->index(0, 0));
        cardWidth = itemSize.width();
    }
    if (cardWidth <= 0)
        return 0;

    const int viewportWidth = listView->viewport()->width();
    if (viewportWidth <= 0)
        return 0;

    return qMax(1, viewportWidth / cardWidth);
}

MediaItem DetailView::episodeForJumpNumber(int episodeNumber) const
{
    if (episodeNumber <= 0)
        return MediaItem{};

    bool hasExplicitEpisodeNumbers = false;
    for (const MediaItem &episode : m_currentSeasonEpisodes)
    {
        if (episode.indexNumber > 0)
            hasExplicitEpisodeNumbers = true;
        if (episode.indexNumber == episodeNumber)
            return episode;
    }

    if (hasExplicitEpisodeNumbers)
        return MediaItem{};

    const int row = episodeNumber - 1;
    if (row >= 0 && row < m_currentSeasonEpisodes.size())
        return m_currentSeasonEpisodes.at(row);

    return MediaItem{};
}

void DetailView::submitEpisodeJump()
{
    if (!m_episodeJumpEdit || m_episodeJumpEdit->isHidden() || m_currentSeasonEpisodes.isEmpty())
    {
        return;
    }

    const QString text = m_episodeJumpEdit->text().trimmed();
    if (text.isEmpty())
        return;

    bool ok = false;
    const int episodeNumber = text.toInt(&ok);
    if (!ok)
        return;

    const MediaItem targetEpisode = episodeForJumpNumber(episodeNumber);
    if (targetEpisode.id.isEmpty())
    {
        qDebug() << "[DetailView] Episode jump target not found"
                 << "seriesId=" << m_currentItemId << "seasonIndex=" << m_currentSeasonIndex
                 << "episodeNumber=" << episodeNumber << "episodeCount=" << m_currentSeasonEpisodes.size();
        ModernToast::showMessage(tr("Episode %1 not found").arg(episodeNumber), 1600);
        m_episodeJumpEdit->selectAll();
        return;
    }

    qDebug() << "[DetailView] Episode jump"
             << "seriesId=" << m_currentItemId << "seasonIndex=" << m_currentSeasonIndex
             << "episodeNumber=" << episodeNumber << "episodeId=" << targetEpisode.id;

    m_episodeJumpEdit->clear();
    markManualSeriesSelection(QStringLiteral("episode-jump"));
    QCoro::connect(applySeriesPlayableItem(targetEpisode, true), this, []() {});
}

QCoro::Task<void> DetailView::loadEpisodesForSeason(int idx, QString highlightEpisodeId, bool scrollToHighlight,
                                                    bool manualSelection)
{
    if (idx < 0 || idx >= m_seriesSeasons.size() || !m_episodeWidget)
        co_return;

    m_currentSeasonIndex = idx;
    const QString seasonId = m_seriesSeasons[idx].id;
    if (seasonId.isEmpty())
        co_return;
    if (manualSelection)
        markManualSeriesSelection(QStringLiteral("season-selection"));
    rememberSeriesSelection(MediaItem{}, QStringLiteral("season-selection"), !manualSelection);

    bool showedCachedEpisodes = false;
    QList<MediaItem> shownCachedEpisodes;
    if (m_hasCachedSeasonEpisodes && m_cachedSeasonIndex == idx && m_cachedSeasonId == seasonId &&
        episodesBelongToSeason(m_cachedSeasonEpisodes, m_seriesSeasons[idx]))
    {
        shownCachedEpisodes = m_cachedSeasonEpisodes;
        if (!m_appliedCachedSeasonEpisodesToUi)
        {
            m_currentSeasonEpisodes = shownCachedEpisodes;
            applyReservedSectionItems(m_episodeSectionReserveWidget, m_episodeWidget, shownCachedEpisodes);
            updateEpisodeJumpControl(shownCachedEpisodes.size());
            if (!highlightEpisodeId.isEmpty())
            {
                m_episodeWidget->gallery()->setHighlightedItemId(highlightEpisodeId);
                if (scrollToHighlight)
                {
                    m_episodeWidget->gallery()->scrollToItemId(highlightEpisodeId);
                }
            }
            m_appliedCachedSeasonEpisodesToUi = true;
        }
        showedCachedEpisodes = true;
        qDebug() << "[DetailView][cache-ui] Applied cached season episodes"
                 << "seriesId=" << m_currentItemId << "seasonId=" << seasonId << "seasonIndex=" << idx
                 << "episodeCount=" << shownCachedEpisodes.size();
    }
    else
    {
        m_currentSeasonEpisodes.clear();
        updateEpisodeJumpControl(0);
    }

    if (!m_core || !m_core->mediaService())
        co_return;

    QPointer<DetailView> safeThis(this);
    const QString seriesId = m_currentItemId;
    QEmbyCore *core = m_core;

    try
    {
        qDebug() << "[DetailView][network] Fetch season episodes"
                 << "seriesId=" << seriesId << "seasonId=" << seasonId << "seasonIndex=" << idx;
        QList<MediaItem> episodes = co_await core->mediaService()->getEpisodes(seriesId, seasonId);

        if (!safeThis || safeThis->m_currentItemId != seriesId || safeThis->m_currentSeasonIndex != idx)
            co_return;

        bool episodesUnchanged = false;
        if (showedCachedEpisodes)
        {
            episodesUnchanged = co_await mediaItemListsEqualAsync(shownCachedEpisodes, episodes);
            if (!safeThis || safeThis->m_currentItemId != seriesId || safeThis->m_currentSeasonIndex != idx)
                co_return;
        }

        safeThis->m_currentSeasonEpisodes = episodes;
        if (!safeThis->m_hasManualSeriesSelection)
        {
            safeThis->m_cachedSeasonEpisodes = episodes;
            safeThis->m_cachedSeasonIndex = idx;
            safeThis->m_cachedSeasonId = seasonId;
            safeThis->m_hasCachedSeasonEpisodes = true;
        }
        else
        {
            qDebug() << "[DetailView] Skip persisting manually selected season "
                        "episodes"
                     << "seriesId=" << seriesId << "seasonId=" << seasonId << "seasonIndex=" << idx;
        }
        if (!manualSelection && !highlightEpisodeId.isEmpty() &&
            safeThis->m_currentPlayableItem.id != highlightEpisodeId)
        {
            for (const MediaItem &episode : episodes)
            {
                if (episode.id == highlightEpisodeId)
                {
                    safeThis->applySeriesPlayableItemToUi(episode);
                    break;
                }
            }
        }
        if (!episodesUnchanged)
        {
            applyReservedSectionItems(safeThis->m_episodeSectionReserveWidget, safeThis->m_episodeWidget, episodes);
        }
        safeThis->updateEpisodeJumpControl(episodes.size());

        qDebug() << "[DetailView][network] Loaded season episodes"
                 << "seriesId=" << seriesId << "seasonId=" << seasonId << "seasonIndex=" << idx
                 << "episodeCount=" << episodes.size() << "fromCache=" << showedCachedEpisodes
                 << "unchanged=" << episodesUnchanged
                 << "visibleCardCapacity=" << safeThis->episodeGalleryVisibleCardCapacity()
                 << "jumpVisible=" << (safeThis->m_episodeJumpEdit && !safeThis->m_episodeJumpEdit->isHidden());

        if (!highlightEpisodeId.isEmpty())
        {
            safeThis->m_episodeWidget->gallery()->setHighlightedItemId(highlightEpisodeId);
            if (scrollToHighlight)
            {
                safeThis->m_episodeWidget->gallery()->scrollToItemId(highlightEpisodeId);
            }
        }
        safeThis->persistDetailCacheSnapshot(QStringLiteral("episodes"));
    }
    catch (...)
    {
        if (safeThis && safeThis->m_currentItemId == seriesId && safeThis->m_currentSeasonIndex == idx)
        {
            if (!showedCachedEpisodes)
            {
                safeThis->m_currentSeasonEpisodes.clear();
                safeThis->updateEpisodeJumpControl(0);
                applyReservedSectionItems(safeThis->m_episodeSectionReserveWidget, safeThis->m_episodeWidget, {});
            }
            qDebug() << "[DetailView] Failed to load season episodes"
                     << "seriesId=" << seriesId << "seasonId=" << seasonId << "seasonIndex=" << idx
                     << "keptCache=" << showedCachedEpisodes;
        }
    }
}


QCoro::Task<void> DetailView::switchToSeason(int idx, bool manualSelection)
{
    co_await loadEpisodesForSeason(idx, QString(), false, manualSelection);
}
