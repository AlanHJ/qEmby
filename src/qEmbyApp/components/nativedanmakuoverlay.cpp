#include "nativedanmakuoverlay.h"

#include "mpvcontroller.h"
#include "mpvwidget.h"
#include <QDebug>
#include <QEvent>
#include <QPainter>
#include <QScreen>
#include <QtConcurrentRun>
#include <algorithm>
#include <cmath>
#include <utility>

NativeDanmakuOverlay::NativeDanmakuOverlay(MpvWidget *widget, QObject *parent)
    : QObject(parent), m_widget(widget), m_controller(widget ? widget->controller() : nullptr)
{
    if (m_widget) {
        m_videoPresented = m_widget->hasPresentedVideo();
        connect(m_widget, &MpvWidget::videoPresentationChanged, this, [this](bool presented) {
            m_videoPresented = presented;
            syncPlaybackClock(presented);
        });
        m_widget->setNativeDanmakuOverlay(this);
        m_widget->installEventFilter(this);
        
        
        connect(m_widget, &QOpenGLWidget::frameSwapped, this,
                &NativeDanmakuOverlay::requestNextFrame, Qt::DirectConnection);
    }

    m_rebuildTimer.setSingleShot(true);
    m_rebuildTimer.setInterval(48);
    connect(&m_rebuildTimer, &QTimer::timeout, this, &NativeDanmakuOverlay::startScheduleBuild);
    m_frameWatchdog.setInterval(100);
    connect(&m_frameWatchdog, &QTimer::timeout, this, [this]() {
        if (shouldAnimate() && (!m_lastPaint.isValid() || m_lastPaint.elapsed() >= 100)) {
            update(); 
        }
    });
    m_prefetchTimer.setInterval(40);
    connect(&m_prefetchTimer, &QTimer::timeout, this, [this]() {
        if (m_prefetchTickClock.isValid()) {
            m_maxGuiTickGapMs = std::max(m_maxGuiTickGapMs,
                m_prefetchTickClock.nsecsElapsed() / 1000000.0);
        }
        m_prefetchTickClock.start();
        prepareSprites();
    });

    connect(&m_layoutWatcher, &QFutureWatcher<NativeDanmakuLayout::Schedule>::finished,
            this, [this]() {
        m_layoutInFlight = false;
        if (m_layoutGeneration != m_generation) {
            if (m_scheduleDirty && m_visibleRequested) {
                m_rebuildTimer.start();
            }
            return;
        }
        m_schedule = m_layoutWatcher.result();
        if (!m_layoutIsReflow) {
            m_spriteCache.clear();
            m_failedSprites.clear();
            m_spriteDpr = devicePixelRatioF();
        }
        updateActiveItems(m_clock.positionMs(), true);
        qInfo() << "[Danmaku][Native] Schedule ready"
                << "| input:" << m_comments.size() << "| scheduled:" << m_schedule.items.size()
                << "| sprites:" << m_schedule.sprites.size() << "| filtered:" << m_schedule.filtered
                << "| crowded:" << m_schedule.crowded << "| unsupported:" << m_schedule.unsupported
                << "| surface:" << m_schedule.surface << "| dpr:" << m_spriteDpr;
        qDebug() << "[Danmaku][Native] Layout applied | geometryOnly:" << m_layoutIsReflow
                 << "| rows:" << m_schedule.rows << "| allowedRows:" << m_schedule.allowedRows
                 << "| independentTypeLanes:" << true
                 << "| velocityPxPerSecond:" << m_schedule.velocity * 1000.0
                 << "| motionReferenceSize:" << m_motionReferenceSize
                 << "| speedScale:" << m_options.speedScale
                 << "| elapsedMs:" << m_layoutClock.elapsed()
                 << "| preservedCacheKiB:" << m_spriteCache.totalCost();
        const std::array<const char *, 4> motionNames{"scroll", "reverse", "top", "bottom"};
        const std::array<bool, 4> hiddenTypes{m_options.hideScroll, m_options.hideScroll,
                                             m_options.hideTop, m_options.hideBottom};
        for (int i = 0; i < 4; ++i) {
            qDebug() << "[Danmaku][Native] Layout type | motion:" << motionNames[i]
                     << "| hidden:" << hiddenTypes[i]
                     << "| input:" << m_schedule.inputByMotion[i]
                     << "| filtered:" << m_schedule.filteredByMotion[i]
                     << "| scheduled:" << m_schedule.scheduledByMotion[i]
                     << "| crowded:" << m_schedule.crowdedByMotion[i];
        }
        if (m_schedule.surface != effectiveSurfaceRect() || m_clock.positionMs() < m_schedule.validFromMs) {
            requestGeometryReflow(m_clock.positionMs() < m_schedule.validFromMs);
        }
        updatePresentation();
        prepareSprites();
    });
    connect(&m_spriteWatcher, &QFutureWatcher<QVector<PreparedSprite>>::finished,
            this, [this]() {
        m_spriteInFlight = false;
        if (m_spriteGeneration == m_generation) {
            if (m_spriteBatchClock.isValid()) {
                m_maxSpriteBatchMs = std::max(m_maxSpriteBatchMs,
                    m_spriteBatchClock.nsecsElapsed() / 1000000.0);
            }
            QElapsedTimer installTime;
            installTime.start();
            const auto prepared = m_spriteWatcher.result();
            for (const auto &sprite : prepared) {
                if (sprite.image.isNull()) {
                    m_failedSprites.insert(sprite.id);
                    qWarning() << "[Danmaku][Native] Sprite allocation failed | id:" << sprite.id;
                    continue;
                }
                const qint64 kib = (sprite.image.sizeInBytes() + 1023) / 1024;
                if (kib > m_spriteCache.maxCost()) {
                    m_failedSprites.insert(sprite.id);
                    qWarning() << "[Danmaku][Native] Sprite exceeds cache budget | id:" << sprite.id;
                    continue;
                }
                m_spriteCache.insert(sprite.id, new QPixmap(QPixmap::fromImage(sprite.image)),
                                     static_cast<int>(std::max<qint64>(1, kib)));
            }
            m_maxSpriteInstallMs = std::max(m_maxSpriteInstallMs,
                installTime.nsecsElapsed() / 1000000.0);
            
            if (!shouldAnimate()) {
                update();
            }
        }
        
        QTimer::singleShot(0, this, &NativeDanmakuOverlay::prepareSprites);
    });

    if (!m_controller) {
        return;
    }
    connect(m_controller, &MpvController::positionChanged, this, [this](double seconds) {
        synchronizePosition(seconds);
    });
    connect(m_controller, &MpvController::playbackStarting, this, [this]() {
        m_clock.reset(0.0);
        m_frozenFramePositionMs = -1.0;
        resetFrameTiming();
        m_anchorOnNextPosition = true;
        updateActiveItems(0.0, true);
    });
    connect(m_controller, &MpvController::playbackStateChanged, this, [this](bool paused) {
        m_paused = paused;
        syncPlaybackClock(true);
    });
    connect(m_controller, &MpvController::propertyChanged, this,
            &NativeDanmakuOverlay::handleControllerPropertyChanged);
    connect(m_controller, &MpvController::fileLoaded, this, [this]() {
        m_ended = false;
        syncPlaybackClock(true);
    });
    connect(m_controller, &MpvController::endOfFile, this, [this](const QString &) {
        m_ended = true;
        syncPlaybackClock();
    });
    connect(m_controller, &QObject::destroyed, this, [this]() {
        m_ended = true;
        syncPlaybackClock();
    });
    m_paused = m_controller->getProperty(QStringLiteral("pause")).toBool();
    m_buffering = m_controller->getProperty(QStringLiteral("paused-for-cache")).toBool();
    m_seeking = m_controller->getProperty(QStringLiteral("seeking")).toBool();
    m_clock.setSpeed(m_controller->getProperty(QStringLiteral("speed")).toDouble());
    m_controller->observeProperty(QStringLiteral("video-out-params"), MPV_FORMAT_NODE);
    m_controller->observeProperty(QStringLiteral("keepaspect"), MPV_FORMAT_FLAG);
    m_controller->observeProperty(QStringLiteral("panscan"), MPV_FORMAT_DOUBLE);
    m_controller->observeProperty(QStringLiteral("video-unscaled"), MPV_FORMAT_STRING);
    syncPlaybackClock(true);
}

