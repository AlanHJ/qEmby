#include "pageturnutils.h"

#include <QLinearGradient>
#include <QList>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QTransform>
#include <QtMath>
#include <algorithm>

namespace {

struct CurlVertex {
    QPointF position;
    qreal depth = 0.0;
};

struct CurlStrip {
    QTransform transform;
    QPainterPath surface;
    QRectF sourceRect;
    qreal depth = 0.0;
    qreal angle = 0.0;
};

QPointF sourcePoint(qreal x, qreal y, const QRectF& bounds, bool mirror)
{
    return bounds.topLeft() + QPointF(mirror ? bounds.width() - x : x, y);
}

CurlVertex projectCurl(const QPointF& source, qreal fold, qreal slant,
                       qreal radius, const QRectF& bounds, bool mirror)
{
    const qreal pi = qDegreesToRadians(180.0);
    const qreal u = mirror ? bounds.right() - source.x()
                           : source.x() - bounds.left();
    const qreal v = source.y() - bounds.top();
    const qreal normalLength = qSqrt(1.0 + slant * slant);
    const QPointF normal(1.0 / normalLength, -slant / normalLength);
    const qreal distance = qMax(qreal(0),
        (u - fold - slant * (v - bounds.height() / 2)) / normalLength);
    const qreal angle = qMin(pi, distance / radius);
    
    
    const QPointF anchor = QPointF(u, v) - normal * distance;
    const qreal curvedDistance = distance <= pi * radius
        ? radius * qSin(angle) : -(distance - pi * radius);
    const qreal z = radius * (1.0 - qCos(angle));
    QPointF position = anchor + normal * curvedDistance;
    const QPointF center(bounds.width() / 2, bounds.height() / 2);
    const qreal cameraDistance = bounds.width() * 3.0;
    const qreal perspective = cameraDistance / (cameraDistance - z);
    position = center + (position - center) * perspective;
    return {sourcePoint(position.x(), position.y(), bounds, mirror), z};
}



void drawPagePatch(QPainter& painter, const QPixmap& page,
                   const QRectF& bounds, const QRectF& patch)
{
    const QRectF pixels((patch.left() - bounds.left()) * page.width() / bounds.width(),
                        (patch.top() - bounds.top()) * page.height() / bounds.height(),
                        patch.width() * page.width() / bounds.width(),
                        patch.height() * page.height() / bounds.height());
    painter.drawPixmap(patch, page, pixels);
}

} 

