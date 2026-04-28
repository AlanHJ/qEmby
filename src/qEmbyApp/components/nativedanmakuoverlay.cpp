#include "nativedanmakuoverlay.h"

#include "mpvcontroller.h"

#include <QFont>
#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPen>
#include <QPointF>
#include <QRectF>
#include <QResizeEvent>
#include <QScreen>
#include <QTimer>
#include <QtGlobal>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <utility>

namespace {

constexpr qreal kReferenceWidth = 1920.0;
constexpr qreal kReferenceHeight = 1080.0;
constexpr qreal kTopMargin = 24.0;
constexpr qreal kBottomMargin = 48.0;
constexpr int kResizeRebuildDebounceMs = 48;
constexpr int kDefaultFrameIntervalMs = 8;
constexpr qreal kScrollStartPadding = 32.0;
constexpr qreal kScrollEndPadding = 48.0;
constexpr qreal kScrollLaneGapPadding = 96.0;
constexpr qreal kScrollReferenceWidthFactor = 8.0;
constexpr qreal kScrollSpeedEaseExponent = 0.55;
constexpr qreal kScrollReferenceDurationMs = 12000.0;
constexpr qreal kMaxScrollPixelsPerMs = 1.12;
constexpr qint64 kMinScrollDurationMs = 2200;
constexpr qint64 kMaxScrollDurationMs = 22000;
constexpr qreal kMaxScrollQueueDelayRatio = 0.42;
constexpr qint64 kMaxStaticQueueDelayMs = 1800;
constexpr qreal kSubtitleProtectRatio = 0.85;
constexpr int kSpriteCacheSoftLimit = 4096;
constexpr int kSpriteWarmupLimit = 32;
constexpr qint64 kSpriteWarmupAheadMs = 5000;

QStringList normalizeBlockedKeywords(const QStringList &keywords)
{
    QStringList result;
    for (const QString &keyword : keywords) {
        const QString trimmed = keyword.trimmed();
        if (!trimmed.isEmpty()) {
            result.append(trimmed);
        }
    }
    result.removeDuplicates();
    return result;
}

bool isBlocked(const QString &text, const QStringList &blockedKeywords)
{
    for (const QString &keyword : blockedKeywords) {
        if (text.contains(keyword, Qt::CaseInsensitive)) {
            return true;
        }
    }
    return false;
}

double effectiveScrollSpeedScale(double configuredSpeedScale)
{
    if (configuredSpeedScale <= 1.0) {
        return configuredSpeedScale;
    }
    return std::pow(configuredSpeedScale, kScrollSpeedEaseExponent);
}

QString sanitizeCommentText(QString text)
{
    text.replace(QStringLiteral("\r"), QString());
    text.replace(QStringLiteral("\n"), QStringLiteral(" "));
    return text.trimmed();
}

QColor applyOpacity(QColor color, double opacity)
{
    color.setAlphaF(qBound(0.05, opacity, 1.0));
    return color;
}

qreal viewportScaleForSize(const QSize &size)
{
    if (size.isEmpty()) {
        return 1.0;
    }

    return std::max<qreal>(
        0.4,
        std::min(size.width() / kReferenceWidth, size.height() / kReferenceHeight));
}

bool renderOptionsEqual(const DanmakuRenderOptions &lhs,
                        const DanmakuRenderOptions &rhs)
{
    const auto almostEqual = [](double left, double right) {
        return std::abs(left - right) <= 0.0001;
    };

    return lhs.enabled == rhs.enabled &&
           almostEqual(lhs.opacity, rhs.opacity) &&
           almostEqual(lhs.fontScale, rhs.fontScale) &&
           lhs.fontWeight == rhs.fontWeight &&
           almostEqual(lhs.outlineSize, rhs.outlineSize) &&
           almostEqual(lhs.shadowOffset, rhs.shadowOffset) &&
           lhs.areaPercent == rhs.areaPercent &&
           lhs.density == rhs.density &&
           almostEqual(lhs.speedScale, rhs.speedScale) &&
           lhs.offsetMs == rhs.offsetMs &&
           lhs.hideScroll == rhs.hideScroll &&
           lhs.hideTop == rhs.hideTop &&
           lhs.hideBottom == rhs.hideBottom &&
           lhs.dualSubtitle == rhs.dualSubtitle &&
           lhs.blockedKeywords == rhs.blockedKeywords;
}

bool renderAppearanceEqual(const DanmakuRenderOptions &lhs,
                           const DanmakuRenderOptions &rhs)
{
    const auto almostEqual = [](double left, double right) {
        return std::abs(left - right) <= 0.0001;
    };

    return almostEqual(lhs.opacity, rhs.opacity) &&
           almostEqual(lhs.fontScale, rhs.fontScale) &&
           lhs.fontWeight == rhs.fontWeight &&
           almostEqual(lhs.outlineSize, rhs.outlineSize) &&
           almostEqual(lhs.shadowOffset, rhs.shadowOffset);
}

} 

