#include "detailview.h"
#include "../../components/detailactionwidget.h"
#include "../../components/detailbottominfowidget.h"
#include "../../components/detailcontentwidget.h"
#include "../../components/flowlayout.h"
#include "../../components/horizontallistviewgallery.h"
#include "../../components/mediasectionwidget.h"
#include "../../components/modernmenubutton.h"
#include "../../components/moderntoast.h"
#include "../../managers/playbackmanager.h"
#include "../../utils/numberextractor.h"
#include "medialistmodel.h"
#include "overviewdialog.h"
#include <config/config_keys.h>
#include <config/configstore.h>
#include <qembycore.h>
#include <services/media/mediaservice.h>

#include <QClipboard>
#include <QDebug>
#include <QEvent>
#include <QGraphicsDropShadowEffect>
#include <QGridLayout>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QListView>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QScrollBar>
#include <QTextDocument>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

DetailView::DetailView(QEmbyCore *core, QWidget *parent)
    : BaseView(core, parent) {
  setAttribute(Qt::WA_StyledBackground, true);
  setObjectName("detail-view");
  setProperty("showGlobalSearch", true);
  setupUi();
}

void DetailView::scrollToTop() {
  if (m_mainScrollArea && m_mainScrollArea->verticalScrollBar()) {
    if (m_vScrollAnim && m_vScrollAnim->state() == QAbstractAnimation::Running)
      m_vScrollAnim->stop();
    m_vScrollTarget = 0;
    m_mainScrollArea->verticalScrollBar()->setValue(0);
  }
}