NativeDanmakuOverlay::~NativeDanmakuOverlay()
{
    if (m_cancelled) {
        m_cancelled->store(true, std::memory_order_relaxed);
    }
    
}

void NativeDanmakuOverlay::invalidateWork()
{
    ++m_generation;
    if (m_cancelled) {
        m_cancelled->store(true, std::memory_order_relaxed);
    }
}

void NativeDanmakuOverlay::setRenderOptions(DanmakuRenderOptions options)
{
    if (NativeDanmakuLayout::optionsEqual(m_options, options)) {
        return;
    }
    
    auto previous = m_options;
    previous.opacity = options.opacity;
    const bool onlyOpacity = NativeDanmakuLayout::optionsEqual(previous, options);
    m_options = std::move(options);
    if (!onlyOpacity) {
        requestScheduleRebuild();
    }
    updatePresentation();
}

void NativeDanmakuOverlay::setComments(QList<DanmakuComment> comments)
{
    m_comments = std::move(comments);
    m_schedule = {};
    m_activeIndexes.clear();
    m_spriteCache.clear();
    requestScheduleRebuild();
    updatePresentation();
}

void NativeDanmakuOverlay::clearDanmaku(bool resetMotionReference)
{
    invalidateWork();
    m_rebuildTimer.stop();
    m_comments.clear();
    if (resetMotionReference) {
        m_motionReferenceSize = {};
    }
    m_schedule = {};
    m_activeIndexes.clear();
    m_spriteCache.clear();
    m_failedSprites.clear();
    m_scheduleDirty = false;
    m_lastPositionMs = -1.0;
    m_nextIndex = 0;
    updatePresentation();
}

