#include "danmakuheatmaputils.h"
#include <QPainter>
#include <QTransform>

#include <algorithm>
#include <cmath>

void DanmakuHeatmapUtils::paint(QPainter &painter, const QPainterPath &path,
                               const QRectF &bounds, const QColor &color)
{
    if (path.isEmpty() || bounds.isEmpty()) return;
    QTransform transform;
    transform.translate(bounds.left(), bounds.top());
    transform.scale(bounds.width(), bounds.height());
    painter.fillPath(transform.map(path), color);
}

QPainterPath DanmakuHeatmapUtils::build(const QVector<qint64> &timesMs,
                                      double durationSeconds, int offsetMs)
{
    if (timesMs.isEmpty() || !std::isfinite(durationSeconds) || durationSeconds <= 0.0) return {};
    const int bins = static_cast<int>(std::clamp(std::ceil(durationSeconds / 10.0), 60.0, 360.0));
    QVector<double> counts(bins, 0.0);
    const double durationMs = durationSeconds * 1000.0;
    for (qint64 time : timesMs) {
        const double shifted = static_cast<double>(time) + offsetMs;
        if (shifted < 0.0 || shifted > durationMs) continue;
        const int bin = std::min(bins - 1, static_cast<int>(shifted / durationMs * bins));
        ++counts[bin];
    }
    QVector<double> heights(bins, 0.0);
    double peak = 0.0;
    for (int i = 0; i < bins; ++i) {
        
        double weight = 0.0;
        for (int j = std::max(0, i - 2); j <= std::min(bins - 1, i + 2); ++j) {
            const double w = 3 - std::abs(i - j);
            heights[i] += counts[j] * w;
            weight += w;
        }
        heights[i] /= weight;
        peak = std::max(peak, heights[i]);
    }
    if (peak <= 0.0) return {};
    QPainterPath path;
    path.moveTo(0.0, 1.0);
    QPointF previous(0.0, 1.0);
    for (int i = 0; i < bins; ++i) {
        
        const QPointF point((i + 0.5) / bins, 1.0 - std::sqrt(heights[i] / peak));
        const double mid = (previous.x() + point.x()) / 2.0;
        path.cubicTo(QPointF(mid, previous.y()), QPointF(mid, point.y()), point);
        previous = point;
    }
    const double mid = (previous.x() + 1.0) / 2.0;
    path.cubicTo(QPointF(mid, previous.y()), QPointF(mid, 1.0), QPointF(1.0, 1.0));
    path.closeSubpath();
    return path;
}
