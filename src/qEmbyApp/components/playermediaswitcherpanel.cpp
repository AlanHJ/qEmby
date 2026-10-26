#include "playermediaswitcherpanel.h"

#include "horizontallistviewgallery.h"
#include "../utils/mediaitemutils.h"
#include "../utils/mediapartutils.h"

#include <qembycore.h>

#include <QDebug>
#include <QHBoxLayout>
#include <QHideEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>

namespace {

constexpr int kMovieGalleryHeight = 162;
constexpr int kSeasonGalleryHeight = 136;


constexpr int kLandscapeCardWidth = 154;
constexpr int kLandscapeCardHeight = 116;
constexpr int kLandscapeGalleryHeight = kLandscapeCardHeight + 4;
constexpr int kSpinnerSize = 28;
constexpr int kSpinnerContainerHeight = 60;
constexpr int kStatusPanelMinimumHeight = 120;

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

    m_retryButton = new QPushButton(tr("Retry"), this);
    m_retryButton->setObjectName("playerMediaSwitcherRetry");
    m_retryButton->setCursor(Qt::PointingHandCursor);
    layout->addWidget(m_retryButton, 0, Qt::AlignHCenter);
    connect(m_retryButton, &QPushButton::clicked, this,
            &PlayerMediaSwitcherPanel::retryRequested);

    m_partsLabel = new QLabel(tr("Chapters"), this);
    m_partsLabel->setObjectName("playerMediaSwitcherSectionTitle");
    layout->addWidget(m_partsLabel);

    m_partsGallery = new HorizontalListViewGallery(core, this);
    m_partsGallery->setObjectName("playerMediaSwitcherEpisodeGallery");
    m_partsGallery->setCardStyle(MediaCardDelegate::LibraryTile);
    m_partsGallery->setTileSize(QSize(kLandscapeCardWidth, kLandscapeCardHeight));
    m_partsGallery->setTextPixelSizes(10, 9);
    m_partsGallery->setContentPadding(6);
    m_partsGallery->setHoverControls(MediaCardDelegate::HoverControlPlay);
    m_partsGallery->setFixedHeight(kLandscapeGalleryHeight);
    layout->addWidget(m_partsGallery);
    connect(m_partsGallery, &HorizontalListViewGallery::itemClicked, this,
            &PlayerMediaSwitcherPanel::onPlayableItemClicked);
    connect(m_partsGallery, &HorizontalListViewGallery::playRequested, this,
            &PlayerMediaSwitcherPanel::onPlayableItemClicked);

    
    m_moviesLabel = new QLabel(tr("Continue Watching"), this);
    m_moviesLabel->setObjectName("playerMediaSwitcherSectionTitle");
    layout->addWidget(m_moviesLabel);
    m_movieGallery = new HorizontalListViewGallery(core, this);
    m_movieGallery->setObjectName("playerMediaSwitcherMovieGallery");
    m_movieGallery->setCardStyle(MediaCardDelegate::Poster);
    m_movieGallery->setTileSize(QSize(90, 154));
    m_movieGallery->setTextPixelSizes(10, 9);
    m_movieGallery->setContentPadding(5);
    m_movieGallery->setHoverControls(MediaCardDelegate::HoverControlPlay);
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
    m_seasonGallery->setHoverControls(
        MediaCardDelegate::HoverControls());
    m_seasonGallery->setFixedHeight(kSeasonGalleryHeight);
    layout->addWidget(m_seasonGallery);

    m_episodesLabel = new QLabel(tr("Episodes"), this);
    m_episodesLabel->setObjectName("playerMediaSwitcherSectionTitle");
    layout->addWidget(m_episodesLabel);

    m_episodeGallery = new HorizontalListViewGallery(core, this);
    m_episodeGallery->setObjectName("playerMediaSwitcherEpisodeGallery");
    m_episodeGallery->setCardStyle(MediaCardDelegate::LibraryTile);
    m_episodeGallery->setTileSize(QSize(kLandscapeCardWidth, kLandscapeCardHeight));
    m_episodeGallery->setTextPixelSizes(10, 9);
    m_episodeGallery->setContentPadding(6);
    m_episodeGallery->setHoverControls(MediaCardDelegate::HoverControlPlay);
    m_episodeGallery->setFixedHeight(kLandscapeGalleryHeight);
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

    
    
    m_uiReady = true;
    hide();
    setStatusMessage(QString());
    applySeriesMode();
}

