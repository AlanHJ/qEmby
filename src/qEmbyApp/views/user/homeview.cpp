#include "homeview.h"
#include "../../components/searchcompleterpopup.h"
#include "../../components/elidedlabel.h"
#include "../../components/moderntoast.h"
#include "../../components/slidingstackedwidget.h"
#include "../../managers/searchhistorymanager.h"
#include "../../managers/playbackmanager.h"
#include "../admin/manageview.h" 
#include "../media/detailview.h"
#include "../media/libraryview.h"
#include "../media/playerview.h"
#include "../media/seasonview.h" 
#include "../search/searchview.h"
#include "../settings/settingsview.h"
#include "categoryview.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include "dashboardview.h"
#include "favoritesview.h"
#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QBoxLayout>
#include <QCompleter>
#include <QCursor>
#include <QDebug>
#include <QEvent>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMargins>
#include <QPointer> 
#include <QPropertyAnimation>
#include <QPushButton>
#include <QShowEvent>
#include <QStringListModel>
#include <QTimer>
#include <QVBoxLayout>
#include <models/profile/serverprofile.h>
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>

namespace
{
constexpr int kFloatingSidebarWidth = 240;
constexpr int kPinnedSidebarWidth = 136;
constexpr int kSidebarHiddenOffset = 30;
constexpr int kLeftEdgeTriggerWidth = 15;
constexpr int kRightEdgeTriggerWidth = 20;
} 

HomeView::HomeView(QEmbyCore *core, QWidget *parent) : QWidget(parent), m_core(core)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setObjectName("home-view");

    
    QString pos = ConfigStore::instance()->get<QString>(ConfigKeys::SidebarPosition, "left");
    m_sidebarOnRight = (pos == "right");
    m_sidebarPinned = ConfigStore::instance()->get<bool>(ConfigKeys::SidebarPinned, false);

    setupUi();
    if (!m_sidebarPinned)
    {
        hideSidebar();
    }

    
    PlaybackManager::instance()->init(m_core);
    connect(PlaybackManager::instance(), &PlaybackManager::requestEmbeddedPlay, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { pushView(createPlayerView(id, title, url, ticks, extraData)); });

    
    
    connect(PlaybackManager::instance(), &PlaybackManager::playbackFinished, this,
            [this]()
            {
                QWidget *current = m_contentSwitcher->currentWidget();
                if (current)
                {
                    QShowEvent showEvent;
                    QCoreApplication::sendEvent(current, &showEvent);
                }
            });
}


PlayerView *HomeView::activePlayerView() const
{
    QWidget *current = m_contentSwitcher->currentWidget();
    if (current && current->property("routeType").toString() == "PlayerView")
    {
        return qobject_cast<PlayerView *>(current);
    }
    return nullptr;
}

void HomeView::setupUi()
{
    this->setProperty("showGlobalSearch", true);
    this->setProperty("viewTitle", qApp->applicationName());
    this->setProperty("showGlobalBack", true);
    this->setProperty("showGlobalHome", true);
    this->setProperty("showGlobalFav", true);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    
    m_contentLayout = new QHBoxLayout();
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(0);

    m_contentSwitcher = new SlidingStackedWidget(this);
    m_contentSwitcher->setObjectName("home-content");

    
    m_dashboardView = new DashboardView(m_core, this);
    m_favoritesView = new FavoritesView(m_core, this);

    connect(m_dashboardView, &DashboardView::navigateToLibrary, this,
            [this](const QString &id, const QString &name)
            {
                m_libraryList->clearSelection();

                
                for (int i = 0; i < m_libraryList->count(); ++i)
                {
                    if (m_libraryList->item(i)->data(Qt::UserRole).toString() == id)
                    {
                        m_libraryList->item(i)->setSelected(true);
                        break;
                    }
                }
                pushView(createLibraryView(id, name));
            });

    connect(m_dashboardView, &DashboardView::navigateToCategory, this,
            [this](const QString &categoryId, const QString &title)
            {
                m_libraryList->clearSelection();
                pushView(createCategoryView(categoryId, title));
            });

    connect(m_favoritesView, &FavoritesView::navigateToCategory, this,
            [this](const QString &categoryId, const QString &title)
            {
                m_libraryList->clearSelection();
                pushView(createCategoryView("Favorite_" + categoryId, title));
            });

    auto navigateToDetailSlot = [this](const QString &itemId, const QString &itemName)
    { pushView(createDetailView(itemId, itemName)); };

    connect(m_dashboardView, &DashboardView::navigateToDetail, this, navigateToDetailSlot);
    connect(m_favoritesView, &FavoritesView::navigateToDetail, this, navigateToDetailSlot);
    auto navigateToFolderSlot = [this](const QString &id, const QString &name)
    { pushView(createLibraryView(id, name)); };
    connect(m_favoritesView, &FavoritesView::navigateToFolder, this, navigateToFolderSlot);
    auto navigateToPersonSlot = [this](const QString &id, const QString &name)
    { pushView(createPersonView(id, name)); };
    connect(m_favoritesView, &FavoritesView::navigateToPerson, this, navigateToPersonSlot);

    
    
    
    auto navigateToPlayerSlot = [this](const QString &id, const QString &title, const QString &url, long long ticks,
                                       const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); };
    connect(m_dashboardView, &BaseView::navigateToPlayer, this, navigateToPlayerSlot);
    connect(m_favoritesView, &BaseView::navigateToPlayer, this, navigateToPlayerSlot);

    
    auto navigateToSeasonSlot = [this](const QString &seriesId, const QString &seasonId, const QString &seasonName)
    { pushView(createSeasonView(seriesId, seasonId, seasonName)); };
    connect(m_dashboardView, &BaseView::navigateToSeason, this, navigateToSeasonSlot);
    connect(m_favoritesView, &BaseView::navigateToSeason, this, navigateToSeasonSlot);

    m_contentSwitcher->addWidget(m_dashboardView);
    m_contentSwitcher->addWidget(m_favoritesView);
    m_lastRouteType = m_contentSwitcher->currentWidget()
                          ? m_contentSwitcher->currentWidget()->property("routeType").toString()
                          : QString();

    connect(m_contentSwitcher, &QStackedWidget::currentChanged, this,
            [this](int )
            {
                QWidget *current = m_contentSwitcher->currentWidget();
                const QString currentRouteType = current ? current->property("routeType").toString() : QString();

                if (m_lastRouteType == "ManageView" && currentRouteType != "ManageView")
                {
                    qDebug() << "[HomeView] Left management view — refreshing sidebar library list";
                    scheduleProfileRefresh();
                }
                m_lastRouteType = currentRouteType;

                Q_EMIT homeContentSwitched();

                if (current)
                {
                    bool isImmersive = current->property("isImmersive").toBool();
                    syncSidebarVisibility();
                    
                    Q_EMIT immersiveStateChanged(isImmersive);
                }
            });

    m_contentLayout->addWidget(m_contentSwitcher, 1);
    mainLayout->addLayout(m_contentLayout);

    
    m_navStack.clear();

    setupSidebar();

    m_edgeTrigger = new QWidget(this);
    m_edgeTrigger->setFixedWidth(15);
    m_edgeTrigger->setCursor(Qt::PointingHandCursor);
    m_edgeTrigger->setAttribute(Qt::WA_TransparentForMouseEvents, false);
    m_edgeTrigger->installEventFilter(this);

    
    
    
    if (m_sidebarPinned)
    {
        
        m_sidebar->hide();
        m_edgeTrigger->hide();
    }
}