NativeDanmakuOverlay::NativeDanmakuOverlay(MpvController *controller,
                                           QWidget *parent)
    : QWidget(parent), m_controller(controller)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_NoSystemBackground, true);
    setAttribute(Qt::WA_StaticContents, true);
    hide();

    m_frameTimer = new QTimer(this);
    m_frameTimer->setTimerType(Qt::PreciseTimer);
    m_frameTimer->setInterval(kDefaultFrameIntervalMs);
    connect(m_frameTimer, &QTimer::timeout, this, [this]() {
        const qint64 positionMs = currentPositionMs();
        if (m_lastResolvedPositionMs < 0 || positionMs < m_lastResolvedPositionMs ||
            positionMs - m_lastResolvedPositionMs > 1500) {
            rebuildActiveItems(positionMs);
        } else {
            updateActiveItems(positionMs);
        }
        update();
    });

    m_scheduleRebuildTimer = new QTimer(this);
    m_scheduleRebuildTimer->setSingleShot(true);
    m_scheduleRebuildTimer->setInterval(kResizeRebuildDebounceMs);
    connect(m_scheduleRebuildTimer, &QTimer::timeout, this,
            &NativeDanmakuOverlay::rebuildScheduleNow);

    if (!m_controller) {
        return;
    }

    connect(m_controller, &MpvController::positionChanged, this,
            [this](double positionSeconds) {
                const qint64 positionMs = std::max<qint64>(
                    0, static_cast<qint64>(std::llround(positionSeconds * 1000.0)));
                const qint64 driftMs =
                    std::llabs(positionMs - currentPositionMs());
                refreshBasePosition(positionMs);
                if (m_playbackPaused || driftMs > 240) {
                    rebuildActiveItems(positionMs);
                    update();
                }
            });
    connect(m_controller, &MpvController::playbackStateChanged, this,
            [this](bool isPaused) {
                m_basePositionMs = currentPositionMs();
                m_playbackPaused = isPaused;
                m_positionClock.invalidate();
                if (!m_playbackPaused) {
                    m_positionClock.start();
                }
                syncFrameTimer();
                update();
            });
    connect(m_controller, &MpvController::propertyChanged, this,
            [this](const QString &property, const QVariant &value) {
                handleControllerPropertyChanged(property, value);
            });
}

void NativeDanmakuOverlay::setRenderOptions(DanmakuRenderOptions options)
{
    if (renderOptionsEqual(m_options, options)) {
        return;
    }

    const bool appearanceChanged = !renderAppearanceEqual(m_options, options);
    m_options = options;
    requestScheduleRebuild(false, appearanceChanged);
}

void NativeDanmakuOverlay::setComments(QList<DanmakuComment> comments)
{
    m_comments = comments;
    m_sortedComments = comments;
    std::sort(m_sortedComments.begin(), m_sortedComments.end(),
              [](const DanmakuComment &lhs, const DanmakuComment &rhs) {
                  if (lhs.timeMs == rhs.timeMs) {
                      return lhs.text < rhs.text;
                  }
                  return lhs.timeMs < rhs.timeMs;
              });
    requestScheduleRebuild(false, false);
}

void NativeDanmakuOverlay::clearDanmaku()
{
    if (m_scheduleRebuildTimer) {
        m_scheduleRebuildTimer->stop();
    }
    m_comments.clear();
    m_sortedComments.clear();
    m_scheduleDirty = false;
    m_clearSpriteCacheOnNextRebuild = false;
    clearScheduledState(true);
    hide();
    syncFrameTimer();
    update();
}

bool NativeDanmakuOverlay::hasComments() const
{
    return !m_comments.isEmpty();
}

