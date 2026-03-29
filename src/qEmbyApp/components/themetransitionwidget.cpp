#include "themetransitionwidget.h"
#include <QPainter>
#include <QPainterPath>

ThemeTransitionWidget::ThemeTransitionWidget(const QPixmap& oldSnapshot, const QPoint& center, QWidget* parent)
    : QWidget(parent), m_oldSnapshot(oldSnapshot), m_center(center), m_radius(0)
{
    
    setAttribute(Qt::WA_TransparentForMouseEvents);
    
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
}

int ThemeTransitionWidget::radius() const { return m_radius; }

void ThemeTransitionWidget::setRadius(int r) {
    m_radius = r;
    update(); 
}

void ThemeTransitionWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    
    QPainterPath path;
    path.addRect(rect()); 
    path.addEllipse(m_center, m_radius, m_radius); 
    path.setFillRule(Qt::OddEvenFill); 

    
    painter.setClipPath(path);
    
    painter.drawPixmap(0, 0, m_oldSnapshot);
}