QWidget *HomeView::createDetailView(const QString &itemId, const QString &itemName)
{
    auto *view = new DetailView(m_core, this);

    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "DetailView");
    view->setProperty("routeId", itemId);
    view->setProperty("routeTitle", itemName);

    
    view->loadItem(itemId);

    connect(view, &DetailView::navigateToDetail, this,
            [this](const QString &id, const QString &name) { pushView(createDetailView(id, name)); });
    connect(view, &DetailView::navigateToFolder, this,
            [this](const QString &id, const QString &name) { pushView(createLibraryView(id, name)); });
    connect(view, &BaseView::navigateToPerson, this,
            [this](const QString &id, const QString &name)
            {
                pushView(createPersonView(id, name)); 
            });

    connect(view, &BaseView::navigateToPlayer, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); });

    
    connect(view, &BaseView::navigateToSeason, this,
            [this](const QString &seriesId, const QString &seasonId, const QString &seasonName)
            { pushView(createSeasonView(seriesId, seasonId, seasonName)); });

    connect(view, &DetailView::triggerSearch, this, &HomeView::triggerSearch);
    connect(view, &BaseView::navigateToFilteredView, this,
            [this](const QString &type, const QString &value)
            { pushView(createFilteredView(type, value)); });

    return view;
}

QWidget *HomeView::createCategoryView(const QString &categoryId, const QString &title)
{
    auto *view = new CategoryView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "CategoryView");
    view->setProperty("routeId", categoryId);
    view->setProperty("routeTitle", title);

    view->loadCategory(categoryId, title);

    connect(view, &CategoryView::navigateToDetail, this,
            [this](const QString &id, const QString &name) { pushView(createDetailView(id, name)); });

    connect(view, &CategoryView::navigateToFolder, this,
            [this](const QString &id, const QString &name) { pushView(createLibraryView(id, name)); });

    connect(view, &CategoryView::navigateToPerson, this,
            [this](const QString &id, const QString &name) { pushView(createPersonView(id, name)); });

    
    connect(view, &BaseView::navigateToPlayer, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); });

    
    connect(view, &BaseView::navigateToSeason, this,
            [this](const QString &seriesId, const QString &seasonId, const QString &seasonName)
            { pushView(createSeasonView(seriesId, seasonId, seasonName)); });

    return view;
}

QWidget *HomeView::createLibraryView(const QString &libraryId, const QString &title)
{
    auto *view = new LibraryView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "LibraryView");
    view->setProperty("routeId", libraryId);
    view->setProperty("routeTitle", title);

    view->loadLibrary(libraryId, title);

    connect(view, &LibraryView::navigateToDetail, this,
            [this](const QString &id, const QString &name) { pushView(createDetailView(id, name)); });

    connect(view, &LibraryView::navigateToFolder, this,
            [this](const QString &id, const QString &name) { pushView(createLibraryView(id, name)); });

    connect(view, &BaseView::navigateToPerson, this,
            [this](const QString &id, const QString &name) { pushView(createPersonView(id, name)); });

    
    connect(view, &BaseView::navigateToPlayer, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); });

    
    connect(view, &BaseView::navigateToSeason, this,
            [this](const QString &seriesId, const QString &seasonId, const QString &seasonName)
            { pushView(createSeasonView(seriesId, seasonId, seasonName)); });

    return view;
}


QWidget *HomeView::createPersonView(const QString &personId, const QString &personName)
{
    
    auto *view = new LibraryView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "PersonView"); 
    view->setProperty("routeId", personId);
    view->setProperty("routeTitle", personName);

    
    
    
    connect(view, &BaseView::navigateBack, this, &HomeView::navigateBack);

    
    connect(view, &BaseView::navigateToDetail, this,
            [this](const QString &id, const QString &name) { pushView(createDetailView(id, name)); });

    
    connect(view, &BaseView::navigateToPerson, this,
            [this](const QString &id, const QString &name) { pushView(createPersonView(id, name)); });

    
    connect(view, &BaseView::navigateToPlayer, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); });

    
    connect(view, &BaseView::navigateToSeason, this,
            [this](const QString &seriesId, const QString &seasonId, const QString &seasonName)
            { pushView(createSeasonView(seriesId, seasonId, seasonName)); });

    
    connect(view, &BaseView::triggerSearch, this, [this](const QString &query) { triggerSearch(query); });

    
    
    
    view->loadPerson(personId, personName);

    return view;
}

QWidget *HomeView::createSearchView(const QString &query)
{
    auto *view = new SearchView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "SearchView");
    view->setProperty("routeId", query);

    view->performSearch(query);

    connect(view, &SearchView::navigateToDetail, this,
            [this](const QString &id, const QString &name) { pushView(createDetailView(id, name)); });

    connect(view, &SearchView::navigateToFolder, this,
            [this](const QString &id, const QString &name) { pushView(createLibraryView(id, name)); });

    connect(view, &SearchView::navigateToPerson, this,
            [this](const QString &id, const QString &name) { pushView(createPersonView(id, name)); });

    
    connect(view, &BaseView::navigateToPlayer, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); });

    
    connect(view, &BaseView::navigateToSeason, this,
            [this](const QString &seriesId, const QString &seasonId, const QString &seasonName)
            { pushView(createSeasonView(seriesId, seasonId, seasonName)); });

    return view;
}




