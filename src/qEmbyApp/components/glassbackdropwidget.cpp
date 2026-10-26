#include "glassbackdropwidget.h"
#include "../utils/imageutils.h"

#include <QEvent>
#include <QPainter>
#include <QDebug>
#include <QtMath>

GlassBackdropWidget::GlassBackdropWidget(QWidget* parent)
    : QWidget(parent), m_tintColor(palette().window().color())
{
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
}

void GlassBackdropWidget::setBackdropSource(DetailContentWidget* source)
{
    if (m_source == source) return;
    if (m_source) disconnect(m_source.data(), nullptr, this, nullptr);
    m_source = source;
    m_logAppearance = true;
    if (source) {
        connect(source, &DetailContentWidget::backdropFrameChanged,
                this, &GlassBackdropWidget::invalidateBackdrop);
        connect(source, &QObject::destroyed, this,
                &GlassBackdropWidget::invalidateBackdrop);
    }
    invalidateBackdrop();
}

void GlassBackdropWidget::setEffectTarget(QWidget* target)
{
    if (m_target == target) return;
    if (m_target) {
        m_target->removeEventFilter(this);
        disconnect(m_target.data(), nullptr, this, nullptr);
    }
    m_target = target;
    if (target) {
        target->installEventFilter(this);
        connect(target, &QObject::destroyed, this,
                &GlassBackdropWidget::invalidateBackdrop);
    }
    invalidateBackdrop();
}

void GlassBackdropWidget::setTintColor(const QColor& color)
{
    if (m_tintColor == color) return;
    m_tintColor = color;
    m_logAppearance = true;
    m_layerDirty = true;
    update();
}

void GlassBackdropWidget::setLightAnchor(QWidget* anchor)
{
    if (m_lightAnchor == anchor) return;
    if (m_lightAnchor) {
        m_lightAnchor->removeEventFilter(this);
        disconnect(m_lightAnchor.data(), nullptr, this, nullptr);
    }
    m_lightAnchor = anchor;
    if (anchor) {
        anchor->installEventFilter(this);
        connect(anchor, &QObject::destroyed, this,
                &GlassBackdropWidget::invalidateBackdrop);
    }
    invalidateBackdrop();
}

void GlassBackdropWidget::setCornerLight(bool enabled)
{
    if (m_cornerLight == enabled) return;
    m_cornerLight = enabled;
    m_logAppearance = true;
    invalidateBackdrop();
}

void GlassBackdropWidget::setBlurRadius(qreal radius)
{
    radius = qBound(qreal(0), radius, qreal(32));
    if (qFuzzyCompare(m_blurRadius, radius)) return;
    m_blurRadius = radius;
    m_logAppearance = true;
    invalidateBackdrop();
}

void GlassBackdropWidget::setFadeOpacity(qreal opacity)
{
    opacity = qBound(qreal(0), opacity, qreal(1));
    if (qFuzzyCompare(m_fadeOpacity, opacity)) return;
    m_fadeOpacity = opacity;
    m_logAppearance = true;
    m_layerDirty = true;
    update();
}

void GlassBackdropWidget::setHorizontalSpread(qreal spread)
{
    m_horizontalSpread = qBound(qreal(0), spread, qreal(240));
    m_logAppearance = true;
    invalidateBackdrop();
}

void GlassBackdropWidget::setVerticalSpread(qreal spread)
{
    m_verticalSpread = qBound(qreal(0), spread, qreal(160));
    m_logAppearance = true;
    invalidateBackdrop();
}

void GlassBackdropWidget::invalidateBackdrop()
{
    m_dirty = true;
    m_layerDirty = true;
    if (m_source && m_target && parentWidget() == m_source.data() &&
        m_source->isAncestorOf(m_target.data())) {
        const QRectF targetArea(m_target->mapTo(m_source.data(), QPoint()), m_target->size());
        QRectF effectArea = targetArea.adjusted(
            -m_horizontalSpread, -m_verticalSpread, m_horizontalSpread, m_verticalSpread);
        if (m_cornerLight) {
            QRectF anchorArea = targetArea;
            if (m_lightAnchor && m_source->isAncestorOf(m_lightAnchor.data())) {
                anchorArea = QRectF(m_lightAnchor->mapTo(m_source.data(), QPoint()),
                                    m_lightAnchor->size());
            }
            
            
            
            const QPointF origin(0, anchorArea.bottom() + m_verticalSpread * 0.35);
            const qreal radiusX = qMax(qreal(1), targetArea.right()
                                                   + m_horizontalSpread * 2);
            const qreal radiusY = qMax(qreal(1), origin.y() - targetArea.top()
                                                   + m_verticalSpread);
            effectArea = QRectF(origin.x() - radiusX, origin.y() - radiusY,
                                radiusX * 2, radiusY * 2);
        }
        const QRect canvas = effectArea.toAlignedRect().intersected(m_source->rect());
        m_effectArea = effectArea.translated(-canvas.topLeft());
        setGeometry(canvas);
        setVisible(m_target->isVisible() && !canvas.isEmpty());
    } else {
        hide();
    }
    update();
}

