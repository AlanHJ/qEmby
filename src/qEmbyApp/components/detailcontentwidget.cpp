#include "detailcontentwidget.h"
#include "../utils/pageturnutils.h"
#include <QPainter>
#include <QStyleOption>
#include <QLinearGradient>
#include <QHideEvent>
#include <QVariantAnimation>
#include <QDebug>
#include <QResizeEvent>
#include <QtMath>

DetailContentWidget::DetailContentWidget(QWidget* parent)
    : QWidget(parent),
    m_cachedWidth(-1),
    m_baseColor(QColor("#FFFFFF")),
    m_gradientColor(QColor(249, 250, 251))
{
    m_pageShadowColor = palette().shadow().color();
    
    setAttribute(Qt::WA_StyledBackground, true);
    m_transition = new QVariantAnimation(this);
    m_transition->setDuration(880);
    m_transition->setStartValue(0.0);
    m_transition->setEndValue(1.0);
    m_transition->setEasingCurve(QEasingCurve::InOutSine);
    connect(m_transition, &QVariantAnimation::valueChanged, this,
            [this](const QVariant& value) {
                m_transitionProgress = value.toReal();
                update();
                Q_EMIT backdropFrameChanged();
            });
    connect(m_transition, &QVariantAnimation::finished, this, [this]() {
        m_previousBackdrop = {};
        m_scaledPreviousBackdrop = {};
        qDebug() << "[DetailContentWidget] Page turn finished";
        update();
        Q_EMIT backdropFrameChanged();
    });
}

void DetailContentWidget::setBackdrop(const QPixmap& pix) {
    m_transition->stop();
    m_transitionProgress = 1.0;
    m_previousBackdrop = {};
    m_scaledPreviousBackdrop = {};
    m_backdrop = pix;
    m_cachedWidth = -1; 
    update();
    Q_EMIT backdropFrameChanged();
}

QPixmap DetailContentWidget::backdropRegion(const QRect& region, qreal dpr)
{
    if (region.isEmpty() || dpr <= 0) return {};
    QPixmap result(qMax(1, qCeil(region.width() * dpr)),
                   qMax(1, qCeil(region.height() * dpr)));
    result.setDevicePixelRatio(dpr);
    result.fill(m_baseColor);
    {
        QPainter painter(&result);
        painter.translate(-region.topLeft());
        paintBackdrop(painter);
    }
    return result;
}

void DetailContentWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    
    
    
    if (event->size().width() != event->oldSize().width() &&
        isBackdropTransitionRunning()) {
        finishBackdropTransition();
        return;
    }
    Q_EMIT backdropFrameChanged();
}

bool DetailContentWidget::isBackdropTransitionRunning() const
{
    return !m_previousBackdrop.isNull() && m_transitionProgress < 1.0;
}

QRectF DetailContentWidget::backdropTransitionBounds() const
{
    const QPixmap page = isBackdropTransitionRunning() ? m_previousBackdrop : m_backdrop;
    if (page.isNull()) return {};
    return QRectF(0, 0, width(), width() * page.height() / qMax(1, page.width()));
}

QSize DetailContentWidget::backdropSnapshotSize() const
{
    const int currentHeight = width() * m_backdrop.height() / qMax(1, m_backdrop.width());
    const int previousHeight = width() * m_previousBackdrop.height()
        / qMax(1, m_previousBackdrop.width());
    return QSize(width(), qMax(currentHeight, previousHeight));
}

void DetailContentWidget::finishBackdropTransition()
{
    m_transition->stop();
    m_transitionProgress = 1.0;
    m_previousBackdrop = {};
    m_scaledPreviousBackdrop = {};
    update();
    Q_EMIT backdropFrameChanged();
}

void DetailContentWidget::setBackdropWithTransition(const QPixmap& pix,
                                                   int direction)
{
    
    if (pix.isNull() || m_backdrop.isNull() || !isVisible()) {
        setBackdrop(pix);
        return;
    }
    QPixmap outgoing = m_backdrop;
    const bool interrupted = m_transition->state() == QAbstractAnimation::Running;
    if (interrupted && width() > 0) {
        
        
        const int imageHeight = backdropSnapshotSize().height();
        if (imageHeight > 0) {
            outgoing = QPixmap(width(), imageHeight);
            outgoing.fill(m_baseColor);
            QPainter snapshot(&outgoing);
            paintBackdropLayers(snapshot);
        }
    }
    m_transition->stop();
    m_previousBackdrop = outgoing;
    m_backdrop = pix;
    m_cachedWidth = -1;
    m_transitionDirection = direction < 0 ? -1 : 1;
    m_transitionProgress = 0.0;
    qDebug() << "[DetailContentWidget] Page turn started | direction="
             << m_transitionDirection << "| interrupted=" << interrupted
             << "| durationMs=" << m_transition->duration();
    m_transition->start();
    Q_EMIT backdropFrameChanged();
}

