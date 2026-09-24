#include "nativedanmakulayout.h"

#include <QFontMetricsF>
#include <QHash>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <algorithm>
#include <cmath>
#include <utility>

namespace NativeDanmakuLayout {

bool optionsEqual(const DanmakuRenderOptions &a, const DanmakuRenderOptions &b)
{
    return a.enabled == b.enabled && a.opacity == b.opacity &&
        a.fontScale == b.fontScale && a.fontWeight == b.fontWeight &&
        a.outlineSize == b.outlineSize && a.shadowOffset == b.shadowOffset &&
        a.areaPercent == b.areaPercent && a.density == b.density &&
        a.speedScale == b.speedScale && a.offsetMs == b.offsetMs &&
        a.hideScroll == b.hideScroll && a.hideTop == b.hideTop &&
        a.hideBottom == b.hideBottom && a.dualSubtitle == b.dualSubtitle &&
        a.blockedKeywords == b.blockedKeywords;
}

Schedule build(QList<DanmakuComment> comments, DanmakuRenderOptions options,
               QRectF surface, QFont font, Cancellation cancelled, QSizeF motionReferenceSize)
{
    Schedule result;
    result.baseFont = font;
    result.surface = surface;
    if (surface.isEmpty() || !options.enabled) {
        return result;
    }
    std::stable_sort(comments.begin(), comments.end(),
        [](const DanmakuComment &a, const DanmakuComment &b) {
            return a.timeMs < b.timeMs;
        });
    const qreal fontScale = std::clamp(options.fontScale, 0.6, 2.4);
    const qreal outline = std::clamp(options.outlineSize, 0.0, 6.0);
    const qreal shadow = std::clamp(options.shadowOffset, 0.0, 4.0);
    const qreal padding = std::ceil(outline + shadow) + 2.0;
    font.setWeight(static_cast<QFont::Weight>(std::clamp(options.fontWeight, 100, 900)));
    font.setPixelSize(qRound(25.0 * fontScale));
    const qreal rowHeight = std::ceil(QFontMetricsF(font).height() + padding * 2.0 + 2.0);
    const qreal scale = std::max(0.4, std::min(surface.width() / 1920.0,
                                             surface.height() / 1080.0));
    result.rowHeight = rowHeight;
    result.areaPercent = options.areaPercent;
    result.density = options.density;
    const qreal configuredSpeed = std::clamp(options.speedScale, 0.5, 3.0);
    const qreal speedScale = configuredSpeed <= 1.0 ? configuredSpeed
        : std::pow(configuredSpeed, 0.55);
    const qreal edgePadding = 32.0 * scale;
    const qreal gap = 96.0 * scale;
    
    
    
    
    
    if (motionReferenceSize.isEmpty()) motionReferenceSize = surface.size();
    const qreal motionScale = std::max(0.4, std::min(motionReferenceSize.width() / 1920.0,
                                                   motionReferenceSize.height() / 1080.0));
    const qreal velocity = std::max(0.01, std::min(1.12 * motionScale,
        (motionReferenceSize.width() + 80.0 * motionScale + 25.0 * 8.0) *
        speedScale / 12000.0));
    result.velocity = velocity;
    result.gap = gap;
    result.edgePadding = edgePadding;
    QStringList keywords;
    for (QString keyword : options.blockedKeywords) {
        keyword = keyword.trimmed();
        if (!keyword.isEmpty()) {
            keywords.append(keyword);
        }
    }
    QHash<QString, int> spriteIds;
    result.sources.reserve(comments.size());
    for (const DanmakuComment &comment : std::as_const(comments)) {
        if (cancelled->load(std::memory_order_relaxed)) {
            return {};
        }
        if (!comment.isValid()) {
            ++result.filtered;
            continue;
        }
        Motion motion;
        switch (comment.mode) {
        case 1: case 2: case 3: motion = Motion::Scroll; break;
        case 4: motion = Motion::Bottom; break;
        case 5: motion = Motion::Top; break;
        case 6: motion = Motion::Reverse; break;
        default:
            
            
            ++result.unsupported;
            continue;
        }
        const int motionIndex = static_cast<int>(motion);
        ++result.inputByMotion[motionIndex];
        const bool moving = motion == Motion::Scroll || motion == Motion::Reverse;
        QString text = comment.text;
        text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
        text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
        text = text.trimmed();
        if ((moving && options.hideScroll) ||
            (motion == Motion::Top && options.hideTop) ||
            (motion == Motion::Bottom && options.hideBottom) ||
            std::any_of(keywords.cbegin(), keywords.cend(), [&text](const QString &word) {
                return text.contains(word, Qt::CaseInsensitive);
            })) {
            ++result.filtered;
            ++result.filteredByMotion[motionIndex];
            continue;
        }
        const int pixels = std::clamp(qRound((comment.fontLevel > 0
            ? comment.fontLevel : 25) * fontScale), 8, 96);
        const QColor color = comment.color.isValid() ? comment.color : QColor(Qt::white);
        const QString key = QString::number(pixels) + QLatin1Char(':') +
            QString::number(color.rgba()) + QLatin1Char(':') + text;
        int spriteId = spriteIds.value(key, -1);
        if (spriteId < 0) {
            Sprite sprite;
            sprite.text = text;
            sprite.font = font;
            sprite.font.setPixelSize(pixels);
            sprite.color = color;
            sprite.outline = outline;
            sprite.shadow = shadow;
            sprite.padding = padding;
            const QFontMetricsF metrics(sprite.font);
            const QStringList lines = text.split(QLatin1Char('\n'));
            qreal right = 0.0;
            qreal left = 0.0;
            for (const QString &line : lines) {
                const QRectF ink = metrics.tightBoundingRect(line);
                left = std::min(left, ink.left());
                right = std::max({right, ink.right(), metrics.horizontalAdvance(line)});
            }
            sprite.leftBearing = left;
            sprite.baseline = metrics.ascent();
            sprite.size = QSizeF(std::ceil(right - left + padding * 2.0),
                std::ceil(metrics.height() + (lines.size() - 1) * metrics.lineSpacing() +
                          padding * 2.0));
            spriteId = result.sprites.size();
            result.sprites.append(std::move(sprite));
            spriteIds.insert(key, spriteId);
        }
        result.sources.append({motion, static_cast<qreal>(comment.timeMs) + options.offsetMs, spriteId});
    }
    return reflow(result, surface, -1.0, cancelled);
}

QImage rasterize(const Sprite &sprite, qreal dpr)
{
    const qreal width = std::ceil(sprite.size.width() * dpr);
    const qreal height = std::ceil(sprite.size.height() * dpr);
    
    
    if (width <= 0.0 || height <= 0.0 || width * height * 4.0 > 64.0 * 1024 * 1024) {
        return {};
    }
    QImage image(QSize(static_cast<int>(width), static_cast<int>(height)),
                 QImage::Format_ARGB32_Premultiplied);
    if (image.isNull()) {
        return image;
    }
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setFont(sprite.font);
    const QFontMetricsF metrics(sprite.font);
    const QStringList lines = sprite.text.split(QLatin1Char('\n'));
    QPainterPath path;
    QPointF baseline(sprite.padding - sprite.leftBearing, sprite.padding + sprite.baseline);
    for (const QString &line : lines) {
        path.addText(baseline, sprite.font, line);
        baseline.ry() += metrics.lineSpacing();
    }
    if (sprite.shadow > 0.0) {
        QPainterPath shadowPath = path.translated(sprite.shadow, sprite.shadow);
        painter.fillPath(shadowPath, QColor(0, 0, 0, qRound(sprite.color.alphaF() * 96)));
    }
    if (sprite.outline > 0.0) {
        painter.strokePath(path, QPen(QColor(0, 0, 0, qRound(sprite.color.alphaF() * 180)),
            sprite.outline * 2.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    }
    
    painter.setPen(sprite.color);
    baseline = QPointF(sprite.padding - sprite.leftBearing, sprite.padding + sprite.baseline);
    for (const QString &line : lines) {
        painter.drawText(baseline, line);
        baseline.ry() += metrics.lineSpacing();
    }
    return image;
}

} 
