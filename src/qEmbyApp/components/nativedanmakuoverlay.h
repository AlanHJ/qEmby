#ifndef NATIVEDANMAKUOVERLAY_H
#define NATIVEDANMAKUOVERLAY_H

#include <QCache>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QObject>
#include <QPixmap>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include "../utils/danmakuclock.h"
#include "../utils/danmakuframeclock.h"
#include "../utils/nativedanmakulayout.h"

class MpvController;
class MpvWidget;
class QPainter;


class NativeDanmakuOverlay : public QObject
{
public:
    explicit NativeDanmakuOverlay(MpvWidget *widget, QObject *parent = nullptr);
    ~NativeDanmakuOverlay() override;

    void setRenderOptions(DanmakuRenderOptions options);
    void setComments(QList<DanmakuComment> comments);
    void clearDanmaku(bool resetMotionReference = false);
    bool hasComments() const;
    void setDanmakuVisible(bool visible);
    void setBottomSubtitleProtected(bool enabled);
    bool isActive() const;
    qreal beginFrame();
    void paint(QPainter &painter, qreal positionMs);
    void invalidateGraphicsCache();
    void setGraphicsAvailable(bool available);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct PreparedSprite {
        int id = -1;
        QImage image;
    };
    void requestScheduleRebuild();
    void requestGeometryReflow(bool resetTimeline = false);
    void startScheduleBuild();
    void invalidateWork();
    void updatePresentation();
    void updateActiveItems(qreal positionMs, bool rebuild = false);
    void prepareSprites();
    void syncPlaybackClock(bool anchor = false);
    void synchronizePosition(double seconds, bool discontinuity = false);
    void handleControllerPropertyChanged(const QString &property, const QVariant &value);
    void refreshVideoGeometry(const QString &property, const QVariant &value);
    QRectF effectiveSurfaceRect() const;
    void syncFramePump();
    void resetFrameTiming();
    void requestNextFrame();
    bool shouldAnimate() const;
    qreal refreshIntervalMs() const;
    void update();
    bool isVisible() const;
    qreal devicePixelRatioF() const;

    QPointer<MpvWidget> m_widget;
    QPointer<MpvController> m_controller;
    DanmakuClock m_clock;
    DanmakuFrameClock m_frameClock;
    DanmakuRenderOptions m_options;
    QList<DanmakuComment> m_comments;
    NativeDanmakuLayout::Schedule m_schedule;
    
    QSizeF m_motionReferenceSize;
    NativeDanmakuLayout::Cancellation m_cancelled;
    QFutureWatcher<NativeDanmakuLayout::Schedule> m_layoutWatcher;
    QFutureWatcher<QVector<PreparedSprite>> m_spriteWatcher;
    QCache<int, QPixmap> m_spriteCache{64 * 1024}; 
    QSet<int> m_failedSprites;
    QVector<int> m_activeIndexes;
    QTimer m_rebuildTimer;
    QTimer m_frameWatchdog;
    QTimer m_prefetchTimer;
    QElapsedTimer m_lastPaint;
    QElapsedTimer m_statisticsClock;
    QElapsedTimer m_swapClock;
    QElapsedTimer m_prefetchTickClock;
    QElapsedTimer m_spriteBatchClock;
    QElapsedTimer m_layoutClock;
    int m_swapCount = 0;
    qreal m_maxSwapGapMs = 0.0;
    qreal m_maxGuiTickGapMs = 0.0;
    qreal m_maxSpriteInstallMs = 0.0;
    qreal m_maxSpriteBatchMs = 0.0;
    QSizeF m_videoSize;
    qreal m_panscan = 0.0;
    bool m_keepAspect = true;
    bool m_videoUnscaled = false;
    quint64 m_generation = 0;
    quint64 m_layoutGeneration = 0;
    quint64 m_spriteGeneration = 0;
    qreal m_spriteDpr = 1.0;
    qreal m_lastPositionMs = -1.0;
    int m_nextIndex = 0;
    int m_frameCount = 0;
    int m_lateFrames = 0;
    int m_spriteMisses = 0;
    qreal m_maxFrameGapMs = 0.0;
    qreal m_lastFramePositionMs = -1.0;
    qreal m_frozenFramePositionMs = -1.0;
    qreal m_minFrameStepMs = -1.0;
    qreal m_maxFrameStepMs = 0.0;
    qreal m_maxPhaseErrorMs = 0.0;
    bool m_visibleRequested = false;
    bool m_paused = true;
    bool m_buffering = false;
    bool m_seeking = false;
    bool m_ended = false;
    bool m_bottomSubtitleProtected = false;
    bool m_scheduleDirty = false;
    bool m_layoutInFlight = false;
    bool m_geometryOnly = false;
    bool m_layoutIsReflow = false;
    bool m_layoutResetsTimeline = false;
    bool m_resetReflowTimeline = false;
    QFont m_layoutBaseFont;
    bool m_spriteInFlight = false;
    bool m_graphicsAvailable = true;
    bool m_videoPresented = false;
    bool m_anchorOnNextPosition = true;
};

#endif 
