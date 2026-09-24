#ifndef DANMAKUHEATMAPUTILS_H
#define DANMAKUHEATMAPUTILS_H

#include <QPainterPath>
#include <QVector>

class QPainter;
class QColor;

namespace DanmakuHeatmapUtils {

QPainterPath build(const QVector<qint64> &timesMs, double durationSeconds, int offsetMs);
void paint(QPainter &painter, const QPainterPath &path, const QRectF &bounds, const QColor &color);
}

#endif
