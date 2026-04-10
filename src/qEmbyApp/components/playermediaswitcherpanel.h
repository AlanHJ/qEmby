#ifndef PLAYERMEDIASWITCHERPANEL_H
#define PLAYERMEDIASWITCHERPANEL_H

#include <QFrame>
#include <QHash>
#include <QList>

#include <models/media/mediaitem.h>

class QLabel;
class QTimer;
class HorizontalListViewGallery;
class QEmbyCore;
class QWidget;

class PlayerMediaSwitcherPanel : public QFrame {
    Q_OBJECT
public:
    explicit PlayerMediaSwitcherPanel(QEmbyCore *core, QWidget *parent = nullptr);
    ~PlayerMediaSwitcherPanel() override = default;

    int preferredDrawerHeight() const;
    void setLoadingState(const QString &title, const QString &message,
                         bool seriesMode);
    void setMovieItems(const QList<MediaItem> &items, const QString &currentMediaId);
    void setSeriesData(const QString &seriesName,
                       const QList<MediaItem> &seasons,
                       const QHash<QString, QList<MediaItem>> &episodesBySeasonId,
                       const QString &currentMediaId);

Q_SIGNALS:
    void playRequested(QString mediaId, QString displayTitle,
                       long long startPositionTicks);

private Q_SLOTS:
    void onSeasonItemClicked(const MediaItem &season);
    void onPlayableItemClicked(const MediaItem &item);

private:
    void applySeriesMode();
    void populateMovieGallery();
    void populateSeasonGallery();
    void populateEpisodeGallery();
    void setStatusMessage(const QString &message);
    QString formatEpisodeDisplayTitle(const MediaItem &episode) const;
    QString formatResumeDisplayTitle(const MediaItem &item) const;
    QString currentSelectedSeasonId() const;
    int visibleHeightForWidget(const QWidget *widget) const;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_seasonsLabel = nullptr;
    QLabel *m_episodesLabel = nullptr;
    QWidget *m_spinnerContainer = nullptr;
    QWidget *m_spinnerWidget = nullptr;
    QTimer *m_spinnerTimer = nullptr;
    qreal m_spinnerAngle = 0.0;
    HorizontalListViewGallery *m_movieGallery = nullptr;
    HorizontalListViewGallery *m_seasonGallery = nullptr;
    HorizontalListViewGallery *m_episodeGallery = nullptr;

    QList<MediaItem> m_movieItems;
    QList<MediaItem> m_seasons;
    QHash<QString, QList<MediaItem>> m_episodesBySeasonId;
    QString m_currentMediaId;
    QString m_currentSeriesName;
    QString m_selectedSeasonId;
    bool m_isLoading = false;
    bool m_loadingSeriesMode = false;
    bool m_isSeriesMode = false;
};

#endif 
