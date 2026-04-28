#ifndef NATIVEDANMAKUOVERLAY_H
#define NATIVEDANMAKUOVERLAY_H

#include <QElapsedTimer>
#include <QHash>
#include <QPixmap>
#include <QPointer>
#include <QSize>
#include <QVector>
#include <QWidget>

#include <models/danmaku/danmakumodels.h>

class MpvController;
class QPaintEvent;
class QResizeEvent;
class QTimer;
class QVariant;
class QRectF;

class NativeDanmakuOverlay : public QWidget
{
public:
    explicit NativeDanmakuOverlay(MpvController *controller,
                                  QWidget *parent = nullptr);

    void setRenderOptions(DanmakuRenderOptions options);
    void setComments(QList<DanmakuComment> comments);
    void clearDanmaku();
    bool hasComments() const;

    void setDanmakuVisible(bool visible);
    void setBottomSubtitleProtected(bool enabled);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    enum class ItemType {
        Scroll,
        Top,
        Bottom
    };

    struct ScheduledItem {
        ItemType type = ItemType::Scroll;
        qint64 startMs = 0;
        qint64 endMs = 0;
        int laneIndex = 0;
        int fontSize = 0;
        int fontWeight = 400;
        QColor color = QColor(Qt::white);
        QString text;
        qreal textWidth = 0.0;
        qreal visualWidth = 0.0;
    };

    void rebuildSchedule();
    void requestScheduleRebuild(bool immediate = false,
                                bool clearSpriteCache = false);
    void rebuildScheduleNow();
    void warmSpriteCacheAround(qreal positionMs);
    void rebuildActiveItems(qint64 positionMs);
    void updateActiveItems(qint64 positionMs);
    void clearScheduledState(bool clearSpriteCache = false);
    void syncFrameTimer();
    int targetFrameIntervalMs() const;
    void refreshBasePosition(qint64 positionMs);
    qreal currentPositionMsPrecise() const;
    qint64 currentPositionMs() const;
    QRectF effectiveSurfaceRect() const;
    qreal laneTop(const ScheduledItem &item) const;
    qreal spriteLogicalWidth(const QPixmap &sprite) const;
    QString spriteCacheKey(const ScheduledItem &item) const;
    const QPixmap *spriteForItem(const ScheduledItem &item);
    void handleControllerPropertyChanged(const QString &property,
                                         const QVariant &value);

    QPointer<MpvController> m_controller;
    QTimer *m_frameTimer = nullptr;
    QTimer *m_scheduleRebuildTimer = nullptr;
    QElapsedTimer m_positionClock;
    QList<DanmakuComment> m_comments;
    QList<DanmakuComment> m_sortedComments;
    DanmakuRenderOptions m_options;
    QVector<ScheduledItem> m_schedule;
    QVector<int> m_activeIndexes;
    QHash<QString, QPixmap> m_spriteCache;
    qint64 m_basePositionMs = 0;
    qint64 m_lastResolvedPositionMs = -1;
    int m_nextScheduleIndex = 0;
    double m_playbackSpeed = 1.0;
    qreal m_lineHeight = 0.0;
    qreal m_topMargin = 0.0;
    qreal m_bottomMargin = 0.0;
    qreal m_scrollStartPadding = 0.0;
    qreal m_scrollEndPadding = 0.0;
    qreal m_scrollLaneGapPadding = 0.0;
    qreal m_outlineSize = 0.0;
    qreal m_shadowOffset = 0.0;
    qreal m_spritePadding = 0.0;
    bool m_visibleRequested = false;
    bool m_playbackPaused = true;
    bool m_bottomSubtitleProtected = false;
    bool m_scheduleDirty = false;
    bool m_clearSpriteCacheOnNextRebuild = false;
};

#endif 
