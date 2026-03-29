#include "dashboardview.h"
#include "../../components/horizontallistviewgallery.h"
#include "../../components/mediasectionwidget.h"
#include "../media/mediacarddelegate.h"
#include "../media/medialistmodel.h"
#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPointer> 
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet> 
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>

DashboardView::DashboardView(QEmbyCore *core, QWidget *parent)
    : BaseView(core, parent), m_vScrollAnim(nullptr), m_vScrollTarget(0) {
  setObjectName("dashboard-view");

  
  m_libraryDelegate =
      new MediaCardDelegate(MediaCardDelegate::LibraryTile, this);

  connect(m_libraryDelegate, &MediaCardDelegate::playRequested, this,
          &BaseView::handlePlayRequested);
  connect(m_libraryDelegate, &MediaCardDelegate::favoriteRequested, this,
          &BaseView::handleFavoriteRequested);
  connect(m_libraryDelegate, &MediaCardDelegate::moreMenuRequested, this,
          &BaseView::handleMoreMenuRequested);

  setupUi();

  
  connect(m_libraryListView, &QListView::clicked, this,
          [this](const QModelIndex &index) {
            MediaItem item = m_libraryModel->getItem(index);
            Q_EMIT navigateToLibrary(item.id, item.name);
          });
}

void DashboardView::setupUi() {
  auto *dashLayout = new QVBoxLayout(this);
  dashLayout->setContentsMargins(0, 0, 0, 0);

  m_mainScrollArea = new QScrollArea(this);
  m_mainScrollArea->setObjectName("dashboard-scroll");
  m_mainScrollArea->setWidgetResizable(true);
  m_mainScrollArea->setFrameShape(QFrame::NoFrame);
  m_mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  QWidget *container = new QWidget(m_mainScrollArea);
  container->setObjectName("dashboard-container");
  auto *containerLayout = new QVBoxLayout(container);
  containerLayout->setContentsMargins(20, 20, 20, 20);
  containerLayout->setSpacing(0);
  m_containerLayout = containerLayout; 

  
  bool isTile = (ConfigStore::instance()->get<QString>(ConfigKeys::DefaultLibraryView, "poster") == "tile");
  MediaCardDelegate::CardStyle galleryStyle = isTile ? MediaCardDelegate::LibraryTile : MediaCardDelegate::Poster;
  int galleryHeight = isTile ? 210 : 300;

  
  auto addGallerySection =
      [this, containerLayout, galleryHeight](QWidget *header,
                              HorizontalListViewGallery **outGallery,
                              MediaCardDelegate::CardStyle style) {
        auto *section = new QWidget();
        auto *layout = new QVBoxLayout(section);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0); 
        layout->addWidget(header);

        
        *outGallery = new HorizontalListViewGallery(m_core, section);
        (*outGallery)->setFixedHeight(galleryHeight);
        (*outGallery)->setCardStyle(style); 

        
        
        (*outGallery)->listView()->setProperty("isHorizontalListView", true);
        (*outGallery)->listView()->viewport()->installEventFilter(this);

        
        
        
        
        connect(*outGallery, &HorizontalListViewGallery::itemClicked, this,
                [this](const MediaItem &item) {
                  if (item.type == "BoxSet" || item.type == "Playlist" || item.type == "Folder") {
                    Q_EMIT navigateToLibrary(item.id, item.name);
                  } else {
                    Q_EMIT navigateToDetail(item.id, item.name);
                  }
                });
        connect(*outGallery, &HorizontalListViewGallery::playRequested, this,
                &BaseView::handlePlayRequested);
        connect(*outGallery, &HorizontalListViewGallery::favoriteRequested,
                this, &BaseView::handleFavoriteRequested);
        connect(*outGallery, &HorizontalListViewGallery::moreMenuRequested,
                this, &BaseView::handleMoreMenuRequested);

        layout->addWidget(*outGallery);
        containerLayout->addWidget(section);
      };

  m_resumeHeader = createSectionHeader(tr("Continue Watching"), "resume");
  addGallerySection(m_resumeHeader, &m_resumeGallery,
                    galleryStyle);

  
  m_latestHeader = createSectionHeader(tr("Latest Media"), "latest");
  addGallerySection(m_latestHeader, &m_latestGallery,
                    galleryStyle);

  
  m_recommendHeader = createSectionHeader(tr("Recommended"), "recommended");
  addGallerySection(m_recommendHeader, &m_recommendGallery,
                    galleryStyle);

  
  auto addGridSection = [containerLayout](QWidget *header, QWidget *content) {
    auto *section = new QWidget();
    auto *layout = new QVBoxLayout(section);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(header);
    layout->addWidget(content);
    containerLayout->addWidget(section);
  };
  m_libraryTitle = new QLabel(tr("All Libraries"), container);
  m_libraryTitle->setObjectName("dashboard-section-title");
  m_libraryListView = createGridListView(&m_libraryModel);
  addGridSection(m_libraryTitle, m_libraryListView);

  containerLayout->addStretch();
  m_mainScrollArea->setWidget(container);
  dashLayout->addWidget(m_mainScrollArea);

  m_vScrollAnim = new QPropertyAnimation(m_mainScrollArea->verticalScrollBar(),
                                         "value", this);
  m_vScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_vScrollAnim->setDuration(450);

  
  m_mainScrollArea->viewport()->installEventFilter(this);
}