bool NativeDanmakuOverlay::hasComments() const
{
    return !m_comments.isEmpty();
}

void NativeDanmakuOverlay::setDanmakuVisible(bool visible)
{
    if (m_visibleRequested == visible && !m_scheduleDirty) {
        return;
    }
    m_visibleRequested = visible;
    if (visible && m_scheduleDirty) {
        m_rebuildTimer.start(0);
    }
    updatePresentation();
}

void NativeDanmakuOverlay::setBottomSubtitleProtected(bool enabled)
{
    if (m_bottomSubtitleProtected != enabled) {
        m_bottomSubtitleProtected = enabled;
        requestGeometryReflow();
    }
}

void NativeDanmakuOverlay::invalidateGraphicsCache()
{
    
    
    m_spriteCache.clear();
    m_graphicsAvailable = true;
    m_lastPaint.invalidate();
    resetFrameTiming();
    syncFramePump();
    qInfo() << "[Danmaku][Native] Shared video surface | refreshHz:"
            << 1000.0 / refreshIntervalMs() << "| frameTiming: presentation";
}

void NativeDanmakuOverlay::setGraphicsAvailable(bool available)
{
    m_graphicsAvailable = available;
    syncFramePump();
}

qreal NativeDanmakuOverlay::beginFrame()
{
    if (!shouldAnimate()) {
        return m_frozenFramePositionMs >= 0.0 ? m_frozenFramePositionMs : m_clock.positionMs();
    }
    
    
    const qreal offset = m_frameClock.nextFrameOffsetMs(refreshIntervalMs());
    return std::max(m_lastFramePositionMs, m_clock.positionMs(offset));
}

