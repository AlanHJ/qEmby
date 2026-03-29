#include "searchview.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include "../../components/mediagridwidget.h"
#include "../media/mediacarddelegate.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QButtonGroup>
#include <QScrollArea>
#include <QDebug>
#include <QPointer> 

namespace {
enum SearchTabId {
    SearchTabAll = 0,
    SearchTabMovies = 1,
    SearchTabShows = 2,
    SearchTabCollections = 3,
    SearchTabPeople = 4
};

QString searchIncludeItemTypesForTab(int tabId)
{
    switch (tabId) {
    case SearchTabAll:
        return QStringLiteral("Movie,Series,BoxSet,Person");
    case SearchTabShows:
        return QStringLiteral("Series");
    case SearchTabCollections:
        return QStringLiteral("BoxSet");
    case SearchTabPeople:
        return QStringLiteral("Person");
    case SearchTabMovies:
    default:
        return QStringLiteral("Movie");
    }
}
}

SearchView::SearchView(QEmbyCore* core, QWidget *parent)
    : BaseView(core, parent)
{
    setObjectName("search-view");
    setProperty("showGlobalSearch", true);
    setAttribute(Qt::WA_StyledBackground, true);

    setupUi();

    
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    connect(m_tabGroup, &QButtonGroup::idClicked, this, &SearchView::onTabChanged);
#else
    connect(m_tabGroup, QOverload<int>::of(&QButtonGroup::buttonClicked), this, &SearchView::onTabChanged);
#endif

    

    connect(m_mediaGrid, &MediaGridWidget::itemClicked, this, [this](const MediaItem& item){
        if (item.type == "BoxSet" || item.type == "Playlist" || item.type == "Folder") {
            Q_EMIT navigateToFolder(item.id, item.name);
        } else if (item.type == "Person") { 
            Q_EMIT navigateToPerson(item.id, item.name);
        } else {
            Q_EMIT navigateToDetail(item.id, item.name);
        }
    });

    
    
    
    connect(m_mediaGrid, &MediaGridWidget::playRequested, this, &BaseView::handlePlayRequested);
    connect(m_mediaGrid, &MediaGridWidget::favoriteRequested, this, &BaseView::handleFavoriteRequested);
    connect(m_mediaGrid, &MediaGridWidget::moreMenuRequested, this, &BaseView::handleMoreMenuRequested);
    
}

void SearchView::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    
    QScrollArea* headerScrollArea = new QScrollArea(this);
    headerScrollArea->setFrameShape(QFrame::NoFrame);
    headerScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    headerScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    headerScrollArea->setWidgetResizable(true);

    headerScrollArea->setStyleSheet("QScrollArea { background: transparent; border: none; } "
                                    "QWidget#search-header-container { background: transparent; }");

    headerScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    headerScrollArea->setFixedHeight(65);

    QWidget* headerContainer = new QWidget(headerScrollArea);
    headerContainer->setObjectName("search-header-container");

    auto *headerLayout = new QHBoxLayout(headerContainer);
    headerLayout->setContentsMargins(20, 15, 20, 10);
    headerLayout->setSpacing(20);
    headerLayout->setAlignment(Qt::AlignVCenter);

    setupTopBar(headerLayout);
    headerScrollArea->setWidget(headerContainer);

    mainLayout->addWidget(headerScrollArea);
    

    
    m_mediaGrid = new MediaGridWidget(m_core, this);
    mainLayout->addWidget(m_mediaGrid, 1);
}

