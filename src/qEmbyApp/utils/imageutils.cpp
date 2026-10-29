#include "imageutils.h"

#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <QPixmapCache>
#include <QImage>
#include <QVector>
#include <QRadialGradient>
#include <QtMath>
#include <algorithm>

namespace {

QImage boxBlurPass(const QImage& source, int radius, bool horizontal)
{
    QImage result(source.size(), QImage::Format_ARGB32_Premultiplied);
    const int lines = horizontal ? source.height() : source.width();
    const int length = horizontal ? source.width() : source.height();
    const int diameter = radius * 2 + 1;
    for (int line = 0; line < lines; ++line) {
        auto pixelAt = [&](int position) {
            position = qBound(0, position, length - 1);
            const int x = horizontal ? position : line;
            const int y = horizontal ? line : position;
            return reinterpret_cast<const QRgb*>(source.constScanLine(y))[x];
        };
        int red = 0, green = 0, blue = 0, alpha = 0;
        for (int i = -radius; i <= radius; ++i) {
            const QRgb p = pixelAt(i);
            red += qRed(p); green += qGreen(p); blue += qBlue(p); alpha += qAlpha(p);
        }
        for (int position = 0; position < length; ++position) {
            const int x = horizontal ? position : line;
            const int y = horizontal ? line : position;
            reinterpret_cast<QRgb*>(result.scanLine(y))[x] =
                qRgba(red / diameter, green / diameter, blue / diameter, alpha / diameter);
            const QRgb removed = pixelAt(position - radius);
            const QRgb added = pixelAt(position + radius + 1);
            red += qRed(added) - qRed(removed);
            green += qGreen(added) - qGreen(removed);
            blue += qBlue(added) - qBlue(removed);
            alpha += qAlpha(added) - qAlpha(removed);
        }
    }
    return result;
}

} 

namespace ImageUtils {

QPixmap radiallyFadedPixmap(const QPixmap& source, const QRectF& area, qreal opacity)
{
    if (source.isNull() || area.isEmpty()) return {};
    QPixmap result = source;
    {
        QPainter painter(&result);
        painter.setCompositionMode(QPainter::CompositionMode_DestinationIn);
        painter.translate(area.center());
        painter.scale(area.width() / 2, area.height() / 2);
        QRadialGradient mask(QPointF(0, 0), 1.0);
        const qreal peak = qBound(qreal(0), opacity, qreal(1));
        
        
        
        constexpr int steps = 24;
        for (int i = 0; i <= steps; ++i) {
            const qreal t = qreal(i) / steps;
            const qreal distance = t * t;
            const qreal weight = (1 - distance) * (1 - distance) * (1 + 2 * distance);
            mask.setColorAt(t, QColor(255, 255, 255, qRound(255 * peak * weight)));
        }
        painter.fillRect(QRectF(-2, -2, 4, 4), mask);
    }
    return result;
}

QPixmap blurredPixmap(const QPixmap& source, qreal radius)
{
    if (source.isNull() || radius <= 0) return source;
    QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied);
    const int boxRadius = qMax(1, qCeil(qBound(qreal(0), radius, qreal(32))
                                      * source.devicePixelRatioF() / 3));
    for (int pass = 0; pass < 3; ++pass) {
        image = boxBlurPass(image, boxRadius, true);
        image = boxBlurPass(image, boxRadius, false);
    }
    QPixmap result = QPixmap::fromImage(image);
    result.setDevicePixelRatio(source.devicePixelRatioF());
    return result;
}

QPixmap adaptiveLightPixmap(const QPixmap& source, const QColor& tint)
{
    if (source.isNull() || !tint.isValid() || tint.alpha() == 0) return {};
    const QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    QImage light(image.size(), QImage::Format_ARGB32_Premultiplied);
    const qreal maximumTint = tint.alphaF();
    const qreal tintLuminance = (0.2126 * tint.red() + 0.7152 * tint.green()
                                 + 0.0722 * tint.blue()) / 255;
    const qreal targetLuminance = tintLuminance * maximumTint;
    for (int y = 0; y < image.height(); ++y) {
        const auto* sourceRow = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        auto* lightRow = reinterpret_cast<QRgb*>(light.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const QRgb pixel = sourceRow[x];
            const qreal luminance = (0.2126 * qRed(pixel) + 0.7152 * qGreen(pixel)
                                     + 0.0722 * qBlue(pixel)) / 255;
            
            
            const qreal requiredTint = luminance < targetLuminance
                ? qBound(qreal(0), (targetLuminance - luminance)
                                      / qMax(qreal(0.001), tintLuminance - luminance),
                         maximumTint)
                : 0;
            const int alpha = qRound(qAlpha(pixel) * requiredTint / maximumTint);
            const int red = qRound(qRed(pixel) * (1 - maximumTint)
                                    + tint.red() * maximumTint);
            const int green = qRound(qGreen(pixel) * (1 - maximumTint)
                                      + tint.green() * maximumTint);
            const int blue = qRound(qBlue(pixel) * (1 - maximumTint)
                                     + tint.blue() * maximumTint);
            lightRow[x] = qRgba((red * alpha + 127) / 255,
                                (green * alpha + 127) / 255,
                                (blue * alpha + 127) / 255, alpha);
        }
    }
    QPixmap result = QPixmap::fromImage(light);
    result.setDevicePixelRatio(source.devicePixelRatioF());
    return result;
}

