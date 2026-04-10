#include "playermediaswitcherpanel.h"

#include "horizontallistviewgallery.h"
#include "../utils/mediaitemutils.h"

#include <qembycore.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kMovieGalleryHeight = 162;
constexpr int kSeasonGalleryHeight = 136;
constexpr int kEpisodeGalleryHeight = 110;
constexpr int kSpinnerSize = 28;
constexpr int kSpinnerContainerHeight = 60;

class SpinnerPainter : public QWidget {
public:
    explicit SpinnerPainter(QWidget *parent = nullptr)
        : QWidget(parent) {
        setFixedSize(kSpinnerSize, kSpinnerSize);
        setAttribute(Qt::WA_TranslucentBackground);
    }

    void setAngle(qreal angle) {
        m_angle = angle;
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const qreal penWidth = 3.0;
        const qreal margin = penWidth / 2.0;
        const QRectF arcRect(margin, margin,
                             width() - penWidth, height() - penWidth);

        
        QPen trackPen(QColor(255, 255, 255, 30), penWidth);
        trackPen.setCapStyle(Qt::RoundCap);
        p.setPen(trackPen);
        p.drawArc(arcRect, 0, 360 * 16);

        
        QPen sweepPen(QColor(96, 165, 250, 230), penWidth);
        sweepPen.setCapStyle(Qt::RoundCap);
        p.setPen(sweepPen);
        const int startAngle = static_cast<int>(m_angle * 16);
        p.drawArc(arcRect, startAngle, 90 * 16);
    }

private:
    qreal m_angle = 0.0;
};

} 

PlayerMediaSwitcherPanel::PlayerMediaSwitcherPanel(QEmbyCore *core,
                                                   QWidget *parent)
    : QFrame(parent) {
    setObjectName("playerMediaSwitcherPanel");
    setAttribute(Qt::WA_StyledBackground, true);
    hide();

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(3);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setObjectName("playerMediaSwitcherTitle");
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    m_statusLabel = new QLabel(this);
    m_statusLabel->setObjectName("playerMediaSwitcherStatus");
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    
    m_spinnerContainer = new QWidget(this);
    m_spinnerContainer->setObjectName("playerMediaSwitcherLoading");
    m_spinnerContainer->setFixedHeight(kSpinnerContainerHeight);

    auto *spinnerLayout = new QHBoxLayout(m_spinnerContainer);
    spinnerLayout->setContentsMargins(0, 0, 0, 0);
    spinnerLayout->addStretch();

    auto *spinner = new SpinnerPainter(m_spinnerContainer);
    m_spinnerWidget = spinner;
    spinnerLayout->addWidget(spinner);
    spinnerLayout->addStretch();

    m_spinnerTimer = new QTimer(this);
    m_spinnerTimer->setInterval(16); 
    connect(m_spinnerTimer, &QTimer::timeout, this, [this, spinner]() {
        m_spinnerAngle -= 4.0; 
        if (m_spinnerAngle < -360.0)
            m_spinnerAngle += 360.0;
        spinner->setAngle(m_spinnerAngle);
    });

    layout->addWidget(m_spinnerContainer);

    
    m_movieGallery = new HorizontalListViewGallery(core, this);
    m_movieGallery->setObjectName("playerMediaSwitcherMovieGallery");
    m_movieGallery->setCardStyle(MediaCardDelegate::Poster);
    m_movieGallery->setTileSize(QSize(90, 154));
    m_movieGallery->setTextPixelSizes(10, 9);
    m_movieGallery->setContentPadding(5);
    m_movieGallery->setFixedHeight(kMovieGalleryHeight);
    layout->addWidget(m_movieGallery);

    m_seasonsLabel = new QLabel(tr("Seasons"), this);
    m_seasonsLabel->setObjectName("playerMediaSwitcherSectionTitle");
    layout->addWidget(m_seasonsLabel);

    m_seasonGallery = new HorizontalListViewGallery(core, this);
    m_seasonGallery->setObjectName("playerMediaSwitcherSeasonGallery");
    m_seasonGallery->setCardStyle(MediaCardDelegate::Poster);
    m_seasonGallery->setTileSize(QSize(74, 132));
    m_seasonGallery->setTextPixelSizes(10, 9);
    m_seasonGallery->setContentPadding(4);
    m_seasonGallery->setFixedHeight(kSeasonGalleryHeight);
    layout->addWidget(m_seasonGallery);

    m_episodesLabel = new QLabel(tr("Episodes"), this);
    m_episodesLabel->setObjectName("playerMediaSwitcherSectionTitle");
    layout->addWidget(m_episodesLabel);

    m_episodeGallery = new HorizontalListViewGallery(core, this);
    m_episodeGallery->setObjectName("playerMediaSwitcherEpisodeGallery");
    m_episodeGallery->setCardStyle(MediaCardDelegate::LibraryTile);
    m_episodeGallery->setTileSize(QSize(154, 102));
    m_episodeGallery->setTextPixelSizes(10, 9);
    m_episodeGallery->setContentPadding(6);
    m_episodeGallery->setFixedHeight(kEpisodeGalleryHeight);
    layout->addWidget(m_episodeGallery);

    connect(m_movieGallery, &HorizontalListViewGallery::itemClicked, this,
            &PlayerMediaSwitcherPanel::onPlayableItemClicked);
    connect(m_movieGallery, &HorizontalListViewGallery::playRequested, this,
            &PlayerMediaSwitcherPanel::onPlayableItemClicked);

    connect(m_seasonGallery, &HorizontalListViewGallery::itemClicked, this,
            &PlayerMediaSwitcherPanel::onSeasonItemClicked);
    connect(m_seasonGallery, &HorizontalListViewGallery::playRequested, this,
            &PlayerMediaSwitcherPanel::onSeasonItemClicked);

    connect(m_episodeGallery, &HorizontalListViewGallery::itemClicked, this,
            &PlayerMediaSwitcherPanel::onPlayableItemClicked);
    connect(m_episodeGallery, &HorizontalListViewGallery::playRequested, this,
            &PlayerMediaSwitcherPanel::onPlayableItemClicked);

    setStatusMessage(QString());
    applySeriesMode();
}

