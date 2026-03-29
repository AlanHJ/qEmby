#include "seasonview.h"
#include "../../components/detailcontentwidget.h"
#include "../../components/mediagridwidget.h"
#include "../../components/modernsortbutton.h"
#include "../../components/splitplayerbutton.h"
#include "../../managers/externalplayerdetector.h"
#include "../../managers/playbackmanager.h"
#include "../../managers/thememanager.h" 
#include "mediacarddelegate.h"
#include <QFileInfo>
#include <QFont>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSet>
#include <QStyle>
#include <QVBoxLayout>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <qembycore.h>
#include <services/media/mediaservice.h>

SeasonView::SeasonView(QEmbyCore *core, QWidget *parent) : BaseView(core, parent)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("season-view");
    setProperty("showGlobalSearch", true);

    setupUi();
}

void SeasonView::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    m_contentWidget = new DetailContentWidget(this);
    m_contentWidget->setObjectName("season-content-container");

    auto *contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 40, 0, 0);
    contentLayout->setSpacing(0);

    
    
    
    auto *headerLayout = new QHBoxLayout();
    headerLayout->setSpacing(30);
    headerLayout->setContentsMargins(40, 0, 40, 0);
    headerLayout->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    
    m_posterLabel = new QLabel(m_contentWidget);
    m_posterLabel->setFixedSize(250, 375);
    m_posterLabel->setObjectName("detail-poster-label");
    m_posterLabel->setAlignment(Qt::AlignCenter);

    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(20);
    shadow->setColor(QColor(0, 0, 0, 80));
    shadow->setOffset(0, 8);
    m_posterLabel->setGraphicsEffect(shadow);

    
    auto *textLayout = new QVBoxLayout();
    textLayout->setSpacing(10);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setAlignment(Qt::AlignTop);

    m_seriesTitleLabel = new QLabel(m_contentWidget);
    m_seriesTitleLabel->setObjectName("detail-title");
    m_seriesTitleLabel->setWordWrap(true);

    m_titleLabel = new QLabel(m_contentWidget);
    m_titleLabel->setObjectName("detail-series-title");
    m_titleLabel->setWordWrap(true);

    m_overviewLabel = new QLabel(m_contentWidget);
    m_overviewLabel->setObjectName("detail-overview");
    m_overviewLabel->setWordWrap(true);
    m_overviewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    m_overviewLabel->setContentsMargins(0, 5, 0, 15);

    auto *actionsLayout = new QHBoxLayout();
    actionsLayout->setSpacing(12);
    actionsLayout->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    m_playBtn = new QPushButton(tr("▶ Play"), m_contentWidget);
    m_playBtn->setObjectName("detail-play-btn");
    m_playBtn->setCursor(Qt::PointingHandCursor);
    m_playBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    m_favBtn = new QPushButton(m_contentWidget);
    m_favBtn->setObjectName("detail-fav-btn");
    m_favBtn->setIconSize(QSize(20, 20));
    m_favBtn->setFixedSize(36, 36);
    m_favBtn->setCursor(Qt::PointingHandCursor);

    
    m_extPlayerBtn = new SplitPlayerButton(m_contentWidget);
    m_extPlayerBtn->setObjectName("detail-ext-player-btn");
    m_extPlayerBtn->setIconOnly(true);
    m_extPlayerBtn->hide();

    actionsLayout->addWidget(m_playBtn);
    actionsLayout->addWidget(m_extPlayerBtn);
    actionsLayout->addWidget(m_favBtn);

    textLayout->addWidget(m_seriesTitleLabel);
    textLayout->addWidget(m_titleLabel);
    textLayout->addWidget(m_overviewLabel);
    textLayout->addLayout(actionsLayout);

    headerLayout->addWidget(m_posterLabel, 0, Qt::AlignTop);
    headerLayout->addLayout(textLayout, 1);

    
    
    
    auto *filterLayout = new QHBoxLayout();
    filterLayout->setContentsMargins(40, 0, 40, 0);

    m_sortButton = new ModernSortButton(this);
    m_sortButton->setSortItems({tr("Episode Number"), tr("Title"), tr("Premiere Date"), tr("Runtime")});
    m_statsLabel = new QLabel(tr("0 Episodes"), this);
    m_statsLabel->setObjectName("library-stats-label");

    filterLayout->addStretch();
    filterLayout->addWidget(m_statsLabel);
    filterLayout->addSpacing(15);
    filterLayout->addWidget(m_sortButton);

    
    
    
    m_mediaGrid = new MediaGridWidget(m_core, m_contentWidget);
    m_mediaGrid->setCardStyle(MediaCardDelegate::EpisodeList);
    m_mediaGrid->setBasePadding(40);

    contentLayout->addLayout(headerLayout);
    contentLayout->addSpacing(0);
    contentLayout->addLayout(filterLayout);
    contentLayout->addWidget(m_mediaGrid, 1);

    mainLayout->addWidget(m_contentWidget);

    connect(m_playBtn, &QPushButton::clicked, this,
            [this]() -> QCoro::Task<void>
            {
                if (m_currentEpisodes.isEmpty())
                    co_return;

                
                if (!m_nextUpEpisodeId.isEmpty())
                {
                    for (const auto &ep : m_currentEpisodes)
                    {
                        if (ep.id == m_nextUpEpisodeId)
                        {
                            co_await executeInternalPlay(ep);
                            co_return;
                        }
                    }
                }

                
                co_await executeInternalPlay(m_currentEpisodes.first());
            });

    
    connect(m_extPlayerBtn, &SplitPlayerButton::playRequested, this,
            [this](const QString &playerPath) -> QCoro::Task<void>
            {
                if (m_currentEpisodes.isEmpty())
                    co_return;
                
                MediaItem target;
                if (!m_nextUpEpisodeId.isEmpty())
                {
                    for (const auto &ep : m_currentEpisodes)
                    {
                        if (ep.id == m_nextUpEpisodeId)
                        {
                            target = ep;
                            break;
                        }
                    }
                }
                if (target.id.isEmpty())
                    target = m_currentEpisodes.first();
                co_await executeExternalPlay(target, playerPath);
            });
    connect(m_extPlayerBtn, &SplitPlayerButton::playerSelected, this,
            [this](const QString &playerPath) -> QCoro::Task<void>
            {
                if (m_currentEpisodes.isEmpty())
                    co_return;
                MediaItem target;
                if (!m_nextUpEpisodeId.isEmpty())
                {
                    for (const auto &ep : m_currentEpisodes)
                    {
                        if (ep.id == m_nextUpEpisodeId)
                        {
                            target = ep;
                            break;
                        }
                    }
                }
                if (target.id.isEmpty())
                    target = m_currentEpisodes.first();
                co_await executeExternalPlay(target, playerPath);
            });

    connect(m_favBtn, &QPushButton::clicked, this,
            [this]()
            {
                if (!m_currentSeasonId.isEmpty())
                {
                    handleFavoriteRequested(m_currentSeasonItem);
                }
            });

    connect(m_sortButton, &ModernSortButton::sortChanged, this, [this]() { onFilterChanged(); });

    connect(m_mediaGrid, &MediaGridWidget::itemClicked, this,
            [this](const MediaItem &item) { Q_EMIT navigateToDetail(item.id, item.name); });
    connect(m_mediaGrid, &MediaGridWidget::playRequested, this, &BaseView::handlePlayRequested);
    connect(m_mediaGrid, &MediaGridWidget::favoriteRequested, this, &BaseView::handleFavoriteRequested);
    connect(m_mediaGrid, &MediaGridWidget::moreMenuRequested, this, &BaseView::handleMoreMenuRequested);
}

