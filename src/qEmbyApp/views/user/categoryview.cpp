#include "categoryview.h"
#include "../../components/elidedlabel.h"
#include "../../components/mediagridwidget.h"
#include "../../components/modernsortbutton.h"
#include "../../managers/thememanager.h"
#include "../../utils/dashboardrequestlimitutils.h"
#include "../../utils/mediaitemutils.h"
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPointer> 
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScrollArea>
#include <QSet> 
#include <QStringList>
#include <QVBoxLayout>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <qembycore.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>
#include <utility>
#include <vector>

namespace {
constexpr int kDashboardCategoryFirstPageSize = 100;
constexpr int kDashboardCategoryBackgroundPageSize = 300;

QString pageFingerprint(const QList<MediaItem> &items) {
  QString fingerprint;
  for (const MediaItem &item : items) {
    const QString id = item.id.trimmed();
    if (id.isEmpty()) {
      continue;
    }
    fingerprint += id;
    fingerprint += QLatin1Char('|');
  }
  return fingerprint;
}
} 

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
              Q_EMIT navigateToDetail(item.id, item.name, item);
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

bool CategoryView::isProgressiveDashboardCategory(
    const QString &categoryType) const {
  return categoryType == "resume" || categoryType == "latest" ||
         categoryType == "played";
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

int CategoryView::dashboardCategoryRequestLimit(
    const QString &categoryType) const {
  const QString serverId =
      (m_core && m_core->serverManager())
          ? m_core->serverManager()->activeProfile().id
          : QString();

  if (categoryType == "resume") {
    return DashboardRequestLimitUtils::configuredRequestLimit(
        serverId, ConfigKeys::ContinueWatchingRequestLimit, 0);
  }
  if (categoryType == "latest") {
    return DashboardRequestLimitUtils::configuredRequestLimit(
        serverId, ConfigKeys::LatestMediaRequestLimit, 1000);
  }
  if (categoryType == "recommended") {
    return DashboardRequestLimitUtils::configuredRequestLimit(
        serverId, ConfigKeys::RecommendedRequestLimit, 1000);
  }
  if (categoryType == "played") {
    return DashboardRequestLimitUtils::configuredRequestLimit(
        serverId, ConfigKeys::CompletedWatchingRequestLimit, 0);
  }
  return 0;
}

CategoryView::DashboardCategoryQuery
CategoryView::buildDashboardCategoryQuery(const QString &sortBy,
                                          const QString &sortOrder) const {
  DashboardCategoryQuery query;
  query.category = m_currentCategory;
  query.sortBy = sortBy;
  query.sortOrder = sortOrder;
  query.requestLimit = dashboardCategoryRequestLimit(m_currentCategory);
  query.firstPageSize = kDashboardCategoryFirstPageSize;
  query.pageSize = kDashboardCategoryBackgroundPageSize;

  qDebug() << "[CategoryView] dashboard category query"
           << "| category=" << query.category
           << "| sortBy=" << query.sortBy
           << "| sortOrder=" << query.sortOrder
           << "| requestLimit=" << query.requestLimit
           << "| firstPageSize=" << query.firstPageSize
           << "| pageSize=" << query.pageSize;
  return query;
}

void CategoryView::setLoadedItems(const QList<MediaItem> &items) {
  m_loadedItems = items;
  m_statsLabel->setText(tr("%1 Items").arg(m_loadedItems.size()));
  m_mediaGrid->setItems(m_loadedItems);
}

void CategoryView::appendUniqueLoadedItems(const QList<MediaItem> &items) {
  QSet<QString> existingIds;
  for (const MediaItem &item : std::as_const(m_loadedItems)) {
    const QString id = item.id.trimmed();
    if (!id.isEmpty()) {
      existingIds.insert(id);
    }
  }

  for (const MediaItem &item : items) {
    const QString id = item.id.trimmed();
    if (!id.isEmpty() && existingIds.contains(id)) {
      continue;
    }
    if (!id.isEmpty()) {
      existingIds.insert(id);
    }
    m_loadedItems.append(item);
  }

  m_statsLabel->setText(tr("%1 Items").arg(m_loadedItems.size()));
  m_mediaGrid->setItems(m_loadedItems);
}

QCoro::Task<CategoryView::DashboardCategoryPage>
CategoryView::fetchDashboardCategoryPage(DashboardCategoryQuery query,
                                         int startIndex, int limit) {
  QPointer<CategoryView> guard(this);
  DashboardCategoryPage categoryPage;
  auto *mediaService = m_core->mediaService();
  MediaQueryPage page;

  if (query.category == "resume") {
    page = co_await mediaService->getResumeItemsPage(
        query.sortBy, query.sortOrder, startIndex, limit);
    if (!guard)
      co_return categoryPage;

    categoryPage.rawItemCount = page.items.size();
    categoryPage.fingerprint = pageFingerprint(page.items);

    QSet<QString> seenSeriesIds;
    QStringList seriesIdsToFetch;
    QList<int> insertIndices;
    QList<MediaItem> resumeContextItems;
    QList<MediaItem> displayItems;

    for (MediaItem item : page.items) {
      if (item.type == "Episode" && !item.seriesId.isEmpty()) {
        if (seenSeriesIds.contains(item.seriesId)) {
          continue;
        }
        seenSeriesIds.insert(item.seriesId);
        seriesIdsToFetch.append(item.seriesId);
        insertIndices.append(displayItems.size());
        resumeContextItems.append(item);
        displayItems.append(MediaItem {});
      } else {
        displayItems.append(MediaItemUtils::withResumeContext(item, item));
      }
    }

    std::vector<QCoro::Task<MediaItem>> detailTasks;
    detailTasks.reserve(seriesIdsToFetch.size());
    for (const QString &seriesId : std::as_const(seriesIdsToFetch)) {
      detailTasks.push_back(mediaService->getItemDetail(seriesId));
    }

    for (int i = 0; i < static_cast<int>(detailTasks.size()); ++i) {
      try {
        MediaItem seriesItem = co_await std::move(detailTasks[i]);
        if (!guard)
          co_return categoryPage;
        displayItems[insertIndices[i]] = MediaItemUtils::withResumeContext(
            seriesItem, resumeContextItems[i]);
      } catch (const std::exception &e) {
        if (!guard)
          co_return categoryPage;
        qWarning() << "[CategoryView] failed to resolve resume series"
                   << "| seriesId=" << seriesIdsToFetch.value(i)
                   << "| error=" << e.what();
      }
    }

    for (int i = displayItems.size() - 1; i >= 0; --i) {
      if (displayItems.at(i).id.isEmpty()) {
        displayItems.removeAt(i);
      }
    }
    categoryPage.items = std::move(displayItems);
  } else if (query.category == "latest") {
    page = co_await mediaService->getLatestItemsPage(
        query.sortBy, query.sortOrder, startIndex, limit);
    if (!guard)
      co_return categoryPage;
    categoryPage.items = page.items;
    categoryPage.rawItemCount = page.items.size();
    categoryPage.fingerprint = pageFingerprint(page.items);
  } else if (query.category == "played") {
    page = co_await mediaService->getPlayedItemsPage(
        query.sortBy, query.sortOrder, startIndex, limit);
    if (!guard)
      co_return categoryPage;
    categoryPage.items = page.items;
    categoryPage.rawItemCount = page.items.size();
    categoryPage.fingerprint = pageFingerprint(page.items);
  }

  categoryPage.totalRecordCount = page.totalRecordCount;
  categoryPage.hasTotalRecordCount = page.hasTotalRecordCount;

  qDebug() << "[CategoryView] fetched dashboard category page"
           << "| category=" << query.category
           << "| startIndex=" << startIndex
           << "| limit=" << limit
           << "| rawReturned=" << categoryPage.rawItemCount
           << "| displayReturned=" << categoryPage.items.size()
           << "| hasTotal=" << categoryPage.hasTotalRecordCount
           << "| total=" << categoryPage.totalRecordCount;
  co_return categoryPage;
}

QCoro::Task<void>
CategoryView::loadDashboardCategoryProgressively(
    DashboardCategoryQuery query) {
  QPointer<CategoryView> guard(this);
  const int generation = m_requestGeneration;

  m_loadedItems.clear();
  QSet<QString> pageFingerprints;
  int loadedRawCount = 0;
  bool firstDisplayUpdate = true;

  while (true) {
    if (!guard || generation != m_requestGeneration) {
      co_return;
    }

    const int preferredPageSize =
        firstDisplayUpdate ? query.firstPageSize : query.pageSize;
    int pageLimit = qMax(1, preferredPageSize);
    if (query.requestLimit > 0) {
      const int remaining = query.requestLimit - loadedRawCount;
      if (remaining <= 0) {
        break;
      }
      pageLimit = qMin(pageLimit, remaining);
    }

    DashboardCategoryPage page =
        co_await fetchDashboardCategoryPage(query, loadedRawCount, pageLimit);
    if (!guard || generation != m_requestGeneration) {
      co_return;
    }

    if (page.rawItemCount <= 0) {
      qDebug() << "[CategoryView] progressive load stopped on empty page"
               << "| category=" << query.category
               << "| loadedRaw=" << loadedRawCount;
      break;
    }

    if (!page.fingerprint.isEmpty()) {
      if (pageFingerprints.contains(page.fingerprint)) {
        qWarning() << "[CategoryView] progressive load stopped on repeated page"
                   << "| category=" << query.category
                   << "| startIndex=" << loadedRawCount
                   << "| limit=" << pageLimit;
        break;
      }
      pageFingerprints.insert(page.fingerprint);
    }

    loadedRawCount += page.rawItemCount;
    if (firstDisplayUpdate) {
      setLoadedItems(page.items);
      firstDisplayUpdate = false;
      m_mediaGrid->setLoading(false);
    } else {
      appendUniqueLoadedItems(page.items);
    }

    if (query.requestLimit > 0 && loadedRawCount >= query.requestLimit) {
      break;
    }
    if (page.hasTotalRecordCount && loadedRawCount >= page.totalRecordCount) {
      break;
    }
    if (page.rawItemCount < pageLimit) {
      break;
    }
  }

  if (guard && generation == m_requestGeneration) {
    m_mediaGrid->setLoading(false);
    m_statsLabel->setText(tr("%1 Items").arg(m_loadedItems.size()));
    qDebug() << "[CategoryView] progressive load complete"
             << "| category=" << query.category
             << "| loadedRaw=" << loadedRawCount
             << "| displayed=" << m_loadedItems.size();
  }
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
  } else if (categoryType == "played") {
    m_sortButton->setCurrentIndex(1); 
    m_sortButton->setDescending(true);
    m_sortButton->setEnabled(true);
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

  
  m_mediaGrid->setItems(QList<MediaItem>());
  m_mediaGrid->setLoading(false);
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
  const int generation = ++m_requestGeneration;
  m_loadedItems.clear();

  if (isProgressiveDashboardCategory(m_currentCategory)) {
    co_await loadDashboardCategoryProgressively(
        buildDashboardCategoryQuery(sortBy, sortOrder));
    co_return;
  }

  const int requestLimit = dashboardCategoryRequestLimit(m_currentCategory);

  auto *mediaService = m_core->mediaService();

  try {
    QList<MediaItem> resultItems;

    
    if (m_currentCategory == "resume") {
      QList<MediaItem> rawItems =
          co_await mediaService->getResumeItems(requestLimit, sortBy,
                                                sortOrder);
      if (!guard)
        co_return;

      
      QSet<QString> seenSeriesIds;
      for (MediaItem item : rawItems) {
        if (item.type == "Episode" && !item.seriesId.isEmpty()) {
          if (seenSeriesIds.contains(item.seriesId))
            continue;
          seenSeriesIds.insert(item.seriesId);
          try {
            MediaItem seriesItem =
                co_await mediaService->getItemDetail(item.seriesId);
            if (!guard)
              co_return;
            resultItems.append(
                MediaItemUtils::withResumeContext(seriesItem, item));
          } catch (...) {
            if (!guard)
              co_return;
          }
        } else {
          resultItems.append(MediaItemUtils::withResumeContext(item, item));
        }
      }
    } else if (m_currentCategory == "latest") {
      resultItems =
          co_await mediaService->getLatestItems(requestLimit, sortBy,
                                                sortOrder);
    } else if (m_currentCategory == "recommended") {
      resultItems = co_await mediaService->getRecommendedMovies(
          requestLimit, QStringLiteral("Random"), QStringLiteral("Ascending"));
    } else if (m_currentCategory == "played") {
      resultItems =
          co_await mediaService->getPlayedItems(requestLimit, sortBy,
                                                sortOrder);
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

    
    
    
    if (!guard || generation != m_requestGeneration)
      co_return;

    
    m_statsLabel->setText(tr("%1 Items").arg(resultItems.size()));
    m_mediaGrid->setItems(resultItems);
    m_mediaGrid->setLoading(false);

  } catch (const std::exception &e) {
    
    if (!guard)
      co_return;
    m_mediaGrid->setLoading(false);
    m_statsLabel->setText(tr("Error Loading Items"));
    qDebug() << "Category View fetching error:" << e.what();
  }
}




void CategoryView::onMediaItemUpdated(const MediaItem &item) {
  if (m_mediaGrid) {
    
    if (m_currentCategory == "resume") {
      const bool isSeriesDetailWithoutResumeContext =
          item.type == "Series" && !item.hasResumeContext;
      const bool canRemoveFromResume =
          MediaItemUtils::canRemoveFromResume(item);

      
      if ((!isSeriesDetailWithoutResumeContext || item.userData.played) &&
          !canRemoveFromResume) {
        m_mediaGrid->removeItem(item.id);
        
        m_statsLabel->setText(tr("%1 Items").arg(m_mediaGrid->itemCount()));
        return;
      }
    }
    if (m_currentCategory == "played") {
      if (MediaItemUtils::isCompletedWatchingItem(item)) {
        m_mediaGrid->prependOrUpdateItem(item);
      } else {
        m_mediaGrid->removeItem(item.id);
      }
      m_statsLabel->setText(tr("%1 Items").arg(m_mediaGrid->itemCount()));
      return;
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
