#ifndef DETAILVIEW_H
#define DETAILVIEW_H

#include "../baseview.h"
#include "models/media/mediaitem.h"
#include <qcorotask.h>

class QScrollArea;
class QVBoxLayout;
class QGridLayout;
class QLabel;
class QPushButton;
class QLineEdit;
class QIntValidator;
class FlowLayout;
class ModernMenuButton;
class SmoothScrollController;

class DetailActionWidget;
class DetailBottomInfoWidget;
class MediaSectionWidget; 
class QGraphicsDropShadowEffect;

class DetailView : public BaseView {
  Q_OBJECT

public:
  explicit DetailView(QEmbyCore *core, QWidget *parent = nullptr);
  
  
  
  
  void loadItem(QString itemId, MediaItem seedItem = {});

public Q_SLOTS:
  void onMediaItemUpdated(const MediaItem &item) override;

  void beginOptimisticPlayedUpdate() override;
  void endOptimisticPlayedUpdate() override;

protected:
  void resizeEvent(QResizeEvent *event) override;
  bool eventFilter(QObject *obj, QEvent *event) override;
  void showEvent(QShowEvent *event) override;

private slots:
  void scrollToTop() override;
  QCoro::Task<void> onVersionChanged(int index);
  void onOverviewMoreClicked(const QString &link);

private:
  void setupUi();
  void updateBackdrop();
  void updateOverviewElidedText();
  void updateTagLayoutHeight();
  void updateMetaRow(const MediaItem &item,
                     const QString &leadingMeta = QString());
  QString formatRunTime(long long ticks);
  void clearLayout(QLayout *layout);
  bool shouldShowDisplayNumber(const MediaItem &item) const;
  bool isCurrentActionItem(const QString &itemId) const;
  QStringList buildNumberCandidates(const MediaItem &item) const;
  QString extractDisplayNumber(const MediaItem &item) const;
  void updateDisplayNumber(const QString &number);
  void copyDisplayedNumber();
  void markManualSeriesSelection(const QString &reason);
  void rememberSeriesSelection(const MediaItem &episode,
                               const QString &reason = QString(),
                               bool persist = false);
  void applyRememberedStreams(const MediaItem &item,
                              const MediaSourceInfo &source);
  void applySeriesPlayableItemToUi(const MediaItem &playableItem,
                                   bool scrollToEpisode = false);
  QCoro::Task<void> applySeriesPlayableItem(MediaItem playableItem,
                                            bool scrollToEpisode = false);

  QCoro::Task<void> updateUi(MediaItem item, bool isSilentRefresh = false);

  
  
  QCoro::Task<void> fetchAndApplyPlaybackInfo(QString itemId);

  
  
  
  void applySeedToUi(const MediaItem &seed);

  
  
  void buildTagButtons(const QStringList &genres);
  void applyCastSection(const MediaItem &item);
  void applyCollectionGalleryLayout(const QList<MediaItem> &collections);
  QCoro::Task<void> executePlay(MediaItem targetItem, long long startTicks);
  QCoro::Task<void> executePlaySeason(MediaItem seasonItem);
  QCoro::Task<void> executeExternalPlay(MediaItem targetItem, QString playerPath);
  QCoro::Task<void> fetchSeriesNextUp(QString targetId,
                                      bool applyToUi = true);
  void updateSeasonSwitcher(int currentIndex);
  void updateEpisodeHeaderControlsVisibility();
  void updateEpisodeJumpControl(int episodeCount);
  int episodeGalleryVisibleCardCapacity() const;
  MediaItem episodeForJumpNumber(int episodeNumber) const;
  void submitEpisodeJump();
  QCoro::Task<void> loadEpisodesForSeason(int idx,
                                          QString highlightEpisodeId = QString(),
                                          bool scrollToHighlight = false,
                                          bool manualSelection = false);

  
  static QCoro::Task<void>
  executeFetchSecondaries(QPointer<DetailView> safeThis, QEmbyCore *core,
                          QString targetId, QString itemType);
  static QCoro::Task<void> executeSilentRefresh(QPointer<DetailView> safeThis,
                                                QEmbyCore *core,
                                                QString itemId);
  static QCoro::Task<void> executeLoadImages(QPointer<DetailView> safeThis,
                                             QEmbyCore *core, MediaItem item,
                                             bool allowPrimaryBackdropFallback);

  
  
  QCoro::Task<void> loadDetailCacheAndStartPrefetch(QString itemId,
                                                    MediaItem seedItem,
                                                    QString cacheServerId,
                                                    QString cacheUserId);
  
  QCoro::Task<void> prefetchItemDetail(QString itemId, QString cacheServerId,
                                       QString cacheUserId,
                                       QString cachedFingerprint);
  
  
  
  void maybeFlushDeferredUpdate(const QString &itemId);
  
  QCoro::Task<void> executeDeferredUpdate(MediaItem item,
                                          bool isSilentRefresh = false);
  void setDeferredPresentationBatchActive(bool active);
  void finishDeferredPresentationBatch(const QString &targetId,
                                       const QString &detectedType);
  void startSecondaryFetches(const QString &targetId,
                             const QString &detectedType);
  void applyCachedSecondaryData(const QString &targetId,
                                const QString &detectedType,
                                bool revealBottomInfo = true);
  void persistDetailCacheSnapshot(const QString &reason = QString());