void DetailContentWidget::setBackdropFadeAnchor(QWidget* anchor)
{
    if (m_fadeAnchor) m_fadeAnchor->removeEventFilter(this);
    m_fadeAnchor = anchor;
    if (m_fadeAnchor) m_fadeAnchor->installEventFilter(this);
    update();
    Q_EMIT backdropFrameChanged();
}

bool DetailContentWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_fadeAnchor &&
        (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
         event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        update();
        Q_EMIT backdropFrameChanged();
    }
    return QWidget::eventFilter(watched, event);
}

void DetailContentWidget::setPageShadowColor(const QColor& color)
{
    if (m_pageShadowColor != color) {
        m_pageShadowColor = color;
        update();
        Q_EMIT backdropFrameChanged();
    }
}

void DetailContentWidget::hideEvent(QHideEvent* event)
{
    finishBackdropTransition();
    QWidget::hideEvent(event);
}

QColor DetailContentWidget::baseColor() const {
    return m_baseColor;
}

void DetailContentWidget::setBaseColor(const QColor& color) {
    if (m_baseColor != color) {
        m_baseColor = color;
        update(); 
        Q_EMIT backdropFrameChanged();
    }
}

QColor DetailContentWidget::gradientColor() const {
    return m_gradientColor;
}

void DetailContentWidget::setGradientColor(const QColor& color) {
    if (m_gradientColor != color) {
        m_gradientColor = color;
        update();
        Q_EMIT backdropFrameChanged();
    }
}

void DetailContentWidget::paintEvent(QPaintEvent* event) {
    Q_UNUSED(event);

    
    QStyleOption opt;
    opt.initFrom(this);
    QPainter painter(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &painter, this);

    paintBackdrop(painter);
}

void DetailContentWidget::paintBackdrop(QPainter& painter)
{
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    
    painter.fillRect(rect(), m_baseColor);

    if (m_backdrop.isNull()) return;

    const int targetHeight = paintBackdropLayers(painter);
    const int w = width();
    if (targetHeight <= 0) return;

    
    
    const bool anchored = m_fadeAnchor && !m_fadeAnchor->isHidden();
    const int fadeStart = anchored
        ? qMax(0, m_fadeAnchor->mapTo(this, QPoint(0, m_fadeAnchor->height())).y())
        : 0;
    if (fadeStart >= targetHeight) return;
    QLinearGradient gradient(0, fadeStart, 0, targetHeight);
    const int r = m_gradientColor.red();
    const int g = m_gradientColor.green();
    const int b = m_gradientColor.blue();
    gradient.setColorAt(0.0, QColor(r, g, b, 0));
    gradient.setColorAt(anchored ? 0.20 : 0.15,
                        QColor(r, g, b, anchored ? 85 : 150));
    gradient.setColorAt(anchored ? 0.60 : 0.4, QColor(r, g, b, 180));
    gradient.setColorAt(1.0, QColor(r, g, b, 255));
    painter.fillRect(0, fadeStart, w, targetHeight - fadeStart, gradient);
}

int DetailContentWidget::paintBackdropLayers(QPainter& painter)
{
    const int w = width();
    int imgW = m_backdrop.width();
    int imgH = m_backdrop.height();
    if (imgW <= 0 || w <= 0) return 0;

    
    int targetHeight = w * imgH / imgW;

    
    if (w != m_cachedWidth) {
        m_scaledBackdrop = m_backdrop.scaled(w, targetHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        if (!m_previousBackdrop.isNull()) {
            m_scaledPreviousBackdrop = m_previousBackdrop.scaledToWidth(
                w, Qt::SmoothTransformation);
        }
        m_cachedWidth = w;
    }

    
    if (!m_previousBackdrop.isNull() && m_transitionProgress < 1.0) {
        
        painter.drawPixmap(0, 0, m_scaledBackdrop);
        const int previousHeight = m_scaledPreviousBackdrop.height();
        const PageTurnUtils::PageTurnStyle pageStyle {
            m_pageShadowColor, m_gradientColor.lighter(130)};
        PageTurnUtils::drawTurningPage(
            painter, m_scaledPreviousBackdrop, QRectF(0, 0, w, previousHeight),
            m_transitionProgress, m_transitionDirection, pageStyle);
        targetHeight = qMax(targetHeight, previousHeight);
    } else {
        painter.drawPixmap(0, 0, m_scaledBackdrop);
    }

    return targetHeight;
}