void SearchView::setupTopBar(QHBoxLayout* headerLayout)
{
    
    m_titleLabel = new QLabel(tr("Search Results"), this);
    m_titleLabel->setObjectName("library-title");
    m_titleLabel->setStyleSheet("padding: 0px;");
    headerLayout->addWidget(m_titleLabel);

    
    QWidget* tabBarWidget = new QWidget(this);
    auto* tabLayout = new QHBoxLayout(tabBarWidget);
    tabLayout->setContentsMargins(0, 0, 0, 0);
    tabLayout->setSpacing(8);

    m_tabGroup = new QButtonGroup(this);
    m_tabGroup->setExclusive(true);

    auto *tabAll = new QPushButton(tr("All"), tabBarWidget);
    tabAll->setObjectName("library-tab-btn");
    tabAll->setCheckable(true);
    tabAll->setChecked(true);
    m_tabGroup->addButton(tabAll, SearchTabAll);

    auto *tabMovies = new QPushButton(tr("Movies"), tabBarWidget);
    tabMovies->setObjectName("library-tab-btn");
    tabMovies->setCheckable(true);
    m_tabGroup->addButton(tabMovies, SearchTabMovies);

    auto *tabShows = new QPushButton(tr("Shows"), tabBarWidget);
    tabShows->setObjectName("library-tab-btn");
    tabShows->setCheckable(true);
    m_tabGroup->addButton(tabShows, SearchTabShows);

    auto *tabCollections = new QPushButton(tr("Collections"), tabBarWidget);
    tabCollections->setObjectName("library-tab-btn");
    tabCollections->setCheckable(true);
    m_tabGroup->addButton(tabCollections, SearchTabCollections);

    auto *tabActors = new QPushButton(tr("People"), tabBarWidget);
    tabActors->setObjectName("library-tab-btn");
    tabActors->setCheckable(true);
    m_tabGroup->addButton(tabActors, SearchTabPeople);

    tabLayout->addWidget(tabAll);
    tabLayout->addWidget(tabMovies);
    tabLayout->addWidget(tabShows);
    tabLayout->addWidget(tabCollections);
    tabLayout->addWidget(tabActors);

    headerLayout->addWidget(tabBarWidget);
    headerLayout->addStretch();

    
    QWidget* filterBarWidget = new QWidget(this);
    auto* filterLayout = new QHBoxLayout(filterBarWidget);
    filterLayout->setContentsMargins(0, 0, 0, 0);
    filterLayout->setSpacing(10);

    
    m_btnViewToggle = new QPushButton(filterBarWidget);
    m_btnViewToggle->setObjectName("icon-action-btn");
    m_btnViewToggle->setCheckable(true);
    m_btnViewToggle->setFixedSize(32, 32);
    m_btnViewToggle->setCursor(Qt::PointingHandCursor);
    m_btnViewToggle->setToolTip(tr("Toggle View Mode"));;
    connect(m_btnViewToggle, &QPushButton::clicked, this, &SearchView::onViewModeChanged);

    m_statsLabel = new QLabel(tr("0 Results"), filterBarWidget);
    m_statsLabel->setObjectName("library-stats-label");
    m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

    filterLayout->addWidget(m_btnViewToggle);
    filterLayout->addWidget(m_statsLabel);

    headerLayout->addWidget(filterBarWidget);
}


QCoro::Task<void> SearchView::performSearch(QString query)
{
    
    QPointer<SearchView> guard(this);

    query = query.trimmed();
    if (query.isEmpty()) co_return;

    m_currentQuery = query;
    m_titleLabel->setText(tr("Search: \"%1\"").arg(query));
    m_statsLabel->setText(tr("Searching..."));

    
    m_currentItems.clear();
    m_mediaGrid->setItems(QList<MediaItem>());

    const int checkedTabId = m_tabGroup ? m_tabGroup->checkedId() : SearchTabAll;
    const QString typeFilter = searchIncludeItemTypesForTab(checkedTabId);
    qDebug() << "[SearchView] Performing search | query:" << query
             << "| tabId:" << checkedTabId
             << "| includeTypes:" << typeFilter;

    try {
        
        QList<MediaItem> results = co_await m_core->mediaService()->searchMedia(
            query,
            typeFilter,
            "",
            "",
            0);

        if (!guard) co_return; 

        m_statsLabel->setText(tr("%1 Results").arg(results.size()));
        qDebug() << "[SearchView] Search finished | query:" << query
                 << "| includeTypes:" << typeFilter
                 << "| resultCount:" << results.size();
        
        m_currentItems = results;
        m_mediaGrid->setItems(m_currentItems);

    } catch (const std::exception& e) {
        if (!guard) co_return; 
        m_statsLabel->setText(tr("Error Loading Results"));
        qDebug() << "[SearchView] Search failed | query:" << query
                 << "| includeTypes:" << typeFilter
                 << "| error:" << e.what();
    }
}


QCoro::Task<void> SearchView::onTabChanged(int tabId)
{
    
    QPointer<SearchView> guard(this);
    Q_UNUSED(tabId);
    if (!m_currentQuery.isEmpty()) {
        co_await performSearch(m_currentQuery);
        if (!guard) co_return; 
    }
}

void SearchView::onViewModeChanged()
{
    if (m_btnViewToggle->isChecked()) {
        m_mediaGrid->setCardStyle(MediaCardDelegate::LibraryTile);
    } else {
        m_mediaGrid->setCardStyle(MediaCardDelegate::Poster);
    }

    if (!m_currentItems.isEmpty()) {
        m_mediaGrid->setItems(m_currentItems);
    }
}




void SearchView::onMediaItemUpdated(const MediaItem& item)
{
    
    for (int i = 0; i < m_currentItems.size(); ++i) {
        if (m_currentItems[i].id == item.id) {
            m_currentItems[i] = item;
            break;
        }
    }

    
    
    if (m_mediaGrid) {
        m_mediaGrid->updateItem(item);
    }
}