void NativeDanmakuOverlay::paint(QPainter &painter, qreal position)
{
    if (!m_videoPresented || !m_visibleRequested || !m_options.enabled || m_schedule.items.isEmpty()) {
        return;
    }
    if (!qFuzzyCompare(m_spriteDpr, devicePixelRatioF())) {
        m_spriteDpr = devicePixelRatioF();
        requestScheduleRebuild();
        return;
    }
    updateActiveItems(position);
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setClipRect(m_schedule.surface.intersected(effectiveSurfaceRect()));
    painter.setOpacity(std::clamp(m_options.opacity, 0.0, 1.0));
    for (int index : std::as_const(m_activeIndexes)) {
        const auto &item = m_schedule.items.at(index);
        const QPixmap *sprite = m_spriteCache.object(item.sprite);
        if (!sprite) {
            ++m_spriteMisses;
            continue;
        }
        const qreal progress = std::clamp((position - item.startMs) /
            (item.endMs - item.startMs), 0.0, 1.0);
        const qreal x = item.xFrom + (item.xTo - item.xFrom) * progress;
        
        painter.drawPixmap(QPointF(x, item.y), *sprite);
    }
    painter.restore();
    if (shouldAnimate()) {
        if (m_lastFramePositionMs >= 0.0) {
            
            
            const qreal step = (position - m_lastFramePositionMs) / m_clock.speed();
            m_minFrameStepMs = m_minFrameStepMs < 0.0 ? step : std::min(m_minFrameStepMs, step);
            m_maxFrameStepMs = std::max(m_maxFrameStepMs, step);
        }
        m_lastFramePositionMs = position;
        if (m_lastPaint.isValid()) {
            const qreal gap = m_lastPaint.nsecsElapsed() / 1000000.0;
            m_maxFrameGapMs = std::max(m_maxFrameGapMs, gap);
            if (gap > refreshIntervalMs() * 1.8) {
                ++m_lateFrames;
            }
        }
        m_lastPaint.start();
        ++m_frameCount;
        if (!m_statisticsClock.isValid()) {
            m_statisticsClock.start();
        } else if (m_statisticsClock.elapsed() >= 2000) {
            qDebug() << "[Danmaku][Native] Frame cadence | frames:" << m_frameCount
                     << "| elapsedMs:" << m_statisticsClock.elapsed()
                     << "| late:" << m_lateFrames << "| maxGapMs:" << m_maxFrameGapMs
                     << "| swaps:" << m_swapCount << "| maxSwapGapMs:" << m_maxSwapGapMs
                     << "| refreshHz:" << 1000.0 / refreshIntervalMs()
                     << "| minStepMs:" << m_minFrameStepMs << "| maxStepMs:" << m_maxFrameStepMs
                     << "| maxPhaseErrorMs:" << m_maxPhaseErrorMs
                     << "| maxGuiTickGapMs:" << m_maxGuiTickGapMs
                     << "| maxSpriteInstallMs:" << m_maxSpriteInstallMs
                     << "| maxSpriteBatchMs:" << m_maxSpriteBatchMs
                     << "| missingSprites:" << m_spriteMisses
                     << "| active:" << m_activeIndexes.size()
                     << "| cacheKiB:" << m_spriteCache.totalCost();
            m_statisticsClock.restart();
            m_frameCount = m_lateFrames = m_spriteMisses = 0;
            m_swapCount = 0;
            m_maxSwapGapMs = 0.0;
            m_maxGuiTickGapMs = m_maxSpriteInstallMs = 0.0;
            m_maxSpriteBatchMs = 0.0;
            m_maxFrameGapMs = 0.0;
            m_minFrameStepMs = -1.0;
            m_maxFrameStepMs = m_maxPhaseErrorMs = 0.0;
        }
    }
}

bool NativeDanmakuOverlay::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_widget) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::ScreenChangeInternal) {
            if (event->type() == QEvent::ScreenChangeInternal) resetFrameTiming();
            if (!qFuzzyCompare(m_spriteDpr, devicePixelRatioF())) requestScheduleRebuild();
            else requestGeometryReflow();
        } else if (event->type() == QEvent::FontChange) {
            const QFont preparedFont = m_layoutInFlight ? m_layoutBaseFont : m_schedule.baseFont;
            if (m_widget->font() != preparedFont) requestScheduleRebuild();
        } else if (event->type() == QEvent::Show || event->type() == QEvent::Hide ||
                   event->type() == QEvent::WindowStateChange) {
            resetFrameTiming();
            QTimer::singleShot(0, this, [this]() {
                updateActiveItems(m_clock.positionMs(), true);
                syncFramePump();
                prepareSprites();
            });
        }
    }
    return QObject::eventFilter(watched, event);
}

