#ifndef IMAGEUTILS_H
#define IMAGEUTILS_H

#include <QPixmap>
#include <QSize>

namespace ImageUtils {





QPixmap roundedPixmap(const QPixmap &src, int radiusLeft, int radiusRight);





QPixmap coverRoundedPixmap(const QPixmap& src, const QSize& targetSize,
                           int radius);







QPixmap containRoundedPixmap(const QPixmap& src, const QSize& targetSize,
                             int radius, bool allowUpscale = false);

} 

#endif 
