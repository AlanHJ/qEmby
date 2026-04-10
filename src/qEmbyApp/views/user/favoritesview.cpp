#include "favoritesview.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <services/manager/servermanager.h>
#include "../../components/horizontallistviewgallery.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QScrollBar>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QWheelEvent>
#include <QDebug>
#include <QPointer> 

FavoritesView::FavoritesView(QEmbyCore* core, QWidget *parent)
    : BaseView(core, parent), m_vScrollAnim(nullptr), m_vScrollTarget(0)
{
    setObjectName("favorites-view");
    
    
    setupUi();
}

void FavoritesView::setupUi()
{
    auto *favLayout = new QVBoxLayout(this);
    favLayout->setContentsMargins(0, 0, 0, 0);

    m_mainScrollArea = new QScrollArea(this);
    m_mainScrollArea->setObjectName("favorites-scroll");
    m_mainScrollArea->setWidgetResizable(true);
    m_mainScrollArea->setFrameShape(QFrame::NoFrame);
    m_mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QWidget *container = new QWidget(m_mainScrollArea);
    container->setObjectName("favorites-container");
    auto *containerLayout = new QVBoxLayout(container);
    containerLayout->setContentsMargins(20, 20, 20, 20);
    containerLayout->setSpacing(0); 

    
    bool isTile = (ConfigStore::instance()->get<QString>(ConfigKeys::DefaultLibraryView, "poster") == "tile");
    MediaCardDelegate::CardStyle defaultStyle = isTile ? MediaCardDelegate::LibraryTile : MediaCardDelegate::Poster;
    int galleryHeight = isTile ? 210 : 300;

    
    auto addGallerySection = [this, containerLayout, galleryHeight](QWidget* header, HorizontalListViewGallery** outGallery, MediaCardDelegate::CardStyle style) {
        auto *section = new QWidget();
        auto *layout = new QVBoxLayout(section);
        layout->setContentsMargins(0, 0, 0, 30); 
        layout->setSpacing(0);
        layout->addWidget(header);

        
        *outGallery = new HorizontalListViewGallery(m_core, section);
        (*outGallery)->setFixedHeight(style == MediaCardDelegate::Cast ? 300 : galleryHeight);
        (*outGallery)->setCardStyle(style); 

        
        (*outGallery)->listView()->setProperty("isHorizontalListView", true);
        (*outGallery)->listView()->viewport()->installEventFilter(this);

        
        
        
        connect(*outGallery, &HorizontalListViewGallery::itemClicked, this, [this](const MediaItem& item) {
            if (item.type == "BoxSet" || item.type == "Playlist" || item.type == "Folder") {
                Q_EMIT navigateToFolder(item.id, item.name);
            } else if (item.type == "Person") { 
                Q_EMIT navigateToPerson(item.id, item.name);
            } else {
                Q_EMIT navigateToDetail(item.id, item.name);
            }
        });

        
        connect(*outGallery, &HorizontalListViewGallery::playRequested, this, &BaseView::handlePlayRequested);
        connect(*outGallery, &HorizontalListViewGallery::favoriteRequested, this, &BaseView::handleFavoriteRequested);
        connect(*outGallery, &HorizontalListViewGallery::moreMenuRequested, this, &BaseView::handleMoreMenuRequested);

        layout->addWidget(*outGallery);
        containerLayout->addWidget(section);
    };

    
    m_moviesHeader = createSectionHeader(tr("Favorite Movies"), "Movie");
    addGallerySection(m_moviesHeader, &m_moviesGallery, defaultStyle);

    
    m_seriesHeader = createSectionHeader(tr("Favorite Shows"), "Series");
    addGallerySection(m_seriesHeader, &m_seriesGallery, defaultStyle);

    
    m_collectionsHeader = createSectionHeader(tr("Favorite Collections"), "BoxSet");
    addGallerySection(m_collectionsHeader, &m_collectionsGallery, defaultStyle);

    
    m_playlistsHeader = createSectionHeader(tr("Favorite Playlists"), "Playlist");
    addGallerySection(m_playlistsHeader, &m_playlistsGallery, defaultStyle);

    
    m_peopleHeader = createSectionHeader(tr("Favorite People"), "Person");
    addGallerySection(m_peopleHeader, &m_peopleGallery, MediaCardDelegate::Cast);

    
    m_foldersHeader = createSectionHeader(tr("Favorite Folders"), "Folder");
    addGallerySection(m_foldersHeader, &m_foldersGallery, defaultStyle);

    containerLayout->addStretch();
    m_mainScrollArea->setWidget(container);
    favLayout->addWidget(m_mainScrollArea);

    m_vScrollAnim = new QPropertyAnimation(m_mainScrollArea->verticalScrollBar(), "value", this);
    m_vScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_vScrollAnim->setDuration(450);

    m_mainScrollArea->viewport()->installEventFilter(this);
}