  QCoro::Task<void> switchToSeason(int idx,
                                   bool manualSelection = false);

  
  QString m_currentItemId;
  MediaItem m_currentMediaItem;
  MediaItem m_currentPlayableItem;
  bool m_isFavorite = false;
  QString m_cleanOverviewText;
  int m_lastOverviewWidth = -1;
  int m_overviewElideGeneration = 0;

  
  
  
  bool m_skipNextSilentRefresh = false;
  
  bool m_skipSilentRefresh = false;
  
  
  QString m_pendingDeferredFetchId;
  
  MediaItem m_pendingFetchedItem;
  bool m_pendingFetchReady = false;
  bool m_pendingAnimationGuardDone = false;
  bool m_pendingFetchedItemFromCache = false;
  bool m_deferredPresentationBatchActive = false;
  bool m_deferBottomInfoReveal = false;
  QString m_detailCacheServerId;
  QString m_detailCacheUserId;
  QString m_cachedDetailFingerprint;
  QString m_cachedSectionsFingerprint;
  MediaItem m_cachedSeriesPlayableItem;
  QList<MediaItem> m_cachedSeriesSeasons;
  QList<MediaItem> m_cachedSeasonEpisodes;
  QList<MediaItem> m_cachedSimilarItems;
  QList<MediaItem> m_cachedCollections;
  QList<MediaItem> m_cachedAdditionalParts;
  QString m_cachedSeasonId;
  QString m_manualSelectedSeasonId;
  QString m_manualSelectedEpisodeId;
  int m_cachedSeasonIndex = -1;
  int m_manualSelectedSeasonIndex = -1;
  bool m_hasManualSeriesSelection = false;
  bool m_hasCachedSeriesPlayableItem = false;
  bool m_hasCachedSeriesSeasons = false;
  bool m_hasCachedSeasonEpisodes = false;
  bool m_hasCachedSimilarItems = false;
  bool m_hasCachedCollections = false;
  bool m_hasCachedAdditionalParts = false;
  bool m_hasCachedCastData = false;
  bool m_appliedCachedCastToUi = false;
  bool m_appliedCachedSeriesSeasonsToUi = false;
  bool m_appliedCachedSeasonEpisodesToUi = false;
  bool m_appliedCachedSimilarItemsToUi = false;
  bool m_appliedCachedCollectionsToUi = false;
  bool m_appliedCachedAdditionalPartsToUi = false;
  QString m_appliedCastFingerprint;
  QString m_appliedSeriesPlayableFingerprint;

  
  
  
  QString m_appliedSourcesFingerprint;

  
  
  
  QString m_appliedBottomInfoFingerprint;

  
  QScrollArea *m_mainScrollArea;
  QWidget *m_contentWidget;
  QGridLayout *m_infoLayout;

  QLabel *m_logoLabel;
  QLabel *m_posterLabel;
  QGraphicsDropShadowEffect *m_posterShadow = nullptr; 
  QPixmap m_currentPosterPix;
  QPixmap m_currentBackdropPix;

  QWidget *m_textContainer;
  QLabel *m_titleLabel;
  QWidget *m_metaRowWidget;
  QLabel *m_ratingStarLabel;
  QLabel *m_metaLabel;
  QPushButton *m_numberButton;
  QLabel *m_taglineLabel;
  QLabel *m_overviewLabel;

  QWidget *m_tagsWidget;
  FlowLayout *m_tagsLayout;

  DetailActionWidget *m_actionWidget;
  DetailBottomInfoWidget *m_bottomInfoWidget;

  
  QWidget *m_seasonSectionReserveWidget = nullptr;
  MediaSectionWidget *m_seasonWidget;  
  QWidget *m_episodeSectionReserveWidget = nullptr;
  MediaSectionWidget *m_episodeWidget; 
  QWidget *m_castSectionReserveWidget = nullptr;
  MediaSectionWidget *m_castWidget;
  QWidget *m_additionalPartsSectionReserveWidget = nullptr;
  MediaSectionWidget *m_additionalPartsWidget;
  QWidget *m_collectionSectionReserveWidget = nullptr;
  MediaSectionWidget *m_collectionWidget;
  QWidget *m_similarSectionReserveWidget = nullptr;
  MediaSectionWidget *m_similarWidget;
  QWidget *m_bottomInfoReserveWidget = nullptr;

  
  QWidget *m_episodeHeaderControls = nullptr;
  ModernMenuButton *m_seasonSwitcher =
      nullptr; 
  QLineEdit *m_episodeJumpEdit = nullptr;
  QIntValidator *m_episodeJumpValidator = nullptr;
  int m_episodeTileWidth = 0;
  QList<MediaItem> m_seriesSeasons; 
  QList<MediaItem> m_currentSeasonEpisodes; 
  int m_currentSeasonIndex = 0;     

  SmoothScrollController *m_vScrollController = nullptr;
};

#endif 
