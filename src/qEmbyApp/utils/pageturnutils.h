#ifndef PAGETURNUTILS_H
#define PAGETURNUTILS_H

#include <QColor>
#include <QPixmap>
#include <QRectF>

class QPainter;

namespace PageTurnUtils {

struct PageTurnStyle {
    QColor shadowColor;
    QColor paperColor;
    
    
    bool foregroundOnly = false;
};




void drawTurningPage(QPainter& painter, const QPixmap& page,
                     const QRectF& bounds, qreal progress, int direction,
                     const PageTurnStyle& style);

} 

#endif 
