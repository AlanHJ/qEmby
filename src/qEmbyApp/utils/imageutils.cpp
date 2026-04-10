#include "imageutils.h"

#include <QPainter>
#include <QPainterPath>
#include <QRect>
#include <algorithm>

namespace ImageUtils {

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