void DetailView::setupUi() {
  auto *mainLayout = new QVBoxLayout(this);
  mainLayout->setContentsMargins(0, 0, 0, 0);

  m_mainScrollArea = new QScrollArea(this);
  m_mainScrollArea->setWidgetResizable(true);
  m_mainScrollArea->setFrameShape(QFrame::NoFrame);
  m_mainScrollArea->setObjectName("detail-scrollarea");
  m_mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_mainScrollArea->setStyleSheet("QScrollArea { background: transparent; }");

  m_vScrollAnim = new QPropertyAnimation(m_mainScrollArea->verticalScrollBar(),
                                         "value", this);
  m_vScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
  m_vScrollAnim->setDuration(450);

  m_contentWidget = new DetailContentWidget(m_mainScrollArea);
  m_contentWidget->setObjectName("detail-content-container");

  auto *contentLayout = new QVBoxLayout(m_contentWidget);
  contentLayout->setContentsMargins(0, 0, 0, 10);
  contentLayout->setSpacing(0);

  QWidget *infoContainer = new QWidget(m_contentWidget);
  infoContainer->setObjectName("detail-info-container");
  infoContainer->setStyleSheet(
      "QWidget#detail-info-container { background: transparent; }");

  m_infoLayout = new QGridLayout(infoContainer);
  m_infoLayout->setContentsMargins(40, 40, 40, 0);
  m_infoLayout->setHorizontalSpacing(40);
  m_infoLayout->setVerticalSpacing(0);
  m_infoLayout->setRowMinimumHeight(0, 110);

  m_logoLabel = new QLabel(infoContainer);
  m_logoLabel->setObjectName("detail-logo-label");
  m_logoLabel->setAlignment(Qt::AlignRight | Qt::AlignTop);
  m_infoLayout->addWidget(m_logoLabel, 0, 1, 1, 2,
                          Qt::AlignRight | Qt::AlignTop);

  m_posterLabel = new QLabel(infoContainer);
  m_posterLabel->setFixedSize(250, 375);
  m_posterLabel->setObjectName("detail-poster-label");
  m_posterLabel->setAlignment(Qt::AlignCenter);

  auto *shadow = new QGraphicsDropShadowEffect(this);
  shadow->setBlurRadius(20);
  shadow->setColor(QColor(0, 0, 0, 80));
  shadow->setOffset(0, 8);
  m_posterLabel->setGraphicsEffect(shadow);
  m_infoLayout->addWidget(m_posterLabel, 1, 0, 1, 1, Qt::AlignTop);

  m_textContainer = new QWidget(infoContainer);
  m_textContainer->setMaximumWidth(1200);
  m_textContainer->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Preferred);
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
  m_ratingStarLabel->setPixmap(
      QIcon(":/svg/dark/star-filled.svg").pixmap(16, 16));
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
  connect(m_numberButton, &QPushButton::clicked, this,
          &DetailView::copyDisplayedNumber);

  metaLayout->addWidget(m_ratingStarLabel);
  metaLayout->addWidget(m_metaLabel, 0, Qt::AlignLeft);
  metaLayout->addWidget(m_numberButton, 0, Qt::AlignLeft);
  metaLayout->addStretch(1);
  m_metaRowWidget->hide();

  m_tagsWidget = new QWidget(m_textContainer);
  m_tagsLayout = new FlowLayout(m_tagsWidget, 0, 8, 8);

  m_actionWidget = new DetailActionWidget(m_textContainer);
  connect(m_actionWidget, &DetailActionWidget::playRequested, this,
          [this]() -> QCoro::Task<void> {
            co_await executePlay(m_currentPlayableItem, 0);
          });
  connect(m_actionWidget, &DetailActionWidget::resumeRequested, this,
          [this]() -> QCoro::Task<void> {
            co_await executePlay(
                m_currentPlayableItem,
                m_currentPlayableItem.userData.playbackPositionTicks);
          });
  connect(m_actionWidget, &DetailActionWidget::favoriteRequested, this,
          [this]() {
            if (!m_currentMediaItem.id.isEmpty())
              handleFavoriteRequested(m_currentMediaItem);
          });
  connect(m_actionWidget, &DetailActionWidget::sourceVersionChanged, this,
          &DetailView::onVersionChanged);
  connect(m_actionWidget, &DetailActionWidget::externalPlayRequested, this,
          [this](const QString &playerPath) -> QCoro::Task<void> {
            co_await executeExternalPlay(m_currentPlayableItem, playerPath);
          });

  m_taglineLabel = new QLabel(m_textContainer);
  m_taglineLabel->setObjectName("detail-tagline");

  m_overviewLabel = new QLabel(m_textContainer);
  m_overviewLabel->setObjectName("detail-overview");
  m_overviewLabel->setWordWrap(true);
  m_overviewLabel->setSizePolicy(QSizePolicy::Preferred,
                                 QSizePolicy::Expanding);
  m_overviewLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
  m_overviewLabel->setTextInteractionFlags(Qt::TextBrowserInteraction);
  connect(m_overviewLabel, &QLabel::linkActivated, this,
          &DetailView::onOverviewMoreClicked);

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

  
  
  
  m_seasonWidget =
      new MediaSectionWidget(tr("Seasons"), m_core, m_contentWidget);
  m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
  m_seasonWidget->setGalleryHeight(300);

  
  m_episodeWidget =
      new MediaSectionWidget(tr("Episodes"), m_core, m_contentWidget);
  m_episodeWidget->setCardStyle(MediaCardDelegate::LibraryTile);
  
  
  {
    int imgH = 100;
    int imgW = qRound(imgH * 16.0 / 9.0); 
    m_episodeWidget->setTileSize(
        QSize(imgW + 16, 8 + imgH + 6 + 20)); 
  }
  m_episodeWidget->setGalleryHeight(145);

  
  m_seasonSwitcher = new ModernMenuButton(m_episodeWidget);
  m_seasonSwitcher->hide(); 
  m_episodeWidget->setHeaderWidget(m_seasonSwitcher);
  connect(m_seasonSwitcher, &ModernMenuButton::currentIndexChanged, this,
          [this](int idx) {
            if (idx >= 0 && idx != m_currentSeasonIndex) {
              m_currentSeasonIndex = idx;
              switchToSeason(idx);
            }
          });

  m_castWidget =
      new MediaSectionWidget(tr("Cast & Crew"), m_core, m_contentWidget);
  m_castWidget->setCardStyle(MediaCardDelegate::Cast);
  m_castWidget->setGalleryHeight(280);

  m_additionalPartsWidget =
      new MediaSectionWidget(tr("Additional Parts"), m_core, m_contentWidget);
  m_additionalPartsWidget->setCardStyle(MediaCardDelegate::Poster);
  m_additionalPartsWidget->setGalleryHeight(300);

  m_collectionWidget =
      new MediaSectionWidget(tr("Collection"), m_core, m_contentWidget);
  m_collectionWidget->setCardStyle(MediaCardDelegate::Poster);
  m_collectionWidget->setGalleryHeight(300);

  m_similarWidget =
      new MediaSectionWidget(tr("More Like This"), m_core, m_contentWidget);
  m_similarWidget->setCardStyle(MediaCardDelegate::Poster);
  m_similarWidget->setGalleryHeight(300);

  m_seasonWidget->gallery()->listView()->viewport()->installEventFilter(this);
  m_episodeWidget->gallery()->listView()->viewport()->installEventFilter(this);
  m_castWidget->gallery()->listView()->viewport()->installEventFilter(this);
  m_additionalPartsWidget->gallery()
      ->listView()
      ->viewport()
      ->installEventFilter(this);
  m_collectionWidget->gallery()->listView()->viewport()->installEventFilter(
      this);
  m_similarWidget->gallery()->listView()->viewport()->installEventFilter(this);

  auto connectGallerySignals = [this](MediaSectionWidget *widget) {
    connect(widget, &MediaSectionWidget::playRequested, this,
            &BaseView::handlePlayRequested);
    connect(widget, &MediaSectionWidget::favoriteRequested, this,
            &BaseView::handleFavoriteRequested);
    connect(widget, &MediaSectionWidget::moreMenuRequested, this,
            &BaseView::handleMoreMenuRequested);
  };

  connectGallerySignals(m_seasonWidget);
  connectGallerySignals(m_episodeWidget);
  connectGallerySignals(m_castWidget);
  connectGallerySignals(m_additionalPartsWidget);
  connectGallerySignals(m_collectionWidget);
  connectGallerySignals(m_similarWidget);

  auto commonClicked = [this](const MediaItem &item) {
    if (item.type == "BoxSet" || item.type == "Playlist" ||
        item.type == "Folder")
      Q_EMIT navigateToFolder(item.id, item.name);
    else if (item.type == "Person")
      Q_EMIT navigateToPerson(item.id, item.name);
    else
      Q_EMIT navigateToDetail(item.id, item.name);
  };

  
  connect(m_seasonWidget, &MediaSectionWidget::itemClicked, this,
          [this](const MediaItem &item) {
            if (item.type == "Season")
              Q_EMIT navigateToSeason(m_currentItemId, item.id, item.name);
            else
              Q_EMIT navigateToDetail(item.id, item.name);
          });

  
  connect(m_episodeWidget, &MediaSectionWidget::itemClicked, this,
          [this, commonClicked](const MediaItem &item) {
            if (item.type == "Episode")
              Q_EMIT navigateToDetail(item.id, item.name);
            else
              commonClicked(item);
          });

  connect(m_castWidget, &MediaSectionWidget::itemClicked, this,
          [this](const MediaItem &item) {
            Q_EMIT navigateToPerson(item.id, item.name);
          });
  connect(m_additionalPartsWidget, &MediaSectionWidget::itemClicked, this,
          commonClicked);
  connect(m_collectionWidget, &MediaSectionWidget::itemClicked, this,
          commonClicked);
  connect(m_similarWidget, &MediaSectionWidget::itemClicked, this,
          commonClicked);

  contentLayout->addWidget(m_episodeWidget);
  contentLayout->addWidget(m_seasonWidget);
  contentLayout->addWidget(m_castWidget);
  contentLayout->addWidget(m_additionalPartsWidget);
  contentLayout->addWidget(m_collectionWidget);
  contentLayout->addWidget(m_similarWidget);

  m_bottomInfoWidget = new DetailBottomInfoWidget(m_contentWidget);
  connect(m_bottomInfoWidget, &DetailBottomInfoWidget::filterClicked, this,
          [this](const QString &type, const QString &value) {
            Q_EMIT navigateToFilteredView(type, value);
          });
  contentLayout->addWidget(m_bottomInfoWidget);

  contentLayout->addStretch();
  m_mainScrollArea->setWidget(m_contentWidget);
  mainLayout->addWidget(m_mainScrollArea);
  m_mainScrollArea->viewport()->installEventFilter(this);
}