int PlayerMediaSwitcherPanel::preferredDrawerHeight() const {
    auto *rootLayout = qobject_cast<QVBoxLayout *>(layout());
    if (!rootLayout) {
        return sizeHint().height();
    }

    const QMargins margins = rootLayout->contentsMargins();
    int height = margins.top() + margins.bottom();
    bool hasVisibleWidget = false;

    const auto accumulateHeight = [&](const QWidget *widget) {
        const int widgetHeight = visibleHeightForWidget(widget);
        if (widgetHeight <= 0) {
            return;
        }
        if (hasVisibleWidget) {
            height += rootLayout->spacing();
        }
        height += widgetHeight;
        hasVisibleWidget = true;
    };

    accumulateHeight(m_titleLabel);
    accumulateHeight(m_statusLabel);
    accumulateHeight(m_spinnerContainer);
    accumulateHeight(m_movieGallery);
    accumulateHeight(m_seasonsLabel);
    accumulateHeight(m_seasonGallery);
    accumulateHeight(m_episodesLabel);
    accumulateHeight(m_episodeGallery);

    return height;
}

void PlayerMediaSwitcherPanel::setLoadingState(const QString &title,
                                               const QString &message,
                                               bool seriesMode) {
    m_titleLabel->setText(title);
    m_isLoading = true;
    m_loadingSeriesMode = seriesMode;
    m_isSeriesMode = seriesMode;
    m_currentMediaId.clear();
    m_currentSeriesName.clear();
    m_selectedSeasonId.clear();
    m_movieItems.clear();
    m_seasons.clear();
    m_episodesBySeasonId.clear();

    m_movieGallery->setItems(QList<MediaItem>{});
    m_seasonGallery->setItems(QList<MediaItem>{});
    m_episodeGallery->setItems(QList<MediaItem>{});

    setStatusMessage(message);
    applySeriesMode();
}

void PlayerMediaSwitcherPanel::setMovieItems(const QList<MediaItem> &items,
                                             const QString &currentMediaId) {
    m_isLoading = false;
    m_loadingSeriesMode = false;
    m_isSeriesMode = false;
    m_currentMediaId = currentMediaId;
    m_currentSeriesName.clear();
    m_selectedSeasonId.clear();
    m_movieItems = items;
    m_seasons.clear();
    m_episodesBySeasonId.clear();

    m_titleLabel->setText(tr("Continue Watching"));
    populateMovieGallery();
    applySeriesMode();
}

void PlayerMediaSwitcherPanel::setSeriesData(
    const QString &seriesName, const QList<MediaItem> &seasons,
    const QHash<QString, QList<MediaItem>> &episodesBySeasonId,
    const QString &currentMediaId) {
    m_isLoading = false;
    m_loadingSeriesMode = true;
    m_isSeriesMode = true;
    m_currentSeriesName = seriesName;
    m_currentMediaId = currentMediaId;
    m_movieItems.clear();
    m_seasons = seasons;
    m_episodesBySeasonId = episodesBySeasonId;

    m_titleLabel->setText(seriesName.isEmpty() ? tr("Episodes") : seriesName);

    m_selectedSeasonId.clear();
    for (const MediaItem &season : m_seasons) {
        const auto episodes = m_episodesBySeasonId.value(season.id);
        for (const MediaItem &episode : episodes) {
            if (episode.id == m_currentMediaId) {
                m_selectedSeasonId = season.id;
                break;
            }
        }
        if (!m_selectedSeasonId.isEmpty()) {
            break;
        }
    }
    if (m_selectedSeasonId.isEmpty() && !m_seasons.isEmpty()) {
        m_selectedSeasonId = m_seasons.first().id;
    }

    populateSeasonGallery();
    populateEpisodeGallery();
    applySeriesMode();
}