QWidget* FavoritesView::createSectionHeader(const QString& title, const QString& itemType)
{
    QWidget* w = new QWidget(this);
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 10); 

    QLabel* label = new QLabel(title, w);
    label->setObjectName("dashboard-section-title");

    l->addWidget(label);
    l->addStretch();

    
    if (!itemType.isEmpty()) {
        QPushButton* moreBtn = new QPushButton(tr("See All >"), w);
        moreBtn->setObjectName("section-more-btn");
        moreBtn->setCursor(Qt::PointingHandCursor);

        connect(moreBtn, &QPushButton::clicked, this, [this, title, itemType]() {
            
            Q_EMIT navigateToCategory(itemType, title);
        });

        l->addWidget(moreBtn);
    }

    return w;
}

bool FavoritesView::eventFilter(QObject* obj, QEvent* event)
{
    if (event->type() == QEvent::Wheel) {
        bool isHorizontalViewport = obj->parent() && obj->parent()->property("isHorizontalListView").toBool();
        bool isMainViewport = (obj == m_mainScrollArea->viewport());

        if (isHorizontalViewport || isMainViewport) {
            QWheelEvent* we = static_cast<QWheelEvent*>(event);
            QScrollBar* vBar = m_mainScrollArea->verticalScrollBar();

            if (vBar) {
                int currentVal = vBar->value();
                if (m_vScrollAnim->state() == QAbstractAnimation::Running) {
                    currentVal = m_vScrollTarget;
                }

                int step = we->angleDelta().y();
                int newTarget = currentVal - step;
                newTarget = qBound(vBar->minimum(), newTarget, vBar->maximum());

                if (newTarget != vBar->value()) {
                    m_vScrollTarget = newTarget;
                    m_vScrollAnim->stop();
                    m_vScrollAnim->setStartValue(vBar->value());
                    m_vScrollAnim->setEndValue(m_vScrollTarget);
                    m_vScrollAnim->start();
                }
            }
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void FavoritesView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    loadFavoritesData(); 
}


QCoro::Task<void> FavoritesView::loadFavoritesData()
{
    auto* mediaService = m_core->mediaService();
    
    QPointer<FavoritesView> guard(this);

    
    bool isTileFav = (ConfigStore::instance()->get<QString>(ConfigKeys::DefaultLibraryView, "poster") == "tile");
    MediaCardDelegate::CardStyle favStyle = isTileFav ? MediaCardDelegate::LibraryTile : MediaCardDelegate::Poster;
    int favGalleryHeight = isTileFav ? 210 : 300;
    auto applyStyle = [&](HorizontalListViewGallery* g) {
        if (g) { g->setCardStyle(favStyle); g->setFixedHeight(favGalleryHeight); }
    };
    applyStyle(m_moviesGallery);
    applyStyle(m_seriesGallery);
    applyStyle(m_collectionsGallery);
    applyStyle(m_playlistsGallery);
    applyStyle(m_foldersGallery);
    

    
    try {
        QList<MediaItem> movies = co_await mediaService->getFavoriteMovies(50);
        if (!guard) co_return; 
        m_moviesGallery->setItems(movies);
        if (m_moviesHeader && m_moviesHeader->parentWidget()) {
            m_moviesHeader->parentWidget()->setVisible(!movies.isEmpty());
        }
    } catch (const std::exception& e) {
        qDebug() << "Failed to fetch favorite movies: " << e.what();
    }

    
    try {
        QList<MediaItem> series = co_await mediaService->getFavoriteSeries(50);
        if (!guard) co_return;
        m_seriesGallery->setItems(series);
        if (m_seriesHeader && m_seriesHeader->parentWidget()) {
            m_seriesHeader->parentWidget()->setVisible(!series.isEmpty());
        }
    } catch (const std::exception& e) {
        qDebug() << "Failed to fetch favorite series: " << e.what();
    }

    
    try {
        QList<MediaItem> collections = co_await mediaService->getFavoriteCollections(50);
        if (!guard) co_return;
        m_collectionsGallery->setItems(collections);
        if (m_collectionsHeader && m_collectionsHeader->parentWidget()) {
            m_collectionsHeader->parentWidget()->setVisible(!collections.isEmpty());
        }
    } catch (const std::exception& e) {
        qDebug() << "Failed to fetch favorite collections: " << e.what();
    }

    
    try {
        QList<MediaItem> playlists = co_await mediaService->getFavoritePlaylists(50);
        if (!guard) co_return;
        m_playlistsGallery->setItems(playlists);
        if (m_playlistsHeader && m_playlistsHeader->parentWidget()) {
            m_playlistsHeader->parentWidget()->setVisible(!playlists.isEmpty());
        }
    } catch (const std::exception& e) {
        qDebug() << "Failed to fetch favorite playlists: " << e.what();
    }

    
    try {
        QList<MediaItem> people = co_await mediaService->getFavoritePeople(50);
        if (!guard) co_return;
        m_peopleGallery->setItems(people);
        if (m_peopleHeader && m_peopleHeader->parentWidget()) {
            m_peopleHeader->parentWidget()->setVisible(!people.isEmpty());
        }
    } catch (const std::exception& e) {
        qDebug() << "Failed to fetch favorite people: " << e.what();
    }
    
    
    
    QString sid = m_core->serverManager()->activeProfile().id;
    bool showFavFolders = ConfigStore::instance()->get<bool>(
        ConfigKeys::forServer(sid, ConfigKeys::ShowFavoriteFolders), false);

    if (!showFavFolders) {
        if (m_foldersHeader && m_foldersHeader->parentWidget())
            m_foldersHeader->parentWidget()->setVisible(false);
    } else {
        try {
            QList<MediaItem> folders = co_await mediaService->getFavoriteFolders(50);
            if (!guard) co_return;
            m_foldersGallery->setItems(folders);
            if (m_foldersHeader && m_foldersHeader->parentWidget()) {
                m_foldersHeader->parentWidget()->setVisible(!folders.isEmpty());
            }
        } catch (const std::exception& e) {
            qDebug() << "Failed to fetch favorite folders: " << e.what();
        }
    }
}




void FavoritesView::onMediaItemUpdated(const MediaItem& item)
{
    
    
    
    
    
    if (m_moviesGallery) m_moviesGallery->updateItem(item);
    if (m_seriesGallery) m_seriesGallery->updateItem(item);
    if (m_collectionsGallery) m_collectionsGallery->updateItem(item);
    if (m_playlistsGallery) m_playlistsGallery->updateItem(item);
    if (m_peopleGallery) m_peopleGallery->updateItem(item);
    if (m_foldersGallery) m_foldersGallery->updateItem(item);
}

void FavoritesView::onMediaItemRemoved(const QString& itemId)
{
    if (!m_playlistsGallery) {
        return;
    }

    m_playlistsGallery->removeItem(itemId);
    if (m_playlistsHeader && m_playlistsHeader->parentWidget()) {
        m_playlistsHeader->parentWidget()->setVisible(m_playlistsGallery->itemCount() > 0);
    }
}