QCoro::Task<void> DetailView::loadItem(const QString &itemId) {
  m_currentItemId = itemId;

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
  m_castWidget->clear();
  m_similarWidget->clear();
  m_collectionWidget->clear();
  m_additionalPartsWidget->clear();

  
  m_seriesSeasons.clear();
  m_currentSeasonIndex = 0;
  if (m_seasonSwitcher) {
    QSignalBlocker blocker(m_seasonSwitcher);
    m_seasonSwitcher->clear();
    m_seasonSwitcher->hide();
  }

  scrollToTop();

  QPointer<DetailView> safeThis(this);
  QString targetId = itemId;
  QString detectedType = "";

  try {
    MediaItem item = co_await m_core->mediaService()->getItemDetail(targetId);
    if (!safeThis || safeThis->m_currentItemId != targetId)
      co_return;

    detectedType = item.type;
    m_currentPlayableItem = item;
    co_await safeThis->updateUi(item);
  } catch (...) {
    if (safeThis)
      safeThis->m_titleLabel->setText(tr("Error Loading Item"));
    co_return;
  }

  if (!safeThis)
    co_return;

  
  executeFetchSecondaries(safeThis, safeThis->m_core, targetId, detectedType);
}







QCoro::Task<void>
DetailView::executeFetchSecondaries(QPointer<DetailView> safeThis,
                                    QEmbyCore *core, QString targetId,
                                    QString itemType) {
  if (!safeThis)
    co_return;

  
  if (itemType == "Series") {
    co_await safeThis->fetchSeriesNextUp(targetId);
    if (!safeThis)
      co_return;

    
    auto seasons = co_await core->mediaService()->getSeasons(targetId);
    if (!safeThis)
      co_return;

    safeThis->m_seriesSeasons = seasons;

    
    int targetSeasonIdx = 0;
    int playableParentIdx = safeThis->m_currentPlayableItem.parentIndexNumber;
    if (playableParentIdx >= 0) {
      for (int i = 0; i < seasons.size(); ++i) {
        if (seasons[i].indexNumber == playableParentIdx) {
          targetSeasonIdx = i;
          break;
        }
      }
    }
    safeThis->m_currentSeasonIndex = targetSeasonIdx;

    
    if (safeThis->m_seasonWidget) {
      safeThis->m_seasonWidget->setTitle(tr("Seasons"));
      safeThis->m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
      safeThis->m_seasonWidget->setGalleryHeight(300);
      safeThis->m_seasonWidget->setItems(seasons);
    }

    
    if (!seasons.isEmpty() && safeThis->m_episodeWidget) {
      
      if (safeThis->m_seasonSwitcher) {
        QSignalBlocker blocker(safeThis->m_seasonSwitcher);
        safeThis->m_seasonSwitcher->clear();
        for (const auto &s : seasons)
          safeThis->m_seasonSwitcher->addItem(s.name, "");
        safeThis->m_seasonSwitcher->setCurrentIndex(targetSeasonIdx);
        safeThis->m_seasonSwitcher->setVisible(seasons.size() > 1);
      }

      auto targetSeasonId = seasons[targetSeasonIdx].id;
      QString playableItemId = safeThis->m_currentPlayableItem.id;
      co_await safeThis->m_episodeWidget->loadAsync(
          [core, targetId, targetSeasonId,
           safeThis]() -> QCoro::Task<QList<MediaItem>> {
            auto episodes = co_await core->mediaService()->getEpisodes(
                targetId, targetSeasonId);
            co_return episodes;
          });

      
      if (safeThis && !playableItemId.isEmpty()) {
        auto *listView = safeThis->m_episodeWidget->gallery()->listView();
        auto *model = listView->model();
        for (int row = 0; row < model->rowCount(); ++row) {
          QModelIndex idx = model->index(row, 0);
          MediaItem ep =
              idx.data(MediaListModel::ItemDataRole).value<MediaItem>();
          if (ep.id == playableItemId) {
            listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
            safeThis->m_episodeWidget->gallery()->setHighlightedItemId(
                playableItemId);
            break;
          }
        }
      }
    }
    if (!safeThis)
      co_return;
  }

  
  co_await safeThis->m_similarWidget->loadAsync([core, targetId]() {
    return core->mediaService()->getSimilarItems(targetId, 15);
  });
  if (!safeThis)
    co_return;

  
  co_await safeThis->m_collectionWidget->loadAsync([core, targetId]() {
    return core->mediaService()->getItemCollections(targetId);
  });
  if (!safeThis)
    co_return;

  
  co_await safeThis->m_additionalPartsWidget->loadAsync([core, targetId]() {
    return core->mediaService()->getAdditionalParts(targetId);
  });
}

QCoro::Task<void> DetailView::fetchSeriesNextUp(const QString &targetId) {
  QPointer<DetailView> safeThis(this);
  try {
    MediaItem playableItem;
    auto nextUp = co_await m_core->mediaService()->getNextUp(targetId);

    if (!nextUp.isEmpty())
      playableItem = nextUp.first();
    else {
      auto seasons = co_await m_core->mediaService()->getSeasons(targetId);
      if (!seasons.isEmpty()) {
        auto episodes = co_await m_core->mediaService()->getEpisodes(
            targetId, seasons.first().id);
        if (!episodes.isEmpty())
          playableItem = episodes.first();
      }
    }

    if (safeThis && safeThis->m_currentItemId == targetId &&
        !playableItem.id.isEmpty()) {
      safeThis->m_currentPlayableItem = playableItem;
      QString epTag = QString("S%1:E%2")
                          .arg(playableItem.parentIndexNumber)
                          .arg(playableItem.indexNumber);
      safeThis->m_actionWidget->setupSeriesMode(playableItem, epTag);
    }
  } catch (...) {
  }
}

QCoro::Task<void> DetailView::onVersionChanged(int index) {
  if (index < 0 || index >= m_currentMediaItem.mediaSources.size())
    co_return;
  MediaSourceInfo source = m_currentMediaItem.mediaSources[index];
  QPointer<DetailView> safeThis(this);

  if (source.mediaStreams.isEmpty()) {
    try {
      PlaybackInfo info =
          co_await m_core->mediaService()->getPlaybackInfo(m_currentItemId);
      if (!safeThis)
        co_return;
      if (!info.mediaSources.isEmpty()) {
        safeThis->m_currentMediaItem.mediaSources = info.mediaSources;

        
        
        if (safeThis->m_currentMediaItem.type != "Series") {
          safeThis->m_currentPlayableItem.mediaSources = info.mediaSources;
        }

        if (index < safeThis->m_currentMediaItem.mediaSources.size()) {
          source = safeThis->m_currentMediaItem.mediaSources[index];
        }
      }
    } catch (...) {
      co_return;
    }
  }
  if (!safeThis || source.mediaStreams.isEmpty())
    co_return;

  safeThis->m_actionWidget->setStreams(source);
  QList<MediaSourceInfo> singleSource;
  singleSource.append(source);
  safeThis->m_bottomInfoWidget->setInfo(safeThis->m_currentMediaItem,
                                        singleSource);
}