void NativeDanmakuOverlay::setDanmakuVisible(bool visible)
{
    m_visibleRequested = visible;
    if (m_visibleRequested && m_scheduleDirty) {
        rebuildScheduleNow();
    }
    const bool shouldShow = m_visibleRequested && !m_schedule.isEmpty();
    QWidget::setVisible(shouldShow);
    if (shouldShow) {
        rebuildActiveItems(currentPositionMs());
    }
    syncFrameTimer();
    update();
}

void NativeDanmakuOverlay::setBottomSubtitleProtected(bool enabled)
{
    if (m_bottomSubtitleProtected == enabled) {
        return;
    }

    m_bottomSubtitleProtected = enabled;
    requestScheduleRebuild(false, false);
}

void NativeDanmakuOverlay::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setCompositionMode(QPainter::CompositionMode_Source);
    painter.fillRect(event ? event->rect() : rect(), Qt::transparent);

    if (!m_visibleRequested || m_activeIndexes.isEmpty()) {
        return;
    }

    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    const qreal positionMs = currentPositionMsPrecise();
    const QRectF surfaceRect = effectiveSurfaceRect();
    const qreal dpr = std::max<qreal>(1.0, devicePixelRatioF());
    const auto alignToDevicePixel = [dpr](qreal value) {
        return std::round(value * dpr) / dpr;
    };
    for (int index : std::as_const(m_activeIndexes)) {
        if (index < 0 || index >= m_schedule.size()) {
            continue;
        }

        const ScheduledItem &item = m_schedule.at(index);
        const QPixmap *sprite = spriteForItem(item);
        if (!sprite || sprite->isNull()) {
            continue;
        }

        qreal x = surfaceRect.left();
        qreal y = laneTop(item);
        switch (item.type) {
        case ItemType::Scroll: {
            const qreal durationMs = std::max<qint64>(1, item.endMs - item.startMs);
            const qreal progress = qBound<qreal>(
                0.0, (positionMs - item.startMs) / durationMs, 1.0);
            const qreal startX = surfaceRect.left() + surfaceRect.width() +
                                 m_scrollStartPadding;
            const qreal endX = surfaceRect.left() - item.visualWidth -
                               m_scrollEndPadding;
            x = startX + (endX - startX) * progress;
            break;
        }
        case ItemType::Top:
        case ItemType::Bottom:
            x = surfaceRect.left() +
                (surfaceRect.width() - spriteLogicalWidth(*sprite)) / 2.0;
            break;
        }

        painter.drawPixmap(QPointF(x, alignToDevicePixel(y)),
                           *sprite);
    }
}

void NativeDanmakuOverlay::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    requestScheduleRebuild(false, false);
}

void NativeDanmakuOverlay::rebuildSchedule()
{
    requestScheduleRebuild(true, false);
}

void NativeDanmakuOverlay::requestScheduleRebuild(bool immediate,
                                                  bool clearSpriteCache)
{
    m_scheduleDirty = true;
    m_clearSpriteCacheOnNextRebuild =
        m_clearSpriteCacheOnNextRebuild || clearSpriteCache;

    if (immediate) {
        if (m_scheduleRebuildTimer) {
            m_scheduleRebuildTimer->stop();
        }
        rebuildScheduleNow();
        return;
    }

    if (m_scheduleRebuildTimer && (m_visibleRequested || isVisible())) {
        m_scheduleRebuildTimer->start();
    }
}

