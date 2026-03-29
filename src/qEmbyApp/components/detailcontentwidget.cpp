#include "detailcontentwidget.h"
#include <QPainter>
#include <QStyleOption>
#include <QLinearGradient>

DetailContentWidget::DetailContentWidget(QWidget* parent)
    : QWidget(parent),
    m_cachedWidth(-1),
    m_baseColor(QColor("#FFFFFF")),
    m_gradientColor(QColor(249, 250, 251))
{
    
    setAttribute(Qt::WA_StyledBackground, true);
}

void DetailContentWidget::setBackdrop(const QPixmap& pix) {
    m_backdrop = pix;
    m_cachedWidth = -1; 
    update();
}

QColor DetailContentWidget::baseColor() const {
    return m_baseColor;
}

void DetailContentWidget::setBaseColor(const QColor& color) {
    if (m_baseColor != color) {
        m_baseColor = color;
        update(); 
    }
}

QColor DetailContentWidget::gradientColor() const {
    return m_gradientColor;
}

void DetailContentWidget::setGradientColor(const QColor& color) {
    if (m_gradientColor != color) {
        m_gradientColor = color;
        update();
    }
}

void DetailContentWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    
    painter.fillRect(rect(), m_baseColor);

    if (m_backdrop.isNull()) return;

    int w = width();
    int imgW = m_backdrop.width();
    int imgH = m_backdrop.height();
    if (imgW <= 0) return;

    
    int targetHeight = w * imgH / imgW;

    
    if (w != m_cachedWidth) {
        m_scaledBackdrop = m_backdrop.scaled(w, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_cachedWidth = w;
    }

    
    painter.drawPixmap(0, 0, m_scaledBackdrop);

    
    QLinearGradient gradient(0, 0, 0, targetHeight);
    int r = m_gradientColor.red();
    int g = m_gradientColor.green();
    int b = m_gradientColor.blue();

    
    gradient.setColorAt(0.0,  QColor(r, g, b, 0));
    gradient.setColorAt(0.15, QColor(r, g, b, 150));
    gradient.setColorAt(0.4,  QColor(r, g, b, 180));
    gradient.setColorAt(1.0,  QColor(r, g, b, 255));

    painter.fillRect(0, 0, w, targetHeight, gradient);
}