QWidget *HomeView::createFilteredView(const QString &filterType, const QString &filterValue)
{
    auto *view = new LibraryView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "FilteredView");
    view->setProperty("routeId", filterType + ":" + filterValue);
    view->setProperty("routeTitle", filterValue);
    view->setProperty("routeExtraId", filterType);

    connect(view, &BaseView::navigateToDetail, this,
            [this](const QString &id, const QString &name) { pushView(createDetailView(id, name)); });
    connect(view, &BaseView::navigateToFolder, this,
            [this](const QString &id, const QString &name) { pushView(createLibraryView(id, name)); });
    connect(view, &BaseView::navigateToPerson, this,
            [this](const QString &id, const QString &name) { pushView(createPersonView(id, name)); });
    connect(view, &BaseView::navigateToPlayer, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); });
    connect(view, &BaseView::navigateToSeason, this,
            [this](const QString &seriesId, const QString &seasonId, const QString &seasonName)
            { pushView(createSeasonView(seriesId, seasonId, seasonName)); });

    view->loadFiltered(filterType, filterValue);

    return view;
}




QWidget *HomeView::createSeasonView(const QString &seriesId, const QString &seasonId, const QString &seasonName)
{
    auto *view = new SeasonView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "SeasonView");
    view->setProperty("routeId", seasonId);
    
    view->setProperty("routeExtraId", seriesId);
    view->setProperty("routeTitle", seasonName);

    
    view->loadSeason(seriesId, seasonId, seasonName);

    connect(view, &BaseView::navigateBack, this, &HomeView::navigateBack);

    
    connect(view, &BaseView::navigateToDetail, this,
            [this](const QString &id, const QString &name) { pushView(createDetailView(id, name)); });

    
    connect(view, &BaseView::navigateToPlayer, this,
            [this](const QString &id, const QString &title, const QString &url, long long ticks,
                   const QVariant &extraData) { launchPlayer(id, title, url, ticks, extraData); });

    return view;
}

QWidget *HomeView::createPlayerView(const QString &mediaId, const QString &title, const QString &streamUrl,
                                    long long startPositionTicks, const QVariant &extraData)
{
    auto *view = new PlayerView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "PlayerView");
    view->setProperty("routeId", mediaId);
    view->setProperty("routeTitle", title);

    
    connect(view, &BaseView::navigateBack, this, &HomeView::navigateBack);

    
    view->playMedia(mediaId, title, streamUrl, startPositionTicks, extraData);
    return view;
}




void HomeView::launchPlayer(const QString &mediaId, const QString &title, const QString &streamUrl,
                            long long startPositionTicks, const QVariant &extraData)
{
    PlaybackManager::instance()->startPlayback(mediaId, title, streamUrl, startPositionTicks, extraData);
}

QWidget *HomeView::createSettingsView()
{
    auto *view = new SettingsView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "SettingsView");
    view->setProperty("routeId", "settings_global");
    view->setProperty("routeTitle", tr("Settings"));

    
    connect(view, &BaseView::navigateBack, this, &HomeView::navigateBack);

    return view;
}

QWidget *HomeView::createManageView()
{
    auto *view = new ManageView(m_core, this);
    view->setProperty("isDynamic", true);
    view->setProperty("routeType", "ManageView");
    view->setProperty("routeId", "manage_global");
    view->setProperty("routeTitle", tr("Server Management"));

    connect(view, &BaseView::navigateBack, this, &HomeView::navigateBack);

    return view;
}

