#ifndef NATIVEDANMAKULAYOUT_H
#define NATIVEDANMAKULAYOUT_H

#include <QFont>
#include <QImage>
#include <QRectF>
#include <QVector>
#include <array>
#include <atomic>
#include <memory>
#include <models/danmaku/danmakumodels.h>

namespace NativeDanmakuLayout {

enum class Motion { Scroll, Reverse, Top, Bottom };

struct Sprite {
    QString text;
    QFont font;
    QColor color;
    QSizeF size;
    qreal baseline = 0.0;
    qreal leftBearing = 0.0;
    qreal padding = 0.0;
    qreal outline = 0.0;
    qreal shadow = 0.0;
};

struct Item {
    Motion motion = Motion::Scroll;
    qreal startMs = 0.0;
    qreal endMs = 0.0;
    qreal xFrom = 0.0;
    qreal xTo = 0.0;
    qreal y = 0.0;
    int sprite = 0;
    int source = -1;
    int row = 0;
    int span = 1;
};

struct Source {
    Motion motion = Motion::Scroll;
    qreal requestedMs = 0.0;
    int sprite = 0;
};

struct Schedule {
    QVector<Item> items;
    QVector<Sprite> sprites;
    QVector<Source> sources;
    QFont baseFont;
    QRectF surface;
    qreal maxDurationMs = 0.0;
    int filtered = 0;
    int crowded = 0;
    int unsupported = 0;
    
    std::array<int, 4> inputByMotion{};
    std::array<int, 4> filteredByMotion{};
    std::array<int, 4> scheduledByMotion{};
    std::array<int, 4> crowdedByMotion{};
    qreal rowHeight = 0.0;
    qreal velocity = 0.0;
    qreal gap = 0.0;
    qreal edgePadding = 0.0;
    qreal validFromMs = 0.0;
    int rows = 0;
    int allowedRows = 0;
    int areaPercent = 100;
    int density = 100;
};

using Cancellation = std::shared_ptr<std::atomic_bool>;

bool optionsEqual(const DanmakuRenderOptions &a, const DanmakuRenderOptions &b);
Schedule build(QList<DanmakuComment> comments, DanmakuRenderOptions options,
               QRectF surface, QFont font, Cancellation cancelled, QSizeF motionReferenceSize);
QImage rasterize(const Sprite &sprite, qreal dpr);


Schedule reflow(const Schedule &previous, QRectF surface, qreal positionMs,
                Cancellation cancelled);

} 
#endif