void NativeDanmakuOverlay::requestScheduleRebuild()
{
    invalidateWork();
    m_geometryOnly = false;
    m_resetReflowTimeline = false;
    m_scheduleDirty = true;
    if (m_visibleRequested) {
        m_rebuildTimer.start(48);
    }
}

void NativeDanmakuOverlay::requestGeometryReflow(bool resetTimeline)
{
    
    
    if ((m_layoutInFlight && !m_layoutIsReflow) || (m_scheduleDirty && !m_geometryOnly)) return;
    if (m_schedule.sources.isEmpty()) {
        requestScheduleRebuild();
        return;
    }
    if (!resetTimeline && !m_scheduleDirty && !m_layoutInFlight &&
        m_schedule.surface == effectiveSurfaceRect()) return;
    invalidateWork();
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    m_geometryOnly = true;
    m_resetReflowTimeline = m_resetReflowTimeline || resetTimeline ||
                           (m_layoutInFlight && m_layoutResetsTimeline);
    m_scheduleDirty = true;
    if (m_visibleRequested) m_rebuildTimer.start(48);
}

void NativeDanmakuOverlay::startScheduleBuild()
{
    if (!m_scheduleDirty || !m_visibleRequested || m_layoutInFlight) {
        return;
    }
    m_scheduleDirty = false;
    m_layoutInFlight = true;
    m_layoutClock.start();
    m_layoutGeneration = m_generation;
    m_layoutIsReflow = m_geometryOnly && !m_schedule.sources.isEmpty();
    m_layoutResetsTimeline = m_layoutIsReflow && m_resetReflowTimeline;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    const auto comments = m_comments;
    const auto options = m_options;
    const auto surface = effectiveSurfaceRect();
    const auto baseFont = m_widget ? m_widget->font() : QFont();
    m_layoutBaseFont = baseFont;
    const auto cancelled = m_cancelled;
    if (m_layoutIsReflow) {
        const auto previous = m_schedule;
        const qreal position = m_resetReflowTimeline ? -1.0 : m_clock.positionMs();
        m_resetReflowTimeline = false;
        auto future = QtConcurrent::run([previous, surface, position, cancelled]() {
            return NativeDanmakuLayout::reflow(previous, surface, position, cancelled);
        });
        m_layoutWatcher.setFuture(future);
        return;
    }
    
    if (m_motionReferenceSize.isEmpty() && !surface.isEmpty() && options.enabled && !comments.isEmpty()) {
        m_motionReferenceSize = surface.size();
    }
    const auto motionReferenceSize = m_motionReferenceSize;
    auto future = QtConcurrent::run([comments, options, surface, baseFont, cancelled, motionReferenceSize]() {
        return NativeDanmakuLayout::build(comments, options, surface, baseFont, cancelled, motionReferenceSize);
    });
    m_layoutWatcher.setFuture(future);
}

void NativeDanmakuOverlay::updatePresentation()
{
    syncFramePump();
    
    if (m_widget) {
        m_widget->update();
    }
}

void NativeDanmakuOverlay::updateActiveItems(qreal position, bool rebuild)
{
    const auto &items = m_schedule.items;
    if (rebuild || m_lastPositionMs < 0.0 || position < m_lastPositionMs ||
        position - m_lastPositionMs > 1500.0) {
        m_activeIndexes.clear();
        const auto first = std::lower_bound(items.cbegin(), items.cend(),
            position - m_schedule.maxDurationMs, [](const auto &item, qreal time) {
                return item.startMs < time;
            });
        m_nextIndex = static_cast<int>(first - items.cbegin());
    }
    while (m_nextIndex < items.size() && items.at(m_nextIndex).startMs <= position) {
        if (items.at(m_nextIndex).endMs > position) {
            m_activeIndexes.append(m_nextIndex);
        }
        ++m_nextIndex;
    }
    m_activeIndexes.erase(std::remove_if(m_activeIndexes.begin(), m_activeIndexes.end(),
        [&items, position](int index) { return items.at(index).endMs <= position; }),
        m_activeIndexes.end());
    m_lastPositionMs = position;
}

