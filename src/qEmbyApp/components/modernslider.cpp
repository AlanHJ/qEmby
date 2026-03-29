#include "modernslider.h"
#include <QPainter>
#include <QPainterPath>

ModernSlider::ModernSlider(Qt::Orientation orientation, QWidget *parent)
    : QSlider(orientation, parent) {
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    setAttribute(Qt::WA_Hover, true); 
}

void ModernSlider::setBufferValue(int value) {
    if (m_bufferValue != value) {
        m_bufferValue = value;
        update();
    }
}

int ModernSlider::bufferValue() const {
    return m_bufferValue;
}

void ModernSlider::mousePressEvent(QMouseEvent *ev) {
    if (ev->button() == Qt::LeftButton) {
        m_isPressed = true;

        
        int handleRadius = 6;
        double trackLeft = handleRadius;
        double trackWidth = width() - 2 * handleRadius;

        if (orientation() == Qt::Horizontal) {
            double pos = (ev->pos().x() - trackLeft) / trackWidth;
            pos = qBound(0.0, pos, 1.0);
            setValue(pos * (maximum() - minimum()) + minimum());
            emit sliderMoved(value());
        } else {
            double trackTop = handleRadius;
            double trackHeight = height() - 2 * handleRadius;
            double pos = 1.0 - ((ev->pos().y() - trackTop) / trackHeight);
            pos = qBound(0.0, pos, 1.0);
            setValue(pos * (maximum() - minimum()) + minimum());
            emit sliderMoved(value());
        }
        ev->accept();
        update();
    }
    QSlider::mousePressEvent(ev);
}

void ModernSlider::mouseReleaseEvent(QMouseEvent *ev) {
    if (ev->button() == Qt::LeftButton) {
        m_isPressed = false;
        update();
    }
    QSlider::mouseReleaseEvent(ev);
}

void ModernSlider::mouseMoveEvent(QMouseEvent *ev) {
    QSlider::mouseMoveEvent(ev);
}


bool ModernSlider::event(QEvent *ev) {
    if (ev->type() == QEvent::Enter) {
        m_isHovered = true;
        update();
    } else if (ev->type() == QEvent::Leave) {
        m_isHovered = false;
        update();
    }
    return QSlider::event(ev);
}


void ModernSlider::paintEvent(QPaintEvent *ev) {
    Q_UNUSED(ev);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    
    int trackHeight = m_isHovered ? 4 : 2;
    int handleRadius = (m_isHovered || m_isPressed) ? 6 : 4;

    
    QRectF trackRect;
    if (orientation() == Qt::Horizontal) {
        trackRect = QRectF(handleRadius, (height() - trackHeight) / 2.0, width() - 2 * handleRadius, trackHeight);
    } else {
        trackRect = QRectF((width() - trackHeight) / 2.0, handleRadius, trackHeight, height() - 2 * handleRadius);
    }

    
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_trackColor);
    painter.drawRoundedRect(trackRect, trackHeight / 2.0, trackHeight / 2.0);

    double range = maximum() - minimum();
    double valueRatio = range > 0 ? static_cast<double>(value() - minimum()) / range : 0.0;
    double bufferRatio = range > 0 ? static_cast<double>(m_bufferValue - minimum()) / range : 0.0;
    if (bufferRatio > 1.0) bufferRatio = 1.0;

    
    if (bufferRatio > 0) {
        QRectF bufferRect = trackRect;
        if (orientation() == Qt::Horizontal) {
            bufferRect.setWidth(trackRect.width() * bufferRatio);
        } else {
            bufferRect.setTop(trackRect.bottom() - trackRect.height() * bufferRatio);
        }
        painter.setBrush(m_bufferColor);
        painter.drawRoundedRect(bufferRect, trackHeight / 2.0, trackHeight / 2.0);
    }

    
    if (valueRatio > 0) {
        QRectF playedRect = trackRect;
        if (orientation() == Qt::Horizontal) {
            playedRect.setWidth(trackRect.width() * valueRatio);
        } else {
            playedRect.setTop(trackRect.bottom() - trackRect.height() * valueRatio);
        }
        painter.setBrush(m_themeColor);
        painter.drawRoundedRect(playedRect, trackHeight / 2.0, trackHeight / 2.0);
    }

    
    QPointF handleCenter;
    if (orientation() == Qt::Horizontal) {
        handleCenter = QPointF(trackRect.left() + trackRect.width() * valueRatio, height() / 2.0);
    } else {
        handleCenter = QPointF(width() / 2.0, trackRect.bottom() - trackRect.height() * valueRatio);
    }

    painter.setBrush(m_handleColor);
    painter.drawEllipse(handleCenter, handleRadius, handleRadius);
}
