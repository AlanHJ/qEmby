#include "textoutlineeffect.h"
#include "../utils/imageutils.h"

#include <QPainter>
#include <QPixmap>
#include <QtMath>

TextOutlineEffect::TextOutlineEffect(QObject* parent)
    : QGraphicsEffect(parent)
{
    updateEnabledState();
}

void TextOutlineEffect::updateEnabledState()
{
    const bool hasOutline = m_width > 0 && m_color.isValid() && m_color.alpha() > 0;
    const bool hasShadow = m_shadowColor.isValid() && m_shadowColor.alpha() > 0;
    
    
    
    setEnabled(hasOutline || hasShadow);
}

void TextOutlineEffect::setOutlineColor(const QColor& color)
{
    if (m_color == color) return;
    m_color = color;
    m_outline = {};
    updateEnabledState();
    update();
}

void TextOutlineEffect::setOutlineWidth(qreal width)
{
    width = qBound(qreal(0), width, qreal(4));
    if (qFuzzyCompare(m_width, width)) return;
    m_width = width;
    m_outline = {};
    updateEnabledState();
    updateBoundingRect();
    update();
}

void TextOutlineEffect::setShadowColor(const QColor& color)
{
    if (m_shadowColor == color) return;
    m_shadowColor = color;
    m_shadow = {};
    updateEnabledState();
    updateBoundingRect();
    update();
}

void TextOutlineEffect::setShadowRadius(qreal radius)
{
    radius = qBound(qreal(0), radius, qreal(12));
    if (qFuzzyCompare(m_shadowRadius, radius)) return;
    m_shadowRadius = radius;
    m_shadow = {};
    updateBoundingRect();
    update();
}

void TextOutlineEffect::setShadowOffset(const QPointF& offset)
{
    if (m_shadowOffset == offset) return;
    m_shadowOffset = offset;
    updateBoundingRect();
    update();
}

QRectF TextOutlineEffect::boundingRectFor(const QRectF& sourceRect) const
{
    const qreal padding = qCeil(m_width) + 1;
    QRectF result = sourceRect.adjusted(-padding, -padding, padding, padding);
    if (m_shadowColor.isValid() && m_shadowColor.alpha() > 0) {
        const qreal spread = qCeil(m_shadowRadius) + 1;
        result = result.united(sourceRect.translated(m_shadowOffset)
            .adjusted(-spread, -spread, spread, spread));
    }
    return result;
}

void TextOutlineEffect::draw(QPainter* painter)
{
    const bool hasOutline = m_width > 0 && m_color.isValid() && m_color.alpha() > 0;
    const bool hasShadow = m_shadowColor.isValid() && m_shadowColor.alpha() > 0;
    
    
    QPoint offset;
    const QPixmap source = sourcePixmap(Qt::LogicalCoordinates, &offset,
                                         PadToEffectiveBoundingRect);
    if (source.isNull()) {
        
        drawSource(painter);
        return;
    }
    painter->save();
    painter->setRenderHint(QPainter::SmoothPixmapTransform);
    if (hasShadow) {
        if (m_shadow.isNull() || m_shadowSourceKey != source.cacheKey()) {
            m_shadow = ImageUtils::blurredAlphaPixmap(source, m_shadowColor, m_shadowRadius);
            m_shadowSourceKey = source.cacheKey();
        }
        painter->drawPixmap(QPointF(offset) + m_shadowOffset, m_shadow);
    }
    if (hasOutline) {
        if (m_outline.isNull() || m_outlineSourceKey != source.cacheKey()) {
            m_outline = ImageUtils::outlinedAlphaPixmap(source, m_color, m_width);
            m_outlineSourceKey = source.cacheKey();
        }
        painter->drawPixmap(offset, m_outline);
    }
    painter->drawPixmap(offset, source);
    painter->restore();
}