void PlayerMediaSwitcherPanel::onSeasonItemClicked(const MediaItem &season) {
    if (season.id.isEmpty() || season.id == m_selectedSeasonId) {
        return;
    }

    m_selectedSeasonId = season.id;
    populateSeasonGallery();
    populateEpisodeGallery();
    applySeriesMode();
}

void PlayerMediaSwitcherPanel::onPlayableItemClicked(const MediaItem &item) {
    if (item.id.isEmpty()) {
        return;
    }

    Q_EMIT playRequested(item.id, formatResumeDisplayTitle(item),
                         item.userData.playbackPositionTicks);
}

void PlayerMediaSwitcherPanel::applySeriesMode() {
    const bool hasStatus = !m_statusLabel->text().trimmed().isEmpty();
    const bool showSeriesSections = !m_isLoading && m_isSeriesMode;
    const bool hasSeasons = !m_seasons.isEmpty();
    const bool hasEpisodeItems =
        !m_episodesBySeasonId.value(currentSelectedSeasonId()).isEmpty();

    m_statusLabel->setVisible(hasStatus);

    
    m_spinnerContainer->setVisible(m_isLoading);
    if (m_isLoading) {
        m_spinnerAngle = 0.0;
        m_spinnerTimer->start();
    } else {
        m_spinnerTimer->stop();
    }

    m_movieGallery->setVisible(!m_isLoading && !m_isSeriesMode && !hasStatus);
    m_seasonsLabel->setVisible(showSeriesSections && hasSeasons);
    m_seasonGallery->setVisible(showSeriesSections && hasSeasons);
    m_episodesLabel->setVisible(showSeriesSections && hasEpisodeItems);
    m_episodeGallery->setVisible(showSeriesSections && hasEpisodeItems);

    setFixedHeight(preferredDrawerHeight());
}

void PlayerMediaSwitcherPanel::populateMovieGallery() {
    setStatusMessage(QString());
    m_movieGallery->setItems(m_movieItems);
    m_movieGallery->setHighlightedItemId(m_currentMediaId);

    if (m_movieItems.isEmpty()) {
        setStatusMessage(tr("No active media found."));
    }
}

void PlayerMediaSwitcherPanel::populateSeasonGallery() {
    setStatusMessage(QString());
    m_seasonGallery->setItems(m_seasons);
    m_seasonGallery->setHighlightedItemId(currentSelectedSeasonId());
}

void PlayerMediaSwitcherPanel::populateEpisodeGallery() {
    setStatusMessage(QString());
    const QList<MediaItem> episodes =
        m_episodesBySeasonId.value(currentSelectedSeasonId());
    m_episodeGallery->setItems(episodes);
    m_episodeGallery->setHighlightedItemId(m_currentMediaId);

    if (episodes.isEmpty()) {
        setStatusMessage(tr("No episodes found."));
    }
}

void PlayerMediaSwitcherPanel::setStatusMessage(const QString &message) {
    m_statusLabel->setText(message);
}

QString PlayerMediaSwitcherPanel::formatEpisodeDisplayTitle(
    const MediaItem &episode) const {
    return MediaItemUtils::playbackTitle(episode, m_currentSeriesName);
}

QString PlayerMediaSwitcherPanel::formatResumeDisplayTitle(
    const MediaItem &item) const {
    if (item.type == QStringLiteral("Episode")) {
        return formatEpisodeDisplayTitle(item);
    }

    return item.name;
}

QString PlayerMediaSwitcherPanel::currentSelectedSeasonId() const {
    if (!m_selectedSeasonId.isEmpty()) {
        return m_selectedSeasonId;
    }
    return m_seasons.isEmpty() ? QString() : m_seasons.first().id;
}

int PlayerMediaSwitcherPanel::visibleHeightForWidget(const QWidget *widget) const
{
    if (!widget || !widget->isVisible()) {
        return 0;
    }

    if (widget->minimumHeight() == widget->maximumHeight() &&
        widget->minimumHeight() > 0) {
        return widget->minimumHeight();
    }

    if (widget->height() > 0) {
        return widget->height();
    }

    return widget->sizeHint().height();
}
