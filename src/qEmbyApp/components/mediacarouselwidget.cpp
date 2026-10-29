#include "mediacarouselwidget.h"
#include "elidedlabel.h"
#include "carouselnavigationwidget.h"
#include "textoutlineeffect.h"
#include "glassbackdropwidget.h"
#include "pageturnoverlaywidget.h"
#include "../utils/mediaitemutils.h"
#include "../utils/featuredmediautils.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QIcon>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QScopedValueRollback>
#include <QTimer>
#include <QVBoxLayout>
#include <services/media/mediaservice.h>
#include <stdexcept>
#include <utility>

MediaCarouselWidget::MediaCarouselWidget(MediaService* service, QWidget* parent)
    : QFrame(parent), m_service(service)
{
    setObjectName("media-carousel");
    setFocusPolicy(Qt::StrongFocus);
    setAccessibleName(tr("Media Carousel"));
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setContextMenuPolicy(Qt::PreventContextMenu);

    auto* layout = new QVBoxLayout(this);
    
    
    layout->setContentsMargins(0, 24, 0, 12);
    layout->setSpacing(0);

    auto* content = new QWidget(this);
    m_content = content;
    content->setObjectName("carousel-content");
    content->setMaximumWidth(720);
    auto* wrapperLayout = new QVBoxLayout(content);
    wrapperLayout->setContentsMargins(0, 0, 0, 0);
    m_textContent = new QWidget(content);
    m_textContent->setObjectName("carousel-text");
    QSizePolicy textPolicy = m_textContent->sizePolicy();
    textPolicy.setRetainSizeWhenHidden(true);
    m_textContent->setSizePolicy(textPolicy);
    wrapperLayout->addWidget(m_textContent);
    auto* contentLayout = new QVBoxLayout(m_textContent);
    
    contentLayout->setContentsMargins(0, 14, 24, 14);
    contentLayout->setSpacing(6);
    contentLayout->setAlignment(Qt::AlignTop);
    
    
    

    m_title = new ElidedLabel(m_textContent);
    m_title->setObjectName("carousel-title");
    m_title->setTextFormat(Qt::PlainText);
    contentLayout->addWidget(m_title);
    auto* metadataLayout = new QHBoxLayout();
    metadataLayout->setContentsMargins(0, 0, 0, 0);
    metadataLayout->setSpacing(6);
    m_ratingStar = new QLabel(m_textContent);
    m_ratingStar->setObjectName("carousel-rating-star");
    m_ratingStar->setFixedSize(16, 16);
    m_ratingStar->setAlignment(Qt::AlignCenter);
    
    m_ratingStar->setPixmap(QIcon(":/svg/dark/star-filled.svg").pixmap(16, 16));
    m_ratingStar->hide();
    metadataLayout->addWidget(m_ratingStar, 0, Qt::AlignVCenter);
    m_metadata = new ElidedLabel(m_textContent);
    m_metadata->setObjectName("carousel-metadata");
    m_metadata->setTextFormat(Qt::PlainText);
    
    m_metadata->setMinimumHeight(16);
    metadataLayout->addWidget(m_metadata, 1);
    contentLayout->addLayout(metadataLayout);
    m_tags = new ElidedLabel(m_textContent);
    m_tags->setObjectName("carousel-tags");
    m_tags->setTextFormat(Qt::PlainText);
    m_tags->hide();
    contentLayout->addWidget(m_tags);
    m_overview = new ElidedLabel(m_textContent);
    m_overview->setObjectName("carousel-overview");
    m_overview->setTextFormat(Qt::PlainText);
    m_overview->setMaximumLines(5, true);
    m_overview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_overview->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    contentLayout->addWidget(m_overview);
    
    m_textOutlineColor = palette().shadow().color();
    m_textShadowColor = palette().shadow().color();
    for (QLabel* label : {static_cast<QLabel*>(m_title),
                         static_cast<QLabel*>(m_metadata),
                         m_ratingStar,
                         static_cast<QLabel*>(m_tags), static_cast<QLabel*>(m_overview)}) {
        auto* outline = new TextOutlineEffect(label);
        outline->setOutlineColor(m_textOutlineColor);
        outline->setOutlineWidth(m_textOutlineWidth);
        outline->setShadowColor(m_textShadowColor);
        outline->setShadowRadius(m_textShadowRadius);
        outline->setShadowOffset(QPointF(0, m_textShadowOffset));
        label->setGraphicsEffect(outline);
        m_textOutlines.append(outline);
        m_textShadows.append(outline);
    }
    auto* informationRow = new QHBoxLayout();
    informationRow->setSpacing(0);
    informationRow->addWidget(content, 3);
    informationRow->addStretch(2);
    layout->addLayout(informationRow);
    
    layout->addStretch(1);

    m_navigation = new CarouselNavigationWidget(this);
    
    connect(m_navigation, &CarouselNavigationWidget::refreshRequested,
            this, &MediaCarouselWidget::changeSelectionRequested);

    
    
    
    m_logoLayer = new QWidget(this);
    m_logoLayer->setObjectName("carousel-logo-layer");
    m_logoLayer->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_logo = new QLabel(m_logoLayer);
    m_logo->setObjectName("carousel-logo");
    m_logo->setAlignment(Qt::AlignTop | Qt::AlignRight);
    m_logo->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto* logoOutline = new TextOutlineEffect(m_logo);
    logoOutline->setOutlineColor(m_textOutlineColor);
    logoOutline->setOutlineWidth(m_textOutlineWidth);
    m_logo->setGraphicsEffect(logoOutline);
    m_textOutlines.append(logoOutline);
    m_logoLayer->hide();

    m_timer = new QTimer(this);
    m_timer->setInterval(6000);
    connect(m_timer, &QTimer::timeout, this, [this]() {
        
        
        qDebug() << "[MediaCarousel] Auto advance | currentIndex=" << m_index;
        selectSlide(m_index + 1);
    });
    connect(m_navigation, &CarouselNavigationWidget::stepRequested, this,
            [this](int offset) { selectSlide(m_index + offset); });
    connect(m_navigation, &CarouselNavigationWidget::slideRequested, this,
            &MediaCarouselWidget::selectSlide);
    hide();
}