void SeasonView::updateFavBtnState()
{
    m_favBtn->setProperty("isFavorite", m_isFavorite);

    
    bool isDark = ThemeManager::instance()->isDarkMode();
    QString themeFolder = isDark ? "dark" : "light";
    QString iconState = m_isFavorite ? "fill" : "outline";

    m_favBtn->setIcon(QIcon(QString(":/svg/%1/heart-%2.svg").arg(themeFolder).arg(iconState)));

    m_favBtn->style()->unpolish(m_favBtn);
    m_favBtn->style()->polish(m_favBtn);
}

QCoro::Task<void> SeasonView::loadSeason(QString seriesId, QString seasonId, QString seasonName)
{
    QPointer<SeasonView> guard(this);
    m_currentSeriesId = seriesId;
    m_currentSeasonId = seasonId;
    m_currentSeasonItem = MediaItem();
    m_currentSeasonItem.id = seasonId; 

    m_seriesTitleLabel->setText(tr("Loading..."));
    m_titleLabel->setText(seasonName);

    
    
    m_overviewLabel->clear();

    m_isFavorite = false;
    updateFavBtnState();

    m_contentWidget->setBackdrop(QPixmap());
    m_posterLabel->setPixmap(QPixmap());

    m_sortButton->blockSignals(true);
    m_sortButton->setSortItems({tr("Episode Number"), tr("Title"), tr("Premiere Date"), tr("Runtime")});
    m_sortButton->setDescending(false); 
    m_sortButton->blockSignals(false);

    
    refreshExtPlayerButton();

    QString fallbackBackdropTag;

    
    if (!seriesId.isEmpty())
    {
        try
        {
            MediaItem seriesItem = co_await m_core->mediaService()->getItemDetail(seriesId);
            if (guard)
            {
                m_seriesTitleLabel->setText(seriesItem.name);
                fallbackBackdropTag = seriesItem.images.backdropTag;

                
                m_overviewLabel->setText(seriesItem.overview);
            }
        }
        catch (...)
        {
            if (guard)
                m_seriesTitleLabel->setText(tr("Unknown Series"));
        }
    }

    
    if (!seasonId.isEmpty() && guard)
    {
        try
        {
            MediaItem detail = co_await m_core->mediaService()->getItemDetail(seasonId);
            if (guard && detail.id == m_currentSeasonId)
            {
                m_currentSeasonItem = detail;
                m_isFavorite = detail.isFavorite();
                updateFavBtnState();

                
                if (!detail.overview.isEmpty())
                {
                    m_overviewLabel->setText(detail.overview);
                }
            }
        }
        catch (...)
        {
            
        }
    }

    
    if (!guard)
        co_return;

    loadImages(fallbackBackdropTag);
    co_await onFilterChanged();
}

