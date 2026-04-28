#include "categoryview.h"
#include "../../components/elidedlabel.h"
#include "../../components/mediagridwidget.h"
#include "../../components/modernsortbutton.h"
#include "../../managers/thememanager.h"
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPointer> 
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QSet> 
#include <QVBoxLayout>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>


CategoryView::CategoryView(QEmbyCore *core, QWidget *parent)
    : BaseView(core, parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setObjectName("category-view");

  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);
  mainLayout->setSpacing(0);

  
  
  QScrollArea *headerScrollArea = new QScrollArea(this);
  headerScrollArea->setFrameShape(QFrame::NoFrame);
  headerScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  headerScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  headerScrollArea->setWidgetResizable(true);

  headerScrollArea->setStyleSheet(
      "QScrollArea { background: transparent; border: none; } "
      "QWidget#category-header-container { background: transparent; }");

  headerScrollArea->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  headerScrollArea->setFixedHeight(65);

  QWidget *headerContainer = new QWidget(headerScrollArea);
  headerContainer->setObjectName("category-header-container");

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
          [this](const MediaItem &item) {
            if (item.type == "BoxSet" || item.type == "Playlist" ||
                item.type == "Folder") {
              Q_EMIT navigateToFolder(item.id, item.name);
            } else if (item.type == "Person") {
              Q_EMIT navigateToPerson(item.id, item.name);
            } else {
              Q_EMIT navigateToDetail(item.id, item.name);
            }
          });

  
  
  
  connect(m_mediaGrid, &MediaGridWidget::playRequested, this,
          &BaseView::handlePlayRequested);
  connect(m_mediaGrid, &MediaGridWidget::favoriteRequested, this,
          &BaseView::handleFavoriteRequested);
  connect(m_mediaGrid, &MediaGridWidget::moreMenuRequested, this,
          &BaseView::handleMoreMenuRequested);
  
}

void CategoryView::setupTopBar(QHBoxLayout *headerLayout) {
  
  m_titleLabel = new ElidedLabel(this);
  m_titleLabel->setObjectName("library-title");
  m_titleLabel->setStyleSheet("padding: 0px;");
  headerLayout->addWidget(m_titleLabel);

  headerLayout->addStretch();

  
  QWidget *filterBarWidget = new QWidget(this);
  auto *filterLayout = new QHBoxLayout(filterBarWidget);
  filterLayout->setContentsMargins(0, 0, 0, 0);
  filterLayout->setSpacing(10);

  
  m_refreshBtn = new QPushButton(filterBarWidget);
  m_refreshBtn->setObjectName("refresh-btn");
  m_refreshBtn->setFixedSize(28, 28);
  m_refreshBtn->setCursor(Qt::PointingHandCursor);
  m_refreshBtn->setToolTip(tr("Refresh Recommendations"));
  auto refreshIcon = []() {
    return QIcon(ThemeManager::instance()->isDarkMode()
                     ? ":/svg/dark/refresh.svg"
                     : ":/svg/light/refresh.svg");
  };
  m_refreshBtn->setIcon(refreshIcon());
  m_refreshBtn->setIconSize(QSize(16, 16));
  m_refreshBtn->setVisible(false); 

  connect(ThemeManager::instance(), &ThemeManager::themeChanged, this,
          [this, refreshIcon]() { m_refreshBtn->setIcon(refreshIcon()); });

  
  m_refreshAnimation = new QPropertyAnimation(this, QByteArray());
  m_refreshAnimation->setDuration(800);
  m_refreshAnimation->setStartValue(0.0);
  m_refreshAnimation->setEndValue(360.0);
  m_refreshAnimation->setEasingCurve(QEasingCurve::Linear);

  connect(m_refreshAnimation, &QPropertyAnimation::valueChanged, this,
          [this, refreshIcon](const QVariant &value) {
            qreal angle = value.toReal();
            QPixmap originalPix =
                refreshIcon().pixmap(QSize(32, 32)); 
            QPixmap rotatedPix(originalPix.size());
            rotatedPix.fill(Qt::transparent);
            QPainter painter(&rotatedPix);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            painter.translate(rotatedPix.width() / 2.0,
                              rotatedPix.height() / 2.0);
            painter.rotate(angle);
            painter.translate(-originalPix.width() / 2.0,
                              -originalPix.height() / 2.0);
            painter.drawPixmap(0, 0, originalPix);
            painter.end();
            m_refreshBtn->setIcon(QIcon(rotatedPix));
          });

  connect(m_refreshAnimation, &QPropertyAnimation::finished, this,
          [this, refreshIcon]() {
            m_refreshBtn->setIcon(refreshIcon());
            m_refreshBtn->setEnabled(true); 
          });

  connect(m_refreshBtn, &QPushButton::clicked, this,
          [this]() { refreshData(); });

  
  m_sortButton = new ModernSortButton(filterBarWidget);
  m_sortButton->setSortItems({tr("Date Added"), tr("Date Played"), tr("Title"),
                              tr("Runtime"), tr("Premiere Date")});
  connect(m_sortButton, &ModernSortButton::sortChanged, this,
          &CategoryView::onFilterChanged);

  
  m_viewSwitchBtn = new QPushButton(filterBarWidget);
  m_viewSwitchBtn->setObjectName("icon-action-btn");
  m_viewSwitchBtn->setCheckable(true);
  m_viewSwitchBtn->setFixedSize(32, 32);
  m_viewSwitchBtn->setCursor(Qt::PointingHandCursor);
  m_viewSwitchBtn->setToolTip(tr("Toggle View Mode"));

  connect(m_viewSwitchBtn, &QPushButton::clicked, this, [this](bool checked) {
    m_mediaGrid->setCardStyle(checked ? MediaCardDelegate::LibraryTile
                                      : MediaCardDelegate::Poster);
    saveViewPreference();
    onFilterChanged();
  });

  m_statsLabel = new QLabel(tr("0 Items"), filterBarWidget);
  m_statsLabel->setObjectName("library-stats-label");
  m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  filterLayout->addWidget(m_refreshBtn);
  filterLayout->addWidget(m_sortButton);
  filterLayout->addWidget(m_viewSwitchBtn);
  filterLayout->addWidget(m_statsLabel);

  headerLayout->addWidget(filterBarWidget);
}