void NativeDanmakuOverlay::rebuildScheduleNow()
{
    if (m_scheduleRebuildTimer) {
        m_scheduleRebuildTimer->stop();
    }

    const bool clearSpriteCache =
        m_clearSpriteCacheOnNextRebuild || m_spriteCache.size() > kSpriteCacheSoftLimit;
    m_scheduleDirty = false;
    m_clearSpriteCacheOnNextRebuild = false;
    clearScheduledState(clearSpriteCache);

    if (m_sortedComments.isEmpty() || size().isEmpty()) {
        QWidget::setVisible(false);
        syncFrameTimer();
        update();
        return;
    }

    const QRectF surfaceRect = effectiveSurfaceRect();
    const qreal viewportScale = viewportScaleForSize(size());
    const QStringList blockedKeywords =
        normalizeBlockedKeywords(m_options.blockedKeywords);
    const double opacity = qBound(0.05, m_options.opacity, 1.0);
    const double fontScale = qBound(0.6, m_options.fontScale, 2.4);
    const int fontWeight = qBound(100, m_options.fontWeight, 900);
    m_outlineSize = qBound<qreal>(0.0, m_options.outlineSize, 6.0);
    m_shadowOffset = qBound<qreal>(0.0, m_options.shadowOffset, 4.0);
    m_spritePadding =
        std::ceil(std::max(m_outlineSize * 2.0, m_shadowOffset * 2.0)) + 4.0;
    const double speedScale = qBound(0.5, m_options.speedScale, 3.0);
    const double effectiveSpeedScale = effectiveScrollSpeedScale(speedScale);
    const int areaPercent = qBound(
        10, m_bottomSubtitleProtected ? std::min(m_options.areaPercent, 85)
                                      : m_options.areaPercent,
        100);
    const int densityPercent = qBound(20, m_options.density, 100);
    const int defaultCommentFontSize = qBound(
        12, static_cast<int>(std::lround(25.0 * fontScale)), 96);
    m_lineHeight = std::ceil(defaultCommentFontSize * 1.28);
    const int maxLanes = std::max(
        1, static_cast<int>(std::floor((surfaceRect.height() * (areaPercent / 100.0)) /
                                       std::max<qreal>(1.0, m_lineHeight))));
    const int laneCount = std::max(
        1, static_cast<int>(std::round(maxLanes * (densityPercent / 100.0))));
    m_topMargin = kTopMargin * viewportScale;
    m_bottomMargin = kBottomMargin * viewportScale;
    m_scrollStartPadding = kScrollStartPadding * viewportScale;
    m_scrollEndPadding = kScrollEndPadding * viewportScale;
    m_scrollLaneGapPadding = kScrollLaneGapPadding * viewportScale;

    const qint64 referenceDurationMs =
        static_cast<qint64>(std::lround(kScrollReferenceDurationMs /
                                        effectiveSpeedScale));
    const qreal referenceTextWidth = std::clamp(
        defaultCommentFontSize * kScrollReferenceWidthFactor,
        220.0, 1800.0);
    const qreal referenceDistance =
        surfaceRect.width() + m_scrollStartPadding + m_scrollEndPadding +
        referenceTextWidth;
    const qreal scrollPixelsPerMs = std::min(
        kMaxScrollPixelsPerMs * viewportScale,
        std::max<qreal>(0.01, referenceDistance /
                                  std::max<qint64>(1, referenceDurationMs)));

    QFont font;
    font.setFamily(QFont().family());
    font.setWeight(static_cast<QFont::Weight>(fontWeight));
    QHash<int, QFontMetricsF> metricsCache;
    auto metricsForSize = [&font, &metricsCache](int fontSize) {
        const auto it = metricsCache.constFind(fontSize);
        if (it != metricsCache.constEnd()) {
            return it.value();
        }

        QFont sizedFont(font);
        sizedFont.setPixelSize(fontSize);
        const QFontMetricsF metrics(sizedFont);
        metricsCache.insert(fontSize, metrics);
        return metrics;
    };

    QVector<qint64> scrollLaneAvailable(laneCount, 0);
    QVector<qint64> topLaneAvailable(laneCount, 0);
    QVector<qint64> bottomLaneAvailable(laneCount, 0);

    m_schedule.reserve(m_sortedComments.size());
    for (const DanmakuComment &comment : std::as_const(m_sortedComments)) {
        if (!comment.isValid()) {
            continue;
        }

        const QString text = sanitizeCommentText(comment.text);
        if (text.isEmpty() || isBlocked(text, blockedKeywords)) {
            continue;
        }

        const bool hideScroll = comment.mode == 1 && m_options.hideScroll;
        const bool hideTop = comment.mode == 5 && m_options.hideTop;
        const bool hideBottom = comment.mode == 4 && m_options.hideBottom;
        if (hideScroll || hideTop || hideBottom) {
            continue;
        }

        const int sourceFontSize = comment.fontLevel > 0 ? comment.fontLevel : 25;
        const int fontSize = qBound(
            8, static_cast<int>(std::lround(sourceFontSize * fontScale)), 96);
        const QFontMetricsF metrics = metricsForSize(fontSize);
        const qreal textWidth =
            std::max(metrics.horizontalAdvance(text), static_cast<qreal>(fontSize));
        const qreal visualWidth = textWidth + m_spritePadding * 2.0;
        const qint64 startMs = std::max<qint64>(0, comment.timeMs + m_options.offsetMs);

        if (comment.mode == 5 || comment.mode == 4) {
            QVector<qint64> &lanes =
                (comment.mode == 5) ? topLaneAvailable : bottomLaneAvailable;
            const qint64 durationMs = 4200;
            int laneIndex = 0;
            qint64 bestAvailable = lanes[0];
            for (int i = 0; i < lanes.size(); ++i) {
                if (lanes[i] <= startMs) {
                    laneIndex = i;
                    break;
                }
                if (lanes[i] < bestAvailable) {
                    bestAvailable = lanes[i];
                    laneIndex = i;
                }
            }

            const qint64 actualStartMs = std::max(startMs, lanes[laneIndex]);
            if (actualStartMs - startMs > kMaxStaticQueueDelayMs) {
                continue;
            }

            ScheduledItem item;
            item.type = (comment.mode == 5) ? ItemType::Top : ItemType::Bottom;
            item.startMs = actualStartMs;
            item.endMs = actualStartMs + durationMs;
            item.laneIndex = laneIndex;
            item.fontSize = fontSize;
            item.fontWeight = fontWeight;
            item.color = applyOpacity(comment.color, opacity);
            item.text = text;
            item.textWidth = textWidth;
            item.visualWidth = visualWidth;
            m_schedule.append(item);
            lanes[laneIndex] = item.endMs;
            continue;
        }

        const qreal totalDistance =
            surfaceRect.width() + m_scrollStartPadding + m_scrollEndPadding +
            visualWidth;
        const qint64 durationMs = qBound<qint64>(
            kMinScrollDurationMs,
            static_cast<qint64>(std::llround(totalDistance / scrollPixelsPerMs)),
            kMaxScrollDurationMs);
        const qint64 laneGapMs = static_cast<qint64>(std::ceil(
            (visualWidth + m_scrollLaneGapPadding) / scrollPixelsPerMs));

        int laneIndex = 0;
        qint64 bestAvailable = scrollLaneAvailable[0];
        for (int i = 0; i < scrollLaneAvailable.size(); ++i) {
            if (scrollLaneAvailable[i] <= startMs) {
                laneIndex = i;
                break;
            }
            if (scrollLaneAvailable[i] < bestAvailable) {
                bestAvailable = scrollLaneAvailable[i];
                laneIndex = i;
            }
        }

        const qint64 actualStartMs =
            std::max(startMs, scrollLaneAvailable[laneIndex]);
        const qint64 maxQueueDelayMs = std::max<qint64>(
            300, static_cast<qint64>(std::lround(durationMs *
                                                 kMaxScrollQueueDelayRatio)));
        if (actualStartMs - startMs > maxQueueDelayMs) {
            continue;
        }

        ScheduledItem item;
        item.type = ItemType::Scroll;
        item.startMs = actualStartMs;
        item.endMs = actualStartMs + durationMs;
        item.laneIndex = laneIndex;
        item.fontSize = fontSize;
        item.fontWeight = fontWeight;
        item.color = applyOpacity(comment.color, opacity);
        item.text = text;
        item.textWidth = textWidth;
        item.visualWidth = visualWidth;
        m_schedule.append(item);
        scrollLaneAvailable[laneIndex] = actualStartMs + laneGapMs;
    }

    rebuildActiveItems(currentPositionMs());
    warmSpriteCacheAround(currentPositionMsPrecise());
    QWidget::setVisible(m_visibleRequested && !m_schedule.isEmpty());
    syncFrameTimer();
    update();
}