QWidget *DashboardView::createSectionHeader(const QString &title,
                                            const QString &type) {
  QWidget *w = new QWidget(this);
  auto *l = new QHBoxLayout(w);
  l->setContentsMargins(0, 0, 0, 0);

  QLabel *label = new QLabel(title, w);
  label->setObjectName("dashboard-section-title");

  QPushButton *btn = new QPushButton(tr("See All >"), w);
  btn->setObjectName("section-more-btn");
  btn->setCursor(Qt::PointingHandCursor);

  connect(btn, &QPushButton::clicked, this,
          [this, type, title]() { Q_EMIT navigateToCategory(type, title); });

  l->addWidget(label);
  l->addStretch();
  l->addWidget(btn);

  return w;
}

QListView *DashboardView::createGridListView(MediaListModel **outModel) {
  QListView *listView = new QListView(this);
  listView->setSelectionMode(QAbstractItemView::NoSelection);
  listView->setViewMode(QListView::IconMode);
  listView->setFlow(QListView::LeftToRight);
  listView->setWrapping(true);
  listView->setSpacing(0);
  listView->setMovement(QListView::Static);
  listView->setResizeMode(QListView::Adjust);
  listView->setUniformItemSizes(true);
  listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  listView->setFrameShape(QFrame::NoFrame);
  listView->setMouseTracking(true);
  listView->viewport()->setAttribute(Qt::WA_Hover);
  listView->setStyleSheet(
      "QListView { background: transparent; outline: none; }");

  *outModel = new MediaListModel(500, m_core, this);
  listView->setModel(*outModel);
  listView->setItemDelegate(m_libraryDelegate);
  return listView;
}


