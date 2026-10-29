#ifndef TEXTOUTLINEEFFECT_H
#define TEXTOUTLINEEFFECT_H

#include <QColor>
#include <QGraphicsEffect>
#include <QPixmap>
#include <QPointF>


class TextOutlineEffect : public QGraphicsEffect
{
    Q_OBJECT
public:
    explicit TextOutlineEffect(QObject* parent = nullptr);
    void setOutlineColor(const QColor& color);
    void setOutlineWidth(qreal width);
    void setShadowColor(const QColor& color);
    void setShadowRadius(qreal radius);
    void setShadowOffset(const QPointF& offset);

protected:
    QRectF boundingRectFor(const QRectF& sourceRect) const override;
    void draw(QPainter* painter) override;

private:
    void updateEnabledState();
    QColor m_color;
    qreal m_width = 1.0;
    qint64 m_outlineSourceKey = 0;
    QPixmap m_outline;
    QColor m_shadowColor;
    qreal m_shadowRadius = 0.0;
    QPointF m_shadowOffset;
    qint64 m_shadowSourceKey = 0;
    QPixmap m_shadow;
};

#endif 