void NativeDanmakuOverlay::warmSpriteCacheAround(qreal positionMs)
{
    int warmed = 0;
    for (int i = 0; i < m_schedule.size() && warmed < kSpriteWarmupLimit; ++i) {
        const ScheduledItem &item = m_schedule.at(i);
        if (item.endMs < positionMs) {
            continue;
        }
        if (item.startMs > positionMs + kSpriteWarmupAheadMs) {
            break;
        }

        spriteForItem(item);
        ++warmed;
    }
}

void NativeDanmakuOverlay::rebuildActiveItems(qint64 positionMs)
{
    m_activeIndexes.clear();
    m_nextScheduleIndex = m_schedule.size();
    for (int i = 0; i < m_schedule.size(); ++i) {
        const ScheduledItem &item = m_schedule.at(i);
        if (item.endMs <= positionMs) {
            continue;
        }
        if (item.startMs <= positionMs) {
            m_activeIndexes.append(i);
            continue;
        }

        m_nextScheduleIndex = i;
        break;
    }
    m_lastResolvedPositionMs = positionMs;
}

void NativeDanmakuOverlay::updateActiveItems(qint64 positionMs)
{
    while (m_nextScheduleIndex < m_schedule.size() &&
           m_schedule.at(m_nextScheduleIndex).startMs <= positionMs) {
        if (m_schedule.at(m_nextScheduleIndex).endMs > positionMs) {
            m_activeIndexes.append(m_nextScheduleIndex);
        }
        ++m_nextScheduleIndex;
    }

    m_activeIndexes.erase(
        std::remove_if(m_activeIndexes.begin(), m_activeIndexes.end(),
                       [this, positionMs](int index) {
                           return index < 0 || index >= m_schedule.size() ||
                                  m_schedule.at(index).endMs <= positionMs;
                       }),
        m_activeIndexes.end());
    m_lastResolvedPositionMs = positionMs;
}

