#ifndef SEASONVIEW_H
#define SEASONVIEW_H

#include "../baseview.h"
#include <QPointer>
#include <qcorotask.h>

class MediaGridWidget;
class ModernSortButton;
class QPushButton;
class QLabel;
class DetailContentWidget;
class SplitPlayerButton;

class SeasonView : public BaseView {
  Q_OBJECT
public:
  explicit SeasonView(QEmbyCore *core, QWidget *parent = nullptr);

  QCoro::Task<void> loadSeason(QString seriesId, QString seasonId,
                               QString seasonName);

protected:
  void onMediaItemUpdated(const MediaItem &item) override;
  void showEvent(QShowEvent *event) override;

private slots:
  QCoro::Task<void> onFilterChanged(bool preserveScroll = false);

private:
  void setupUi();
  void updateFavBtnState();
  void refreshExtPlayerButton();
  QCoro::Task<void> loadImages(const QString &fallbackSeriesBackdropTag);
  QCoro::Task<void> updatePlayButtonFromNextUp();
  QCoro::Task<void> executeExternalPlay(MediaItem targetItem, QString playerPath);
  QCoro::Task<void> executeInternalPlay(MediaItem targetItem);

  QString m_currentSeriesId;
  QString m_currentSeasonId;
  MediaItem m_currentSeasonItem;
  QList<MediaItem> m_currentEpisodes; 
  QString m_nextUpEpisodeId;          
  bool m_isFavorite = false;

  DetailContentWidget *m_contentWidget = nullptr;

  
  QLabel *m_posterLabel = nullptr;      
  QLabel *m_seriesTitleLabel = nullptr; 
  QLabel *m_titleLabel = nullptr;       
  QLabel *m_overviewLabel = nullptr;    

  QPushButton *m_playBtn = nullptr;
  QPushButton *m_favBtn = nullptr;
  ModernSortButton *m_sortButton = nullptr;
  QLabel *m_statsLabel = nullptr;
  MediaGridWidget *m_mediaGrid = nullptr;
  SplitPlayerButton *m_extPlayerBtn = nullptr;
};

#endif 