void NativeDanmakuOverlay::prepareSprites()
{
    if (!m_visibleRequested || !m_options.enabled || !m_graphicsAvailable || !isVisible() ||
        (m_scheduleDirty && !m_geometryOnly) || (m_layoutInFlight && !m_layoutIsReflow) ||
        m_spriteInFlight || m_schedule.items.isEmpty()) {
        return;
    }
    const qreal position = m_clock.positionMs();
    
    
    const auto &items = m_schedule.items;
    const auto first = std::lower_bound(items.cbegin(), items.cend(),
        position - m_schedule.maxDurationMs, [](const auto &item, qreal time) {
            return item.startMs < time;
        });
    const auto next = std::upper_bound(first, items.cend(), position,
        [](qreal time, const auto &item) { return time < item.startMs; });
    QVector<int> ids;
    QSet<int> selected;
    qreal workingBytes = 0.0;
    qreal batchBytes = 0.0;
    const auto select = [&](int index) {
        const int id = m_schedule.items.at(index).sprite;
        if (selected.contains(id)) {
            return;
        }
        selected.insert(id);
        const auto size = m_schedule.sprites.at(id).size;
        const qreal bytes = std::ceil(size.width() * m_spriteDpr) *
                            std::ceil(size.height() * m_spriteDpr) * 4.0;
        workingBytes += bytes;
        if (!m_spriteCache.contains(id) && !m_failedSprites.contains(id)) {
            batchBytes += bytes;
            ids.append(id);
        }
    };
    for (auto it = first; it != next; ++it) {
        if (it->endMs > position) select(static_cast<int>(it - items.cbegin()));
        if (ids.size() >= 8 || batchBytes >= 8.0 * 1024 * 1024) {
            break;
        }
    }
    
    const qreal ahead = position + 2000.0 * m_clock.speed();
    for (int i = static_cast<int>(next - items.cbegin()); i < items.size() && ids.size() < 8 &&
         batchBytes < 8.0 * 1024 * 1024; ++i) {
        
        
        if (m_schedule.items.at(i).startMs > ahead || workingBytes >= 16.0 * 1024 * 1024 ||
            selected.size() >= 256) {
            break;
        }
        select(i);
    }
    if (ids.isEmpty()) {
        return;
    }
    QVector<NativeDanmakuLayout::Sprite> sprites;
    for (int id : std::as_const(ids)) {
        sprites.append(m_schedule.sprites.at(id));
    }
    const qreal dpr = m_spriteDpr;
    const auto cancelled = m_cancelled;
    m_spriteGeneration = m_generation;
    m_spriteInFlight = true;
    m_spriteBatchClock.start();
    auto future = QtConcurrent::run([ids, sprites, dpr, cancelled]() {
        QVector<PreparedSprite> prepared;
        for (int i = 0; i < ids.size(); ++i) {
            if (cancelled && cancelled->load(std::memory_order_relaxed)) {
                break;
            }
            prepared.append({ids.at(i), NativeDanmakuLayout::rasterize(sprites.at(i), dpr)});
        }
        return prepared;
    });
    m_spriteWatcher.setFuture(future);
}

void NativeDanmakuOverlay::synchronizePosition(double seconds, bool discontinuity)
{
    if (!std::isfinite(seconds)) {
        return;
    }
    const qreal previousPosition = m_clock.positionMs();
    if (m_seeking || std::abs(seconds * 1000.0 - previousPosition) > 500.0) {
        
        m_frozenFramePositionMs = -1.0;
    }
    discontinuity = discontinuity || m_anchorOnNextPosition;
    m_anchorOnNextPosition = false;
    const bool reset = m_clock.synchronize(seconds * 1000.0, discontinuity || m_seeking);
    if (reset) {
        resetFrameTiming();
        const bool timelineResetPending = (m_scheduleDirty && m_resetReflowTimeline) ||
            (m_layoutInFlight && m_layoutResetsTimeline);
        if (!timelineResetPending && (seconds * 1000.0 < m_schedule.validFromMs ||
            (m_layoutInFlight && m_layoutIsReflow && std::abs(seconds * 1000.0 - previousPosition) > 500.0))) {
            requestGeometryReflow(true);
        }
        if (m_visibleRequested && (discontinuity || std::abs(seconds * 1000.0 - previousPosition) > 50.0)) {
            qDebug() << "[Danmaku][Native] Clock reanchored | deltaMs:"
                     << seconds * 1000.0 - previousPosition << "| forced:" << discontinuity
                     << "| seeking:" << m_seeking << "| running:" << m_clock.isRunning();
        }
        updateActiveItems(m_clock.positionMs(), true);
        if (m_visibleRequested) {
            prepareSprites();
            update();
        }
    }
}