int PlayerMediaSwitcherPanel::preferredDrawerHeight(int availableWidth) const {
    auto *rootLayout = qobject_cast<QVBoxLayout *>(layout());
    if (!rootLayout) {
        return sizeHint().height();
    }

    const QMargins margins = rootLayout->contentsMargins();
    const QMargins frameMargins = contentsMargins();
    const int contentWidth = qMax(1, (availableWidth > 0 ? availableWidth : width()) -
                                       margins.left() - margins.right() -
                                       frameMargins.left() - frameMargins.right());
    int height = margins.top() + margins.bottom() + frameMargins.top() + frameMargins.bottom();
    bool hasVisibleWidget = false;

    const auto accumulateHeight = [&](const QWidget *widget) {
        const int widgetHeight = visibleHeightForWidget(widget, contentWidth);
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
    accumulateHeight(m_retryButton);
    accumulateHeight(m_partsLabel);
    accumulateHeight(m_partsGallery);
    accumulateHeight(m_moviesLabel);
    accumulateHeight(m_movieGallery);
    accumulateHeight(m_seasonsLabel);
    accumulateHeight(m_seasonGallery);
    accumulateHeight(m_episodesLabel);
    accumulateHeight(m_episodeGallery);

    return m_displayState == DisplayState::Content ? height : qMax(height, kStatusPanelMinimumHeight);
}

void PlayerMediaSwitcherPanel::setLoadingState(const QString &title,
                                               const QString &message,
                                               bool seriesMode) {
    m_titleLabel->setText(title);
    m_displayState = DisplayState::Loading;
    m_isSeriesMode = seriesMode;
    m_currentMediaId.clear();
    m_currentSeriesName.clear();
    m_selectedSeasonId.clear();
    m_movieItems.clear();
    m_parts.clear();
    m_seasons.clear();
    m_episodesBySeasonId.clear();

    m_movieGallery->setItems(QList<MediaItem>{});
    m_partsGallery->setItems(QList<MediaItem>{});
    m_seasonGallery->setItems(QList<MediaItem>{});
    m_episodeGallery->setItems(QList<MediaItem>{});

    setStatusMessage(message);
    applySeriesMode();
}

void PlayerMediaSwitcherPanel::setMovieItems(const QList<MediaItem> &items,
                                             const QString &currentMediaId) {
    m_displayState = DisplayState::Content;
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

void PlayerMediaSwitcherPanel::setErrorState(const QString &message) {
    m_displayState = DisplayState::Error;
    m_titleLabel->setText(tr("Media Switcher"));
    setStatusMessage(message);
    applySeriesMode();
}

void PlayerMediaSwitcherPanel::setVideoParts(const QList<MediaItem> &parts,
                                            const QString &currentMediaId) {
    m_parts = parts;
    QList<MediaItem> displayParts = parts;
    for (int i = 0; i < displayParts.size(); ++i) {
        displayParts[i].name = MediaPartUtils::displayTitle(parts, i);
    }
    m_partsGallery->setItems(displayParts);
    m_partsGallery->setHighlightedItemId(currentMediaId);
    m_partsGallery->scrollToItemId(currentMediaId);
    if (!m_isSeriesMode) {
        m_titleLabel->setText(parts.size() > 1 ? tr("Media Switcher") : tr("Continue Watching"));
        if (parts.size() > 1 && m_movieItems.isEmpty()) setStatusMessage(QString());
    }
    applySeriesMode();
}

void PlayerMediaSwitcherPanel::setSeriesData(
    const QString &seriesName, const QList<MediaItem> &seasons,
    const QHash<QString, QList<MediaItem>> &episodesBySeasonId,
    const QString &currentMediaId) {
    m_displayState = DisplayState::Content;
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

    const int partIndex = MediaPartUtils::indexOf(m_parts, item.id);
    if (partIndex >= 0) {
        Q_EMIT playRequested(item.id, MediaPartUtils::displayTitle(m_parts, partIndex),
                            MediaPartUtils::resumeTicks(m_parts.at(partIndex)));
        return;
    }
    Q_EMIT playRequested(item.id, formatResumeDisplayTitle(item),
                         item.userData.playbackPositionTicks);
}

void PlayerMediaSwitcherPanel::applySeriesMode() {
    const bool hasStatus = !m_statusLabel->text().trimmed().isEmpty();
    const bool loading = m_displayState == DisplayState::Loading;
    const bool content = m_displayState == DisplayState::Content;
    const bool showSeriesSections = content && m_isSeriesMode;
    const bool hasSeasons = !m_seasons.isEmpty();
    const bool hasEpisodeItems =
        !m_episodesBySeasonId.value(currentSelectedSeasonId()).isEmpty();

    m_titleLabel->show();
    m_statusLabel->setVisible(hasStatus);
    m_statusLabel->setAlignment(content ? Qt::AlignLeft : Qt::AlignCenter);
    m_retryButton->setVisible(m_displayState == DisplayState::Error);

    
    m_spinnerContainer->setVisible(loading);
    if (loading && isVisible()) {
        m_spinnerTimer->start();
    } else {
        m_spinnerTimer->stop();
    }

    m_movieGallery->setVisible(content && !m_isSeriesMode && !hasStatus && !m_movieItems.isEmpty());
    m_moviesLabel->setVisible(content && !m_isSeriesMode && m_parts.size() > 1 && !m_movieItems.isEmpty());
    m_partsLabel->setVisible(content && m_parts.size() > 1);
    m_partsGallery->setVisible(content && m_parts.size() > 1);
    m_seasonsLabel->setVisible(showSeriesSections && hasSeasons);
    m_seasonGallery->setVisible(showSeriesSections && hasSeasons);
    m_episodesLabel->setVisible(showSeriesSections && hasEpisodeItems);
    m_episodeGallery->setVisible(showSeriesSections && hasEpisodeItems);

    updatePreferredHeight();
    qDebug() << "[PlayerMediaSwitcherPanel] Display state updated"
             << "| state=" << (loading ? "loading" : content ? "content" : "error")
             << "| height=" << minimumHeight() << "| width=" << width()
             << "| visible=" << isVisible();
}

void PlayerMediaSwitcherPanel::updatePreferredHeight() {
    if (!m_uiReady) return;
    ensurePolished();
    const int preferredHeight = preferredDrawerHeight();
    if (minimumHeight() != preferredHeight || maximumHeight() != preferredHeight) {
        setFixedHeight(preferredHeight);
        Q_EMIT preferredHeightChanged();
    }
}

void PlayerMediaSwitcherPanel::showEvent(QShowEvent *event) {
    QFrame::showEvent(event);
    if (!m_uiReady) return;
    updatePreferredHeight();
    if (m_displayState == DisplayState::Loading) m_spinnerTimer->start();
}

void PlayerMediaSwitcherPanel::hideEvent(QHideEvent *event) {
    if (m_uiReady) m_spinnerTimer->stop();
    QFrame::hideEvent(event);
}

void PlayerMediaSwitcherPanel::resizeEvent(QResizeEvent *event) {
    QFrame::resizeEvent(event);
    if (m_uiReady && event->size().width() != event->oldSize().width()) updatePreferredHeight();
}

void PlayerMediaSwitcherPanel::populateMovieGallery() {
    setStatusMessage(QString());
    m_movieGallery->setItems(m_movieItems);
    m_movieGallery->setHighlightedItemId(m_currentMediaId);
    const QString targetMovieId =
        m_currentMediaId.isEmpty() && !m_movieItems.isEmpty()
            ? m_movieItems.first().id
            : m_currentMediaId;
    m_movieGallery->scrollToItemId(targetMovieId);

    if (m_movieItems.isEmpty()) {
        setStatusMessage(tr("No active media found."));
    }
}

void PlayerMediaSwitcherPanel::populateSeasonGallery() {
    setStatusMessage(QString());
    const QString selectedSeasonId = currentSelectedSeasonId();
    m_seasonGallery->setItems(m_seasons);
    m_seasonGallery->setHighlightedItemId(selectedSeasonId);
    m_seasonGallery->scrollToItemId(selectedSeasonId);
}

void PlayerMediaSwitcherPanel::populateEpisodeGallery() {
    setStatusMessage(QString());
    const QList<MediaItem> episodes =
        m_episodesBySeasonId.value(currentSelectedSeasonId());
    QString targetEpisodeId = m_currentMediaId;
    bool hasCurrentEpisode = false;
    for (const MediaItem &episode : episodes) {
        if (episode.id == m_currentMediaId) {
            hasCurrentEpisode = true;
            break;
        }
    }
    if (!hasCurrentEpisode && !episodes.isEmpty()) {
        targetEpisodeId = episodes.first().id;
    }

    m_episodeGallery->setItems(episodes);
    m_episodeGallery->setHighlightedItemId(m_currentMediaId);
    m_episodeGallery->scrollToItemId(targetEpisodeId);

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

int PlayerMediaSwitcherPanel::visibleHeightForWidget(const QWidget *widget, int availableWidth) const
{
    
    
    if (!widget || widget->isHidden()) {
        return 0;
    }

    if (widget->minimumHeight() == widget->maximumHeight() &&
        widget->minimumHeight() > 0) {
        return widget->minimumHeight();
    }

    
    
    const int hint = widget->hasHeightForWidth()
                         ? widget->heightForWidth(availableWidth)
                         : widget->sizeHint().height();
    return qMax(widget->minimumSizeHint().height(), qMax(widget->minimumHeight(), hint));
}