void MediaCarouselWidget::setBackdropSource(DetailContentWidget* source)
{
    if (m_backdropSource != source) {
        if (m_backdropSource) m_backdropSource->finishBackdropTransition();
        delete m_pageOverlay.data();
        m_backdropSource = source;
    }
    if (!source) {
        if (m_glassBackdrop) m_glassBackdrop->setBackdropSource(nullptr);
        updateHeight();
        return;
    }
    if (!m_glassBackdrop) {
        
        
        m_glassBackdrop = new GlassBackdropWidget(source);
        m_glassBackdrop->setObjectName("carousel-glass");
        m_glassBackdrop->setEffectTarget(m_content);
        m_glassBackdrop->setLightAnchor(this);
    } else if (m_glassBackdrop->parentWidget() != source) {
        m_glassBackdrop->setParent(source);
    }
    m_glassBackdrop->setBackdropSource(source);
    m_glassBackdrop->lower();
    if (!m_pageOverlay) {
        m_pageOverlay = new PageTurnOverlayWidget(source);
        
        m_pageOverlay->stackUnder(this);
        connect(m_pageOverlay.data(), &PageTurnOverlayWidget::finished, this, [this]() {
            m_textContent->show();
            updateLogo();
        });
    }
    updateHeight();
}

MediaCarouselWidget::~MediaCarouselWidget()
{
    
    
    delete m_pageOverlay.data();
    delete m_glassBackdrop.data();
}

void MediaCarouselWidget::setPreferredHeight(int height)
{
    m_preferredHeight = qMax(1, height);
    updateHeight();
}

void MediaCarouselWidget::updateHeight()
{
    if (m_preferredHeight <= 0) return;
    
    
    
    const int backdropWidth = m_backdropSource ? m_backdropSource->width() : width();
    const int backdropHeight = !m_image.isNull() && backdropWidth > 0
        ? int(qint64(backdropWidth) * m_image.height() / m_image.width()) : 0;
    const int heightLimit = backdropHeight > 0
        ? qMax(1, int(qint64(backdropHeight) * 2 / 3)) : m_preferredHeight;
    const int targetHeight = qMin(m_preferredHeight, heightLimit);
    if (minimumHeight() == targetHeight && maximumHeight() == targetHeight) return;
    setFixedHeight(targetHeight);
    updateNavigationGeometry();
    qDebug() << "[MediaCarousel] Height updated | preferredHeight=" << m_preferredHeight
             << "| backdropSize=" << QSize(backdropWidth, backdropHeight)
             << "| heightLimit=" << heightLimit << "| appliedHeight=" << height()
             << "| navigationRect=" << m_navigation->geometry();
}

