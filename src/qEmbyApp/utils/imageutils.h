#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QPixmap>
#include <QSize>
#include <QColor>
#include <QRectF>

namespace ImageUtils {


QPixmap blurredPixmap(const QPixmap& source, qreal radius);



QPixmap adaptiveLightPixmap(const QPixmap& source, const QColor& tint);


QPixmap radiallyFadedPixmap(const QPixmap& source, const QRectF& area, qreal opacity);


QPixmap blurredAlphaPixmap(const QPixmap& source, const QColor& color,
                          qreal radius);



QPixmap outlinedAlphaPixmap(const QPixmap& source, const QColor& color,
                           qreal width);


QPixmap scaledCoverPixmap(const QPixmap& source, const QSize& logicalSize,
                          qreal devicePixelRatio);





QPixmap roundedPixmap(const QPixmap &src, int radiusLeft, int radiusRight);





QPixmap coverRoundedPixmap(const QPixmap& src, const QSize& targetSize,
                           int radius);







QPixmap containRoundedPixmap(const QPixmap& src, const QSize& targetSize,
                             int radius, bool allowUpscale = false);

} 

#endif 