bool CategoryView::isCastStyleCategory(const QString &categoryType) const {
  return categoryType == "Favorite_Person" || categoryType == "Person";
}

QString CategoryView::currentViewPreferenceCategoryId() const {
  const QString categoryType = m_currentCategory.trimmed();
  if (categoryType.isEmpty() || isCastStyleCategory(categoryType)) {
    return QString();
  }

  return QStringLiteral("category_%1").arg(categoryType);
}

void CategoryView::applyViewMode(bool isTile) {
  m_viewSwitchBtn->blockSignals(true);
  m_viewSwitchBtn->setChecked(isTile);
  m_viewSwitchBtn->blockSignals(false);
  m_mediaGrid->setCardStyle(isTile ? MediaCardDelegate::LibraryTile
                                   : MediaCardDelegate::Poster);
}

void CategoryView::saveViewPreference() {
  if (!m_core || !m_core->serverManager()) {
    return;
  }

  const QString serverId = m_core->serverManager()->activeProfile().id;
  const QString categoryId = currentViewPreferenceCategoryId();
  if (serverId.isEmpty() || categoryId.isEmpty()) {
    return;
  }

  const QString viewMode = m_viewSwitchBtn->isChecked()
                               ? QStringLiteral("tile")
                               : QStringLiteral("poster");
  auto *store = ConfigStore::instance();
  store->set(
      ConfigKeys::forCategory(serverId, categoryId, ConfigKeys::CategoryViewMode),
      viewMode);

  qDebug() << "[CategoryView] View preference saved:"
           << "server=" << serverId << "category=" << categoryId
           << "mode=" << viewMode;
}

void CategoryView::restoreViewPreference() {
  auto *store = ConfigStore::instance();
  const QString defaultViewMode =
      store->get<QString>(ConfigKeys::DefaultLibraryView, QStringLiteral("poster"));

  QString viewMode = defaultViewMode;
  if (m_core && m_core->serverManager()) {
    const QString serverId = m_core->serverManager()->activeProfile().id;
    const QString categoryId = currentViewPreferenceCategoryId();
    if (!serverId.isEmpty() && !categoryId.isEmpty()) {
      viewMode = store->get<QString>(
          ConfigKeys::forCategory(serverId, categoryId, ConfigKeys::CategoryViewMode),
          defaultViewMode);
      qDebug() << "[CategoryView] View preference restored:"
               << "server=" << serverId << "category=" << categoryId
               << "mode=" << viewMode << "default=" << defaultViewMode;
    } else {
      qDebug() << "[CategoryView] View preference fallback to default:"
               << "mode=" << defaultViewMode;
    }
  }

  applyViewMode(viewMode == QLatin1String("tile"));
}


QCoro::Task<void> CategoryView::loadCategory(const QString &categoryType,
                                             const QString &title) {
  
  QPointer<CategoryView> guard(this);

  m_currentCategory = categoryType;
  m_titleLabel->setFullText(title);

  
  m_sortButton->blockSignals(true);
  if (categoryType == "latest") {
    m_sortButton->setCurrentIndex(0); 
    m_sortButton->setDescending(true);
    m_sortButton->setEnabled(false);
  } else if (categoryType == "resume") {
    m_sortButton->setCurrentIndex(1); 
    m_sortButton->setDescending(true);
    m_sortButton->setEnabled(false);
  } else if (categoryType == "Favorite_Movie" || categoryType == "Movie") {
    m_sortButton->setCurrentIndex(1); 
    m_sortButton->setDescending(true);
    m_sortButton->setEnabled(true);
  } else if (categoryType == "recommended") {
    m_sortButton->setCurrentIndex(0);
    m_sortButton->setDescending(true);
    m_sortButton->setEnabled(false); 
  } else {
    m_sortButton->setCurrentIndex(
        0); 
    m_sortButton->setDescending(true);
    m_sortButton->setEnabled(true);
  }
  m_sortButton->blockSignals(false);

  
  if (isCastStyleCategory(categoryType)) {
    m_mediaGrid->setCardStyle(MediaCardDelegate::Cast);
    m_viewSwitchBtn->setVisible(false); 
  } else {
    restoreViewPreference();
    m_viewSwitchBtn->setVisible(true);
  }

  
  m_refreshBtn->setVisible(categoryType == "recommended");

  
  co_await onFilterChanged();
}