QCoro::Task<void> SeasonView::loadImages(const QString &fallbackSeriesBackdropTag)
{
    QPointer<SeasonView> guard(this);

    
    if (!guard || !m_core || !m_core->mediaService())
        co_return;

    bool adaptive = ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages, true);

    
    
    
    {
        QString posterType = "Primary";
        QString posterTag = m_currentSeasonItem.images.primaryTag;
        QString posterId = m_currentSeasonId;

        
        if (posterTag.isEmpty() && !m_currentSeriesId.isEmpty())
        {
            posterId = m_currentSeriesId;
            
            if (adaptive)
            {
                auto best = m_currentSeasonItem.images.bestPoster();
                posterTag = best.first;
                posterType = best.second;
                posterId = m_currentSeasonItem.images.isParentTag(posterTag) ? m_currentSeriesId : m_currentSeasonId;
            }
        }

        if (!posterTag.isEmpty())
        {
            try
            {
                int posterMaxWidth = (posterType == "Primary") ? 400 : 800;
                QPixmap pix =
                    co_await m_core->mediaService()->fetchImage(posterId, posterType, posterTag, posterMaxWidth);
                if (guard && !pix.isNull())
                {
                    const QSize posterSize(250, 375);
                    QPixmap scaled = pix.scaled(posterSize, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
                    int cropX = (scaled.width() - posterSize.width()) / 2;
                    int cropY = (scaled.height() - posterSize.height()) / 2;
                    QPixmap cropped = scaled.copy(cropX, cropY, posterSize.width(), posterSize.height());

                    
                    QPixmap rounded(posterSize);
                    rounded.fill(Qt::transparent);
                    QPainter p(&rounded);
                    p.setRenderHint(QPainter::Antialiasing);
                    QPainterPath path;
                    path.addRoundedRect(QRectF(0, 0, posterSize.width(), posterSize.height()), 12, 12);
                    p.setClipPath(path);
                    p.drawPixmap(0, 0, cropped);
                    p.end();
                    m_posterLabel->setPixmap(rounded);
                }
            }
            catch (...)
            {
            }
        }
    }

    if (!guard)
        co_return;

    
    
    
    {
        QString tag = m_currentSeasonItem.images.backdropTag;
        QString imageId = m_currentSeasonId;

        if (tag.isEmpty() && !fallbackSeriesBackdropTag.isEmpty())
        {
            tag = fallbackSeriesBackdropTag;
            imageId = m_currentSeriesId;
        }

        
        
        QString imgType = "Backdrop";
        if (tag.isEmpty() && adaptive)
        {
            auto best = m_currentSeasonItem.images.bestBackdrop();
            tag = best.first;
            imgType = best.second;
            
            imageId = m_currentSeasonItem.images.isParentTag(tag) ? m_currentSeriesId : m_currentSeasonId;
        }

        if (!tag.isEmpty())
        {
            try
            {
                QPixmap pix = co_await m_core->mediaService()->fetchImage(imageId, imgType, tag, 1920);
                if (guard && !pix.isNull())
                {
                    m_contentWidget->setBackdrop(pix);
                }
            }
            catch (...)
            {
            }
        }
    }
}

