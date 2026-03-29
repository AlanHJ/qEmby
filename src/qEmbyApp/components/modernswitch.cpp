#include "modernswitch.h"
#include <QPainterPath>

ModernSwitch::ModernSwitch(QWidget* parent)
    : QAbstractButton(parent),
    m_offset(0.0),
    m_trackColorActive(QColor("#00A4FF")),
    m_trackColorInactive(QColor("#555555")),
    m_thumbColor(QColor("#FFFFFF")),
    m_isHovered(false)
{
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);

    
    m_animation = new QPropertyAnimation(this, "offset", this);
    m_animation->setDuration(150);
    m_animation->setEasingCurve(QEasingCurve::InOutSine);

    connect(this, &ModernSwitch::toggled, this, &ModernSwitch::updateAnimation);
}

ModernSwitch::~ModernSwitch() = default;

qreal ModernSwitch::offset() const { return m_offset; }
void ModernSwitch::setOffset(qreal offset) {
    m_offset = offset;
    update(); 
}

QColor ModernSwitch::trackColorActive() const { return m_trackColorActive; }
void ModernSwitch::setTrackColorActive(const QColor& color) {
    m_trackColorActive = color;
    update();
}

QColor ModernSwitch::trackColorInactive() const { return m_trackColorInactive; }
void ModernSwitch::setTrackColorInactive(const QColor& color) {
    m_trackColorInactive = color;
    update();
}

QColor ModernSwitch::thumbColor() const { return m_thumbColor; }
void ModernSwitch::setThumbColor(const QColor& color) {
    m_thumbColor = color;
    update();
}

QSize ModernSwitch::sizeHint() const {
    return QSize(44, 24); 
}

void ModernSwitch::paintEvent(QPaintEvent* ) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    
    QRect trackRect(0, 0, width(), height());
    int radius = height() / 2;

    
    QColor trackColor = isChecked() ? m_trackColorActive : m_trackColorInactive;

    
    if (m_isHovered) {
        trackColor = trackColor.lighter(110);
    }

    painter.setBrush(trackColor);
    painter.drawRoundedRect(trackRect, radius, radius);

    
    int thumbMargin = 3;
    int thumbRadius = radius - thumbMargin;
    int thumbX = thumbMargin + m_offset * (width() - 2 * radius);
    int thumbY = thumbMargin;

    painter.setBrush(m_thumbColor);
    painter.drawEllipse(thumbX, thumbY, thumbRadius * 2, thumbRadius * 2);
}

void ModernSwitch::nextCheckState() {
    QAbstractButton::nextCheckState();
}

void ModernSwitch::enterEvent(QEnterEvent* event) {
    m_isHovered = true;
    update();
    QAbstractButton::enterEvent(event);
}

void ModernSwitch::leaveEvent(QEvent* event) {
    m_isHovered = false;
    update();
    QAbstractButton::leaveEvent(event);
}

void ModernSwitch::updateAnimation(bool checked) {
    m_animation->stop();
    m_animation->setStartValue(m_offset);
    m_animation->setEndValue(checked ? 1.0 : 0.0);
    m_animation->start();
}