void MediaCarouselWidget::updateNavigationGeometry()
{
    if (!m_navigation) return;
    m_navigation->adjustSize();
    m_navigation->move(qMax(0, width() - m_navigation->width() - 4),
                       qMax(0, height() - m_navigation->height() - 10));
    m_navigation->raise();
}

void MediaCarouselWidget::setTextOutlineColor(const QColor& color)
{
    m_textOutlineColor = color;
    for (auto* outline : m_textOutlines) outline->setOutlineColor(color);
}

void MediaCarouselWidget::setTextOutlineWidth(qreal width)
{
    m_textOutlineWidth = qBound(qreal(0), width, qreal(4));
    for (auto* outline : m_textOutlines) outline->setOutlineWidth(m_textOutlineWidth);
}

void MediaCarouselWidget::setTextShadowColor(const QColor& color)
{
    m_textShadowColor = color;
    for (auto* effect : m_textShadows) effect->setShadowColor(color);
}

void MediaCarouselWidget::setTextShadowRadius(qreal radius)
{
    m_textShadowRadius = qBound(qreal(0), radius, qreal(12));
    for (auto* effect : m_textShadows) effect->setShadowRadius(m_textShadowRadius);
}

void MediaCarouselWidget::setTextShadowOffset(qreal offset)
{
    m_textShadowOffset = qBound(qreal(-8), offset, qreal(8));
    for (auto* effect : m_textShadows)
        effect->setShadowOffset(QPointF(0, m_textShadowOffset));
}

void MediaCarouselWidget::setItems(const QList<MediaItem>& items)
{
    QList<MediaItem> nextItems;
    QSet<QString> ids;
    for (const MediaItem& item : items) {
        if (item.id.isEmpty() || ids.contains(item.id) || item.images.backdropTag.isEmpty()) continue;
        ids.insert(item.id);
        nextItems.append(item);
        if (nextItems.size() == FeaturedMediaUtils::MaximumSlides) break;
    }
    bool sameArtwork = !m_items.isEmpty() && nextItems.size() == m_items.size();
    for (int i = 0; sameArtwork && i < m_items.size(); ++i) {
        const MediaItem& previous = m_items.at(i);
        const MediaItem& next = nextItems.at(i);
        sameArtwork = previous.id == next.id &&
            previous.images.backdropTag == next.images.backdropTag &&
            previous.images.logoTag == next.images.logoTag;
    }
    if (sameArtwork) {
        
        
        for (int i = 0; i < nextItems.size(); ++i)
            updateItem(MediaItemUtils::withMissingDisplayMetadata(nextItems.at(i), m_items.at(i)));
        qDebug() << "[MediaCarousel] Refreshed metadata in place | slides=" << m_items.size();
        return;
    }
    const QString currentId = m_index >= 0 ? m_items.at(m_index).id : QString();
    if (m_backdropSource) m_backdropSource->finishBackdropTransition();
    if (m_pageOverlay) m_pageOverlay->clearFrames();
    ++m_generation;
    m_timer->stop();
    m_items.clear();
    m_artwork.clear();
    m_pendingArtwork.clear();
    m_image = {};
    updateHeight();
    m_logoPixmap = {};
    updateLogo();
    m_index = -1;
    m_presentedIndex = -1;
    m_items = std::move(nextItems);
    qDebug() << "[MediaCarousel] Content changed | slides=" << m_items.size()
             << "| generation=" << m_generation;
    QStringList titles;
    for (const MediaItem& item : std::as_const(m_items)) titles.append(item.name);
    m_navigation->setSlides(titles);
    updateNavigationGeometry();
    setVisible(!m_items.isEmpty());
    
    
    Q_EMIT backdropChanged({}, 1);
    if (m_items.isEmpty()) {
        m_title->setFullText({});
        m_metadata->setFullText({});
        m_ratingStar->hide();
        m_tags->setFullText({});
        m_tags->hide();
        m_overview->setFullText({});
        update();
        return;
    }
    int index = 0;
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id == currentId) index = i;
    }
    selectSlide(index);
}