QCoro::Task<void> SeasonView::onFilterChanged(bool preserveScroll)
{
    QPointer<SeasonView> guard(this);

    
    if (!guard || !m_core || !m_core->mediaService())
        co_return;

    
    int savedScrollPos = 0;
    if (preserveScroll)
    {
        savedScrollPos = m_mediaGrid->saveScrollPosition();
    }
    else
    {
        m_mediaGrid->setItems(QList<MediaItem>());
    }
    m_statsLabel->setText(tr("Loading..."));

    QString sortBy = "ParentIndexNumber,IndexNumber";
    switch (m_sortButton->currentIndex())
    {
    case 0:
        sortBy = "ParentIndexNumber,IndexNumber";
        break;
    case 1:
        sortBy = "SortName";
        break;
    case 2:
        sortBy = "PremiereDate";
        break;
    case 3:
        sortBy = "Runtime";
        break;
    }
    QString sortOrder = m_sortButton->isDescending() ? "Descending" : "Ascending";

    try
    {
        auto episodes =
            co_await m_core->mediaService()->getEpisodes(m_currentSeriesId, m_currentSeasonId, sortBy, sortOrder);
        if (!guard)
            co_return;

        m_currentEpisodes = episodes; 
        m_statsLabel->setText(tr("%1 Episodes").arg(episodes.size()));
        m_mediaGrid->setItems(episodes);

        
        if (preserveScroll)
        {
            m_mediaGrid->restoreScrollPosition(savedScrollPos);
        }

        
        if (!guard)
            co_return;

        
        co_await updatePlayButtonFromNextUp();
    }
    catch (...)
    {
        if (!guard)
            co_return;
        m_statsLabel->setText(tr("Error Loading Episodes"));
    }
}

void SeasonView::showEvent(QShowEvent *event)
{
    BaseView::showEvent(event);
    
    
    
    
    if (!m_currentSeasonId.isEmpty() && m_core && m_core->mediaService() && isVisible())
    {
        onFilterChanged(true);
        refreshExtPlayerButton();
    }
}

void SeasonView::onMediaItemUpdated(const MediaItem &item)
{
    if (m_currentSeasonId == item.id)
    {
        m_currentSeasonItem = item;
        m_isFavorite = item.isFavorite();
        updateFavBtnState();
    }
    if (m_mediaGrid)
    {
        m_mediaGrid->updateItem(item);
    }

    
    for (auto &ep : m_currentEpisodes)
    {
        if (ep.id == item.id)
        {
            ep = item;
            break;
        }
    }
}

QCoro::Task<void> SeasonView::updatePlayButtonFromNextUp()
{
    QPointer<SeasonView> guard(this);
    m_nextUpEpisodeId.clear();

    
    if (!guard || !m_core || !m_core->mediaService())
        co_return;

    try
    {
        auto nextUp = co_await m_core->mediaService()->getNextUp(m_currentSeriesId);
        if (!guard)
            co_return;

        if (!nextUp.isEmpty())
        {
            MediaItem nextEp = nextUp.first();
            
            for (const auto &ep : m_currentEpisodes)
            {
                if (ep.id == nextEp.id)
                {
                    m_nextUpEpisodeId = nextEp.id;
                    m_playBtn->setText(tr("▶ Continue E%1").arg(ep.indexNumber));
                    co_return;
                }
            }
        }
    }
    catch (...)
    {
    }

    if (guard)
    {
        m_playBtn->setText(tr("▶ Play"));
    }
}




