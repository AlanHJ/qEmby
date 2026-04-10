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

        const int edgePadding = qMax(m_normalHandleRadius, m_activeHandleRadius);
        const double trackLeft = edgePadding;
        const double trackWidth = width() - 2 * edgePadding;

        if (orientation() == Qt::Horizontal) {
            double pos = (ev->pos().x() - trackLeft) / trackWidth;
            pos = qBound(0.0, pos, 1.0);
            setValue(pos * (maximum() - minimum()) + minimum());
            emit sliderMoved(value());
        } else {
            const double trackTop = edgePadding;
            const double trackHeight = height() - 2 * edgePadding;
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

    const int trackHeight =
        (m_isHovered || m_isPressed) ? m_hoverTrackHeight : m_normalTrackHeight;
    const int handleRadius =
        (m_isHovered || m_isPressed) ? m_activeHandleRadius : m_normalHandleRadius;
    const int edgePadding = qMax(m_normalHandleRadius, m_activeHandleRadius);

    QRectF trackRect;
    if (orientation() == Qt::Horizontal) {
        trackRect = QRectF(edgePadding, (height() - trackHeight) / 2.0,
                           width() - 2 * edgePadding, trackHeight);
    } else {
        trackRect = QRectF((width() - trackHeight) / 2.0, edgePadding,
                           trackHeight, height() - 2 * edgePadding);
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(m_trackColor);
    painter.drawRoundedRect(trackRect, trackHeight / 2.0, trackHeight / 2.0);

    double range = maximum() - minimum();
    double valueRatio =
        range > 0 ? static_cast<double>(value() - minimum()) / range : 0.0;
    double bufferRatio = range > 0
                             ? static_cast<double>(m_bufferValue - minimum()) / range
                             : 0.0;
    valueRatio = qBound(0.0, valueRatio, 1.0);
    bufferRatio = qBound(0.0, bufferRatio, 1.0);

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
        handleCenter = QPointF(trackRect.left() + trackRect.width() * valueRatio,
                               height() / 2.0);
    } else {
        handleCenter =
            QPointF(width() / 2.0,
                    trackRect.bottom() - trackRect.height() * valueRatio);
    }

    painter.setBrush(m_handleColor);
    painter.drawEllipse(handleCenter, handleRadius, handleRadius);
}