bool DashboardView::eventFilter(QObject *obj, QEvent *event) {
  
  if (event->type() == QEvent::Wheel) {
    bool isHorizontalViewport =
        obj->parent() &&
        obj->parent()->property("isHorizontalListView").toBool();
    bool isMainViewport = (obj == m_mainScrollArea->viewport());

    
    if (isHorizontalViewport || isMainViewport) {
      QWheelEvent *we = static_cast<QWheelEvent *>(event);
      QScrollBar *vBar = m_mainScrollArea->verticalScrollBar();

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

void DashboardView::adjustLibraryGridHeight() {
  if (!m_libraryListView || !m_libraryModel)
    return;

  int count = m_libraryModel->rowCount();
  if (count == 0) {
    m_libraryListView->setFixedHeight(0);
    return;
  }

  m_libraryListView->setSpacing(0);

  int padding = 40;
  int scrollBarWidth = qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
  int availableWidth = this->width() - padding - scrollBarWidth;
  if (availableWidth < 100)
    availableWidth = 100;

  int defaultCellWidth = 196;
  int tolerance = 10;

  int cols = (availableWidth + tolerance) / defaultCellWidth;
  if (cols < 1)
    cols = 1;

  int cellWidth = availableWidth / cols;
  int remainder = availableWidth - (cols * cellWidth);
  int leftPad = remainder / 2;
  int rightPad = remainder - leftPad;

  m_libraryListView->setStyleSheet(
      QString("QListView { background: transparent; outline: none; "
              "padding-left: %1px; padding-right: %2px; }")
          .arg(leftPad)
          .arg(rightPad));

  int imgWidth = cellWidth - 16;
  int imgHeight = qRound(imgWidth * 9.0 / 16.0);
  int cellHeight = imgHeight + 16 + 26;

  m_libraryDelegate->setTileSize(QSize(cellWidth, cellHeight));
  m_libraryListView->doItemsLayout();

  int rows = (count + cols - 1) / cols;
  m_libraryListView->setFixedHeight(rows * cellHeight);
}

void DashboardView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  adjustLibraryGridHeight();
}

void DashboardView::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);
  loadDashboardData(); 
}