void SeasonView::refreshExtPlayerButton()
{
    bool extEnabled = ConfigStore::instance()->get<bool>(ConfigKeys::ExtPlayerEnable, false);
    if (!extEnabled)
    {
        m_extPlayerBtn->hide();
        return;
    }

    QString currentPath = ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerPath);

    
    QList<DetectedPlayer> allPlayers = ExternalPlayerDetector::loadFromConfig();

    
    QSet<QString> knownPaths;
    for (const auto &p : allPlayers)
        knownPaths.insert(p.path);
    if (!currentPath.isEmpty() && currentPath != "custom" && !knownPaths.contains(currentPath) &&
        QFileInfo::exists(currentPath))
    {
        allPlayers.prepend({QFileInfo(currentPath).baseName(), currentPath});
    }

    if (allPlayers.isEmpty())
    {
        m_extPlayerBtn->hide();
        return;
    }

    
    QString activePlayerPath = currentPath;
    if (activePlayerPath.isEmpty() || activePlayerPath == "custom")
    {
        activePlayerPath = allPlayers.first().path;
    }
    m_extPlayerBtn->setPlayers(allPlayers, activePlayerPath);
}




QCoro::Task<void> SeasonView::executeExternalPlay(MediaItem targetItem, QString playerPath)
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
        if (actualItem.mediaSources.size() > 1)
        {
            QString versionPref = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerPreferredVersion).trimmed();
            if (!versionPref.isEmpty())
            {
                QStringList keywords = versionPref.split(',', Qt::SkipEmptyParts);
                for (const QString &kw : keywords)
                {
                    QString keyword = kw.trimmed();
                    if (keyword.isEmpty())
                        continue;
                    bool found = false;
                    for (int i = 0; i < actualItem.mediaSources.size(); ++i)
                    {
                        if (actualItem.mediaSources[i].name.contains(keyword, Qt::CaseInsensitive))
                        {
                            sourceIdx = i;
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        break;
                }
            }
        }

        if (sourceIdx >= actualItem.mediaSources.size())
            sourceIdx = 0;
        MediaSourceInfo modifiedSource = actualItem.mediaSources[sourceIdx];

        
        QString prefAudioLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerAudioLang, "auto");
        QString prefSubLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto");

        {
            int bestAudioIdx = -1;
            int bestSubIdx = -1;
            int firstSubIdx = -1;
            bool hasDefaultSub = false;
            bool audioFound = false, subFound = false;

            for (const auto &stream : modifiedSource.mediaStreams)
            {
                if (stream.type == "Audio" && !audioFound && prefAudioLang != "auto" &&
                    stream.language.compare(prefAudioLang, Qt::CaseInsensitive) == 0)
                {
                    bestAudioIdx = stream.index;
                    audioFound = true;
                }
                else if (stream.type == "Subtitle")
                {
                    if (firstSubIdx < 0)
                        firstSubIdx = stream.index;
                    if (stream.isDefault)
                        hasDefaultSub = true;
                    if (!subFound && prefSubLang != "auto" && prefSubLang != "none" &&
                        stream.language.compare(prefSubLang, Qt::CaseInsensitive) == 0)
                    {
                        bestSubIdx = stream.index;
                        subFound = true;
                    }
                }
            }

            if (bestAudioIdx >= 0)
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Audio")
                        stream.isDefault = (stream.index == bestAudioIdx);
                }
            }

            if (prefSubLang == "none")
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Subtitle")
                        stream.isDefault = false;
                }
            }
            else if (bestSubIdx >= 0)
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Subtitle")
                        stream.isDefault = (stream.index == bestSubIdx);
                }
            }
            else if (firstSubIdx >= 0 && !hasDefaultSub)
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Subtitle")
                        stream.isDefault = (stream.index == firstSubIdx);
                }
            }
        }

        QString streamUrl = m_core->mediaService()->getStreamUrl(actualItem.id, modifiedSource.id);

        
        QString playTitle = actualItem.name;
        if (actualItem.type == "Episode")
        {
            QString seriesName = actualItem.seriesName.isEmpty() ? m_seriesTitleLabel->text() : actualItem.seriesName;
            playTitle = QString("%1 - S%2:E%3 - %4")
                            .arg(seriesName)
                            .arg(actualItem.parentIndexNumber)
                            .arg(actualItem.indexNumber)
                            .arg(actualItem.name);
        }

        long long startTicks = actualItem.userData.playbackPositionTicks;
        PlaybackManager::instance()->startExternalPlayback(playerPath, actualItem.id, playTitle, streamUrl, startTicks,
                                                           QVariant::fromValue(modifiedSource));
    }
    catch (...)
    {
    }
}




