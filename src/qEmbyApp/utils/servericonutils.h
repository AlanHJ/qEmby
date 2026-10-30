#ifndef SERVERICONUTILS_H
#define SERVERICONUTILS_H

#include <QPixmap>
#include <QSize>

struct ServerProfile;

namespace ServerIconUtils {



QPixmap loadIcon(const ServerProfile &profile, const QSize &logicalSize,
                 qreal devicePixelRatio);

} 

#endif 