QCoro::Task<void> CategoryView::onFilterChanged() {
  
  QPointer<CategoryView> guard(this);

  m_mediaGrid->setItems(
      QList<MediaItem>()); 
  m_statsLabel->setText(tr("Loading..."));

  
  QString sortBy = "SortName";
  switch (m_sortButton->currentIndex()) {
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

  auto *mediaService = m_core->mediaService();

  try {
    QList<MediaItem> resultItems;

    
    if (m_currentCategory == "resume") {
      QList<MediaItem> rawItems =
          co_await mediaService->getResumeItems(0, sortBy, sortOrder);
      if (!guard)
        co_return;

      
      QSet<QString> seenSeriesIds;
      for (const auto &item : rawItems) {
        if (item.type == "Episode" && !item.seriesId.isEmpty()) {
          if (seenSeriesIds.contains(item.seriesId))
            continue;
          seenSeriesIds.insert(item.seriesId);
          try {
            MediaItem seriesItem =
                co_await mediaService->getItemDetail(item.seriesId);
            if (!guard)
              co_return;
            resultItems.append(seriesItem);
          } catch (...) {
            if (!guard)
              co_return;
          }
        } else {
          resultItems.append(item);
        }
      }
    } else if (m_currentCategory == "latest") {
      resultItems =
          co_await mediaService->getLatestItems(1000, sortBy, sortOrder);
    } else if (m_currentCategory == "recommended") {
      resultItems =
          co_await mediaService->getRecommendedMovies(1000, sortBy, sortOrder);
    } else if (m_currentCategory == "Favorite_Movie" ||
               m_currentCategory == "Movie") {
      resultItems =
          co_await mediaService->getFavoriteMovies(0, sortBy, sortOrder);
    } else if (m_currentCategory == "Favorite_BoxSet" ||
               m_currentCategory == "BoxSet") {
      resultItems =
          co_await mediaService->getFavoriteCollections(0, sortBy, sortOrder);
    } else if (m_currentCategory == "Favorite_Playlist" ||
               m_currentCategory == "Playlist") {
      resultItems =
          co_await mediaService->getFavoritePlaylists(0, sortBy, sortOrder);
    } else if (m_currentCategory == "Favorite_Folder" ||
               m_currentCategory == "Folder") {
      resultItems =
          co_await mediaService->getFavoriteFolders(0, sortBy, sortOrder);
    } else if (m_currentCategory == "Favorite_Person" ||
               m_currentCategory == "Person") {
      resultItems =
          co_await mediaService->getFavoritePeople(0, sortBy, sortOrder);
    }

    
    
    
    if (!guard)
      co_return;

    
    m_statsLabel->setText(tr("%1 Items").arg(resultItems.size()));
    m_mediaGrid->setItems(resultItems);

  } catch (const std::exception &e) {
    
    if (!guard)
      co_return;
    m_statsLabel->setText(tr("Error Loading Items"));
    qDebug() << "Category View fetching error:" << e.what();
  }
}




void CategoryView::onMediaItemUpdated(const MediaItem &item) {
  if (m_mediaGrid) {
    
    if (m_currentCategory == "resume") {
      bool hasProgress = (item.userData.playbackPositionTicks > 0) ||
                         (item.userData.playedPercentage > 0.0 &&
                          item.userData.playedPercentage < 100.0);

      
      if (item.userData.played || !hasProgress) {
        m_mediaGrid->removeItem(item.id);
        
        m_statsLabel->setText(tr("%1 Items").arg(m_mediaGrid->itemCount()));
        return;
      }
    }

    
    m_mediaGrid->updateItem(item);
  }
}





void CategoryView::onMediaItemRemoved(const QString &itemId) {
  if (m_mediaGrid) {
    m_mediaGrid->removeItem(itemId);
    
    m_statsLabel->setText(tr("%1 Items").arg(m_mediaGrid->itemCount()));
  }
}






QCoro::Task<void> CategoryView::refreshData() {
  QPointer<CategoryView> guard(this);
  m_refreshBtn->setEnabled(false); 

  
  bool reduceAnimations =
      ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
  if (!reduceAnimations) {
    m_refreshAnimation->setLoopCount(-1); 
    m_refreshAnimation->start();
  }

  auto *mediaService = m_core->mediaService();
  mediaService->clearRecommendCache();
  co_await onFilterChanged();

  if (!guard)
    co_return;

  
  if (m_refreshAnimation->state() == QAbstractAnimation::Running) {
    m_refreshAnimation->setLoopCount(m_refreshAnimation->currentLoop() + 1);
    
  } else {
    m_refreshBtn->setEnabled(true); 
  }
}