QCoro::Task<void> SeasonView::executeInternalPlay(MediaItem targetItem)
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
        if (actualItem.mediaSources.size() > 1)
        {
            QString versionPref = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerPreferredVersion).trimmed();
            if (!versionPref.isEmpty())
            {
                QStringList keywords = versionPref.split(',', Qt::SkipEmptyParts);
                for (const QString &kw : keywords)
                {
                    QString keyword = kw.trimmed();
                    if (keyword.isEmpty())
                        continue;
                    bool found = false;
                    for (int i = 0; i < actualItem.mediaSources.size(); ++i)
                    {
                        if (actualItem.mediaSources[i].name.contains(keyword, Qt::CaseInsensitive))
                        {
                            sourceIdx = i;
                            found = true;
                            break;
                        }
                    }
                    if (found)
                        break;
                }
            }
        }

        if (sourceIdx >= actualItem.mediaSources.size())
            sourceIdx = 0;
        MediaSourceInfo modifiedSource = actualItem.mediaSources[sourceIdx];

        
        QString prefAudioLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerAudioLang, "auto");
        QString prefSubLang = ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSubLang, "auto");

        {
            int bestAudioIdx = -1;
            int bestSubIdx = -1;
            int firstSubIdx = -1;
            bool hasDefaultSub = false;
            bool audioFound = false, subFound = false;

            for (const auto &stream : modifiedSource.mediaStreams)
            {
                if (stream.type == "Audio" && !audioFound && prefAudioLang != "auto" &&
                    stream.language.compare(prefAudioLang, Qt::CaseInsensitive) == 0)
                {
                    bestAudioIdx = stream.index;
                    audioFound = true;
                }
                else if (stream.type == "Subtitle")
                {
                    if (firstSubIdx < 0)
                        firstSubIdx = stream.index;
                    if (stream.isDefault)
                        hasDefaultSub = true;
                    if (!subFound && prefSubLang != "auto" && prefSubLang != "none" &&
                        stream.language.compare(prefSubLang, Qt::CaseInsensitive) == 0)
                    {
                        bestSubIdx = stream.index;
                        subFound = true;
                    }
                }
            }

            if (bestAudioIdx >= 0)
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Audio")
                        stream.isDefault = (stream.index == bestAudioIdx);
                }
            }

            if (prefSubLang == "none")
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Subtitle")
                        stream.isDefault = false;
                }
            }
            else if (bestSubIdx >= 0)
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Subtitle")
                        stream.isDefault = (stream.index == bestSubIdx);
                }
            }
            else if (firstSubIdx >= 0 && !hasDefaultSub)
            {
                for (auto &stream : modifiedSource.mediaStreams)
                {
                    if (stream.type == "Subtitle")
                        stream.isDefault = (stream.index == firstSubIdx);
                }
            }
        }

        QString streamUrl = m_core->mediaService()->getStreamUrl(actualItem.id, modifiedSource.id);

        
        QString playTitle = actualItem.name;
        if (actualItem.type == "Episode")
        {
            QString seriesName = actualItem.seriesName.isEmpty() ? m_seriesTitleLabel->text() : actualItem.seriesName;
            playTitle = QString("%1 - S%2:E%3 - %4")
                            .arg(seriesName)
                            .arg(actualItem.parentIndexNumber)
                            .arg(actualItem.indexNumber)
                            .arg(actualItem.name);
        }

        long long startTicks = actualItem.userData.playbackPositionTicks;
        PlaybackManager::instance()->startInternalPlayback(actualItem.id, playTitle, streamUrl, startTicks,
                                                           QVariant::fromValue(modifiedSource));
    }
    catch (...)
    {
    }
}
