#include "pageturnoverlaywidget.h"
#include "../utils/pageturnutils.h"

#include <QGraphicsEffect>
#include <QPainter>
#include <QRegion>
#include <QtMath>
#include <utility>

PageTurnOverlayWidget::PageTurnOverlayWidget(DetailContentWidget* source)
    : QWidget(source), m_source(source)
{
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
    connect(source, &DetailContentWidget::backdropFrameChanged,
            this, &PageTurnOverlayWidget::synchronize);
    connect(source, &QObject::destroyed, this, &PageTurnOverlayWidget::clearFrames);
}

QPixmap PageTurnOverlayWidget::captureWidgets(
    const QList<QWidget*>& widgets, const QSize& sceneSize) const
{
    if (!m_source || sceneSize.isEmpty()) return {};
    const qreal dpr = m_source->devicePixelRatioF();
    QPixmap frame(qCeil(sceneSize.width() * dpr), qCeil(sceneSize.height() * dpr));
    frame.setDevicePixelRatio(dpr);
    frame.fill(Qt::transparent);
    QPainter painter(&frame);
    for (QWidget* widget : widgets) {
        if (!widget || !m_source->isAncestorOf(widget)) continue;
        
        
        widget->ensurePolished();
        
        
        
        
        QList<QWidget*> effectWidgets = widget->findChildren<QWidget*>();
        effectWidgets.prepend(widget);
        for (QWidget* effectWidget : effectWidgets) {
            if (auto* effect = effectWidget->graphicsEffect(); effect && effect->isEnabled()) {
                effect->setEnabled(false);
                effect->setEnabled(true);
            }
        }
        widget->render(&painter, widget->mapTo(m_source.data(), QPoint()),
                       QRegion(), QWidget::DrawChildren);
    }
    return frame;
}

void PageTurnOverlayWidget::setFrames(QPixmap outgoing, QPixmap incoming)
{
    m_outgoing = std::move(outgoing);
    m_incoming = std::move(incoming);
    m_dirty = true;
    updateCanvas();
    
}

void PageTurnOverlayWidget::setIncomingFrame(QPixmap incoming)
{
    if (!hasFrames()) return;
    m_incoming = std::move(incoming);
    m_dirty = true;
    updateCanvas();
    update();
}

void PageTurnOverlayWidget::updateCanvas()
{
    if (!hasFrames()) return;
    const QSizeF outgoingSize = QSizeF(m_outgoing.size()) / m_outgoing.devicePixelRatioF();
    const QSizeF incomingSize = QSizeF(m_incoming.size()) / m_incoming.devicePixelRatioF();
    const QSizeF canvas = outgoingSize.expandedTo(incomingSize);
    setGeometry(0, 0, qCeil(canvas.width()), qCeil(canvas.height()));
}

void PageTurnOverlayWidget::clearFrames()
{
    const bool hadFrames = hasFrames();
    m_outgoing = {};
    m_incoming = {};
    m_frame = {};
    m_dirty = true;
    hide();
    if (hadFrames) Q_EMIT finished();
}

void PageTurnOverlayWidget::synchronize()
{
    if (!hasFrames()) return;
    if (!m_source || !m_source->isBackdropTransitionRunning()) {
        clearFrames();
        return;
    }
    m_dirty = true;
    show();
    update();
}

QPixmap PageTurnOverlayWidget::currentFrame()
{
    if (!hasFrames() || !m_source) return {};
    if (!m_dirty) return m_frame;
    const qreal dpr = m_source->devicePixelRatioF();
    m_frame = QPixmap(qCeil(width() * dpr), qCeil(height() * dpr));
    m_frame.setDevicePixelRatio(dpr);
    m_frame.fill(Qt::transparent);
    {
        QPainter painter(&m_frame);
        painter.drawPixmap(0, 0, m_incoming);
        PageTurnUtils::PageTurnStyle style;
        style.shadowColor = m_source->pageShadowColor();
        style.paperColor = m_source->gradientColor();
        style.foregroundOnly = true;
        PageTurnUtils::drawTurningPage(
            painter, m_outgoing, m_source->backdropTransitionBounds(),
            m_source->backdropTransitionProgress(), m_source->backdropTransitionDirection(), style);
    }
    m_dirty = false;
    return m_frame;
}

void PageTurnOverlayWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.drawPixmap(0, 0, currentFrame());
}