void MediaCarouselWidget::setSelectionLoading(bool loading)
{
    m_navigation->setRefreshEnabled(!loading);
}

void MediaCarouselWidget::updateItem(const MediaItem& item)
{
    for (qsizetype i = 0; i < m_items.size(); ++i) {
        MediaItem& existing = m_items[i];
        if (existing.id == item.id) {
            const bool imageChanged =
                existing.images.backdropTag != item.images.backdropTag ||
                existing.images.logoTag != item.images.logoTag;
            if (imageChanged) {
                
                QList<MediaItem> items = m_items;
                items[i] = item;
                setItems(items);
            } else {
                existing = item;
                QStringList titles;
                for (const MediaItem& media : std::as_const(m_items))
                    titles.append(media.name);
                m_navigation->setSlides(titles);
                updateNavigationGeometry();
                if (m_presentedIndex >= 0 && m_items.at(m_presentedIndex).id == item.id) {
                    updateText(item);
                    updateForegroundFrame();
                }
            }
            return;
        }
    }
}

void MediaCarouselWidget::updateItemMetadata(const MediaItem& item)
{
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).id != item.id) continue;
        m_items[i] = MediaItemUtils::withMissingDisplayMetadata(m_items.at(i), item);
        const MediaItem current = m_items.at(i);
        qDebug() << "[MediaCarousel] Metadata supplemented | itemId=" << current.id
                 << "| visibleSlide=" << (i == m_presentedIndex)
                 << "| overviewChars=" << current.overview.simplified().size()
                 << "| premiereDate=" << current.premiereDate
                 << "| communityRating=" << current.communityRating
                 << "| genres=" << current.genres.size() << "| tags=" << current.tags.size();
        
        
        if (i == m_presentedIndex) {
            updateText(current);
            updateForegroundFrame();
        }
        return;
    }
}

void MediaCarouselWidget::removeItem(const QString& itemId)
{
    QList<MediaItem> items = m_items;
    for (int i = 0; i < items.size(); ++i) {
        if (items.at(i).id == itemId) {
            items.removeAt(i);
            setItems(items);
            return;
        }
    }
}

void MediaCarouselWidget::selectSlide(int index)
{
    if (m_items.isEmpty()) return;
    const int nextIndex = (index % m_items.size() + m_items.size()) % m_items.size();
    const bool firstSlide = m_index < 0;
    const bool changingSlide = m_index >= 0 && nextIndex != m_index;
    if (changingSlide || firstSlide) {
        m_direction = index < m_index ? -1 : 1;
    }
    m_index = nextIndex;
    const MediaItem item = m_items.at(m_index);
    if (changingSlide || firstSlide) {
        qDebug() << "[MediaCarousel] Slide selected | itemId=" << item.id
                 << "| type=" << item.type
                 << "| productionYear=" << item.productionYear
                 << "| premiereDate=" << item.premiereDate
                 << "| communityRating=" << item.communityRating
                 << "| genres=" << item.genres.size() << "| tags=" << item.tags.size()
                 << "| direction=" << m_direction
                 << "| titleChars=" << item.name.size()
                 << "| overviewChars=" << item.overview.simplified().size()
                 << "| backdropCached="
                 << m_artwork.contains(QStringLiteral("Backdrop:") + item.id)
                 << "| hasLogo=" << !item.images.logoTag.isEmpty();
    }
    m_navigation->setCurrentIndex(m_index);
    const QString backdropKey = QStringLiteral("Backdrop:") + item.id;
    if (m_artwork.contains(backdropKey)) {
        storeArtwork(item.id, QStringLiteral("Backdrop"), m_artwork.value(backdropKey));
    } else if (m_presentedIndex < 0) {
        
        
        showImage({});
    }
    requestArtwork(item, true);
    if (m_items.size() > 1) {
        
        for (const int offset : {-1, 1}) {
            const MediaItem next = m_items.at(
                (m_index + offset + m_items.size()) % m_items.size());
            requestArtwork(next, false);
        }
    }
    updateTimer();
}