QPixmap blurredAlphaPixmap(const QPixmap& source, const QColor& color,
                          qreal radius)
{
    if (source.isNull() || !color.isValid() || color.alpha() == 0) return {};
    const QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);
    const int w = image.width();
    const int h = image.height();
    const qreal pixelRadius = qBound(qreal(0), radius, qreal(12)) * source.devicePixelRatioF();
    const int extent = qCeil(pixelRadius);
    const qreal sigma = qMax(qreal(0.5), pixelRadius / 2);
    QVector<qreal> weights(2 * extent + 1);
    qreal total = 0;
    for (int i = -extent; i <= extent; ++i) {
        const qreal weight = qExp(-qreal(i * i) / (2 * sigma * sigma));
        weights[i + extent] = weight;
        total += weight;
    }
    for (qreal& weight : weights) weight /= total;

    
    
    QVector<float> horizontal(w * h);
    for (int y = 0; y < h; ++y) {
        const auto* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < w; ++x) {
            qreal alpha = 0;
            for (int i = -extent; i <= extent; ++i) {
                if (x + i >= 0 && x + i < w)
                    alpha += qAlpha(row[x + i]) * weights[i + extent];
            }
            horizontal[y * w + x] = float(alpha);
        }
    }
    QImage shadow(w, h, QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < h; ++y) {
        auto* row = reinterpret_cast<QRgb*>(shadow.scanLine(y));
        for (int x = 0; x < w; ++x) {
            qreal alpha = 0;
            for (int i = -extent; i <= extent; ++i) {
                if (y + i >= 0 && y + i < h)
                    alpha += horizontal[(y + i) * w + x] * weights[i + extent];
            }
            const int a = qBound(0, qRound(alpha * color.alphaF()), 255);
            row[x] = qRgba((color.red() * a + 127) / 255,
                           (color.green() * a + 127) / 255,
                           (color.blue() * a + 127) / 255, a);
        }
    }
    QPixmap result = QPixmap::fromImage(shadow);
    result.setDevicePixelRatio(source.devicePixelRatioF());
    return result;
}

QPixmap outlinedAlphaPixmap(const QPixmap& source, const QColor& color,
                           qreal width)
{
    if (source.isNull() || !color.isValid() || color.alpha() == 0 || width <= 0) return {};
    const QImage alpha = source.toImage().convertToFormat(QImage::Format_Alpha8);
    const qreal pixelWidth = qBound(qreal(0), width, qreal(4)) * source.devicePixelRatioF();
    const int extent = qCeil(pixelWidth);
    struct Sample {
        int dx;
        int dy;
        qreal coverage;
    };
    QVector<Sample> samples;
    for (int dy = -extent; dy <= extent; ++dy) {
        for (int dx = -extent; dx <= extent; ++dx) {
            
            
            const qreal distance = qSqrt(qreal(dx * dx + dy * dy));
            const qreal coverage = qBound(qreal(0), pixelWidth + 1 - distance, qreal(1));
            if (coverage > 0) samples.append(Sample{dx, dy, coverage});
        }
    }
    QImage outline(alpha.size(), QImage::Format_ARGB32_Premultiplied);
    for (int y = 0; y < alpha.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(outline.scanLine(y));
        for (int x = 0; x < alpha.width(); ++x) {
            qreal coverage = 0;
            for (const Sample& sample : samples) {
                const int sx = x + sample.dx;
                const int sy = y + sample.dy;
                if (sx < 0 || sx >= alpha.width() || sy < 0 || sy >= alpha.height()) continue;
                coverage = qMax(coverage, alpha.constScanLine(sy)[sx] * sample.coverage);
                if (coverage >= 255) break;
            }
            
            
            const int a = qBound(0, qRound(coverage * color.alphaF()), 255);
            row[x] = qRgba((color.red() * a + 127) / 255,
                           (color.green() * a + 127) / 255,
                           (color.blue() * a + 127) / 255, a);
        }
    }
    QPixmap result = QPixmap::fromImage(outline);
    result.setDevicePixelRatio(source.devicePixelRatioF());
    return result;
}

