#include "libraryview.h"
#include "../../components/elidedlabel.h"
#include "../../components/mediagridwidget.h"
#include "../../components/modernsortbutton.h"
#include <QButtonGroup>
#include <QDebug>
#include <QElapsedTimer>
#include <QFontMetrics>
#include <QHash>
#include <QHBoxLayout>
#include <QIcon>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer> 
#include <QPushButton>
#include <QScrollArea>
#include <QStyle>
#include <QVBoxLayout>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>
#include <utility>

LibraryView::LibraryView(QEmbyCore *core, QWidget *parent)
    : BaseView(core, parent), m_currentMode(LibraryMode) 
{
    setProperty("showGlobalSearch", true);
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("library-view");

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    
    QScrollArea *headerScrollArea = new QScrollArea(this);
    headerScrollArea->setFrameShape(QFrame::NoFrame);
    headerScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    headerScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    headerScrollArea->setWidgetResizable(true);

    headerScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; } "
                                    "QWidget#library-header-container { background: transparent; }");

    headerScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerScrollArea->setFixedHeight(65);

    QWidget *headerContainer = new QWidget(headerScrollArea);
    headerContainer->setObjectName("library-header-container");

    auto *headerLayout = new QHBoxLayout(headerContainer);
    headerLayout->setContentsMargins(20, 15, 20, 10);
    headerLayout->setSpacing(20);
    headerLayout->setAlignment(Qt::AlignVCenter);

    setupTopBar(headerLayout);
    headerScrollArea->setWidget(headerContainer);

    mainLayout->addWidget(headerScrollArea);
    

    
    m_mediaGrid = new MediaGridWidget(m_core, this);
    mainLayout->addWidget(m_mediaGrid);

    

    connect(m_mediaGrid, &MediaGridWidget::itemClicked, this,
            [this](const MediaItem &item)
            {
                
                if (item.type == "BoxSet" || item.type == "Playlist" || item.type == "Folder")
                {
                    Q_EMIT navigateToFolder(item.id, item.name);
                }
                else if (item.type == "Person")
                {
                    
                    Q_EMIT navigateToPerson(item.id, item.name);
                }
                else
                {
                    Q_EMIT navigateToDetail(item.id, item.name);
                }
            });

    
    
    
    connect(m_mediaGrid, &MediaGridWidget::playRequested, this, &BaseView::handlePlayRequested);
    connect(m_mediaGrid, &MediaGridWidget::favoriteRequested, this, &BaseView::handleFavoriteRequested);
    connect(m_mediaGrid, &MediaGridWidget::moreMenuRequested, this, &BaseView::handleMoreMenuRequested);
    connect(m_mediaGrid, &MediaGridWidget::loadMoreRequested, this, &LibraryView::onLoadMoreRequested);
    
}

