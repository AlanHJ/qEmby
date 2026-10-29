#ifndef GLASSBACKDROPWIDGET_H
#define GLASSBACKDROPWIDGET_H

#include <QColor>
#include <QPixmap>
#include <QPointer>
#include <QRect>
#include <QWidget>
#include "detailcontentwidget.h"


class GlassBackdropWidget : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(QColor tintColor READ tintColor WRITE setTintColor)
    Q_PROPERTY(qreal blurRadius READ blurRadius WRITE setBlurRadius)
    Q_PROPERTY(qreal fadeOpacity READ fadeOpacity WRITE setFadeOpacity)
    Q_PROPERTY(qreal horizontalSpread READ horizontalSpread WRITE setHorizontalSpread)
    Q_PROPERTY(qreal verticalSpread READ verticalSpread WRITE setVerticalSpread)
    Q_PROPERTY(bool cornerLight READ cornerLight WRITE setCornerLight)
public:
    explicit GlassBackdropWidget(QWidget* parent = nullptr);
    void setBackdropSource(DetailContentWidget* source);
    void setEffectTarget(QWidget* target);
    
    void setLightAnchor(QWidget* anchor);
    QColor tintColor() const { return m_tintColor; }
    void setTintColor(const QColor& color);
    qreal blurRadius() const { return m_blurRadius; }
    void setBlurRadius(qreal radius);
    qreal fadeOpacity() const { return m_fadeOpacity; }
    void setFadeOpacity(qreal opacity);
    qreal horizontalSpread() const { return m_horizontalSpread; }
    void setHorizontalSpread(qreal spread);
    qreal verticalSpread() const { return m_verticalSpread; }
    void setVerticalSpread(qreal spread);
    bool cornerLight() const { return m_cornerLight; }
    void setCornerLight(bool enabled);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void invalidateBackdrop();
    QPointer<DetailContentWidget> m_source;
    QPointer<QWidget> m_target;
    QPointer<QWidget> m_lightAnchor;
    QRectF m_effectArea;
    QPixmap m_blurredBackdrop;
    QPixmap m_glassLayer;
    QRect m_sampleRegion;
    qreal m_sampleDpr = 0;
    bool m_dirty = true;
    bool m_layerDirty = true;
    bool m_logAppearance = true;
    bool m_cornerLight = false;
    QColor m_tintColor;
    qreal m_blurRadius = 14;
    qreal m_fadeOpacity = 0.75;
    qreal m_horizontalSpread = 128;
    qreal m_verticalSpread = 64;
};

#endif 
