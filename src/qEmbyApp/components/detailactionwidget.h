#ifndef DETAILACTIONWIDGET_H
#define DETAILACTIONWIDGET_H

#include <QWidget>
#include "models/media/mediaitem.h"

class QPushButton;
class QProgressBar;
class QLabel;
class ModernMenuButton;
class SplitPlayerButton;

class DetailActionWidget : public QWidget {
    Q_OBJECT
public:
    explicit DetailActionWidget(QWidget* parent = nullptr);

    void setupNormalMode(const MediaItem& item);
    void setupSeriesMode(const MediaItem& nextUpItem, const QString& epTag);
    void setSeriesLoadingMode();

    void setFavoriteState(bool isFavorite);
    void setSources(const QList<MediaSourceInfo>& sources, int currentIndex = 0);
    void setStreams(const MediaSourceInfo& source);
    void clear();

    int currentSourceIndex() const;
    int currentAudioIndex() const;
    int currentSubtitleIndex() const;

    
    void refreshExtPlayerButton();

signals:
    void playRequested();
    void resumeRequested();
    void favoriteRequested();
    void sourceVersionChanged(int index);
    void externalPlayRequested(const QString &playerPath);

private:
    QString formatRunTime(long long ticks);

    QPushButton* m_resumeBtn;
    QPushButton* m_playBtn;
    QPushButton* m_favBtn;

    QWidget* m_progressWidget;
    QProgressBar* m_progressBar;
    QLabel* m_remainingTimeLabel;

    ModernMenuButton* m_versionComboBox;
    ModernMenuButton* m_audioComboBox;
    ModernMenuButton* m_subtitleComboBox;

    
    SplitPlayerButton* m_extPlayerBtn;
};

#endif 
