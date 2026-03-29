#ifndef DETAILVIEW_H
#define DETAILVIEW_H

#include "../baseview.h"
#include "models/media/mediaitem.h"
#include <qcorotask.h>

class QScrollArea;
class QVBoxLayout;
class QGridLayout;
class QLabel;
class QPropertyAnimation;
class QPushButton;
class FlowLayout;
class ModernMenuButton;

class DetailActionWidget;
class DetailBottomInfoWidget;
class MediaSectionWidget; 

class DetailView : public BaseView {
  Q_OBJECT

public:
  explicit DetailView(QEmbyCore *core, QWidget *parent = nullptr);
  QCoro::Task<void> loadItem(const QString &itemId);

public Q_SLOTS:
  void onMediaItemUpdated(const MediaItem &item) override;

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
  QString formatRunTime(long long ticks);
  void clearLayout(QLayout *layout);
  bool shouldShowDisplayNumber(const MediaItem &item) const;
  QStringList buildNumberCandidates(const MediaItem &item) const;
  QString extractDisplayNumber(const MediaItem &item) const;
  void updateDisplayNumber(const QString &number);
  void copyDisplayedNumber();

  QCoro::Task<void> updateUi(MediaItem item, bool isSilentRefresh = false);
  QCoro::Task<void> executePlay(MediaItem targetItem, long long startTicks);
  QCoro::Task<void> executeExternalPlay(MediaItem targetItem, QString playerPath);
  QCoro::Task<void> fetchSeriesNextUp(const QString &targetId);

  
  static QCoro::Task<void>
  executeFetchSecondaries(QPointer<DetailView> safeThis, QEmbyCore *core,
                          QString targetId, QString itemType);
  static QCoro::Task<void> executeSilentRefresh(QPointer<DetailView> safeThis,
                                                QEmbyCore *core,
                                                QString itemId);
  static QCoro::Task<void> executeLoadImages(QPointer<DetailView> safeThis,
                                             QEmbyCore *core, MediaItem item);

  QCoro::Task<void> switchToSeason(int idx);

  
  QString m_currentItemId;
  MediaItem m_currentMediaItem;
  MediaItem m_currentPlayableItem;
  bool m_isFavorite = false;
  QString m_cleanOverviewText;
  int m_lastOverviewWidth = -1;

  
  QScrollArea *m_mainScrollArea;
  QWidget *m_contentWidget;
  QGridLayout *m_infoLayout;

  QLabel *m_logoLabel;
  QLabel *m_posterLabel;
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

  
  MediaSectionWidget *m_seasonWidget;  
  MediaSectionWidget *m_episodeWidget; 
  MediaSectionWidget *m_castWidget;
  MediaSectionWidget *m_additionalPartsWidget;
  MediaSectionWidget *m_collectionWidget;
  MediaSectionWidget *m_similarWidget;

  
  ModernMenuButton *m_seasonSwitcher =
      nullptr; 
  QList<MediaItem> m_seriesSeasons; 
  int m_currentSeasonIndex = 0;     

  
  QPropertyAnimation *m_vScrollAnim;
  int m_vScrollTarget = 0;
};

#endif 
