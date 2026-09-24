#include "danmakuheatmapwidget.h"
#include "../utils/danmakuheatmaputils.h"
#include <QPainter>
#include <QPalette>

DanmakuHeatmapWidget::DanmakuHeatmapWidget(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setFocusPolicy(Qt::NoFocus);
}

void DanmakuHeatmapWidget::setHeatmapPath(const QPainterPath &path)
{
    m_path = path;
    update();
}

QColor DanmakuHeatmapWidget::heatmapColor() const
{
    return m_color.isValid() ? m_color : palette().color(QPalette::Highlight);
}

void DanmakuHeatmapWidget::setHeatmapColor(const QColor &color)
{
    m_color = color;
    update();
}

void DanmakuHeatmapWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    DanmakuHeatmapUtils::paint(painter, m_path, QRectF(rect()), heatmapColor());
}