void HomeView::setupSidebar()
{
    m_sidebar = new QWidget(this);
    m_sidebar->setObjectName("floating-sidebar");
    m_sidebar->setProperty("sidebarSide", m_sidebarOnRight ? "right" : "left");

    
    if (!m_sidebarPinned)
    {
        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(25);
        shadow->setColor(QColor(0, 0, 0, 30));
        shadow->setOffset(m_sidebarOnRight ? -4 : 4, 0);
        m_sidebar->setGraphicsEffect(shadow);
    }

    auto *layout = new QVBoxLayout(m_sidebar);
    layout->setContentsMargins(16, 20, 16, 20);
    layout->setSpacing(6);

    
    auto *serverInfoWidget = new QWidget(m_sidebar);
    m_serverInfoLayout = new QBoxLayout(QBoxLayout::LeftToRight, serverInfoWidget);
    m_serverInfoLayout->setContentsMargins(8, 0, 8, 10);
    m_serverInfoLayout->setSpacing(10);

    m_serverIconLabel = new QLabel(serverInfoWidget);
    m_serverIconLabel->setFixedSize(32, 32);
    m_serverIconLabel->setScaledContents(true);

    m_serverNameLayout = new QVBoxLayout();
    m_serverNameLayout->setContentsMargins(0, 0, 0, 0);
    m_serverNameLayout->setSpacing(0);
    m_serverNameLayout->setAlignment(Qt::AlignVCenter);

    m_serverNameLabel = new ElidedLabel(serverInfoWidget);
    m_serverNameLabel->setObjectName("sidebar-server-name");

    m_serverAddressLabel = new ElidedLabel(serverInfoWidget);
    m_serverAddressLabel->setObjectName("sidebar-server-address");

    m_serverNameLayout->addWidget(m_serverNameLabel);
    m_serverNameLayout->addWidget(m_serverAddressLabel);

    m_serverInfoLayout->addWidget(m_serverIconLabel, 0, Qt::AlignVCenter);
    m_serverInfoLayout->addLayout(m_serverNameLayout);
    layout->addWidget(serverInfoWidget);

    
    m_searchBox = new QLineEdit(m_sidebar);
    m_searchBox->setObjectName("sidebar-search");
    m_searchBox->setPlaceholderText(tr("Search..."));
    auto *searchAction = new QAction(QIcon(":/svg/light/search.svg"), tr("Search"), this);
    m_searchBox->addAction(searchAction, QLineEdit::LeadingPosition);
    layout->addWidget(m_searchBox);
    layout->addSpacing(10);

    
    connect(m_searchBox, &QLineEdit::returnPressed, this,
            [this]()
            {
                if (m_searchCompleter && m_searchCompleter->popup()) {
                    m_searchCompleter->popup()->hide();
                }
                triggerSearch(m_searchBox->text());
                m_searchBox->clear(); 
                if (m_searchCompleter && m_searchCompleter->popup()) {
                    m_searchCompleter->popup()->hide();
                }
            });
    setupSearchHistory();

    m_btnHome = new QPushButton(tr("Home"), m_sidebar);
    m_btnFavorites = new QPushButton(tr("Favorites"), m_sidebar);

    m_btnHome->setObjectName("sidebar-btn");
    m_btnHome->setIcon(QIcon(":/svg/light/home.svg"));

    m_btnFavorites->setObjectName("sidebar-btn");
    m_btnFavorites->setIcon(QIcon(":/svg/light/heart.svg"));

    layout->addWidget(m_btnHome);
    layout->addWidget(m_btnFavorites);

    auto *sep1 = new QFrame(m_sidebar);
    sep1->setObjectName("sidebar-sep");
    layout->addSpacing(8);
    layout->addWidget(sep1);
    layout->addSpacing(8);

    
    auto *libTitle = new QLabel(tr("MEDIA"), m_sidebar);
    libTitle->setObjectName("sidebar-title");
    layout->addWidget(libTitle);

    m_libraryList = new QListWidget(m_sidebar);
    m_libraryList->setObjectName("sidebar-list");
    m_libraryList->setFocusPolicy(Qt::NoFocus);
    layout->addWidget(m_libraryList, 1);

    

    auto *sep2 = new QFrame(m_sidebar);
    sep2->setObjectName("sidebar-sep");
    layout->addSpacing(8);
    layout->addWidget(sep2);
    layout->addSpacing(8);

    
    auto *userInfoWidget = new QWidget(m_sidebar);
    m_userInfoLayout = new QHBoxLayout(userInfoWidget);
    m_userInfoLayout->setContentsMargins(0, 0, 0, 10);
    m_userInfoLayout->setSpacing(10);
    m_userInfoLayout->setAlignment(Qt::AlignVCenter);

    m_userAvatarLabel = new QLabel(userInfoWidget);
    m_userAvatarLabel->setFixedSize(20, 20);
    m_userAvatarLabel->setScaledContents(true);

    m_userNameLabel = new ElidedLabel(userInfoWidget);
    m_userNameLabel->setObjectName("sidebar-user-name");

    m_userInfoLayout->addWidget(m_userAvatarLabel);
    m_userInfoLayout->addWidget(m_userNameLabel, 1);
    layout->addWidget(userInfoWidget);

    m_btnSettings = new QPushButton(tr("Settings"), m_sidebar);
    m_btnManage = new QPushButton(tr("Manage"), m_sidebar);
    m_btnLogout = new QPushButton(tr("Logout"), m_sidebar);

    m_btnSettings->setObjectName("sidebar-btn");
    m_btnSettings->setIcon(QIcon(":/svg/light/settings.svg"));

    m_btnManage->setObjectName("sidebar-btn");
    m_btnManage->setIcon(QIcon(":/svg/light/server.svg"));

    m_btnLogout->setObjectName("sidebar-btn-danger");
    m_btnLogout->setIcon(QIcon(":/svg/light/logout.svg"));

    layout->addWidget(m_btnSettings);
    layout->addWidget(m_btnManage);
    layout->addWidget(m_btnLogout);

    
    connect(m_btnLogout, &QPushButton::clicked, this,
            [this]()
            {
                
                if (m_contentSwitcher->currentWidget() != m_dashboardView)
                {
                    resetToView(m_dashboardView);
                }
                Q_EMIT logoutRequested();
            });

    
    connect(m_btnHome, &QPushButton::clicked, this,
            [this]()
            {
                if (!canGoHome())
                {
                    ModernToast::showMessage(tr("Refreshing Home..."), 1000);
                }
                goHome(); 
            });

    connect(m_btnFavorites, &QPushButton::clicked, this,
            [this]()
            {
                if (!canGoFav())
                {
                    ModernToast::showMessage(tr("Refreshing Favorites..."), 1000);
                }
                goFav(); 
            });

    connect(m_btnSettings, &QPushButton::clicked, this,
            [this]()
            {
                m_libraryList->clearSelection();

                
                QWidget *current = m_contentSwitcher->currentWidget();
                if (current && current->property("routeType").toString() == "SettingsView")
                {
                    return;
                }

                
                pushView(createSettingsView());
            });
    connect(m_btnManage, &QPushButton::clicked, this,
            [this]()
            {
                m_libraryList->clearSelection();

                
                QWidget *current = m_contentSwitcher->currentWidget();
                if (current && current->property("routeType").toString() == "ManageView")
                {
                    return;
                }

                pushView(createManageView());
            });

    connect(m_libraryList, &QListWidget::itemClicked, this,
            [this](QListWidgetItem *item)
            {
                QString viewId = item->data(Qt::UserRole).toString();
                QString cleanName = item->text().mid(3);

                QWidget *current = m_contentSwitcher->currentWidget();
                
                if (current && current->property("routeId").toString() == viewId)
                {
                    return;
                }
                pushView(createLibraryView(viewId, cleanName));
            });

    m_sidebarAnim = new QPropertyAnimation(m_sidebar, "pos", this);
    m_sidebarAnim->setDuration(350);
    m_sidebarAnim->setEasingCurve(QEasingCurve::OutCubic);

    m_sidebar->installEventFilter(this);
    m_sidebar->setMouseTracking(true); 

    
    m_sidebarAutoHideTimer = new QTimer(this);
    m_sidebarAutoHideTimer->setSingleShot(true);
    m_sidebarAutoHideTimer->setInterval(5000);
    connect(m_sidebarAutoHideTimer, &QTimer::timeout, this,
            [this]()
            {
                
                QPoint globalPos = QCursor::pos();
                QPoint localPos = m_sidebar->mapFromGlobal(globalPos);
                if (!m_sidebar->rect().contains(localPos))
                {
                    hideSidebar();
                }
                else
                {
                    
                    m_sidebarAutoHideTimer->start();
                }
            });

    
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &value)
            {
                if (key == ConfigKeys::SidebarPosition)
                {
                    m_sidebarOnRight = (value.toString() == "right");
                    applySidebarPosition();
                }
                else if (key == ConfigKeys::SidebarPinned)
                {
                    bool pinned = value.toBool();
                    m_sidebarPinned = pinned;
                    applySidebarPinned(pinned);
                }
            });

    applySidebarMetrics(m_sidebarPinned);
}


void HomeView::triggerSearch(const QString &query)
{
    const QString trimmedQuery = query.trimmed();
    if (trimmedQuery.isEmpty())
        return;

    SearchHistoryManager::instance()->recordSearch(currentSearchServerId(),
                                                   trimmedQuery);

    
    m_libraryList->clearSelection();
    pushView(createSearchView(trimmedQuery));
}