void NativeDanmakuOverlay::syncPlaybackClock(bool anchor)
{
    const bool wasRunning = m_clock.isRunning();
    m_clock.setRunning(m_videoPresented && !m_paused && !m_buffering && !m_seeking && !m_ended && m_controller);
    if (wasRunning && !m_clock.isRunning() && (m_paused || m_buffering)) {
        
        
        m_frozenFramePositionMs = m_lastFramePositionMs;
    }
    if (m_clock.isRunning() || m_seeking || m_ended || !m_videoPresented || !m_controller) {
        m_frozenFramePositionMs = -1.0;
    }
    if (anchor || wasRunning != m_clock.isRunning()) resetFrameTiming();
    
    
    
    m_anchorOnNextPosition = m_anchorOnNextPosition || anchor;
    qDebug() << "[Danmaku][Native] Clock state | paused:" << m_paused
             << "| buffering:" << m_buffering << "| seeking:" << m_seeking
             << "| ended:" << m_ended << "| speed:" << m_clock.speed();
    syncFramePump();
    update();
}

void NativeDanmakuOverlay::handleControllerPropertyChanged(const QString &property,
                                                          const QVariant &value)
{
    if (property == QLatin1String("speed")) {
        m_clock.setSpeed(value.toDouble());
        resetFrameTiming();
        qDebug() << "[Danmaku][Native] Playback speed:" << m_clock.speed();
    } else if (property == QLatin1String("paused-for-cache")) {
        m_buffering = value.toBool();
        syncPlaybackClock(true);
    } else if (property == QLatin1String("seeking")) {
        m_seeking = value.toBool();
        syncPlaybackClock(true);
    } else if (property == QLatin1String("eof-reached")) {
        m_ended = value.toBool();
        syncPlaybackClock(true);
    } else if (property == QLatin1String("video-out-params") ||
               property == QLatin1String("keepaspect") ||
               property == QLatin1String("panscan") ||
               property == QLatin1String("video-unscaled")) {
        refreshVideoGeometry(property, value);
    }
}

void NativeDanmakuOverlay::refreshVideoGeometry(const QString &property, const QVariant &value)
{
    
    
    QSizeF video = m_videoSize;
    bool keepAspect = m_keepAspect;
    qreal panscan = m_panscan;
    bool unscaled = m_videoUnscaled;
    if (property == QLatin1String("video-out-params")) {
        const QVariantMap params = value.toMap();
        video = QSizeF(params.value(QStringLiteral("dw")).toDouble(),
                       params.value(QStringLiteral("dh")).toDouble());
        if (video.isEmpty()) {
            video = QSizeF(params.value(QStringLiteral("w")).toDouble(),
                           params.value(QStringLiteral("h")).toDouble());
        }
    } else if (property == QLatin1String("keepaspect")) {
        keepAspect = value.toBool();
    } else if (property == QLatin1String("panscan")) {
        panscan = value.toDouble();
    } else if (property == QLatin1String("video-unscaled")) {
        const QString mode = value.toString();
        unscaled = mode == QLatin1String("yes") || mode == QLatin1String("true");
    }
    if (m_videoSize != video || m_keepAspect != keepAspect ||
        m_panscan != panscan || m_videoUnscaled != unscaled) {
        m_videoSize = video;
        m_keepAspect = keepAspect;
        m_panscan = panscan;
        m_videoUnscaled = unscaled;
        requestGeometryReflow();
    }
}

