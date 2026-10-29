#ifndef PLAYERMEDIASWITCHERPANEL_H
#define PLAYERMEDIASWITCHERPANEL_H

#include <QFrame>
#include <QHash>
#include <QList>

#include <models/media/mediaitem.h>

class QLabel;
class QPushButton;
class QTimer;
class HorizontalListViewGallery;
class QEmbyCore;
class QWidget;

class PlayerMediaSwitcherPanel : public QFrame {
    Q_OBJECT
public:
    explicit PlayerMediaSwitcherPanel(QEmbyCore *core, QWidget *parent = nullptr);
    ~PlayerMediaSwitcherPanel() override = default;

    int preferredDrawerHeight(int availableWidth = -1) const;
    void setLoadingState(const QString &title, const QString &message,
                         bool seriesMode);
    void setErrorState(const QString &message);
    void setMovieItems(const QList<MediaItem> &items, const QString &currentMediaId);
    void setVideoParts(const QList<MediaItem> &parts, const QString &currentMediaId);
    void setSeriesData(const QString &seriesName,
                       const QList<MediaItem> &seasons,
                       const QHash<QString, QList<MediaItem>> &episodesBySeasonId,
                       const QString &currentMediaId);

Q_SIGNALS:
    void retryRequested();
    void preferredHeightChanged();
    void playRequested(QString mediaId, QString displayTitle,
                       long long startPositionTicks);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private Q_SLOTS:
    void onSeasonItemClicked(const MediaItem &season);
    void onPlayableItemClicked(const MediaItem &item);

private:
    enum class DisplayState { Content, Loading, Error };
    void applySeriesMode();
    void updatePreferredHeight();
    void populateMovieGallery();
    void populateSeasonGallery();
    void populateEpisodeGallery();
    void setStatusMessage(const QString &message);
    QString formatEpisodeDisplayTitle(const MediaItem &episode) const;
    QString formatResumeDisplayTitle(const MediaItem &item) const;
    QString currentSelectedSeasonId() const;
    int visibleHeightForWidget(const QWidget *widget, int availableWidth) const;

    QLabel *m_titleLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QPushButton *m_retryButton = nullptr;
    QLabel *m_partsLabel = nullptr;
    QLabel *m_moviesLabel = nullptr;
    QLabel *m_seasonsLabel = nullptr;
    QLabel *m_episodesLabel = nullptr;
    QWidget *m_spinnerContainer = nullptr;
    QWidget *m_spinnerWidget = nullptr;
    QTimer *m_spinnerTimer = nullptr;
    qreal m_spinnerAngle = 0.0;
    HorizontalListViewGallery *m_movieGallery = nullptr;
    HorizontalListViewGallery *m_partsGallery = nullptr;
    HorizontalListViewGallery *m_seasonGallery = nullptr;
    HorizontalListViewGallery *m_episodeGallery = nullptr;

    QList<MediaItem> m_movieItems;
    QList<MediaItem> m_parts;
    QList<MediaItem> m_seasons;
    QHash<QString, QList<MediaItem>> m_episodesBySeasonId;
    QString m_currentMediaId;
    QString m_currentSeriesName;
    QString m_selectedSeasonId;
    bool m_uiReady = false;
    DisplayState m_displayState = DisplayState::Content;
    bool m_isSeriesMode = false;
};

#endif 