bool GlassBackdropWidget::eventFilter(QObject* watched, QEvent* event)
{
    if ((watched == m_target || watched == m_lightAnchor) &&
        (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
         event->type() == QEvent::Show || event->type() == QEvent::Hide)) {
        if (event->type() == QEvent::Hide) hide();
        else invalidateBackdrop();
    }
    return QWidget::eventFilter(watched, event);
}

bool GlassBackdropWidget::event(QEvent* event)
{
    if (event->type() == QEvent::Move || event->type() == QEvent::Resize ||
        event->type() == QEvent::Show) {
        m_dirty = true;
        m_layerDirty = true;
        update();
    }
    return QWidget::event(event);
}

void GlassBackdropWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    const int padding = qCeil(m_blurRadius) + 12;
    
    
    const qreal sampleDpr = m_blurRadius > 0
        ? devicePixelRatioF() / 4 : devicePixelRatioF();
    if (m_source && m_source->isAncestorOf(this)) {
        const QRect region(mapTo(m_source.data(), QPoint(-padding, -padding)),
                           size() + QSize(padding * 2, padding * 2));
        if (m_dirty || region != m_sampleRegion || !qFuzzyCompare(sampleDpr, m_sampleDpr)) {
            m_blurredBackdrop = ImageUtils::blurredPixmap(
                m_source->backdropRegion(region, sampleDpr), m_blurRadius);
            m_sampleRegion = region;
            m_sampleDpr = sampleDpr;
            m_layerDirty = true;
        }
    } else {
        if (!m_blurredBackdrop.isNull()) m_layerDirty = true;
        m_blurredBackdrop = {};
    }
    m_dirty = false;

    const qreal dpr = devicePixelRatioF();
    const QSize pixelSize(qCeil(width() * dpr), qCeil(height() * dpr));
    if (m_layerDirty || m_glassLayer.size() != pixelSize ||
        !qFuzzyCompare(m_glassLayer.devicePixelRatioF(), dpr)) {
        QPixmap layer(pixelSize);
        layer.setDevicePixelRatio(dpr);
        layer.fill(Qt::transparent);
        {
            QPainter paintLayer(&layer);
            paintLayer.setRenderHint(QPainter::Antialiasing);
            paintLayer.setRenderHint(QPainter::SmoothPixmapTransform);
            
            
            const QPixmap backdrop = m_cornerLight
                ? ImageUtils::adaptiveLightPixmap(m_blurredBackdrop, m_tintColor)
                : m_blurredBackdrop;
            if (!backdrop.isNull()) {
                paintLayer.drawPixmap(QRectF(-padding, -padding,
                                             width() + padding * 2, height() + padding * 2),
                                       backdrop, QRectF(backdrop.rect()));
            }
            if (!m_cornerLight) paintLayer.fillRect(rect(), m_tintColor);
        }
        
        m_glassLayer = ImageUtils::radiallyFadedPixmap(layer, m_effectArea, m_fadeOpacity);
        m_layerDirty = false;
        if (m_logAppearance) {
            qDebug() << "[GlassBackdrop] Appearance | tintAlpha=" << m_tintColor.alpha()
                     << "| cornerLight=" << m_cornerLight
                     << "| maximumTintAlpha=" << qRound(m_tintColor.alpha() * m_fadeOpacity)
                     << "| fadeOpacity=" << m_fadeOpacity << "| blurRadius=" << m_blurRadius
                     << "| effectArea=" << m_effectArea << "| canvasSize=" << size()
                     << "| backdropAvailable=" << !m_blurredBackdrop.isNull();
            m_logAppearance = false;
        }
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.drawPixmap(0, 0, m_glassLayer);
}