void NativeDanmakuOverlay::clearScheduledState(bool clearSpriteCache)
{
    m_schedule.clear();
    m_activeIndexes.clear();
    if (clearSpriteCache) {
        m_spriteCache.clear();
    }
    m_nextScheduleIndex = 0;
    m_lastResolvedPositionMs = -1;
}

void NativeDanmakuOverlay::syncFrameTimer()
{
    const int targetInterval = targetFrameIntervalMs();
    if (m_frameTimer->interval() != targetInterval) {
        m_frameTimer->setInterval(targetInterval);
    }

    const bool shouldRun =
        m_visibleRequested && isVisible() && !m_schedule.isEmpty() &&
        !m_playbackPaused;
    if (shouldRun) {
        if (!m_frameTimer->isActive()) {
            m_frameTimer->start();
        }
        return;
    }

    m_frameTimer->stop();
}

int NativeDanmakuOverlay::targetFrameIntervalMs() const
{
    const QScreen *targetScreen = screen();
    if (!targetScreen && window()) {
        targetScreen = window()->screen();
    }

    const qreal refreshRate = targetScreen ? targetScreen->refreshRate() : 0.0;
    if (refreshRate <= 0.0) {
        return kDefaultFrameIntervalMs;
    }

    const qreal desiredRefreshRate = std::max<qreal>(120.0, refreshRate);
    return qBound(4,
                  static_cast<int>(std::lround(1000.0 / desiredRefreshRate)),
                  kDefaultFrameIntervalMs);
}

void NativeDanmakuOverlay::refreshBasePosition(qint64 positionMs)
{
    m_basePositionMs = std::max<qint64>(0, positionMs);
    m_positionClock.invalidate();
    if (!m_playbackPaused) {
        m_positionClock.start();
    }
}

qreal NativeDanmakuOverlay::currentPositionMsPrecise() const
{
    if (m_playbackPaused || !m_positionClock.isValid()) {
        return m_basePositionMs;
    }

    const qreal elapsedMs = m_positionClock.nsecsElapsed() / 1000000.0;
    return m_basePositionMs + elapsedMs * m_playbackSpeed;
}

qint64 NativeDanmakuOverlay::currentPositionMs() const
{
    return static_cast<qint64>(std::llround(currentPositionMsPrecise()));
}

QRectF NativeDanmakuOverlay::effectiveSurfaceRect() const
{
    QRectF surface(rect());
    if (surface.isEmpty()) {
        return surface;
    }

    if (m_controller) {
        qreal displayWidth =
            m_controller->getProperty(QStringLiteral("video-params/dw")).toDouble();
        qreal displayHeight =
            m_controller->getProperty(QStringLiteral("video-params/dh")).toDouble();
        if (displayWidth <= 0.0 || displayHeight <= 0.0) {
            displayWidth =
                m_controller->getProperty(QStringLiteral("width")).toDouble();
            displayHeight =
                m_controller->getProperty(QStringLiteral("height")).toDouble();
        }

        if (displayWidth > 0.0 && displayHeight > 0.0 &&
            surface.width() > 0.0 && surface.height() > 0.0) {
            const qreal videoAspect = displayWidth / displayHeight;
            const qreal viewportAspect = surface.width() / surface.height();
            if (videoAspect > 0.0 && viewportAspect > 0.0) {
                qreal fittedWidth = surface.width();
                qreal fittedHeight = surface.height();
                if (videoAspect > viewportAspect) {
                    fittedHeight = fittedWidth / videoAspect;
                } else {
                    fittedWidth = fittedHeight * videoAspect;
                }

                surface = QRectF(
                    surface.left() + (surface.width() - fittedWidth) / 2.0,
                    surface.top() + (surface.height() - fittedHeight) / 2.0,
                    fittedWidth, fittedHeight);
            }
        }
    }

    if (m_bottomSubtitleProtected && surface.height() > 0.0) {
        surface.setHeight(surface.height() * kSubtitleProtectRatio);
    }
    return surface;
}