void LibraryView::setupTopBar(QHBoxLayout *headerLayout)
{
    
    
    
    auto *titleLayout = new QHBoxLayout();
    titleLayout->setContentsMargins(0, 0, 0, 0);
    titleLayout->setSpacing(8);
    

    m_titleLabel = new ElidedLabel(this);
    m_titleLabel->setObjectName("library-title");
    m_titleLabel->setStyleSheet("padding: 0px;");
    
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    
    m_favBtn = new QPushButton(this);
    m_favBtn->setObjectName("detail-fav-btn"); 
    m_favBtn->setIcon(QIcon(":/svg/light/heart-outline.svg"));
    m_favBtn->setIconSize(QSize(20, 20));
    m_favBtn->setFixedSize(36, 36);
    m_favBtn->setCursor(Qt::PointingHandCursor);
    m_favBtn->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_favBtn->setFocusPolicy(Qt::NoFocus);
    m_favBtn->hide(); 

    
    connect(m_favBtn, &QPushButton::clicked, this,
            [this]()
            {
                if (!m_currentMediaItem.id.isEmpty())
                {
                    handleFavoriteRequested(m_currentMediaItem);
                }
            });

    titleLayout->addWidget(m_titleLabel);
    titleLayout->addWidget(m_favBtn);
    headerLayout->addLayout(titleLayout);

    
    m_tabBarWidget = new QWidget(this); 
    auto *tabLayout = new QHBoxLayout(m_tabBarWidget);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(8);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    
    QStringList tabs = {tr("All"), tr("Recent"), tr("Playlists"), tr("Collections"), tr("Favorites"), tr("Folders")};
    for (int i = 0; i < tabs.size(); ++i)
    {
        auto *btn = new QPushButton(tabs[i], m_tabBarWidget);
        btn->setObjectName("library-tab-btn");
        btn->setCheckable(true);
        m_tabGroup->addButton(btn, i);
        tabLayout->addWidget(btn);
    }
    m_tabGroup->button(0)->setChecked(true);
    connect(m_tabGroup, &QButtonGroup::idClicked, this, &LibraryView::onFilterChanged);

    headerLayout->addWidget(m_tabBarWidget);
    headerLayout->addStretch();

    
    QWidget *filterBarWidget = new QWidget(this);
    auto *filterLayout = new QHBoxLayout(filterBarWidget);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(10);

    
    m_sortButton = new ModernSortButton(filterBarWidget);
    m_sortButton->setSortItems({tr("Date Added"), tr("Date Played"), tr("Title"), tr("Runtime"), tr("Premiere Date")});
    connect(m_sortButton, &ModernSortButton::sortChanged, this, &LibraryView::onFilterChanged);

    
    m_viewSwitchBtn = new QPushButton(filterBarWidget);
    m_viewSwitchBtn->setObjectName("icon-action-btn");
    m_viewSwitchBtn->setCheckable(true);
    m_viewSwitchBtn->setFixedSize(32, 32); 
    m_viewSwitchBtn->setCursor(Qt::PointingHandCursor);
    m_viewSwitchBtn->setToolTip(tr("Toggle View Mode"));
    

    connect(m_viewSwitchBtn, &QPushButton::clicked, this,
            [this](bool checked)
            {
                m_mediaGrid->setCardStyle(checked ? MediaCardDelegate::LibraryTile : MediaCardDelegate::Poster);
                saveViewPreference();
                onFilterChanged();
            });

    m_statsLabel = new QLabel(tr("0 Items"), filterBarWidget);
    m_statsLabel->setObjectName("library-stats-label");
    m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    filterLayout->addWidget(m_sortButton);
    filterLayout->addWidget(m_viewSwitchBtn);
    filterLayout->addWidget(m_statsLabel);

    headerLayout->addWidget(filterBarWidget);
}


void LibraryView::updateFavBtnState()
{
    m_favBtn->setProperty("isFavorite", m_isFavorite);
    m_favBtn->setIcon(QIcon(m_isFavorite ? ":/svg/light/heart-fill.svg" : ":/svg/light/heart-outline.svg"));
    m_favBtn->style()->unpolish(m_favBtn);
    m_favBtn->style()->polish(m_favBtn);
}