void HomeView::setupSearchHistory()
{
    if (!m_searchBox) {
        return;
    }

    m_searchHistoryModel = new QStringListModel(this);
    m_searchCompleter = new QCompleter(m_searchHistoryModel, this);
    m_searchCompleter->setCaseSensitivity(Qt::CaseInsensitive);
    m_searchCompleter->setFilterMode(Qt::MatchContains);
    m_searchCompleter->setCompletionMode(QCompleter::PopupCompletion);
    m_searchCompleter->setMaxVisibleItems(8);
    m_searchCompleter->setPopup(new SearchCompleterPopup());
    if (auto *popup =
            qobject_cast<SearchCompleterPopup *>(m_searchCompleter->popup())) {
        popup->setMaxVisibleRows(m_searchCompleter->maxVisibleItems());
    }
    m_searchBox->setCompleter(m_searchCompleter);

    connect(m_searchBox, &QLineEdit::textEdited, this,
            [this](const QString &text) { updateSearchCompleter(text); });

    connect(SearchHistoryManager::instance(), &SearchHistoryManager::historyChanged,
            this, [this](const QString &serverId) {
                if (serverId == currentSearchServerId()) {
                    updateSearchCompleter(m_searchBox ? m_searchBox->text()
                                                      : QString());
                }
            });
    connect(SearchHistoryManager::instance(), &SearchHistoryManager::enabledChanged,
            this, [this](bool ) {
                updateSearchCompleter(m_searchBox ? m_searchBox->text()
                                                  : QString());
            });
    connect(SearchHistoryManager::instance(),
            &SearchHistoryManager::autocompleteEnabledChanged, this,
            [this](bool ) {
                updateSearchCompleter(m_searchBox ? m_searchBox->text()
                                                  : QString());
            });

    if (m_core && m_core->serverManager()) {
        connect(m_core->serverManager(), &ServerManager::activeServerChanged, this,
                [this](const ServerProfile &profile) {
                    Q_UNUSED(profile);
                    updateSearchCompleter(m_searchBox ? m_searchBox->text()
                                                      : QString());
                });
    }

    updateSearchCompleter();
}

void HomeView::updateSearchCompleter(const QString &text)
{
    if (!m_searchHistoryModel || !m_searchCompleter || !m_searchBox) {
        return;
    }

    const QStringList suggestions =
        SearchHistoryManager::instance()->completionSuggestions(
            currentSearchServerId(), text, 8);
    m_searchHistoryModel->setStringList(suggestions);

    if (!SearchHistoryManager::instance()->isAutocompleteEnabled() ||
        text.trimmed().isEmpty() ||
        suggestions.isEmpty() || !m_searchBox->hasFocus()) {
        if (m_searchCompleter->popup()) {
            m_searchCompleter->popup()->hide();
        }
        return;
    }

    if (auto *popup =
            qobject_cast<SearchCompleterPopup *>(m_searchCompleter->popup())) {
        popup->setHighlightText(text);
        popup->syncWidthToAnchor(m_searchBox);
    }

    m_searchCompleter->setCompletionPrefix(text);
    if (auto *popup =
            qobject_cast<SearchCompleterPopup *>(m_searchCompleter->popup())) {
        m_searchCompleter->complete(popup->popupRectForAnchor(m_searchBox));
        return;
    }

    m_searchCompleter->complete();
}

QString HomeView::currentSearchServerId() const
{
    if (!m_core || !m_core->serverManager()) {
        return {};
    }
    return m_core->serverManager()->activeProfile().id;
}





void HomeView::pushView(QWidget *view)
{
    QWidget *current = m_contentSwitcher->currentWidget();
    if (current == view)
        return;

    if (m_contentSwitcher->indexOf(view) == -1)
    {
        m_contentSwitcher->addWidget(view);
    }

    
    if (current)
    {
        RouteInfo info;
        info.widget = current;
        info.isDynamic = current->property("isDynamic").toBool();
        info.routeType = current->property("routeType").toString();
        info.routeId = current->property("routeId").toString();
        info.routeTitle = current->property("routeTitle").toString();
        
        info.routeExtraId = current->property("routeExtraId").toString();
        m_navStack.push(info);
    }

    
    const int MAX_ACTIVE_VIEWS = 12;
    int activeDynamicCount = 0;

    
    for (int i = m_navStack.size() - 1; i >= 0; --i)
    {
        if (m_navStack[i].isDynamic && m_navStack[i].widget)
        {
            activeDynamicCount++;
            if (activeDynamicCount > MAX_ACTIVE_VIEWS)
            {
                
                m_contentSwitcher->removeWidget(m_navStack[i].widget);
                m_navStack[i].widget->deleteLater();
                
                
            }
        }
    }

    
    QMetaObject::invokeMethod(view, "scrollToTop", Qt::QueuedConnection);

    m_contentSwitcher->slideInWgt(view, SlidingStackedWidget::RightToLeft);
    Q_EMIT canNavigateBackChanged(!m_navStack.isEmpty());
}

void HomeView::navigateBack()
{
    QWidget *current = m_contentSwitcher->currentWidget();

    
    
    bool consumed = false;
    if (current)
    {
        QMetaObject::invokeMethod(current, "navigateBack", Qt::DirectConnection, Q_RETURN_ARG(bool, consumed));
    }
    if (consumed)
        return;

    if (!m_navStack.isEmpty())
    {
        RouteInfo prevInfo = m_navStack.pop();
        QWidget *targetView = prevInfo.widget;

        
        if (!targetView && prevInfo.isDynamic)
        {
            if (prevInfo.routeType == "DetailView")
            {
                targetView = createDetailView(prevInfo.routeId, prevInfo.routeTitle);
            }
            else if (prevInfo.routeType == "LibraryView")
            {
                targetView = createLibraryView(prevInfo.routeId, prevInfo.routeTitle);
            }
            else if (prevInfo.routeType == "CategoryView")
            {
                targetView = createCategoryView(prevInfo.routeId, prevInfo.routeTitle);
            }
            else if (prevInfo.routeType == "SearchView")
            {
                targetView = createSearchView(prevInfo.routeId);
            }
            else if (prevInfo.routeType == "SettingsView")
            {
                targetView = createSettingsView();
            }
            else if (prevInfo.routeType == "PersonView")
            {
                targetView = createPersonView(prevInfo.routeId, prevInfo.routeTitle);
            }
            else if (prevInfo.routeType == "SeasonView")
            {
                
                targetView = createSeasonView(prevInfo.routeExtraId, prevInfo.routeId, prevInfo.routeTitle);
            }
            else if (prevInfo.routeType == "ManageView")
            {
                targetView = createManageView();
            }
            else if (prevInfo.routeType == "FilteredView")
            {
                targetView = createFilteredView(prevInfo.routeExtraId, prevInfo.routeTitle);
            }

            if (targetView && m_contentSwitcher->indexOf(targetView) == -1)
            {
                m_contentSwitcher->addWidget(targetView);
            }
        }

        
        if (!targetView)
            targetView = m_dashboardView;

        
        
        
        

        m_contentSwitcher->slideInWgt(targetView, SlidingStackedWidget::LeftToRight);
        Q_EMIT canNavigateBackChanged(!m_navStack.isEmpty());

        
        
        
        if (current && current->property("isDynamic").toBool())
        {
            QPointer<QWidget> safeCurrent(current);
            QTimer::singleShot(500, this,
                               [this, safeCurrent]()
                               {
                                   if (safeCurrent)
                                   {
                                       m_contentSwitcher->removeWidget(safeCurrent);
                                       safeCurrent->deleteLater();
                                   }
                               });
        }
    }
}