qreal NativeDanmakuOverlay::laneTop(const ScheduledItem &item) const
{
    const QRectF surfaceRect = effectiveSurfaceRect();
    if (item.type == ItemType::Bottom) {
        return surfaceRect.bottom() - m_bottomMargin -
               (item.laneIndex + 1) * m_lineHeight;
    }
    return surfaceRect.top() + m_topMargin + item.laneIndex * m_lineHeight;
}

qreal NativeDanmakuOverlay::spriteLogicalWidth(const QPixmap &sprite) const
{
    return sprite.width() / std::max<qreal>(1.0, sprite.devicePixelRatio());
}

QString NativeDanmakuOverlay::spriteCacheKey(const ScheduledItem &item) const
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7")
        .arg(item.text)
        .arg(item.fontSize)
        .arg(item.fontWeight)
        .arg(item.color.rgba())
        .arg(QString::number(m_outlineSize, 'f', 2))
        .arg(QString::number(m_shadowOffset, 'f', 2))
        .arg(QFont().family());
}

const QPixmap *NativeDanmakuOverlay::spriteForItem(const ScheduledItem &item)
{
    const QString key = spriteCacheKey(item);
    auto it = m_spriteCache.find(key);
    if (it != m_spriteCache.end()) {
        return &it.value();
    }

    QFont font;
    font.setFamily(QFont().family());
    font.setPixelSize(item.fontSize);
    font.setWeight(static_cast<QFont::Weight>(item.fontWeight));
    const QFontMetricsF metrics(font);
    QRectF textRect = metrics.tightBoundingRect(item.text);
    if (textRect.width() <= 0.0 || textRect.height() <= 0.0) {
        textRect = QRectF(0.0, -metrics.ascent(), item.textWidth, metrics.height());
    }

    const qreal width = std::ceil(item.visualWidth);
    const qreal height = std::ceil(metrics.height() + m_spritePadding * 2.0);
    const qreal dpr = std::max<qreal>(1.0, devicePixelRatioF());

    QPixmap sprite(static_cast<int>(std::ceil(width * dpr)),
                   static_cast<int>(std::ceil(height * dpr)));
    sprite.setDevicePixelRatio(dpr);
    sprite.fill(Qt::transparent);

    QPainter painter(&sprite);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal baselineX = m_spritePadding - textRect.left();
    const qreal baselineY = m_spritePadding + metrics.ascent();

    QPainterPath textPath;
    textPath.addText(QPointF(baselineX, baselineY), font, item.text);

    if (m_shadowOffset > 0.0) {
        QPainterPath shadowPath(textPath);
        shadowPath.translate(m_shadowOffset, m_shadowOffset);
        QColor shadowColor(0, 0, 0,
                           static_cast<int>(std::lround(item.color.alphaF() * 96.0)));
        painter.fillPath(shadowPath, shadowColor);
    }

    if (m_outlineSize > 0.0) {
        QColor outlineColor(0, 0, 0,
                            static_cast<int>(std::lround(item.color.alphaF() * 180.0)));
        QPen outlinePen(outlineColor, m_outlineSize * 2.0, Qt::SolidLine,
                        Qt::RoundCap, Qt::RoundJoin);
        painter.strokePath(textPath, outlinePen);
    }

    painter.fillPath(textPath, item.color);
    it = m_spriteCache.insert(key, sprite);
    return &it.value();
}

void NativeDanmakuOverlay::handleControllerPropertyChanged(const QString &property,
                                                           const QVariant &value)
{
    if (property == QLatin1String("speed")) {
        const qint64 positionMs = currentPositionMs();
        m_playbackSpeed = qBound(0.1, value.toDouble(), 16.0);
        refreshBasePosition(positionMs);
    } else if (property == QLatin1String("video-params/dw") ||
               property == QLatin1String("video-params/dh") ||
               property == QLatin1String("width") ||
               property == QLatin1String("height")) {
        requestScheduleRebuild(false, false);
    }
}
