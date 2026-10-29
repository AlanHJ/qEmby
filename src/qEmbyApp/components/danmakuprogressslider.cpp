#include "danmakuprogressslider.h"
#include "../utils/danmakuheatmaputils.h"

#include <QDebug>
#include <QCursor>
#include <QPainter>
#include <QPalette>
#include <algorithm>
#include <cmath>

DanmakuProgressSlider::DanmakuProgressSlider(QWidget *parent)
    : ModernSlider(Qt::Horizontal, parent) {}

int DanmakuProgressSlider::positionAt(qreal x) const
{
    const int padding = std::max(normalHandleRadius(), activeHandleRadius());
    const qreal trackWidth = width() - 2.0 * padding;
    if (trackWidth <= 0.0 || maximum() <= minimum()) return sliderPosition();
    const qreal ratio = std::clamp((x - padding) / trackWidth, 0.0, 1.0);
    return qRound(minimum() + ratio * (static_cast<double>(maximum()) - minimum()));
}

void DanmakuProgressSlider::seekAt(qreal x)
{
    
    
    setSliderPosition(positionAt(x));
}

void DanmakuProgressSlider::updateHoverPosition(const QPoint &globalPosition)
{
    const QPoint localPosition = mapFromGlobal(globalPosition);
    const int padding = std::max(normalHandleRadius(), activeHandleRadius());
    if (!isVisible() || !isEnabled() || m_durationSeconds <= 0.0 ||
        maximum() <= minimum() || width() <= 2 * padding || !rect().contains(localPosition)) {
        clearHoverPosition();
        return;
    }
    m_hovering = true;
    const QPoint trackPosition(qBound(padding, localPosition.x(), width() - padding),
                               qRound(horizontalTrackCenter()));
    emit hoverPositionChanged(positionAt(localPosition.x()), mapToGlobal(trackPosition));
}

void DanmakuProgressSlider::clearHoverPosition()
{
    if (!m_hovering) return;
    m_hovering = false;
    emit hoverEnded();
}

void DanmakuProgressSlider::refreshHoverPosition()
{
    if (m_hovering) updateHoverPosition(QCursor::pos());
}

bool DanmakuProgressSlider::event(QEvent *event)
{
    if (event->type() == QEvent::Enter || event->type() == QEvent::ToolTip) {
        updateHoverPosition(QCursor::pos());
        if (event->type() == QEvent::ToolTip) {
            event->accept();
            return true;
        }
    } else if (event->type() == QEvent::Leave || event->type() == QEvent::Hide ||
               (event->type() == QEvent::EnabledChange && !isEnabled())) {
        clearHoverPosition();
    }
    return ModernSlider::event(event);
}

void DanmakuProgressSlider::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        ModernSlider::mousePressEvent(event);
        return;
    }
    
    
    setSliderDown(true);
    seekAt(event->position().x());
    update();
    event->accept();
}

void DanmakuProgressSlider::mouseMoveEvent(QMouseEvent *event)
{
    updateHoverPosition(event->globalPosition().toPoint());
    if (isSliderDown()) {
        seekAt(event->position().x());
        event->accept();
    } else {
        ModernSlider::mouseMoveEvent(event);
    }
}

void DanmakuProgressSlider::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && isSliderDown()) {
        setSliderDown(false);
        update();
        event->accept();
    } else {
        ModernSlider::mouseReleaseEvent(event);
    }
}

void DanmakuProgressSlider::setComments(const QList<DanmakuComment> &comments)
{
    m_timesMs.clear();
    m_timesMs.reserve(comments.size());
    for (const auto &comment : comments) {
        if (comment.isValid()) m_timesMs.append(comment.timeMs);
    }
    rebuildHeatmap();
}

void DanmakuProgressSlider::setMediaDuration(double seconds)
{
    if (!std::isfinite(seconds) || seconds < 0.0) seconds = 0.0;
    if (m_durationSeconds == seconds) return;
    m_durationSeconds = seconds;
    if (m_durationSeconds <= 0.0) clearHoverPosition();
    rebuildHeatmap();
}

void DanmakuProgressSlider::setDanmakuOffset(int milliseconds)
{
    if (m_offsetMs == milliseconds) return;
    m_offsetMs = milliseconds;
    rebuildHeatmap();
}

QColor DanmakuProgressSlider::heatmapColor() const
{
    return m_heatmapColor.isValid() ? m_heatmapColor : palette().color(QPalette::Highlight);
}

void DanmakuProgressSlider::setHeatmapColor(const QColor &color)
{
    m_heatmapColor = color;
    update();
}

void DanmakuProgressSlider::rebuildHeatmap()
{
    m_heatmap = DanmakuHeatmapUtils::build(m_timesMs, m_durationSeconds, m_offsetMs);
    emit heatmapChanged(m_heatmap);
    qDebug() << "[Danmaku][Heatmap] Updated | comments:" << m_timesMs.size()
             << "| durationSeconds:" << m_durationSeconds << "| offsetMs:" << m_offsetMs
             << "| visible:" << !m_heatmap.isEmpty();
    update();
}

qreal DanmakuProgressSlider::horizontalTrackCenter() const
{
    return std::max(height() / 2.0,
        height() - static_cast<double>(std::max(normalHandleRadius(), activeHandleRadius()) + 2));
}

void DanmakuProgressSlider::paintTrackBackground(QPainter &painter, const QRectF &trackRect)
{
    if (m_heatmap.isEmpty() || trackRect.width() <= 0.0) return;
    const qreal waveHeight = std::max(0.0, horizontalTrackCenter() - 3.0);
    DanmakuHeatmapUtils::paint(painter, m_heatmap,
        QRectF(trackRect.left(), horizontalTrackCenter() - waveHeight, trackRect.width(), waveHeight),
        heatmapColor());
}