void HomeView::goHome()
{
    if (m_contentSwitcher->currentWidget() == m_dashboardView)
    {
        
        m_dashboardView->loadDashboardData();
        return;
    }
    m_libraryList->clearSelection();
    resetToView(m_dashboardView);
}

void HomeView::goFav()
{
    if (m_contentSwitcher->currentWidget() == m_favoritesView)
    {
        
        m_favoritesView->loadFavoritesData();
        return;
    }
    m_libraryList->clearSelection();
    resetToView(m_favoritesView);
}

void HomeView::resetToView(QWidget *view)
{
    if (m_contentSwitcher->currentWidget() == view)
        return;

    
    QList<QPointer<QWidget>> widgetsToDelete;
    while (!m_navStack.isEmpty())
    {
        RouteInfo info = m_navStack.pop();
        if (info.isDynamic && info.widget && info.widget != view)
        {
            m_contentSwitcher->removeWidget(info.widget);
            widgetsToDelete.append(info.widget);
        }
    }

    QWidget *current = m_contentSwitcher->currentWidget();
    if (m_contentSwitcher->indexOf(view) == -1)
    {
        m_contentSwitcher->addWidget(view);
    }

    
    QMetaObject::invokeMethod(view, "scrollToTop", Qt::QueuedConnection);

    m_contentSwitcher->slideInWgt(view, SlidingStackedWidget::Automatic);
    Q_EMIT canNavigateBackChanged(false);

    
    
    if (current && current->property("isDynamic").toBool() && current != view)
    {
        widgetsToDelete.append(QPointer<QWidget>(current));
    }

    
    if (!widgetsToDelete.isEmpty())
    {
        QTimer::singleShot(500, this,
                           [this, widgetsToDelete]()
                           {
                               for (auto &w : widgetsToDelete)
                               {
                                   if (w)
                                   {
                                       m_contentSwitcher->removeWidget(w);
                                       w->deleteLater();
                                   }
                               }
                           });
    }
}

bool HomeView::canNavigateBack() const
{
    return !m_navStack.isEmpty();
}

bool HomeView::canGoHome() const
{
    if (!m_contentSwitcher || !m_dashboardView)
        return false;
    return m_contentSwitcher->currentWidget() != m_dashboardView;
}

bool HomeView::canGoFav() const
{
    if (!m_contentSwitcher || !m_favoritesView)
        return false;
    return m_contentSwitcher->currentWidget() != m_favoritesView;
}

void HomeView::scheduleProfileRefresh()
{
    m_pendingProfileRefreshTask = refreshProfile();
}


QCoro::Task<void> HomeView::refreshProfile()
{
    
    QPointer<HomeView> guard(this);

    ServerProfile activeProfile = m_core->serverManager()->activeProfile();

    if (activeProfile.type == ServerProfile::Jellyfin)
    {
        m_serverIconLabel->setPixmap(QPixmap(":/svg/jellyfin.svg"));
    }
    else
    {
        if (!activeProfile.iconBase64.isEmpty())
        {
            QPixmap pix;
            pix.loadFromData(QByteArray::fromBase64(activeProfile.iconBase64.toUtf8()));
            m_serverIconLabel->setPixmap(pix);
        }
        else
        {
            m_serverIconLabel->setPixmap(QIcon(":/svg/qemby_logo.svg").pixmap(32, 32));
        }
    }

    QString displayName = activeProfile.name.isEmpty() ? tr("My Server") : activeProfile.name;
    m_serverNameLabel->setFullText(displayName);
    m_serverAddressLabel->setFullText(activeProfile.url);

    
    if (activeProfile.isAdmin)
    {
        m_userAvatarLabel->setPixmap(QIcon(":/svg/light/user-admin.svg").pixmap(20, 20));
    }
    else
    {
        m_userAvatarLabel->setPixmap(QIcon(":/svg/light/user.svg").pixmap(20, 20));
    }

    
    m_btnManage->setVisible(activeProfile.isAdmin);

    const QString userRoleText = activeProfile.isAdmin ? tr("Administrator") : tr("User");
    QString displayUserName = activeProfile.userName.isEmpty() ? userRoleText : activeProfile.userName;
    m_userNameLabel->setFullText(displayUserName);
    m_userNameLabel->setProperty("userRoleText", userRoleText);
    applySidebarMetrics(m_sidebarPinned);

    try
    {
        QList<MediaItem> views = co_await m_core->mediaService()->getUserViews();
        if (!guard)
            co_return; 

        QWidget *currentView = m_contentSwitcher ? m_contentSwitcher->currentWidget() : nullptr;
        const QString currentRouteType = currentView ? currentView->property("routeType").toString() : QString();
        const QString currentRouteId = currentView ? currentView->property("routeId").toString() : QString();
        QListWidgetItem *selectedLibraryItem = nullptr;

        m_libraryList->clear();
        for (const auto &view : views)
        {
            QString iconStr = "📁 ";
            if (view.collectionType == "movies")
                iconStr = "🎬 ";
            else if (view.collectionType == "tvshows")
                iconStr = "📺 ";
            else if (view.collectionType == "music")
                iconStr = "🎵 ";
            else if (view.collectionType == "homevideos" || view.collectionType == "photos")
                iconStr = "🎞️ ";

            auto *item = new QListWidgetItem(iconStr + view.name);
            item->setData(Qt::UserRole, view.id);
            m_libraryList->addItem(item);

            if (currentRouteType == "LibraryView" && view.id == currentRouteId)
            {
                selectedLibraryItem = item;
            }
        }

        if (selectedLibraryItem)
        {
            m_libraryList->setCurrentItem(selectedLibraryItem);
            selectedLibraryItem->setSelected(true);
        }
        else
        {
            m_libraryList->clearSelection();
        }
    }
    catch (const std::exception &e)
    {
        if (!guard)
            co_return; 
        qDebug() << "Failed to load library views for sidebar:" << e.what();
    }
}

void HomeView::showEvent(QShowEvent *event)
{
    QWidget::showEvent(event);

    
    
    if (m_sidebarPinned && !m_sidebarPinnedApplied)
    {
        m_sidebarPinnedApplied = true;
        applySidebarPinned(true);
    }
    else if (m_sidebarPinned && m_sidebarPinnedApplied)
    {
        
        if (m_contentLayout->indexOf(m_sidebar) == -1)
        {
            if (m_sidebarOnRight)
            {
                m_contentLayout->addWidget(m_sidebar);
            }
            else
            {
                m_contentLayout->insertWidget(0, m_sidebar);
            }
            m_sidebar->show();
        }
    }

    syncSidebarVisibility();
    scheduleProfileRefresh();
}