void PageTurnUtils::drawTurningPage(QPainter& painter, const QPixmap& page,
                                    const QRectF& bounds, qreal progress,
                                    int direction, const PageTurnStyle& style)
{
    if (page.isNull() || bounds.isEmpty() || progress >= 1.0) return;
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.setClipRect(bounds, Qt::IntersectClip);
    if (progress <= 0.0) {
        if (style.foregroundOnly)
            painter.setCompositionMode(QPainter::CompositionMode_Source);
        drawPagePatch(painter, page, bounds, bounds);
        painter.restore();
        return;
    }

    const qreal pi = qDegreesToRadians(180.0);
    const qreal width = bounds.width();
    const qreal height = bounds.height();
    const qreal fold = width * (1.0 - progress);
    const qreal curvature = qSin(progress * pi);
    const qreal radius = qMax(qreal(0.25), width * 0.075 * curvature);
    
    const qreal slant = -0.14 * width / height * curvature;
    const qreal normalLength = qSqrt(1.0 + slant * slant);
    const qreal creaseTop = fold - slant * height / 2;
    const qreal creaseBottom = fold + slant * height / 2;
    const bool mirror = direction < 0;
    QPainterPath pageArea;
    pageArea.addRect(bounds);

    
    const qreal spread = radius * 1.5;
    QPolygonF shadowPolygon;
    shadowPolygon << sourcePoint(creaseTop - spread, 0, bounds, mirror)
                  << sourcePoint(creaseTop + spread, 0, bounds, mirror)
                  << sourcePoint(creaseBottom + spread, height, bounds, mirror)
                  << sourcePoint(creaseBottom - spread, height, bounds, mirror);
    QLinearGradient shadow(sourcePoint(fold - spread, height / 2, bounds, mirror),
                           sourcePoint(fold + spread, height / 2, bounds, mirror));
    QColor transparent = style.shadowColor;
    transparent.setAlpha(0);
    QColor shade = style.shadowColor;
    shade.setAlpha(qRound(65 * curvature));
    shadow.setColorAt(0.0, transparent);
    shadow.setColorAt(0.55, shade);
    shadow.setColorAt(1.0, transparent);
    painter.setPen(Qt::NoPen);
    painter.setBrush(shadow);
    if (!style.foregroundOnly) painter.drawPolygon(shadowPolygon);

    QPolygonF flatPolygon;
    flatPolygon << sourcePoint(0, 0, bounds, mirror)
                << sourcePoint(creaseTop, 0, bounds, mirror)
                << sourcePoint(creaseBottom, height, bounds, mirror)
                << sourcePoint(0, height, bounds, mirror);
    QPainterPath flat;
    flat.addPolygon(flatPolygon);
    flat = flat.intersected(pageArea);
    if (!flat.isEmpty()) {
        painter.save();
        painter.setClipPath(flat, Qt::IntersectClip);
        if (style.foregroundOnly)
            painter.setCompositionMode(QPainter::CompositionMode_Source);
        drawPagePatch(painter, page, bounds, flat.boundingRect());
        painter.restore();
    }

    const qreal maxDistance = (width - qMin(creaseTop, creaseBottom)) / normalLength;
    const qreal curlDistance = qMin(pi * radius, maxDistance);
    QList<qreal> samples;
    constexpr int curvedStripCount = 40;
    for (int i = 0; i <= curvedStripCount; ++i)
        samples.append(curlDistance * i / curvedStripCount);
    if (pi * radius / 2 < curlDistance) samples.append(pi * radius / 2);
    
    if (maxDistance > curlDistance) samples.append(maxDistance);
    std::sort(samples.begin(), samples.end());
    QList<CurlStrip> strips;
    for (int i = 0; i + 1 < samples.size(); ++i) {
        const qreal d0 = samples.at(i);
        const qreal d1 = samples.at(i + 1);
        if (d1 - d0 < 0.001) continue;
        QPolygonF source;
        source << sourcePoint(creaseTop + d0 * normalLength, 0, bounds, mirror)
               << sourcePoint(creaseTop + d1 * normalLength, 0, bounds, mirror)
               << sourcePoint(creaseBottom + d1 * normalLength, height, bounds, mirror)
               << sourcePoint(creaseBottom + d0 * normalLength, height, bounds, mirror);
        QPainterPath sourceArea;
        sourceArea.addPolygon(source);
        sourceArea = sourceArea.intersected(pageArea);
        if (sourceArea.isEmpty()) continue;
        QPolygonF destination;
        qreal depth = 0.0;
        for (const QPointF& vertex : source) {
            const CurlVertex projected = projectCurl(
                vertex, fold, slant, radius, bounds, mirror);
            destination.append(projected.position);
            depth += projected.depth / 4;
        }
        CurlStrip strip;
        if (!QTransform::quadToQuad(source, destination, strip.transform)) continue;
        strip.surface = strip.transform.map(sourceArea);
        strip.sourceRect = sourceArea.boundingRect();
        strip.depth = depth;
        strip.angle = qMin(pi, (d0 + d1) / (2 * radius));
        strips.append(strip);
    }
    std::stable_sort(strips.begin(), strips.end(),
                     [](const CurlStrip& a, const CurlStrip& b) {
                         return a.depth < b.depth;
                     });
    const qreal opacity = painter.opacity();
    
    
    painter.setRenderHint(QPainter::Antialiasing, false);
    for (const CurlStrip& strip : strips) {
        const qreal backFacing = qMax(qreal(0), -qCos(strip.angle));
        painter.save();
        painter.setClipPath(strip.surface, Qt::IntersectClip);
        
        painter.setOpacity(opacity * (1.0 - 0.12 * curvature * backFacing));
        painter.setTransform(strip.transform, true);
        if (style.foregroundOnly)
            painter.setCompositionMode(QPainter::CompositionMode_Source);
        drawPagePatch(painter, page, bounds, strip.sourceRect);
        painter.restore();

        QColor tint = backFacing > 0 ? style.paperColor : style.shadowColor;
        tint.setAlpha(backFacing > 0 ? qRound(22 * curvature * backFacing)
                                    : qRound(36 * qSin(strip.angle)));
        if (!style.foregroundOnly) painter.fillPath(strip.surface, tint);
    }

    
    QPainterPath edge;
    constexpr int edgeSegments = 16;
    for (int i = 0; i <= edgeSegments; ++i) {
        const CurlVertex vertex = projectCurl(
            sourcePoint(width, height * i / edgeSegments, bounds, mirror),
            fold, slant, radius, bounds, mirror);
        if (i == 0) edge.moveTo(vertex.position);
        else edge.lineTo(vertex.position);
    }
    QColor highlight = style.paperColor.lighter(150);
    highlight.setAlpha(qRound(80 * curvature));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(highlight, 0.75));
    painter.setBrush(Qt::NoBrush);
    if (!style.foregroundOnly) painter.drawPath(edge);
    painter.restore();
}