QCoro::Task<void> DetailView::executePlay(MediaItem targetItem,
                                          long long startTicks) {
  if (targetItem.id.isEmpty())
    co_return;
  try {
    MediaItem actualItem = targetItem;
    if (actualItem.mediaSources.isEmpty()) {
      PlaybackInfo info =
          co_await m_core->mediaService()->getPlaybackInfo(actualItem.id);
      actualItem.mediaSources = info.mediaSources;
    }
    if (actualItem.mediaSources.isEmpty())
      co_return;

    int sourceIdx = 0;
    int selectedAudioIdx = -1;
    int selectedSubIdx = -1;

    if (actualItem.id == m_currentMediaItem.id) {
      
      sourceIdx = m_actionWidget->currentSourceIndex();
      selectedAudioIdx = m_actionWidget->currentAudioIndex();
      selectedSubIdx = m_actionWidget->currentSubtitleIndex();
    } else {
      
      
      if (actualItem.mediaSources.size() > 1) {
        QString versionPref =
            ConfigStore::instance()
                ->get<QString>(ConfigKeys::PlayerPreferredVersion)
                .trimmed();
        if (!versionPref.isEmpty()) {
          QStringList keywords = versionPref.split(',', Qt::SkipEmptyParts);
          for (const QString &kw : keywords) {
            QString keyword = kw.trimmed();
            if (keyword.isEmpty())
              continue;
            bool found = false;
            for (int i = 0; i < actualItem.mediaSources.size(); ++i) {
              if (actualItem.mediaSources[i].name.contains(
                      keyword, Qt::CaseInsensitive)) {
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
    }

    if (sourceIdx >= actualItem.mediaSources.size())
      sourceIdx = 0;
    MediaSourceInfo modifiedSource = actualItem.mediaSources[sourceIdx];

    if (actualItem.id == m_currentMediaItem.id) {
      
      
      
      for (auto &stream : modifiedSource.mediaStreams) {
        if (stream.type == "Audio" && selectedAudioIdx >= 0)
          stream.isDefault = (stream.index == selectedAudioIdx);
        else if (stream.type == "Subtitle" && selectedSubIdx >= 0)
          stream.isDefault = (stream.index == selectedSubIdx);
      }
    } else {
      
      QString prefAudioLang = ConfigStore::instance()->get<QString>(
          ConfigKeys::PlayerAudioLang, "auto");
      QString prefSubLang = ConfigStore::instance()->get<QString>(
          ConfigKeys::PlayerSubLang, "auto");

      {
        int bestAudioIdx = -1;
        int bestSubIdx = -1;
        int firstSubIdx = -1;       
        bool hasDefaultSub = false; 
        bool audioFound = false, subFound = false;

        for (const auto &stream : modifiedSource.mediaStreams) {
          if (stream.type == "Audio" && !audioFound &&
              prefAudioLang != "auto" &&
              stream.language.compare(prefAudioLang, Qt::CaseInsensitive) ==
                  0) {
            bestAudioIdx = stream.index;
            audioFound = true;
          } else if (stream.type == "Subtitle") {
            if (firstSubIdx < 0)
              firstSubIdx = stream.index;
            if (stream.isDefault)
              hasDefaultSub = true;
            if (!subFound && prefSubLang != "auto" && prefSubLang != "none" &&
                stream.language.compare(prefSubLang, Qt::CaseInsensitive) ==
                    0) {
              bestSubIdx = stream.index;
              subFound = true;
            }
          }
        }

        
        
        if (bestAudioIdx >= 0) {
          for (auto &stream : modifiedSource.mediaStreams) {
            if (stream.type == "Audio")
              stream.isDefault = (stream.index == bestAudioIdx);
          }
        }

        
        if (prefSubLang == "none") {
          
          for (auto &stream : modifiedSource.mediaStreams) {
            if (stream.type == "Subtitle")
              stream.isDefault = false;
          }
        } else if (bestSubIdx >= 0) {
          
          for (auto &stream : modifiedSource.mediaStreams) {
            if (stream.type == "Subtitle")
              stream.isDefault = (stream.index == bestSubIdx);
          }
        } else if (firstSubIdx >= 0 && !hasDefaultSub) {
          
          for (auto &stream : modifiedSource.mediaStreams) {
            if (stream.type == "Subtitle")
              stream.isDefault = (stream.index == firstSubIdx);
          }
        }
        
      }
    }

    QString streamUrl =
        m_core->mediaService()->getStreamUrl(actualItem.id, modifiedSource.id);

    QString playTitle = actualItem.name;
    if (actualItem.type == "Episode") {
      QString seriesName = actualItem.seriesName.isEmpty()
                               ? m_currentMediaItem.name
                               : actualItem.seriesName;
      playTitle = QString("%1 - S%2:E%3 - %4")
                      .arg(seriesName)
                      .arg(actualItem.parentIndexNumber)
                      .arg(actualItem.indexNumber)
                      .arg(actualItem.name);
    }

    
    PlaybackManager::instance()->startInternalPlayback(
        actualItem.id, playTitle, streamUrl, startTicks,
        QVariant::fromValue(modifiedSource));
  } catch (...) {
  }
}

QCoro::Task<void> DetailView::executeExternalPlay(MediaItem targetItem,
                                                  QString playerPath) {
  if (targetItem.id.isEmpty() || playerPath.isEmpty())
    co_return;
  try {
    MediaItem actualItem = targetItem;
    if (actualItem.mediaSources.isEmpty()) {
      PlaybackInfo info =
          co_await m_core->mediaService()->getPlaybackInfo(actualItem.id);
      actualItem.mediaSources = info.mediaSources;
    }
    if (actualItem.mediaSources.isEmpty())
      co_return;

    
    int sourceIdx = 0;
    if (actualItem.id == m_currentMediaItem.id) {
      sourceIdx = m_actionWidget->currentSourceIndex();
    }
    if (sourceIdx >= actualItem.mediaSources.size())
      sourceIdx = 0;
    MediaSourceInfo modifiedSource = actualItem.mediaSources[sourceIdx];

    if (actualItem.id == m_currentMediaItem.id) {
      int selectedAudioIdx = m_actionWidget->currentAudioIndex();
      int selectedSubIdx = m_actionWidget->currentSubtitleIndex();
      for (auto &stream : modifiedSource.mediaStreams) {
        if (stream.type == "Audio" && selectedAudioIdx >= 0)
          stream.isDefault = (stream.index == selectedAudioIdx);
        else if (stream.type == "Subtitle" && selectedSubIdx >= 0)
          stream.isDefault = (stream.index == selectedSubIdx);
      }
    }

    QString streamUrl =
        m_core->mediaService()->getStreamUrl(actualItem.id, modifiedSource.id);

    QString playTitle = actualItem.name;
    if (actualItem.type == "Episode") {
      QString seriesName = actualItem.seriesName.isEmpty()
                               ? m_currentMediaItem.name
                               : actualItem.seriesName;
      playTitle = QString("%1 - S%2:E%3 - %4")
                      .arg(seriesName)
                      .arg(actualItem.parentIndexNumber)
                      .arg(actualItem.indexNumber)
                      .arg(actualItem.name);
    }

    long long startTicks = actualItem.userData.playbackPositionTicks;
    PlaybackManager::instance()->startExternalPlayback(
        playerPath, actualItem.id, playTitle, streamUrl, startTicks,
        QVariant::fromValue(modifiedSource));
  } catch (...) {
  }
}

void DetailView::updateBackdrop() {
  if (m_contentWidget)
    static_cast<DetailContentWidget *>(m_contentWidget)
        ->setBackdrop(m_currentBackdropPix);
}

void DetailView::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
  if (m_overviewLabel && m_overviewLabel->width() > 10 &&
      qAbs(m_overviewLabel->width() - m_lastOverviewWidth) > 5) {
    m_lastOverviewWidth = m_overviewLabel->width();
    updateOverviewElidedText();
  }
}

bool DetailView::eventFilter(QObject *obj, QEvent *event) {
  if (m_mainScrollArea && obj == m_mainScrollArea->viewport() &&
      event->type() == QEvent::Resize) {
    QResizeEvent *re = static_cast<QResizeEvent *>(event);
    int newWidth = re->size().width();
    if (m_contentWidget && newWidth > 100 &&
        m_contentWidget->maximumWidth() != newWidth) {
      m_contentWidget->setMaximumWidth(newWidth);
    }
  }
  if (event->type() == QEvent::Wheel) {
    bool isHorizontalViewport =
        obj->parent() &&
        obj->parent()->property("isHorizontalListView").toBool();
    bool isMainViewport =
        (m_mainScrollArea && obj == m_mainScrollArea->viewport());
    if (isHorizontalViewport || isMainViewport) {
      QWheelEvent *we = static_cast<QWheelEvent *>(event);
      QScrollBar *vBar = m_mainScrollArea->verticalScrollBar();
      if (vBar) {
        int currentVal = vBar->value();
        if (m_vScrollAnim &&
            m_vScrollAnim->state() == QAbstractAnimation::Running)
          currentVal = m_vScrollTarget;
        int step = we->angleDelta().y();
        int newTarget =
            qBound(vBar->minimum(), currentVal - step, vBar->maximum());
        if (newTarget != vBar->value()) {
          m_vScrollTarget = newTarget;
          if (m_vScrollAnim) {
            m_vScrollAnim->stop();
            m_vScrollAnim->setStartValue(vBar->value());
            m_vScrollAnim->setEndValue(m_vScrollTarget);
            m_vScrollAnim->start();
          }
        }
      }
      return true;
    }
  }
  return QWidget::eventFilter(obj, event);
}

void DetailView::showEvent(QShowEvent *event) {
  BaseView::showEvent(event);
  if (!m_currentItemId.isEmpty() && m_core && m_core->mediaService()) {
    executeSilentRefresh(QPointer<DetailView>(this), m_core, m_currentItemId);
  }
}

void DetailView::clearLayout(QLayout *layout) {
  if (!layout)
    return;
  QLayoutItem *child;
  while ((child = layout->takeAt(0)) != nullptr) {
    if (child->widget()) {
      child->widget()->hide();
      child->widget()->setParent(nullptr);
      child->widget()->deleteLater();
    } else if (child->layout()) {
      clearLayout(child->layout());
    }
    delete child;
  }
}

QString DetailView::formatRunTime(long long ticks) {
  long long totalSeconds = ticks / 10000000;
  long long hours = totalSeconds / 3600;
  long long minutes = (totalSeconds % 3600) / 60;
  if (hours > 0)
    return QString(tr("%1 hr %2 min")).arg(hours).arg(minutes);
  return QString(tr("%1 min")).arg(minutes);
}

bool DetailView::shouldShowDisplayNumber(const MediaItem &item) const {
  if (item.type.compare("Movie", Qt::CaseInsensitive) != 0) {
    return false;
  }

  static const QList<QRegularExpression> adultMarkers = {
      QRegularExpression(R"((^|[^0-9])18\+?([^0-9]|$))",
                         QRegularExpression::CaseInsensitiveOption),
      QRegularExpression(R"(\b(?:R[-\s]?18\+?|NC[-\s]?17|JAV|AV)\b)",
                         QRegularExpression::CaseInsensitiveOption),
      QRegularExpression(QStringLiteral("(成人|无码|有码|無碼|有碼)"))};

  const auto containsAdultMarker = [](const QString &value) -> bool {
    const QString trimmedValue = value.trimmed();
    if (trimmedValue.isEmpty()) {
      return false;
    }

    for (const QRegularExpression &pattern : adultMarkers) {
      if (pattern.match(trimmedValue).hasMatch()) {
        return true;
      }
    }

    return false;
  };

  if (containsAdultMarker(item.officialRating)) {
    return true;
  }

  for (const QString &genre : item.genres) {
    if (containsAdultMarker(genre)) {
      return true;
    }
  }

  for (const QString &tag : item.tags) {
    if (containsAdultMarker(tag)) {
      return true;
    }
  }

  return false;
}

QStringList DetailView::buildNumberCandidates(const MediaItem &item) const {
  QStringList candidates;
  const auto appendCandidate = [&candidates](const QString &value) {
    const QString trimmedValue = value.trimmed();
    if (!trimmedValue.isEmpty() && !candidates.contains(trimmedValue)) {
      candidates.append(trimmedValue);
    }
  };

  for (const MediaSourceInfo &source : item.mediaSources) {
    appendCandidate(source.path);
  }

  appendCandidate(item.path);
  appendCandidate(item.name);
  appendCandidate(item.originalTitle);
  appendCandidate(item.sortName);

  for (const QString &tagline : item.taglines) {
    appendCandidate(tagline);
  }

  for (const QString &tag : item.tags) {
    appendCandidate(tag);
  }

  return candidates;
}

QString DetailView::extractDisplayNumber(const MediaItem &item) const {
  static const NumberExtractor extractor(true);
  const QStringList candidates = buildNumberCandidates(item);
  const QString displayNumber = extractor.extractBestNumber(candidates);

  qDebug() << "[DetailView] adult number extraction itemId=" << item.id
           << "officialRating=" << item.officialRating
           << "candidateCount=" << candidates.size()
           << "selected=" << displayNumber;

  return displayNumber;
}

void DetailView::updateDisplayNumber(const QString &number) {
  if (!m_numberButton || !m_metaRowWidget) {
    return;
  }

  const QString trimmedNumber = number.trimmed();
  const bool hasNumber = !trimmedNumber.isEmpty();
  m_numberButton->setVisible(hasNumber);
  m_numberButton->setText(trimmedNumber);
  m_numberButton->setToolTip(hasNumber ? tr("Click to copy number")
                                       : QString());
  m_metaRowWidget->setVisible(!m_metaLabel->text().isEmpty() || hasNumber);
}

void DetailView::copyDisplayedNumber() {
  if (!m_numberButton) {
    return;
  }

  const QString displayNumber = m_numberButton->text().trimmed();
  if (displayNumber.isEmpty()) {
    return;
  }

  if (QClipboard *clipboard = QGuiApplication::clipboard()) {
    clipboard->setText(displayNumber);
  }

  qDebug() << "[DetailView] copied display number" << displayNumber
           << "for itemId" << m_currentItemId;
  ModernToast::showMessage(tr("Number copied: %1").arg(displayNumber), 2000);
}

void DetailView::updateOverviewElidedText() {
  if (m_cleanOverviewText.isEmpty())
    return;
  int labelWidth = m_overviewLabel->width() <= 10
                       ? m_textContainer->maximumWidth()
                       : m_overviewLabel->width();
  QPointer<DetailView> safeThis(this);
  QTimer::singleShot(0, this, [safeThis, labelWidth]() {
    if (!safeThis)
      return;
    QFontMetrics fm(safeThis->m_overviewLabel->font());
    int maxHeight = fm.lineSpacing() * 5 + 10;
    QTextDocument doc;
    doc.setDefaultFont(safeThis->m_overviewLabel->font());
    doc.setTextWidth(labelWidth);

    QString fullHtml = safeThis->m_cleanOverviewText.toHtmlEscaped();
    fullHtml.replace("\n", "<br>");
    doc.setHtml(fullHtml);

    if (doc.size().height() <= maxHeight) {
      safeThis->m_overviewLabel->setText(fullHtml);
      return;
    }

    int low = 0, high = safeThis->m_cleanOverviewText.length(), best = 0;
    QString moreLink = tr("... <a href='more' style='color:#3B82F6; "
                          "text-decoration:none; font-weight:bold;'>More</a>");
    while (low <= high) {
      int mid = low + (high - low) / 2;
      QString testHtml =
          safeThis->m_cleanOverviewText.left(mid).toHtmlEscaped();
      testHtml.replace("\n", "<br>");
      doc.setHtml(testHtml + moreLink);
      if (doc.size().height() <= maxHeight) {
        best = mid;
        low = mid + 1;
      } else {
        high = mid - 1;
      }
    }

    QString finalHtml =
        safeThis->m_cleanOverviewText.left(best).toHtmlEscaped();
    finalHtml.replace("\n", "<br>");
    safeThis->m_overviewLabel->setText(finalHtml + moreLink);
  });
}

QCoro::Task<void> DetailView::updateUi(MediaItem item, bool isSilentRefresh) {
  m_currentMediaItem = item;
  m_titleLabel->setText(item.name);

  QStringList metas;
  if (item.communityRating > 0) {
    m_ratingStarLabel->show();
    metas << QString::number(item.communityRating, 'f', 1);
  } else {
    m_ratingStarLabel->hide();
  }
  if (item.productionYear > 0)
    metas << QString::number(item.productionYear);
  if (item.runTimeTicks > 0)
    metas << formatRunTime(item.runTimeTicks);
  if (!item.officialRating.isEmpty())
    metas << item.officialRating;
  const QString metaText = metas.join("  \u2022  ");
  m_metaLabel->setText(metaText);

  const bool shouldShowNumber = shouldShowDisplayNumber(item);
  const QString displayNumber =
      shouldShowNumber ? extractDisplayNumber(item) : QString();
  updateDisplayNumber(displayNumber);

  qDebug() << "[DetailView] updateUi itemId=" << item.id
           << "adultMovie=" << shouldShowNumber
           << "displayNumber=" << displayNumber;

  QString text = item.overview;
  text.replace(QRegularExpression("<br\\s*/?>",
                                  QRegularExpression::CaseInsensitiveOption),
               "\n");
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

  if (item.type == "Series") {
    if (!isSilentRefresh)
      m_actionWidget->setSeriesLoadingMode();
  } else {
    
    
    m_currentPlayableItem = item;
    m_actionWidget->setupNormalMode(item);
  }

  m_actionWidget->setSources(item.mediaSources, 0);
  m_actionWidget->refreshExtPlayerButton();

  if (!item.taglines.isEmpty()) {
    m_taglineLabel->setText(item.taglines.first());
    m_taglineLabel->show();
  } else {
    m_taglineLabel->hide();
  }

  m_isFavorite = item.isFavorite();
  m_actionWidget->setFavoriteState(m_isFavorite);

  m_tagsWidget->setUpdatesEnabled(false);
  clearLayout(m_tagsLayout);
  for (const QString &genre : item.genres) {
    auto *tagBtn = new QPushButton(genre, m_tagsWidget);
    tagBtn->setObjectName("detail-genre-tag");
    tagBtn->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    tagBtn->setCursor(Qt::PointingHandCursor);
    connect(tagBtn, &QPushButton::clicked, this,
            [this, genre]() { Q_EMIT navigateToFilteredView("Genre", genre); });
    m_tagsLayout->addWidget(tagBtn);
  }
  m_tagsWidget->setUpdatesEnabled(true);

  executeLoadImages(QPointer<DetailView>(this), m_core, item);

  if (!item.people.isEmpty()) {
    QList<MediaItem> castItems;
    for (const auto &person : item.people) {
      MediaItem fakeItem;
      fakeItem.id = person.id;
      fakeItem.name = person.name;
      fakeItem.images.primaryTag = person.primaryImageTag;
      
      QStringList personParts;
      if (!person.type.isEmpty()) {
        
        static const QHash<QString, const char *> typeMap = {
            {"Actor", QT_TR_NOOP("Actor")},
            {"Director", QT_TR_NOOP("Director")},
            {"Writer", QT_TR_NOOP("Writer")},
            {"Producer", QT_TR_NOOP("Producer")},
            {"Composer", QT_TR_NOOP("Composer")},
            {"Conductor", QT_TR_NOOP("Conductor")},
            {"GuestStar", QT_TR_NOOP("GuestStar")},
            {"Editor", QT_TR_NOOP("Editor")},
            {"Lyricist", QT_TR_NOOP("Lyricist")},
        };
        auto it = typeMap.constFind(person.type);
        personParts << (it != typeMap.constEnd() ? tr(it.value())
                                                 : person.type);
      }
      if (!person.role.isEmpty())
        personParts << person.role;
      fakeItem.overview = personParts.join(" · ");
      fakeItem.type = "Person";
      castItems.append(fakeItem);
    }
    m_castWidget->setItems(castItems);
  }

  QPointer<DetailView> safeThis(this);

  if (!item.mediaSources.isEmpty() &&
      !item.mediaSources.first().mediaStreams.isEmpty()) {
    int defaultIndex = m_actionWidget->currentSourceIndex();
    QList<MediaSourceInfo> defaultSource;
    if (defaultIndex < item.mediaSources.size()) {
      const MediaSourceInfo &dSrc = item.mediaSources[defaultIndex];
      m_actionWidget->setStreams(dSrc);
      defaultSource.append(dSrc);
    }
    m_bottomInfoWidget->setInfo(item, defaultSource);
  } else {
    
    
    m_bottomInfoWidget->setInfo(item, {});
    if (item.mediaType == "Video" || item.mediaType == "Audio" ||
        item.type == "Movie" || item.type == "Episode") {
      try {
        PlaybackInfo info =
            co_await m_core->mediaService()->getPlaybackInfo(item.id);
        if (!safeThis)
          co_return;
        if (item.id == safeThis->m_currentItemId &&
            !info.mediaSources.isEmpty()) {
          safeThis->m_currentMediaItem.mediaSources = info.mediaSources;

          const QString refreshedNumber =
              safeThis->shouldShowDisplayNumber(safeThis->m_currentMediaItem)
                  ? safeThis->extractDisplayNumber(safeThis->m_currentMediaItem)
                  : QString();
          safeThis->updateDisplayNumber(refreshedNumber);

          
          
          if (item.type != "Series") {
            safeThis->m_currentPlayableItem.mediaSources = info.mediaSources;
          }

          int idx = safeThis->m_actionWidget->currentSourceIndex();
          if (idx < safeThis->m_currentMediaItem.mediaSources.size()) {
            const MediaSourceInfo &singleSource =
                safeThis->m_currentMediaItem.mediaSources[idx];
            safeThis->m_actionWidget->setStreams(singleSource);
            QList<MediaSourceInfo> sList;
            sList.append(singleSource);
            safeThis->m_bottomInfoWidget->setInfo(safeThis->m_currentMediaItem,
                                                  sList);
          }
        }
      } catch (...) {
      }
    }
  }

  if (!safeThis)
    co_return;
  safeThis->m_bottomInfoWidget->show();
  if (!isSilentRefresh && safeThis)
    QMetaObject::invokeMethod(safeThis.data(), "scrollToTop",
                              Qt::QueuedConnection);
}

void DetailView::onOverviewMoreClicked(const QString &link) {
  if (link == "more") {
    OverviewDialog dialog(this);
    dialog.setMediaItem(m_currentMediaItem, m_currentPosterPix);
    dialog.exec();
  }
}

QCoro::Task<void>
DetailView::executeSilentRefresh(QPointer<DetailView> safeThis, QEmbyCore *core,
                                 QString itemId) {
  try {
    MediaItem item = co_await core->mediaService()->getItemDetail(itemId);
    if (safeThis && item.id == safeThis->m_currentItemId) {
      co_await safeThis->updateUi(item, true);

      
      if (item.type == "Series") {
        co_await safeThis->fetchSeriesNextUp(itemId);
        if (!safeThis)
          co_return;

        auto seasons = co_await core->mediaService()->getSeasons(itemId);
        if (!safeThis)
          co_return;

        safeThis->m_seriesSeasons = seasons;

        
        int playableParentIdx =
            safeThis->m_currentPlayableItem.parentIndexNumber;
        if (playableParentIdx >= 0) {
          for (int i = 0; i < seasons.size(); ++i) {
            if (seasons[i].indexNumber == playableParentIdx) {
              safeThis->m_currentSeasonIndex = i;
              break;
            }
          }
        }

        if (safeThis->m_seasonWidget) {
          safeThis->m_seasonWidget->setTitle(tr("Seasons"));
          safeThis->m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
          safeThis->m_seasonWidget->setGalleryHeight(300);
          safeThis->m_seasonWidget->setItems(seasons);
        }

        if (!seasons.isEmpty() && safeThis->m_episodeWidget) {
          if (safeThis->m_seasonSwitcher) {
            QSignalBlocker blocker(safeThis->m_seasonSwitcher);
            safeThis->m_seasonSwitcher->clear();
            for (const auto &s : seasons)
              safeThis->m_seasonSwitcher->addItem(s.name, "");
            int idx =
                qBound(0, safeThis->m_currentSeasonIndex, seasons.size() - 1);
            safeThis->m_seasonSwitcher->setCurrentIndex(idx);
            safeThis->m_seasonSwitcher->setVisible(seasons.size() > 1);
          }
          auto seasonId = seasons[qBound(0, safeThis->m_currentSeasonIndex,
                                         seasons.size() - 1)]
                              .id;
          QString playableItemId = safeThis->m_currentPlayableItem.id;
          co_await safeThis->m_episodeWidget->loadAsync(
              [core, itemId, seasonId,
               safeThis]() -> QCoro::Task<QList<MediaItem>> {
                auto episodes = co_await core->mediaService()->getEpisodes(
                    itemId, seasonId);
                co_return episodes;
              });

          
          if (safeThis && !playableItemId.isEmpty()) {
            safeThis->m_episodeWidget->gallery()->setHighlightedItemId(
                playableItemId);
            auto *listView = safeThis->m_episodeWidget->gallery()->listView();
            auto *model = listView->model();
            for (int row = 0; row < model->rowCount(); ++row) {
              QModelIndex idx = model->index(row, 0);
              MediaItem ep =
                  idx.data(MediaListModel::ItemDataRole).value<MediaItem>();
              if (ep.id == playableItemId) {
                listView->scrollTo(idx, QAbstractItemView::PositionAtCenter);
                break;
              }
            }
          }
        }
      }
    }
  } catch (...) {
  }
}

QCoro::Task<void> DetailView::executeLoadImages(QPointer<DetailView> safeThis,
                                                QEmbyCore *core,
                                                MediaItem item) {
  bool adaptive =
      ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages, true);

  
  {
    QString imgType = "Primary";
    QString imgTag = item.images.primaryTag;
    QString imgId = item.id;
    if (imgTag.isEmpty() && adaptive) {
      auto best = item.images.bestPoster();
      imgTag = best.first;
      imgType = best.second;
      
      if (item.images.isParentTag(imgTag)) {
        imgId = item.images.parentImageItemId.isEmpty()
                    ? item.seriesId
                    : item.images.parentImageItemId;
      }
    }
    if (!imgTag.isEmpty()) {
      try {
        
        int posterMaxWidth = (imgType == "Primary") ? 400 : 800;
        QPixmap pix = co_await core->mediaService()->fetchImage(
            imgId, imgType, imgTag, posterMaxWidth);
        if (safeThis && safeThis->m_currentItemId == item.id && !pix.isNull()) {
          safeThis->m_currentPosterPix = pix;
          
          const QSize posterSize(250, 375);
          QPixmap scaled =
              pix.scaled(posterSize, Qt::KeepAspectRatioByExpanding,
                         Qt::SmoothTransformation);
          int cropX = (scaled.width() - posterSize.width()) / 2;
          int cropY = (scaled.height() - posterSize.height()) / 2;
          QPixmap cropped = scaled.copy(cropX, cropY, posterSize.width(),
                                        posterSize.height());

          
          QPixmap rounded(posterSize);
          rounded.fill(Qt::transparent);
          QPainter p(&rounded);
          p.setRenderHint(QPainter::Antialiasing);
          QPainterPath path;
          path.addRoundedRect(
              QRectF(0, 0, posterSize.width(), posterSize.height()), 12, 12);
          p.setClipPath(path);
          p.drawPixmap(0, 0, cropped);
          p.end();
          safeThis->m_posterLabel->setPixmap(rounded);
        }
      } catch (...) {
      }
    }
  }

  
  {
    QString imgType = "Backdrop";
    QString imgTag = item.images.backdropTag;
    QString imgId = item.id;
    if (imgTag.isEmpty() && adaptive) {
      auto best = item.images.bestBackdrop();
      imgTag = best.first;
      imgType = best.second;
      
      if (item.images.isParentTag(imgTag)) {
        imgId = item.images.parentImageItemId.isEmpty()
                    ? item.seriesId
                    : item.images.parentImageItemId;
      }
    }
    if (!imgTag.isEmpty()) {
      try {
        QPixmap pix = co_await core->mediaService()->fetchImage(imgId, imgType,
                                                                imgTag, 1920);
        if (safeThis && safeThis->m_currentItemId == item.id && !pix.isNull()) {
          safeThis->m_currentBackdropPix = pix;
          safeThis->updateBackdrop();
        }
      } catch (...) {
      }
    }
  }

  
  if (!item.images.logoTag.isEmpty()) {
    try {
      QPixmap pix = co_await core->mediaService()->fetchImage(
          item.id, "Logo", item.images.logoTag, 400);
      if (safeThis && safeThis->m_currentItemId == item.id && !pix.isNull()) {
        safeThis->m_logoLabel->setPixmap(pix.scaled(
            250, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        safeThis->m_logoLabel->show();
      }
    } catch (...) {
    }
  }
}

void DetailView::onMediaItemUpdated(const MediaItem &item) {
  if (m_currentItemId == item.id) {
    m_currentMediaItem = item;
    m_actionWidget->setFavoriteState(item.isFavorite());

    if (item.type != "Series") {
      m_currentPlayableItem = item;
      m_actionWidget->setupNormalMode(item);
    } else {
      
      auto refreshSeries = [](QPointer<DetailView> safeThis, QEmbyCore *core,
                              QString targetId) -> QCoro::Task<void> {
        co_await safeThis->fetchSeriesNextUp(targetId);
        if (!safeThis)
          co_return;

        auto seasons = co_await core->mediaService()->getSeasons(targetId);
        if (!safeThis)
          co_return;

        safeThis->m_seriesSeasons = seasons;
        if (safeThis->m_seasonWidget) {
          safeThis->m_seasonWidget->setTitle(DetailView::tr("Seasons"));
          safeThis->m_seasonWidget->setCardStyle(MediaCardDelegate::Poster);
          safeThis->m_seasonWidget->setGalleryHeight(300);
          safeThis->m_seasonWidget->setItems(seasons);
        }

        if (!seasons.isEmpty() && safeThis->m_episodeWidget) {
          if (safeThis->m_seasonSwitcher) {
            QSignalBlocker blocker(safeThis->m_seasonSwitcher);
            safeThis->m_seasonSwitcher->clear();
            for (const auto &s : seasons)
              safeThis->m_seasonSwitcher->addItem(s.name, "");
            int idx =
                qBound(0, safeThis->m_currentSeasonIndex, seasons.size() - 1);
            safeThis->m_seasonSwitcher->setCurrentIndex(idx);
            safeThis->m_seasonSwitcher->setVisible(seasons.size() > 1);
          }
          auto seasonId = seasons[qBound(0, safeThis->m_currentSeasonIndex,
                                         seasons.size() - 1)]
                              .id;
          co_await safeThis->m_episodeWidget->loadAsync(
              [core, targetId, seasonId,
               safeThis]() -> QCoro::Task<QList<MediaItem>> {
                auto episodes = co_await core->mediaService()->getEpisodes(
                    targetId, seasonId);
                co_return episodes;
              });
        }
      };
      refreshSeries(QPointer<DetailView>(this), m_core, item.id);
    }
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


QCoro::Task<void> DetailView::switchToSeason(int idx) {
  if (idx < 0 || idx >= m_seriesSeasons.size() || !m_episodeWidget)
    co_return;

  const QString seasonId = m_seriesSeasons[idx].id;
  if (seasonId.isEmpty())
    co_return; 

  QPointer<DetailView> safeThis(this);
  const QString seriesId = m_currentItemId;
  QEmbyCore *core = m_core;

  co_await m_episodeWidget->loadAsync(
      [core, seriesId, seasonId, safeThis]() -> QCoro::Task<QList<MediaItem>> {
        auto episodes =
            co_await core->mediaService()->getEpisodes(seriesId, seasonId);
        co_return episodes;
      });
}