void MediaCarouselWidget::updateText(const MediaItem& item)
{
    m_title->setFullText(item.name);
    m_title->setToolTip(item.name);
    QStringList metadata;
    const bool hasRating = item.communityRating > 0;
    m_ratingStar->setVisible(hasRating);
    const QString ratingDescription = hasRating
        ? tr("Rating %1").arg(item.communityRating, 0, 'f', 1) : QString();
    m_ratingStar->setToolTip(ratingDescription);
    m_ratingStar->setAccessibleName(ratingDescription);
    if (hasRating) metadata.append(QString::number(item.communityRating, 'f', 1));
    metadata.append(item.type == "Movie" ? tr("Movie") : tr("Series"));
    const QString releaseDate = MediaItemUtils::releaseDateText(item);
    if (!releaseDate.isEmpty()) metadata.append(releaseDate);
    if (!item.officialRating.isEmpty()) metadata.append(item.officialRating);
    m_metadata->setFullText(metadata.join(QStringLiteral(" · ")));
    const QStringList tags = MediaItemUtils::displayTags(item);
    m_tags->setFullText(tags.join(QStringLiteral(" · ")));
    m_tags->setVisible(!tags.isEmpty());
    
    const QString overview = item.overview.simplified();
    m_overview->setFullText(overview);
    
    
}

void MediaCarouselWidget::requestArtwork(const MediaItem& item, bool foreground)
{
    const QList<QPair<QString, QString>> variants {
        {QStringLiteral("Backdrop"), item.images.backdropTag},
        {QStringLiteral("Logo"), item.images.logoTag}};
    for (const auto& variant : variants) {
        if (variant.second.isEmpty()) continue;
        const QString key = variant.first + QLatin1Char(':') + item.id;
        if (m_artwork.contains(key) || m_pendingArtwork.contains(key)) continue;
        m_pendingArtwork.insert(key);
        QCoro::connect(loadImage(item, m_generation, foreground, variant.first),
                       this, []() {});
    }
}

void MediaCarouselWidget::storeArtwork(QString itemId, QString imageType, QPixmap image)
{
    const QString key = imageType + QLatin1Char(':') + itemId;
    
    m_artwork.insert(key, image);
    if (imageType == QLatin1String("Logo")) {
        if (m_presentedIndex < 0 || m_items.at(m_presentedIndex).id != itemId) return;
        m_logoPixmap = image;
        qDebug() << "[MediaCarousel] Updating presented logo | itemId=" << itemId
                 << "| available=" << !image.isNull()
                 << "| updatingPageTurn=" << (m_pageOverlay && m_pageOverlay->hasFrames());
        updateLogo();
    } else {
        if (m_index < 0 || m_items.at(m_index).id != itemId) return;
        showImage(image);
    }
}

QCoro::Task<void> MediaCarouselWidget::loadImage(MediaItem item, int generation,
                                              bool foreground, QString imageType)
{
    QPointer<MediaCarouselWidget> guard(this);
    QPointer<MediaService> service = m_service;
    if (!service) co_return;
    const QString key = imageType + QLatin1Char(':') + item.id;
    const bool isLogo = imageType == QLatin1String("Logo");
    const QString tag = isLogo ? item.images.logoTag : item.images.backdropTag;
    try {
        QPixmap image = co_await service->fetchImage(
            item.id, imageType, tag,
            isLogo ? 600 : 1920, isLogo ? -1 : 0,
            foreground ? ImageRequestPriority::High : ImageRequestPriority::Low,
            guard.data());
        if (!guard || generation != guard->m_generation) co_return;
        guard->m_pendingArtwork.remove(key);
        if (image.isNull()) {
            qWarning() << "[MediaCarousel] Empty artwork | itemId=" << item.id
                       << "| imageType=" << imageType;
        } else {
            qDebug() << "[MediaCarousel] Artwork loaded | itemId=" << item.id
                     << "| imageType=" << imageType << "| size=" << image.size();
        }
        guard->storeArtwork(item.id, imageType, image);
    } catch (const std::exception&) {
        if (!guard || generation != guard->m_generation) co_return;
        guard->m_pendingArtwork.remove(key);
        guard->storeArtwork(item.id, imageType, {});
        
        qWarning() << "[MediaCarousel] Artwork request failed | itemId=" << item.id
                   << "| imageType=" << imageType;
    }
}