QPixmap scaledCoverPixmap(const QPixmap& source, const QSize& logicalSize,
                          qreal devicePixelRatio)
{
    if (source.isNull() || logicalSize.isEmpty()) return {};
    const qreal dpr = qMax(qreal(1), devicePixelRatio);
    const QSize pixelSize(qCeil(logicalSize.width() * dpr),
                          qCeil(logicalSize.height() * dpr));
    const QString key = QStringLiteral("ImageUtils:cover:%1:%2x%3:%4")
                            .arg(source.cacheKey()).arg(pixelSize.width())
                            .arg(pixelSize.height()).arg(dpr, 0, 'g', 16);
    QPixmap result;
    if (QPixmapCache::find(key, &result)) return result;
    result = source.scaled(pixelSize, Qt::KeepAspectRatioByExpanding,
                           Qt::SmoothTransformation);
    result.setDevicePixelRatio(dpr);
    QPixmapCache::insert(key, result);
    return result;
}

QPixmap roundedPixmap(const QPixmap &src, int radiusLeft, int radiusRight) {
    if (src.isNull()) return src;
    QPixmap result(src.size());
    result.fill(Qt::transparent);
    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    QRectF rect(0, 0, src.width(), src.height());
    
    path.moveTo(rect.left() + radiusLeft, rect.top());
    path.lineTo(rect.right() - radiusRight, rect.top());
    path.quadTo(rect.right(), rect.top(), rect.right(), rect.top() + radiusRight);
    path.lineTo(rect.right(), rect.bottom() - radiusRight);
    path.quadTo(rect.right(), rect.bottom(), rect.right() - radiusRight, rect.bottom());
    path.lineTo(rect.left() + radiusLeft, rect.bottom());
    path.quadTo(rect.left(), rect.bottom(), rect.left(), rect.bottom() - radiusLeft);
    path.lineTo(rect.left(), rect.top() + radiusLeft);
    path.quadTo(rect.left(), rect.top(), rect.left() + radiusLeft, rect.top());
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, src);
    return result;
}

QPixmap coverRoundedPixmap(const QPixmap& src, const QSize& targetSize,
                           int radius) {
    if (src.isNull() || !targetSize.isValid()) return src;

    const QPixmap scaled =
        src.scaled(targetSize, Qt::KeepAspectRatioByExpanding,
                   Qt::SmoothTransformation);
    const QRect sourceRect((scaled.width() - targetSize.width()) / 2,
                           (scaled.height() - targetSize.height()) / 2,
                           targetSize.width(), targetSize.height());

    QPixmap result(targetSize);
    result.fill(Qt::transparent);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(QRectF(QPointF(0, 0), QSizeF(targetSize)), radius,
                        radius);
    painter.setClipPath(path);
    painter.drawPixmap(QRect(QPoint(0, 0), targetSize), scaled, sourceRect);
    return result;
}

QPixmap containRoundedPixmap(const QPixmap& src, const QSize& targetSize,
                             int radius, bool allowUpscale) {
    if (src.isNull() || !targetSize.isValid()) return src;

    QSize scaledSize = src.size();
    if (allowUpscale || scaledSize.width() > targetSize.width() ||
        scaledSize.height() > targetSize.height()) {
        scaledSize.scale(targetSize, Qt::KeepAspectRatio);
    }

    const QPixmap scaled =
        scaledSize == src.size()
            ? src
            : src.scaled(scaledSize, Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);

    QPixmap result(targetSize);
    result.fill(Qt::transparent);

    const QRect targetRect((targetSize.width() - scaled.width()) / 2,
                           (targetSize.height() - scaled.height()) / 2,
                           scaled.width(), scaled.height());
    const qreal effectiveRadius =
        std::min<qreal>(radius, std::min(targetRect.width(), targetRect.height()) / 2.0);

    QPainter painter(&result);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath path;
    path.addRoundedRect(QRectF(targetRect), effectiveRadius, effectiveRadius);
    painter.setClipPath(path);
    painter.drawPixmap(targetRect.topLeft(), scaled);
    return result;
}

} 