QRectF NativeDanmakuOverlay::effectiveSurfaceRect() const
{
    const QRectF viewport = m_widget ? QRectF(m_widget->rect()) : QRectF();
    QRectF surface(viewport);
    if (m_keepAspect && !m_videoSize.isEmpty() && !surface.isEmpty()) {
        QSizeF fitted;
        if (m_videoUnscaled) {
            fitted = m_videoSize / devicePixelRatioF();
        } else {
            const QSizeF fit = m_videoSize.scaled(surface.size(), Qt::KeepAspectRatio);
            const QSizeF fill = m_videoSize.scaled(surface.size(), Qt::KeepAspectRatioByExpanding);
            fitted = fit + (fill - fit) * std::clamp(m_panscan, 0.0, 1.0);
        }
        surface = QRectF(QPointF((viewport.width() - fitted.width()) / 2.0,
                                (viewport.height() - fitted.height()) / 2.0), fitted).intersected(viewport);
    }
    if (m_bottomSubtitleProtected) {
        surface.setHeight(surface.height() * 0.85);
    }
    return surface;
}

bool NativeDanmakuOverlay::shouldAnimate() const
{
    return m_videoPresented && isActive() && m_clock.isRunning();
}

bool NativeDanmakuOverlay::isActive() const
{
    
    
    return m_graphicsAvailable && m_visibleRequested && m_options.enabled &&
           isVisible() && !m_schedule.items.isEmpty();
}

bool NativeDanmakuOverlay::isVisible() const
{
    return m_widget && m_widget->isVisible() && !m_widget->window()->isMinimized();
}

void NativeDanmakuOverlay::update()
{
    if (m_widget && isActive()) {
        m_widget->update();
    }
}

qreal NativeDanmakuOverlay::devicePixelRatioF() const
{
    return m_widget ? m_widget->devicePixelRatioF() : 1.0;
}

qreal NativeDanmakuOverlay::refreshIntervalMs() const
{
    const QScreen *display = m_widget ? m_widget->screen() : nullptr;
    const qreal hz = display ? display->refreshRate() : 60.0;
    return 1000.0 / std::clamp(hz > 0.0 ? hz : 60.0, 24.0, 360.0);
}

void NativeDanmakuOverlay::requestNextFrame()
{
    if (!shouldAnimate()) {
        return;
    }
    m_frameClock.frameSwapped(refreshIntervalMs());
    m_maxPhaseErrorMs = std::max(m_maxPhaseErrorMs, std::abs(m_frameClock.phaseErrorMs()));
    if (m_swapClock.isValid()) {
        m_maxSwapGapMs = std::max(m_maxSwapGapMs, m_swapClock.nsecsElapsed() / 1000000.0);
    }
    m_swapClock.start();
    ++m_swapCount;
    
    
    update();
}

void NativeDanmakuOverlay::resetFrameTiming()
{
    m_frameClock.reset();
    m_lastFramePositionMs = -1.0;
    m_minFrameStepMs = -1.0;
    m_maxFrameStepMs = m_maxPhaseErrorMs = 0.0;
}

void NativeDanmakuOverlay::syncFramePump()
{
    if (shouldAnimate()) {
        if (!m_frameWatchdog.isActive()) {
            resetFrameTiming();
            
            m_maxGuiTickGapMs = m_maxSpriteInstallMs = 0.0;
            m_maxSpriteBatchMs = 0.0;
            m_prefetchTickClock.start();
            m_frameWatchdog.start();
            update();
        }
    } else {
        m_frameWatchdog.stop();
        resetFrameTiming();
        m_lastPaint.invalidate();
        m_statisticsClock.invalidate();
        m_swapClock.invalidate();
        m_swapCount = 0;
        m_maxSwapGapMs = 0.0;
        m_maxGuiTickGapMs = m_maxSpriteInstallMs = 0.0;
        m_maxSpriteBatchMs = 0.0;
        m_frameCount = m_lateFrames = m_spriteMisses = 0;
        m_maxFrameGapMs = 0.0;
    }
    if (m_graphicsAvailable && isVisible() && m_visibleRequested && m_options.enabled) {
        if (!m_prefetchTimer.isActive()) {
            m_prefetchTickClock.start();
            m_prefetchTimer.start();
        }
    } else {
        m_prefetchTimer.stop();
        m_prefetchTickClock.invalidate();
    }
}