void MediaCarouselWidget::showImage(const QPixmap& image)
{
    if (m_index < 0) return;
    const MediaItem item = m_items.at(m_index);
    const bool sameSlide = m_presentedIndex == m_index && image.cacheKey() == m_image.cacheKey();
    const bool animate = !sameSlide && m_presentedIndex >= 0 &&
        !m_image.isNull() && !image.isNull() && isVisible() &&
        m_backdropSource && m_pageOverlay;
    
    
    m_image = image;
    updateHeight();
    QPixmap outgoing;
    if (animate) {
        
        outgoing = m_pageOverlay->hasFrames() ? m_pageOverlay->currentFrame()
            : captureForeground(m_backdropSource->backdropSnapshotSize());
    }
    m_presentedIndex = m_index;
    updateText(item);
    m_logoPixmap = item.images.logoTag.isEmpty() ? QPixmap()
        : m_artwork.value(QStringLiteral("Logo:") + item.id);
    m_logo->setAccessibleName(tr("Logo for %1").arg(item.name));
    updateLogo();
    if (sameSlide) {
        updateForegroundFrame();
        return;
    }
    if (animate && !outgoing.isNull()) {
        const QSize incomingSize(m_backdropSource->width(),
            m_backdropSource->width() * image.height() / qMax(1, image.width()));
        m_pageOverlay->setFrames(outgoing, captureForeground(incomingSize));
        if (m_pageOverlay->hasFrames()) {
            m_textContent->hide();
            m_logoLayer->hide();
        }
    } else if (m_pageOverlay) {
        m_pageOverlay->clearFrames();
    }
    qDebug() << "[MediaCarousel] Presenting slide | itemId=" << item.id
             << "| synchronizedPageTurn=" << (m_pageOverlay && m_pageOverlay->hasFrames())
             << "| backdropAvailable=" << !image.isNull() << "| direction=" << m_direction
             << "| overviewChars=" << m_overview->fullText().size()
             << "| textSize=" << m_textContent->size()
             << "| titleRect=" << m_title->geometry()
             << "| metadataRect=" << m_metadata->geometry()
             << "| logoAvailable=" << !m_logoPixmap.isNull()
             << "| logoLayerRect=" << m_logoLayer->geometry()
             << "| devicePixelRatio=" << devicePixelRatioF()
             << "| overviewRect=" << m_overview->geometry();
    Q_EMIT backdropChanged(image, m_direction);
}

QPixmap MediaCarouselWidget::captureForeground(const QSize& sceneSize)
{
    if (!m_pageOverlay || m_capturingForeground) return {};
    
    
    QScopedValueRollback<bool> captureGuard(m_capturingForeground, true);
    const bool textWasHidden = m_textContent->isHidden();
    const bool logoWasHidden = m_logoLayer->isHidden();
    
    
    
    m_textContent->show();
    layout()->activate();
    m_content->layout()->invalidate();
    m_content->layout()->activate();
    m_textContent->layout()->invalidate();
    m_textContent->layout()->activate();
    
    
    const bool includeLogo = !m_logoPixmap.isNull();
    if (includeLogo) m_logoLayer->show();
    QList<QWidget*> widgets {m_textContent};
    if (includeLogo) widgets.append(m_logoLayer);
    const QPixmap frame = m_pageOverlay->captureWidgets(widgets, sceneSize);
    if (textWasHidden) m_textContent->hide();
    if (logoWasHidden) m_logoLayer->hide();
    qDebug() << "[MediaCarousel] Foreground captured"
             << "| sceneSize=" << sceneSize << "| includesLogo=" << includeLogo
             << "| logoSize=" << m_logoPixmap.size() << "| frameAvailable=" << !frame.isNull();
    return frame;
}

void MediaCarouselWidget::updateForegroundFrame()
{
    if (m_capturingForeground || !m_pageOverlay || !m_pageOverlay->hasFrames() ||
        !m_backdropSource || m_image.isNull()) return;
    const QSize sceneSize(m_backdropSource->width(),
        m_backdropSource->width() * m_image.height() / qMax(1, m_image.width()));
    m_pageOverlay->setIncomingFrame(captureForeground(sceneSize));
}

