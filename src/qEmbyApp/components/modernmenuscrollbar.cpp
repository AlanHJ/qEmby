#include "modernmenuscrollbar.h"

#include <QPainter>
#include <algorithm>

namespace {






constexpr int kHandleNormalWidth = 4;
constexpr int kHandleHoverWidth = 8;
constexpr int kHandleMinHeight = 28;
constexpr int kTrackVerticalPadding = 3;




constexpr int kHandleAlphaNormal = 115;
constexpr int kHandleAlphaActive = 178;

}  

ModernMenuScrollBar::ModernMenuScrollBar(Qt::Orientation orientation, QWidget *parent)
    : QScrollBar(orientation, parent) {
    
    
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

ModernMenuScrollBar::ModernMenuScrollBar(QWidget *parent)
    : QScrollBar(parent) {
    setAttribute(Qt::WA_OpaquePaintEvent, false);
}

void ModernMenuScrollBar::enterEvent(QEnterEvent *event) {
    m_hovered = true;
    update();
    QScrollBar::enterEvent(event);
}

void ModernMenuScrollBar::leaveEvent(QEvent *event) {
    m_hovered = false;
    update();
    QScrollBar::leaveEvent(event);
}








QRect ModernMenuScrollBar::computeHandleRect() const {
    if (orientation() != Qt::Vertical) {
        
        
        
        return {};
    }

    const int range = maximum() - minimum();
    const int trackTop = kTrackVerticalPadding;
    const int trackHeight = height() - 2 * kTrackVerticalPadding;

    if (range <= 0 || trackHeight <= 0) {
        
        return {};
    }

    
    
    const int page = pageStep();
    int handleHeight = (page > 0)
        ? static_cast<int>(static_cast<qint64>(trackHeight) * page / (range + page))
        : kHandleMinHeight;
    handleHeight = std::clamp(handleHeight, kHandleMinHeight, trackHeight);

    const int availableTrack = trackHeight - handleHeight;
    int handleY = trackTop;
    if (availableTrack > 0) {
        handleY = trackTop + static_cast<int>(
            static_cast<qint64>(sliderPosition() - minimum()) * availableTrack / range);
    }

    
    const bool active = m_hovered || isSliderDown();
    const int handleW = active ? kHandleHoverWidth : kHandleNormalWidth;
    const int handleX = (width() - handleW) / 2;

    return {handleX, handleY, handleW, handleHeight};
}

void ModernMenuScrollBar::paintEvent(QPaintEvent *event) {
    Q_UNUSED(event);

    const QRect r = computeHandleRect();
    if (r.isEmpty()) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const bool active = isSliderDown() || m_hovered;
    const QColor handleColor(255, 255, 255, active ? kHandleAlphaActive : kHandleAlphaNormal);

    painter.setBrush(handleColor);
    
    const qreal radius = r.width() / 2.0;
    painter.drawRoundedRect(QRectF(r), radius, radius);
}