QCoro::Task<void> DashboardView::loadDashboardData() {
  auto *mediaService = m_core->mediaService();
  
  QPointer<DashboardView> guard(this);

  
  auto *store = ConfigStore::instance();
  QString sid = m_core->serverManager()->activeProfile().id;
  bool showResume = store->get<bool>(ConfigKeys::forServer(sid, ConfigKeys::ShowContinueWatching), true);
  bool showLatest = store->get<bool>(ConfigKeys::forServer(sid, ConfigKeys::ShowLatestAdded), true);
  bool showRecommended = store->get<bool>(ConfigKeys::forServer(sid, ConfigKeys::ShowRecommended), true);
  bool showLibraries = store->get<bool>(ConfigKeys::forServer(sid, ConfigKeys::ShowMediaLibraries), true);
  bool showEachLibrary = store->get<bool>(ConfigKeys::forServer(sid, ConfigKeys::ShowEachLibrary), true);

  
  bool isTileDash = (store->get<QString>(ConfigKeys::DefaultLibraryView, "poster") == "tile");
  MediaCardDelegate::CardStyle dashStyle = isTileDash ? MediaCardDelegate::LibraryTile : MediaCardDelegate::Poster;
  int dashGalleryHeight = isTileDash ? 210 : 300;
  if (m_resumeGallery) { m_resumeGallery->setCardStyle(dashStyle); m_resumeGallery->setFixedHeight(dashGalleryHeight); }
  if (m_latestGallery) { m_latestGallery->setCardStyle(dashStyle); m_latestGallery->setFixedHeight(dashGalleryHeight); }
  if (m_recommendGallery) { m_recommendGallery->setCardStyle(dashStyle); m_recommendGallery->setFixedHeight(dashGalleryHeight); }

  
  if (!showResume) {
    if (m_resumeHeader && m_resumeHeader->parentWidget())
      m_resumeHeader->parentWidget()->setVisible(false);
  } else {
    try {
      QList<MediaItem> rawResumeItems = co_await mediaService->getResumeItems(50);
      if (!guard)
        co_return; 

      
      QList<MediaItem> resumeItems;
      QSet<QString> seenSeriesIds;

      for (const auto &item : rawResumeItems) {
        if (item.type == "Episode" && !item.seriesId.isEmpty()) {
          
          if (seenSeriesIds.contains(item.seriesId))
            continue;
          seenSeriesIds.insert(item.seriesId);

          
          try {
            MediaItem seriesItem =
                co_await mediaService->getItemDetail(item.seriesId);
            if (!guard)
              co_return;
            resumeItems.append(seriesItem);
          } catch (...) {
            
            if (!guard)
              co_return;
          }
        } else {
          resumeItems.append(item);
        }
      }

      m_resumeGallery->setItems(resumeItems);
      if (m_resumeHeader && m_resumeHeader->parentWidget()) {
        m_resumeHeader->parentWidget()->setVisible(!resumeItems.isEmpty());
      }
    } catch (const std::exception &e) {
      if (!guard)
        co_return;
      qDebug() << "Dashboard failed to fetch resume items: " << e.what();
    }
  }

  
  if (!showLatest) {
    if (m_latestHeader && m_latestHeader->parentWidget())
      m_latestHeader->parentWidget()->setVisible(false);
  } else {
    try {
      QList<MediaItem> latestItems = co_await mediaService->getLatestItems(50);
      if (!guard)
        co_return;
      m_latestGallery->setItems(latestItems);
      if (m_latestHeader && m_latestHeader->parentWidget()) {
        m_latestHeader->parentWidget()->setVisible(!latestItems.isEmpty());
      }
    } catch (const std::exception &e) {
      if (!guard)
        co_return;
      qDebug() << "Dashboard failed to fetch latest items: " << e.what();
    }
  }

  
  if (!showRecommended) {
    if (m_recommendHeader && m_recommendHeader->parentWidget())
      m_recommendHeader->parentWidget()->setVisible(false);
  } else {
    try {
      QList<MediaItem> recommendedItems =
          co_await mediaService->getRecommendedMovies(50);
      if (!guard)
        co_return;
      m_recommendGallery->setItems(recommendedItems);
      if (m_recommendHeader && m_recommendHeader->parentWidget()) {
        m_recommendHeader->parentWidget()->setVisible(
            !recommendedItems.isEmpty());
      }
    } catch (const std::exception &e) {
      if (!guard)
        co_return;
      qDebug() << "Dashboard failed to fetch recommended items: " << e.what();
    }
  }

  
  
  if (!showLibraries && !showEachLibrary) {
    
    if (m_libraryTitle && m_libraryTitle->parentWidget())
      m_libraryTitle->parentWidget()->setVisible(false);
    for (auto *w : m_libraryGalleries) {
      m_containerLayout->removeWidget(w);
      w->deleteLater();
    }
    m_libraryGalleries.clear();
  } else {
    try {
      QList<MediaItem> userViews = co_await mediaService->getUserViews();
      if (!guard)
        co_return;

      
      if (!showLibraries) {
        if (m_libraryTitle && m_libraryTitle->parentWidget())
          m_libraryTitle->parentWidget()->setVisible(false);
      } else {
        m_libraryModel->setItems(userViews);
        if (m_libraryTitle && m_libraryTitle->parentWidget()) {
          m_libraryTitle->parentWidget()->setVisible(!userViews.isEmpty());
        }
        
        QTimer::singleShot(0, this, &DashboardView::adjustLibraryGridHeight);
      }

      
      if (!showEachLibrary) {
        
        for (auto *w : m_libraryGalleries) {
          m_containerLayout->removeWidget(w);
          w->deleteLater();
        }
        m_libraryGalleries.clear();
      } else {
        
        
        
        
        bool canReuse = (m_libraryGalleries.size() == userViews.size());
        if (canReuse) {
          for (int i = 0; i < userViews.size(); ++i) {
            if (m_libraryGalleries[i]->property("libraryId").toString() != userViews[i].id) {
              canReuse = false;
              break;
            }
          }
        }

        if (canReuse) {
          
          for (int i = 0; i < userViews.size(); ++i) {
            QString libId = userViews[i].id;
            m_libraryGalleries[i]->loadAsync(
                [mediaService, libId]() -> QCoro::Task<QList<MediaItem>> {
                  co_return co_await mediaService->getLibraryItems(
                      libId, "DateCreated", "Descending", "", "Movie,Series", 0, 20, true);
                });
          }
        } else {
          
          for (auto *w : m_libraryGalleries) {
            m_containerLayout->removeWidget(w);
            w->deleteLater();
          }
          m_libraryGalleries.clear();

          
          for (const auto &view : userViews) {
            auto *section =
                new MediaSectionWidget(view.name, m_core, m_mainScrollArea->widget());
            section->setProperty("libraryId", view.id); 
            
            bool isTileLib = (ConfigStore::instance()->get<QString>(ConfigKeys::DefaultLibraryView, "poster") == "tile");
            MediaCardDelegate::CardStyle libGalleryStyle = isTileLib ? MediaCardDelegate::LibraryTile : MediaCardDelegate::Poster;
            int libGalleryHeight = isTileLib ? 210 : 300;

            section->setCardStyle(libGalleryStyle);
            section->setGalleryHeight(libGalleryHeight);

            
            section->layout()->setContentsMargins(0, 20, 0, 0);

            
            auto *seeAllBtn = new QPushButton(tr("See All >"), section);
            seeAllBtn->setObjectName("section-more-btn");
            seeAllBtn->setCursor(Qt::PointingHandCursor);
            QString libId = view.id;
            QString libName = view.name;
            connect(seeAllBtn, &QPushButton::clicked, this, [this, libId, libName]() {
              Q_EMIT navigateToLibrary(libId, libName);
            });
            
            auto *headerContainer = section->findChild<QWidget*>("section-header");
            if (headerContainer && headerContainer->layout()) {
              headerContainer->layout()->addWidget(seeAllBtn);
            }

            
            section->gallery()->listView()->setProperty("isHorizontalListView", true);
            section->gallery()->listView()->viewport()->installEventFilter(this);

            
            connect(section, &MediaSectionWidget::itemClicked, this,
                    [this](const MediaItem &item) {
                      if (item.type == "BoxSet" || item.type == "Playlist" || item.type == "Folder") {
                        Q_EMIT navigateToLibrary(item.id, item.name);
                      } else {
                        Q_EMIT navigateToDetail(item.id, item.name);
                      }
                    });
            connect(section, &MediaSectionWidget::playRequested, this,
                    &BaseView::handlePlayRequested);
            connect(section, &MediaSectionWidget::favoriteRequested, this,
                    &BaseView::handleFavoriteRequested);
            connect(section, &MediaSectionWidget::moreMenuRequested, this,
                    &BaseView::handleMoreMenuRequested);

            
            int stretchIndex =
                m_containerLayout->count() - 1; 
            m_containerLayout->insertWidget(stretchIndex, section);
            m_libraryGalleries.append(section);

            
            section->loadAsync(
                [mediaService, libId]() -> QCoro::Task<QList<MediaItem>> {
                  co_return co_await mediaService->getLibraryItems(
                      libId, "DateCreated", "Descending", "", "Movie,Series", 0, 20,
                      true);
                });
          }
        }
      }
    } catch (const std::exception &e) {
      if (!guard)
        co_return;
      qDebug() << "Dashboard failed to fetch user views: " << e.what();
    }
  }
}




void DashboardView::onMediaItemUpdated(const MediaItem &item) {
  if (m_resumeGallery) {
    
    bool hasProgress = (item.userData.playbackPositionTicks > 0) ||
                       (item.userData.playedPercentage > 0.0 &&
                        item.userData.playedPercentage < 100.0);

    
    if (item.userData.played || !hasProgress) {
      m_resumeGallery->removeItem(item.id);
    } else {
      
      m_resumeGallery->updateItem(item);
    }
  }

  
  
  
  if (m_latestGallery)
    m_latestGallery->updateItem(item);
  if (m_recommendGallery)
    m_recommendGallery->updateItem(item);

  
  if (m_libraryModel)
    m_libraryModel->updateItem(item);

  
  for (auto *section : m_libraryGalleries) {
    if (section)
      section->updateItem(item);
  }
}





void DashboardView::onMediaItemRemoved(const QString &itemId) {
  
}
