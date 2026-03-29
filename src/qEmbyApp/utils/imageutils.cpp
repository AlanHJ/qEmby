#include "imageutils.h"

#include <QPainter>
#include <QPainterPath>

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

} 