void HomeView::hideEvent(QHideEvent *event)
{
    
    
    if (m_sidebarPinned && m_sidebarPinnedApplied)
    {
        m_contentLayout->removeWidget(m_sidebar);
        m_sidebar->hide();
    }

    QWidget::hideEvent(event);
}

void HomeView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);

    
    if (m_sidebarPinned)
        return;

    const int sidebarW = m_sidebar->width();

    if (m_sidebarOnRight)
    {
        
        m_edgeTrigger->setGeometry(width() - kRightEdgeTriggerWidth, 0, kRightEdgeTriggerWidth, height());

        
        bool isShown = (m_sidebar->x() < width() && m_sidebar->x() >= width() - sidebarW);
        if (isShown)
        {
            m_sidebar->setGeometry(width() - sidebarW, 0, sidebarW, height());
        }
        else
        {
            m_sidebar->setGeometry(width() + kSidebarHiddenOffset, 0, sidebarW, height());
        }
    }
    else
    {
        
        m_edgeTrigger->setGeometry(0, 0, kLeftEdgeTriggerWidth, height());

        if (m_sidebar->x() < 0)
        {
            m_sidebar->setGeometry(-sidebarW - kSidebarHiddenOffset, 0, sidebarW, height());
        }
        else
        {
            m_sidebar->setGeometry(0, 0, sidebarW, height());
        }
    }
}

bool HomeView::eventFilter(QObject *watched, QEvent *event)
{
    
    if (m_sidebarPinned)
    {
        return QWidget::eventFilter(watched, event);
    }

    if (watched == m_edgeTrigger && event->type() == QEvent::Enter)
    {
        showSidebar();
        return true;
    }

    if (watched == m_sidebar)
    {
        if (event->type() == QEvent::Leave)
        {
            QPoint globalPos = QCursor::pos();
            QPoint localPos = m_sidebar->mapFromGlobal(globalPos);
            if (!m_sidebar->rect().contains(localPos))
            {
                hideSidebar();
            }
            return false;
        }
        
        if (event->type() == QEvent::MouseMove)
        {
            if (m_sidebarAutoHideTimer->isActive())
            {
                m_sidebarAutoHideTimer->start(); 
            }
        }
    }
    return QWidget::eventFilter(watched, event);
}

void HomeView::showSidebar()
{
    
    if (m_sidebarPinned || isCurrentViewImmersive())
        return;

    
    bool reduceAnimations = ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    if (reduceAnimations)
    {
        if (m_sidebarOnRight)
        {
            m_sidebar->move(width() - m_sidebar->width(), 0);
        }
        else
        {
            m_sidebar->move(0, 0);
        }
        m_sidebarAutoHideTimer->start(); 
        return;
    }

    if (m_sidebarAnim->state() == QAbstractAnimation::Running)
        m_sidebarAnim->stop();
    m_sidebarAnim->setStartValue(m_sidebar->pos());

    if (m_sidebarOnRight)
    {
        m_sidebarAnim->setEndValue(QPoint(width() - m_sidebar->width(), 0));
    }
    else
    {
        m_sidebarAnim->setEndValue(QPoint(0, 0));
    }
    m_sidebarAnim->start();
    m_sidebarAutoHideTimer->start(); 
}

void HomeView::hideSidebar()
{
    
    if (m_sidebarPinned)
        return;

    
    bool reduceAnimations = ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    if (reduceAnimations)
    {
        if (m_sidebarOnRight)
        {
            m_sidebar->move(width() + kSidebarHiddenOffset, 0);
        }
        else
        {
            m_sidebar->move(-m_sidebar->width() - kSidebarHiddenOffset, 0);
        }
        return;
    }

    m_sidebarAutoHideTimer->stop(); 
    if (m_sidebarAnim->state() == QAbstractAnimation::Running)
        m_sidebarAnim->stop();
    m_sidebarAnim->setStartValue(m_sidebar->pos());

    if (m_sidebarOnRight)
    {
        m_sidebarAnim->setEndValue(QPoint(width() + kSidebarHiddenOffset, 0));
    }
    else
    {
        m_sidebarAnim->setEndValue(QPoint(-m_sidebar->width() - kSidebarHiddenOffset, 0));
    }
    m_sidebarAnim->start();
}

bool HomeView::isCurrentViewImmersive() const
{
    QWidget *current = m_contentSwitcher ? m_contentSwitcher->currentWidget() : nullptr;
    return current && current->property("isImmersive").toBool();
}

int HomeView::sidebarWidthForMode(bool pinned) const
{
    return pinned ? kPinnedSidebarWidth : kFloatingSidebarWidth;
}