QCoro::Task<void> LibraryView::loadLibrary(const QString &libraryId, const QString &libraryName)
{
    
    QPointer<LibraryView> guard(this);

    QElapsedTimer perfTimer;
    perfTimer.start();
    qDebug() << "[LibraryView] loadLibrary START" << "| id=" << libraryId << "| name=" << libraryName;

    m_currentMode = LibraryMode;
    m_currentLibraryId = libraryId;
    m_currentMediaItem = MediaItem(); 

    m_titleLabel->setFullText(libraryName);
    m_titleLabel->ensurePolished(); 
    QFontMetrics fm(m_titleLabel->font());
    m_titleLabel->setMaximumWidth(fm.horizontalAdvance(libraryName) + 15); 

    
    
    
    m_favBtn->hide();
    m_tabBarWidget->hide();
    m_sortButton->show();

    
    m_tabGroup->button(0)->setText(tr("All"));

    m_tabGroup->blockSignals(true);
    m_tabGroup->button(0)->setChecked(true);
    m_tabGroup->blockSignals(false);

    m_sortButton->blockSignals(true);
    m_sortButton->setSortItems({tr("Date Added"), tr("Date Played"), tr("Title"), tr("Runtime"), tr("Premiere Date")});
    m_sortButton->blockSignals(false);

    
    restoreSortPreference();

    restoreViewPreference();

    
    
    const bool shimmerEnabled =
        ConfigStore::instance()->get<bool>(ConfigKeys::ShimmerAnimation, false);
    if (shimmerEnabled && m_mediaGrid) {
        m_mediaGrid->setLoading(true);
    }

    if (!libraryId.isEmpty())
    {
        try
        {
            
            MediaItem detail = co_await m_core->mediaService()->getItemDetail(libraryId);
            qDebug() << "[LibraryView] getItemDetail completed" << "| elapsed=" << perfTimer.elapsed() << "ms";
            if (!guard)
                co_return; 

            
            if (detail.id == m_currentLibraryId)
            {
                m_currentMediaItem = detail; 

                
                
                
                if (detail.collectionType == "movies")
                {
                    m_tabGroup->button(0)->setText(tr("Movies"));
                }
                else if (detail.collectionType == "tvshows")
                {
                    m_tabGroup->button(0)->setText(tr("Shows"));
                }
                else if (detail.collectionType == "music")
                {
                    m_tabGroup->button(0)->setText(tr("Music"));
                }
                else if (detail.collectionType == "homevideos" || detail.collectionType == "photos")
                {
                    m_tabGroup->button(0)->setText(tr("Videos"));
                }
                else
                {
                    m_tabGroup->button(0)->setText(tr("All"));
                }

                
                
                
                bool isItemFolder = (detail.type == "BoxSet" || detail.type == "Playlist" || detail.type == "Folder");
                bool isCollectionLib = (detail.collectionType == "playlists" || detail.collectionType == "boxsets");
                if (isItemFolder)
                {
                    m_isFavorite = detail.isFavorite();
                    updateFavBtnState();
                    m_favBtn->show();
                    m_tabBarWidget->hide();
                }
                else if (isCollectionLib)
                {
                    m_favBtn->hide();
                    m_tabBarWidget->hide();
                }
                else
                {
                    m_favBtn->hide();
                    if (m_currentMode == LibraryMode)
                    {
                        m_tabBarWidget->show();
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            if (!guard)
                co_return; 
            qDebug() << "Failed to detect library type details: " << e.what();
        }
    }

    
    qDebug() << "[LibraryView] entering onFilterChanged" << "| elapsed=" << perfTimer.elapsed() << "ms";
    co_await onFilterChanged();
    qDebug() << "[LibraryView] loadLibrary END" << "| totalElapsed=" << perfTimer.elapsed() << "ms";
}


QCoro::Task<void> LibraryView::loadPerson(const QString &personId, const QString &personName)
{
    
    QPointer<LibraryView> guard(this);

    m_currentMode = PersonMode;
    m_currentPersonId = personId;
    m_currentMediaItem = MediaItem(); 

    m_titleLabel->setFullText(personName);
    m_titleLabel->ensurePolished();
    QFontMetrics fm(m_titleLabel->font());
    m_titleLabel->setMaximumWidth(fm.horizontalAdvance(personName) + 15);

    
    m_favBtn->hide();
    m_tabBarWidget->hide();
    m_sortButton->show();

    m_sortButton->blockSignals(true);
    
    
    m_sortButton->blockSignals(false);

    
    restoreSortPreference();

    restoreViewPreference();

    
    const bool shimmerEnabled =
        ConfigStore::instance()->get<bool>(ConfigKeys::ShimmerAnimation, false);
    if (shimmerEnabled && m_mediaGrid) {
        m_mediaGrid->setLoading(true);
    }

    
    if (!personId.isEmpty())
    {
        try
        {
            
            MediaItem detail = co_await m_core->mediaService()->getItemDetail(personId);
            if (!guard)
                co_return; 

            if (detail.id == m_currentPersonId)
            {
                m_currentMediaItem = detail; 
                m_isFavorite = detail.isFavorite();
                updateFavBtnState();
                m_favBtn->show();
            }
        }
        catch (const std::exception &e)
        {
            if (!guard)
                co_return; 
            qDebug() << "Failed to fetch person details: " << e.what();
        }
    }

    co_await onFilterChanged();
}


QCoro::Task<void> LibraryView::loadFiltered(const QString &filterType, const QString &filterValue)
{
    QPointer<LibraryView> guard(this);

    m_currentMode = FilteredMode;
    m_filterType = filterType;
    m_filterValue = filterValue;
    m_currentLibraryId.clear();
    m_currentPersonId.clear();
    m_currentMediaItem = MediaItem();

    m_titleLabel->setFullText(filterValue);
    m_titleLabel->ensurePolished();
    QFontMetrics fm(m_titleLabel->font());
    m_titleLabel->setMaximumWidth(fm.horizontalAdvance(filterValue) + 15);

    m_favBtn->hide();
    m_tabBarWidget->hide();
    m_sortButton->show();

    m_sortButton->blockSignals(true);
    m_sortButton->setSortItems({tr("Date Added"), tr("Date Played"), tr("Title"), tr("Runtime"), tr("Premiere Date")});
    m_sortButton->blockSignals(false);

    restoreViewPreference();

    
    const bool shimmerEnabled =
        ConfigStore::instance()->get<bool>(ConfigKeys::ShimmerAnimation, false);
    if (shimmerEnabled && m_mediaGrid) {
        m_mediaGrid->setLoading(true);
    }

    co_await onFilterChanged();
}


QCoro::Task<void> LibraryView::onFilterChanged()
{
    
    QPointer<LibraryView> guard(this);

    
    saveSortPreference();
    ++m_requestGeneration;
    resetPaginationState();

    m_mediaGrid->setItems(QList<MediaItem>());
    m_statsLabel->setText(tr("Loading..."));

    
    const bool shimmerEnabled =
        ConfigStore::instance()->get<bool>(ConfigKeys::ShimmerAnimation, false);
    if (shimmerEnabled && m_mediaGrid) {
        m_mediaGrid->setLoading(true);
    }

    
    QString sortBy = "SortName";
    switch (m_sortButton->currentIndex())
    {
    case 0:
        sortBy = "DateCreated";
        break;
    case 1:
        sortBy = "DatePlayed";
        break;
    case 2:
        sortBy = "SortName";
        break;
    case 3:
        sortBy = "Runtime";
        break;
    case 4:
        sortBy = "PremiereDate";
        break;
    }
    QString sortOrder = m_sortButton->isDescending() ? "Descending" : "Ascending";
    m_activeQuery = buildCurrentQueryState(sortBy, sortOrder);
    qDebug() << "[LibraryView] onFilterChanged"
             << "| generation=" << m_requestGeneration
             << "| mode=" << static_cast<int>(m_activeQuery.mode)
             << "| targetId=" << m_activeQuery.targetId
             << "| sortBy=" << m_activeQuery.sortBy
             << "| sortOrder=" << m_activeQuery.sortOrder
             << "| filters=" << m_activeQuery.filters
             << "| includeTypes=" << m_activeQuery.includeTypes
             << "| recursive=" << m_activeQuery.recursive;

    if (!isQueryValid(m_activeQuery))
    {
        if (!guard)
            co_return;
        if (m_mediaGrid) {
            m_mediaGrid->setLoading(false);
        }
        updateStatsLabel();
        co_return;
    }

    co_await loadInitialItems();
}

LibraryView::QueryState LibraryView::buildCurrentQueryState(const QString &sortBy,
                                                            const QString &sortOrder) const
{
    QueryState query;
    query.mode = m_currentMode;
    query.sortBy = sortBy;
    query.sortOrder = sortOrder;

    if (m_currentMode == PersonMode)
    {
        query.targetId = m_currentPersonId;
        return query;
    }

    if (m_currentMode == FilteredMode)
    {
        if (m_filterType == "Genre")
            query.genreFilter = m_filterValue;
        else if (m_filterType == "Tag")
            query.tagFilter = m_filterValue;
        else if (m_filterType == "Studio")
            query.studioFilter = m_filterValue;
        return query;
    }

    query.targetId = m_currentLibraryId;
    query.includeTypes = "Movie,Series,Audio,Video";
    query.recursive = true;
    query.needsPlaylistEnrichment = (m_currentMediaItem.type == "Playlist");

    if (m_currentMediaItem.type == "Folder")
    {
        query.includeTypes.clear();
        query.sortBy = "IsFolder,SortName";
        query.sortOrder = "Ascending";
        query.recursive = false;
        return query;
    }

    if (m_currentMediaItem.type == "BoxSet" || m_currentMediaItem.type == "Playlist" ||
        m_currentMediaItem.collectionType == "playlists" || m_currentMediaItem.collectionType == "boxsets")
    {
        query.includeTypes.clear();
        query.recursive = false;
        return query;
    }

    const int tabIdx = m_tabGroup ? m_tabGroup->checkedId() : 0;
    if (tabIdx == 1)
    {
        query.sortBy = "DateCreated";
        query.sortOrder = "Descending";
    }
    if (tabIdx == 2)
    {
        query.includeTypes = "Playlist";
    }
    if (tabIdx == 3)
    {
        query.includeTypes = "BoxSet";
    }
    if (tabIdx == 4)
    {
        query.filters = "IsFavorite";
        query.includeTypes = "Movie,Series,Audio,Video,BoxSet,Playlist";
    }
    if (tabIdx == 5)
    {
        query.includeTypes.clear();
        query.sortBy = "IsFolder,SortName";
        query.sortOrder = "Ascending";
        query.recursive = false;
    }

    return query;
}

bool LibraryView::isQueryValid(const QueryState &query) const
{
    switch (query.mode)
    {
    case PersonMode:
    case LibraryMode:
        return !query.targetId.trimmed().isEmpty();
    case FilteredMode:
        return !query.genreFilter.trimmed().isEmpty() ||
               !query.tagFilter.trimmed().isEmpty() ||
               !query.studioFilter.trimmed().isEmpty();
    default:
        return false;
    }
}

void LibraryView::resetPaginationState()
{
    m_loadedItems.clear();
    m_totalItemCount = 0;
    m_pageSize = 0;
    m_hasMoreItems = false;
    m_isLoadingMore = false;
}

void LibraryView::updateStatsLabel()
{
    const int loadedCount = m_loadedItems.size();
    const int totalCount = qMax(m_totalItemCount, loadedCount);
    if (totalCount > loadedCount)
    {
        m_statsLabel->setText(tr("%1 / %2 Items").arg(loadedCount).arg(totalCount));
        return;
    }

    m_statsLabel->setText(tr("%1 Items").arg(totalCount));
}

QCoro::Task<void> LibraryView::loadInitialItems()
{
    QPointer<LibraryView> guard(this);
    const int generation = m_requestGeneration;
    const QueryState query = m_activeQuery;

    auto fetchPage = [this](const QueryState &pageQuery, int startIndex, int limit)
        -> QCoro::Task<MediaQueryPage>
    {
        switch (pageQuery.mode)
        {
        case PersonMode:
            co_return co_await m_core->mediaService()->getItemsByPersonPage(
                pageQuery.targetId, pageQuery.sortBy, pageQuery.sortOrder,
                startIndex, limit);
        case FilteredMode:
            co_return co_await m_core->mediaService()->getItemsByFilterPage(
                pageQuery.genreFilter, pageQuery.tagFilter, pageQuery.studioFilter,
                pageQuery.sortBy, pageQuery.sortOrder, startIndex, limit);
        case LibraryMode:
        default:
            co_return co_await m_core->mediaService()->getLibraryItemsPage(
                pageQuery.targetId, pageQuery.sortBy, pageQuery.sortOrder,
                pageQuery.filters, pageQuery.includeTypes, startIndex, limit,
                pageQuery.recursive, pageQuery.includeChildCount);
        }
    };

    m_isLoadingMore = true;

    try
    {
        const MediaQueryPage probePage = co_await fetchPage(query, 0, 1);
        if (!guard || generation != m_requestGeneration)
            co_return;

        m_totalItemCount = qMax(probePage.totalRecordCount, probePage.items.size());
        qDebug() << "[LibraryView] initial count probe"
                 << "| generation=" << generation
                 << "| returned=" << probePage.items.size()
                 << "| total=" << m_totalItemCount;

        QList<MediaItem> initialItems = probePage.items;
        int returnedCount = probePage.items.size();

        if (m_totalItemCount > probePage.items.size())
        {
            const MediaQueryPage fullAttemptPage =
                co_await fetchPage(query, 0, m_totalItemCount);
            if (!guard || generation != m_requestGeneration)
                co_return;

            initialItems = fullAttemptPage.items;
            returnedCount = fullAttemptPage.items.size();
            m_totalItemCount = qMax(fullAttemptPage.totalRecordCount,
                                    fullAttemptPage.items.size());

            qDebug() << "[LibraryView] full fetch attempt"
                     << "| generation=" << generation
                     << "| requested=" << m_totalItemCount
                     << "| returned=" << returnedCount
                     << "| total=" << m_totalItemCount;
        }

        if (query.needsPlaylistEnrichment && !initialItems.isEmpty())
        {
            initialItems =
                co_await enrichPlaylistItemsForRemoval(std::move(initialItems));
            if (!guard || generation != m_requestGeneration)
                co_return;
        }

        m_loadedItems = std::move(initialItems);
        m_totalItemCount = qMax(m_totalItemCount, m_loadedItems.size());

        if (m_loadedItems.size() < m_totalItemCount && returnedCount > 0)
        {
            m_pageSize = returnedCount;
            m_hasMoreItems = true;
            qDebug() << "[LibraryView] server-side page cap detected"
                     << "| generation=" << generation
                     << "| pageSize=" << m_pageSize
                     << "| loaded=" << m_loadedItems.size()
                     << "| total=" << m_totalItemCount;
        }
        else
        {
            if (m_loadedItems.size() < m_totalItemCount && returnedCount <= 0)
            {
                qWarning() << "[LibraryView] Initial fetch returned no items while total count is larger"
                           << "| generation=" << generation
                           << "| loaded=" << m_loadedItems.size()
                           << "| total=" << m_totalItemCount;
            }
            m_pageSize = 0;
            m_hasMoreItems = false;
        }

        updateStatsLabel();
        QElapsedTimer renderTimer;
        renderTimer.start();
        m_mediaGrid->setItems(m_loadedItems);
        m_mediaGrid->setLoading(false);
        qDebug() << "[LibraryView] initial items applied"
                 << "| generation=" << generation
                 << "| loaded=" << m_loadedItems.size()
                 << "| total=" << m_totalItemCount
                 << "| hasMore=" << m_hasMoreItems
                 << "| renderTime=" << renderTimer.elapsed() << "ms";
    }
    catch (const std::exception &e)
    {
        if (!guard || generation != m_requestGeneration)
            co_return;
        if (m_mediaGrid) {
            m_mediaGrid->setLoading(false);
        }
        m_statsLabel->setText(tr("Error Loading Items"));
        qDebug() << "Library View initial fetching error:" << e.what();
    }

    if (guard && generation == m_requestGeneration)
    {
        m_isLoadingMore = false;
    }
}

QCoro::Task<void> LibraryView::onLoadMoreRequested()
{
    QPointer<LibraryView> guard(this);
    if (m_isLoadingMore || !m_hasMoreItems || m_pageSize <= 0)
        co_return;

    const int generation = m_requestGeneration;
    const QueryState query = m_activeQuery;
    const int startIndex = m_loadedItems.size();
    const int remainingCount = m_totalItemCount - startIndex;
    if (remainingCount <= 0)
    {
        m_hasMoreItems = false;
        co_return;
    }

    const int limit = qMin(m_pageSize, remainingCount);
    auto fetchPage = [this](const QueryState &pageQuery, int pageStartIndex,
                            int pageLimit) -> QCoro::Task<MediaQueryPage>
    {
        switch (pageQuery.mode)
        {
        case PersonMode:
            co_return co_await m_core->mediaService()->getItemsByPersonPage(
                pageQuery.targetId, pageQuery.sortBy, pageQuery.sortOrder,
                pageStartIndex, pageLimit);
        case FilteredMode:
            co_return co_await m_core->mediaService()->getItemsByFilterPage(
                pageQuery.genreFilter, pageQuery.tagFilter, pageQuery.studioFilter,
                pageQuery.sortBy, pageQuery.sortOrder, pageStartIndex, pageLimit);
        case LibraryMode:
        default:
            co_return co_await m_core->mediaService()->getLibraryItemsPage(
                pageQuery.targetId, pageQuery.sortBy, pageQuery.sortOrder,
                pageQuery.filters, pageQuery.includeTypes, pageStartIndex,
                pageLimit, pageQuery.recursive, pageQuery.includeChildCount);
        }
    };

    m_isLoadingMore = true;
    qDebug() << "[LibraryView] load more START"
             << "| generation=" << generation
             << "| startIndex=" << startIndex
             << "| limit=" << limit
             << "| loaded=" << m_loadedItems.size()
             << "| total=" << m_totalItemCount;

    try
    {
        MediaQueryPage page = co_await fetchPage(query, startIndex, limit);
        if (!guard || generation != m_requestGeneration)
            co_return;

        QList<MediaItem> pageItems = page.items;
        if (query.needsPlaylistEnrichment && !pageItems.isEmpty())
        {
            pageItems =
                co_await enrichPlaylistItemsForRemoval(std::move(pageItems));
            if (!guard || generation != m_requestGeneration)
                co_return;
        }

        if (pageItems.isEmpty())
        {
            m_hasMoreItems = false;
            qWarning() << "[LibraryView] load more returned empty page"
                       << "| generation=" << generation
                       << "| startIndex=" << startIndex
                       << "| limit=" << limit
                       << "| total=" << m_totalItemCount;
            updateStatsLabel();
        }
        else
        {
            m_loadedItems.append(pageItems);
            m_totalItemCount = qMax(page.totalRecordCount, m_loadedItems.size());
            m_hasMoreItems = m_loadedItems.size() < m_totalItemCount;

            updateStatsLabel();
            m_mediaGrid->setItems(m_loadedItems);
            qDebug() << "[LibraryView] load more END"
                     << "| generation=" << generation
                     << "| loaded=" << m_loadedItems.size()
                     << "| total=" << m_totalItemCount
                     << "| hasMore=" << m_hasMoreItems;
        }
    }
    catch (const std::exception &e)
    {
        if (!guard || generation != m_requestGeneration)
            co_return;
        m_statsLabel->setText(tr("Error Loading Items"));
        qDebug() << "Library View incremental fetching error:" << e.what();
    }

    if (guard && generation == m_requestGeneration)
    {
        m_isLoadingMore = false;
    }
}




void LibraryView::onMediaItemUpdated(const MediaItem &item)
{
    MediaItem mergedItem = item;

    
    QString currentId = (m_currentMode == PersonMode) ? m_currentPersonId : m_currentLibraryId;
    if (currentId == item.id)
    {
        m_currentMediaItem = item; 
        m_isFavorite = item.isFavorite();
        updateFavBtnState();
    }

    for (MediaItem &loadedItem : m_loadedItems)
    {
        if (loadedItem.id == item.id)
        {
            if (mergedItem.playlistId.trimmed().isEmpty()) {
                mergedItem.playlistId = loadedItem.playlistId;
            }
            if (mergedItem.playlistItemId.trimmed().isEmpty()) {
                mergedItem.playlistItemId = loadedItem.playlistItemId;
            }
            loadedItem = mergedItem;
            break;
        }
    }

    
    
    if (m_mediaGrid)
    {
        m_mediaGrid->updateItem(mergedItem);
    }
}

void LibraryView::onMediaItemRemoved(const QString& itemId)
{
    if (!m_mediaGrid) {
        return;
    }

    for (int i = m_loadedItems.size() - 1; i >= 0; --i) {
        if (m_loadedItems.at(i).id == itemId) {
            m_loadedItems.removeAt(i);
            break;
        }
    }

    if (m_totalItemCount > 0) {
        m_totalItemCount = qMax(m_loadedItems.size(), m_totalItemCount - 1);
    }
    m_hasMoreItems = m_loadedItems.size() < m_totalItemCount;
    m_mediaGrid->removeItem(itemId);
    updateStatsLabel();
}

QCoro::Task<QList<MediaItem>> LibraryView::enrichPlaylistItemsForRemoval(QList<MediaItem> items)
{
    if (m_currentMode != LibraryMode || m_currentMediaItem.type != "Playlist" ||
        m_currentLibraryId.trimmed().isEmpty()) {
        co_return items;
    }

    for (MediaItem& item : items) {
        item.playlistId = m_currentLibraryId;
    }

    bool hasMissingPlaylistEntryIds = false;
    for (const MediaItem& item : items) {
        if (item.playlistItemId.trimmed().isEmpty()) {
            hasMissingPlaylistEntryIds = true;
            break;
        }
    }

    if (!hasMissingPlaylistEntryIds || !m_core || !m_core->adminService()) {
        co_return items;
    }

    QPointer<LibraryView> guard(this);

    try
    {
        const QJsonObject response =
            co_await m_core->adminService()->getPlaylistItems(m_currentLibraryId);
        if (!guard) {
            co_return items;
        }

        QHash<QString, QStringList> playlistEntryIdsByItemId;
        const QJsonArray playlistItems = response.value("Items").toArray();
        for (const QJsonValue& value : playlistItems) {
            const QJsonObject obj = value.toObject();
            const QString itemId = obj.value("Id").toString().trimmed();
            const QString playlistItemId =
                obj.value("PlaylistItemId").toString().trimmed();
            if (itemId.isEmpty() || playlistItemId.isEmpty()) {
                continue;
            }

            playlistEntryIdsByItemId[itemId].append(playlistItemId);
        }

        int enrichedCount = 0;
        for (MediaItem& item : items) {
            if (!item.playlistItemId.trimmed().isEmpty()) {
                continue;
            }

            QStringList entryIds = playlistEntryIdsByItemId.value(item.id);
            if (entryIds.isEmpty()) {
                continue;
            }

            item.playlistItemId = entryIds.takeFirst();
            playlistEntryIdsByItemId.insert(item.id, entryIds);
            ++enrichedCount;
        }

        qDebug() << "[LibraryView] Playlist entry ids enriched"
                 << "| playlistId=" << m_currentLibraryId
                 << "| itemCount=" << items.size()
                 << "| enrichedCount=" << enrichedCount;
    }
    catch (const std::exception& e)
    {
        if (!guard) {
            co_return items;
        }

        qWarning() << "[LibraryView] Failed to enrich playlist entry ids"
                   << "| playlistId=" << m_currentLibraryId
                   << "| error=" << e.what();
    }

    co_return items;
}




QString LibraryView::currentPreferenceTargetId() const
{
    if (m_currentMode == PersonMode)
    {
        return m_currentPersonId;
    }

    if (m_currentMode == LibraryMode)
    {
        return m_currentLibraryId;
    }

    return QString();
}

void LibraryView::applyViewMode(bool isTile)
{
    m_viewSwitchBtn->blockSignals(true);
    m_viewSwitchBtn->setChecked(isTile);
    m_viewSwitchBtn->blockSignals(false);
    m_mediaGrid->setCardStyle(isTile ? MediaCardDelegate::LibraryTile : MediaCardDelegate::Poster);
}

void LibraryView::saveSortPreference()
{
    QString sid = m_core->serverManager()->activeProfile().id;
    QString targetId = currentPreferenceTargetId();
    if (sid.isEmpty() || targetId.isEmpty())
        return;

    auto *store = ConfigStore::instance();
    store->set(ConfigKeys::forLibrary(sid, targetId, ConfigKeys::LibrarySortIndex), m_sortButton->currentIndex());
    store->set(ConfigKeys::forLibrary(sid, targetId, ConfigKeys::LibrarySortDescending), m_sortButton->isDescending());
    qDebug() << "[LibraryView] Sort preference saved:"
             << "server=" << sid << "target=" << targetId << "index=" << m_sortButton->currentIndex()
             << "desc=" << m_sortButton->isDescending();
}

void LibraryView::restoreSortPreference()
{
    QString sid = m_core->serverManager()->activeProfile().id;
    QString targetId = currentPreferenceTargetId();
    if (sid.isEmpty() || targetId.isEmpty())
        return;

    auto *store = ConfigStore::instance();
    int savedIndex = store->get<int>(ConfigKeys::forLibrary(sid, targetId, ConfigKeys::LibrarySortIndex), 0);
    bool savedDesc = store->get<bool>(ConfigKeys::forLibrary(sid, targetId, ConfigKeys::LibrarySortDescending), true);

    m_sortButton->blockSignals(true);
    m_sortButton->setCurrentIndex(savedIndex);
    m_sortButton->setDescending(savedDesc);
    m_sortButton->blockSignals(false);

    qDebug() << "[LibraryView] Sort preference restored:"
             << "server=" << sid << "target=" << targetId << "index=" << savedIndex << "desc=" << savedDesc;
}

void LibraryView::saveViewPreference()
{
    QString sid = m_core->serverManager()->activeProfile().id;
    QString targetId = currentPreferenceTargetId();
    if (sid.isEmpty() || targetId.isEmpty())
        return;

    const QString viewMode =
        m_viewSwitchBtn->isChecked() ? QStringLiteral("tile") : QStringLiteral("poster");

    auto *store = ConfigStore::instance();
    store->set(ConfigKeys::forLibrary(sid, targetId, ConfigKeys::LibraryViewMode), viewMode);

    qDebug() << "[LibraryView] View preference saved:"
             << "server=" << sid << "target=" << targetId << "mode=" << viewMode;
}

void LibraryView::restoreViewPreference()
{
    auto *store = ConfigStore::instance();
    const QString defaultViewMode =
        store->get<QString>(ConfigKeys::DefaultLibraryView, QStringLiteral("poster"));

    QString sid = m_core->serverManager()->activeProfile().id;
    QString targetId = currentPreferenceTargetId();
    QString viewMode = defaultViewMode;

    if (!sid.isEmpty() && !targetId.isEmpty())
    {
        viewMode =
            store->get<QString>(ConfigKeys::forLibrary(sid, targetId, ConfigKeys::LibraryViewMode),
                                defaultViewMode);
        qDebug() << "[LibraryView] View preference restored:"
                 << "server=" << sid << "target=" << targetId << "mode=" << viewMode
                 << "default=" << defaultViewMode;
    }
    else
    {
        qDebug() << "[LibraryView] View preference fallback to default:"
                 << "mode=" << defaultViewMode;
    }

    applyViewMode(viewMode == QLatin1String("tile"));
}