void MediaCarouselWidget::updateLogo()
{
    if (!m_logo) return;
    const int logoWidth = qMax(1, qMin(260, width() / 4));
    const int logoHeight = qMin(86, height() / 2);
    
    
    constexpr int outlinePadding = 6;
    m_logoLayer->setGeometry(width() - logoWidth - outlinePadding,
                            12 - outlinePadding, logoWidth + outlinePadding,
                            logoHeight + 2 * outlinePadding);
    m_logo->setGeometry(outlinePadding, outlinePadding, logoWidth, logoHeight);
    m_logoLayer->setVisible(!m_logoPixmap.isNull() &&
        (m_capturingForeground || !(m_pageOverlay && m_pageOverlay->hasFrames())));
    if (m_logoPixmap.isNull()) {
        m_logo->clear();
        updateForegroundFrame();
        return;
    }
    const qreal dpr = devicePixelRatioF();
    QPixmap display = m_logoPixmap.scaled(
        QSize(qRound(logoWidth * dpr), qRound(logoHeight * dpr)),
        Qt::KeepAspectRatio, Qt::SmoothTransformation);
    display.setDevicePixelRatio(dpr);
    m_logo->setPixmap(display);
    updateForegroundFrame();
}

void MediaCarouselWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        event->accept();
        return;
    }
    QFrame::mousePressEvent(event);
}

void MediaCarouselWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_presentedIndex >= 0) {
        const MediaItem item = m_items.at(m_presentedIndex);
        Q_EMIT itemClicked(item);
        event->accept();
        return;
    }
    QFrame::mouseReleaseEvent(event);
}

void MediaCarouselWidget::resizeEvent(QResizeEvent* event)
{
    QFrame::resizeEvent(event);
    
    updateHeight();
    
    if (m_backdropSource && m_backdropSource->isBackdropTransitionRunning())
        m_backdropSource->finishBackdropTransition();
    updateLogo();
    updateNavigationGeometry();
}

void MediaCarouselWidget::updateTimer()
{
    const bool wasActive = m_timer->isActive();
    const bool shouldRun = isVisible() && !m_hovered && m_items.size() > 1;
    
    if (shouldRun) m_timer->start();
    else m_timer->stop();
    if (wasActive != m_timer->isActive()) {
        qDebug() << "[MediaCarousel] Auto advance timer | active=" << m_timer->isActive()
                 << "| visible=" << isVisible() << "| hovered=" << m_hovered
                 << "| slides=" << m_items.size() << "| intervalMs=" << m_timer->interval();
    }
}

void MediaCarouselWidget::showEvent(QShowEvent* event)
{
    QFrame::showEvent(event);
    m_hovered = underMouse();
    updateTimer();
    qDebug() << "[MediaCarousel] Shown; resuming current slide"
             << "| requestedIndex=" << m_index << "| presentedIndex=" << m_presentedIndex
             << "| backdropAvailable=" << !m_image.isNull()
             << "| timerActive=" << m_timer->isActive();
}

void MediaCarouselWidget::hideEvent(QHideEvent* event)
{
    qDebug() << "[MediaCarousel] Hidden; preserving current slide"
             << "| requestedIndex=" << m_index << "| presentedIndex=" << m_presentedIndex
             << "| finishingPageTurn="
             << (m_backdropSource && m_backdropSource->isBackdropTransitionRunning());
    m_timer->stop();
    if (m_backdropSource) m_backdropSource->finishBackdropTransition();
    QFrame::hideEvent(event);
}

bool MediaCarouselWidget::event(QEvent* event)
{
    if (event->type() == QEvent::StyleChange && m_backdropSource &&
        m_backdropSource->isBackdropTransitionRunning())
        m_backdropSource->finishBackdropTransition();
    if (event->type() == QEvent::Enter || event->type() == QEvent::Leave) {
        m_hovered = event->type() == QEvent::Enter;
        if (m_timer) updateTimer();
    }
    return QFrame::event(event);
}

void MediaCarouselWidget::keyPressEvent(QKeyEvent* event)
{
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter ||
         event->key() == Qt::Key_Space) && m_presentedIndex >= 0) {
        const MediaItem item = m_items.at(m_presentedIndex);
        Q_EMIT itemClicked(item);
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Left || event->key() == Qt::Key_Right) {
        selectSlide(m_index + (event->key() == Qt::Key_Left ? -1 : 1));
        event->accept();
        return;
    }
    QFrame::keyPressEvent(event);
}