void HomeView::applySidebarMetrics(bool pinned)
{
    if (!m_sidebar)
    {
        return;
    }

    m_sidebar->setFixedWidth(sidebarWidthForMode(pinned));

    auto *layout = qobject_cast<QVBoxLayout *>(m_sidebar->layout());
    if (layout)
    {
        if (pinned)
        {
            layout->setContentsMargins(12, 18, 12, 18);
            layout->setSpacing(4);
        }
        else
        {
            layout->setContentsMargins(16, 20, 16, 20);
            layout->setSpacing(6);
        }
    }

    if (m_serverInfoLayout)
    {
        m_serverInfoLayout->setDirection(pinned ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight);
        m_serverInfoLayout->setContentsMargins(pinned ? QMargins(4, 0, 4, 10) : QMargins(8, 0, 8, 10));
        m_serverInfoLayout->setSpacing(pinned ? 8 : 10);
        m_serverInfoLayout->setAlignment(m_serverIconLabel, pinned ? Qt::AlignHCenter : Qt::AlignVCenter);
        if (m_serverNameLayout)
        {
            m_serverInfoLayout->setAlignment(m_serverNameLayout, pinned ? Qt::AlignHCenter : Qt::AlignVCenter);
        }
    }

    if (m_serverNameLayout)
    {
        m_serverNameLayout->setAlignment(pinned ? Qt::AlignHCenter : Qt::AlignVCenter);
    }

    if (m_serverNameLabel)
    {
        m_serverNameLabel->setAlignment(pinned ? Qt::AlignHCenter : Qt::AlignLeft);
        const QString serverName = m_serverNameLabel->fullText();
        const QString serverAddress = m_serverAddressLabel ? m_serverAddressLabel->fullText() : QString();

        if (pinned && !serverAddress.isEmpty())
        {
            m_serverNameLabel->setToolTip(serverName.isEmpty() ? serverAddress : serverName + "\n" + serverAddress);
        }
        else
        {
            m_serverNameLabel->setToolTip(serverName);
        }
    }

    if (m_serverAddressLabel)
    {
        m_serverAddressLabel->setAlignment(pinned ? Qt::AlignHCenter : Qt::AlignLeft);
        m_serverAddressLabel->setVisible(!pinned);
        if (!pinned)
        {
            m_serverAddressLabel->setToolTip(m_serverAddressLabel->fullText());
        }
    }

    if (m_userInfoLayout)
    {
        m_userInfoLayout->setContentsMargins(pinned ? QMargins(4, 0, 4, 8)
                                                    : QMargins(0, 0, 0, 10));
        m_userInfoLayout->setSpacing(pinned ? 4 : 10);
        m_userInfoLayout->setAlignment(Qt::AlignVCenter);
    }

    if (m_userAvatarLabel)
    {
        const int avatarSize = pinned ? 12 : 20;
        m_userAvatarLabel->setFixedSize(avatarSize, avatarSize);
        m_userAvatarLabel->setVisible(true);
    }

    if (m_userNameLabel)
    {
        m_userNameLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        const QString userName = m_userNameLabel->fullText();
        const QString userRole = m_userNameLabel->property("userRoleText").toString();
        if (pinned && !userRole.isEmpty())
        {
            m_userNameLabel->setToolTip(userName.isEmpty() ? userRole : userName + "\n" + userRole);
        }
        else
        {
            m_userNameLabel->setToolTip(userName);
        }
    }
}

void HomeView::syncSidebarVisibility()
{
    if (!m_sidebar || !m_edgeTrigger)
    {
        return;
    }

    const bool isImmersive = isCurrentViewImmersive();
    qDebug() << "[HomeView] Sync sidebar visibility:"
             << "pinned =" << m_sidebarPinned << "immersive =" << isImmersive;

    if (m_sidebarAnim && m_sidebarAnim->state() == QAbstractAnimation::Running)
    {
        m_sidebarAnim->stop();
    }
    if (m_sidebarAutoHideTimer)
    {
        m_sidebarAutoHideTimer->stop();
    }

    if (isImmersive)
    {
        m_sidebar->hide();
        m_edgeTrigger->hide();
        return;
    }

    if (m_sidebarPinned)
    {
        m_sidebar->show();
        m_edgeTrigger->hide();
        return;
    }

    const int sidebarW = m_sidebar->width();
    if (m_sidebarOnRight)
    {
        m_sidebar->setGeometry(width() + kSidebarHiddenOffset, 0, sidebarW, height());
        m_edgeTrigger->setGeometry(width() - kRightEdgeTriggerWidth, 0, kRightEdgeTriggerWidth, height());
    }
    else
    {
        m_sidebar->setGeometry(-sidebarW - kSidebarHiddenOffset, 0, sidebarW, height());
        m_edgeTrigger->setGeometry(0, 0, kLeftEdgeTriggerWidth, height());
    }

    m_sidebar->show();
    m_edgeTrigger->show();
}

void HomeView::applySidebarPosition()
{
    m_sidebar->setProperty("sidebarSide", m_sidebarOnRight ? "right" : "left");
    m_sidebar->style()->unpolish(m_sidebar);
    m_sidebar->style()->polish(m_sidebar);

    
    if (m_sidebarPinned)
    {
        m_contentLayout->removeWidget(m_sidebar);
        if (m_sidebarOnRight)
        {
            m_contentLayout->addWidget(m_sidebar);
        }
        else
        {
            m_contentLayout->insertWidget(0, m_sidebar);
        }
        syncSidebarVisibility();
        return;
    }

    const int sidebarW = m_sidebar->width();

    
    if (auto *shadow = qobject_cast<QGraphicsDropShadowEffect *>(m_sidebar->graphicsEffect()))
    {
        shadow->setOffset(m_sidebarOnRight ? -4 : 4, 0);
    }

    
    if (m_sidebarOnRight)
    {
        m_edgeTrigger->setGeometry(width() - kRightEdgeTriggerWidth, 0, kRightEdgeTriggerWidth, height());
    }
    else
    {
        m_edgeTrigger->setGeometry(0, 0, kLeftEdgeTriggerWidth, height());
    }

    
    if (m_sidebarAnim->state() == QAbstractAnimation::Running)
        m_sidebarAnim->stop();
    if (m_sidebarOnRight)
    {
        m_sidebar->setGeometry(width() + kSidebarHiddenOffset, 0, sidebarW, height());
    }
    else
    {
        m_sidebar->setGeometry(-sidebarW - kSidebarHiddenOffset, 0, sidebarW, height());
    }
}




void HomeView::applySidebarPinned(bool pinned)
{
    qDebug() << "[HomeView] Sidebar pinned mode changed:" << pinned
             << "position:" << (m_sidebarOnRight ? "right" : "left");

    if (pinned)
    {
        
        if (m_sidebarAnim->state() == QAbstractAnimation::Running)
            m_sidebarAnim->stop();
        m_sidebarAutoHideTimer->stop();

        
        m_sidebar->setGraphicsEffect(nullptr);

        
        m_sidebar->setProperty("pinned", true);
        m_sidebar->style()->unpolish(m_sidebar);
        m_sidebar->style()->polish(m_sidebar);

        
        applySidebarMetrics(true);
        m_sidebar->setMinimumHeight(0);
        m_sidebar->setMaximumHeight(QWIDGETSIZE_MAX);

        
        if (m_sidebarOnRight)
        {
            m_contentLayout->addWidget(m_sidebar);
        }
        else
        {
            m_contentLayout->insertWidget(0, m_sidebar);
        }
        syncSidebarVisibility();
        m_sidebar->raise(); 
    }
    else
    {
        
        m_contentLayout->removeWidget(m_sidebar);
        m_sidebar->setParent(this); 

        
        auto *shadow = new QGraphicsDropShadowEffect(this);
        shadow->setBlurRadius(25);
        shadow->setColor(QColor(0, 0, 0, 30));
        shadow->setOffset(m_sidebarOnRight ? -4 : 4, 0);
        m_sidebar->setGraphicsEffect(shadow);

        
        m_sidebar->setProperty("pinned", false);
        m_sidebar->style()->unpolish(m_sidebar);
        m_sidebar->style()->polish(m_sidebar);

        
        applySidebarMetrics(false);
        syncSidebarVisibility();
    }
}
