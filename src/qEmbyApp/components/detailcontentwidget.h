#ifndef DETAILCONTENTWIDGET_H
#define DETAILCONTENTWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QColor>
#include <QPointer>
#include <QRectF>

class QVariantAnimation;
class QPainter;

class DetailContentWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor baseColor READ baseColor WRITE setBaseColor)
    Q_PROPERTY(QColor gradientColor READ gradientColor WRITE setGradientColor)
    Q_PROPERTY(QColor pageShadowColor READ pageShadowColor WRITE setPageShadowColor)

public:
    explicit DetailContentWidget(QWidget* parent = nullptr);

    
    void setBackdrop(const QPixmap& pix);
    
    void setBackdropWithTransition(const QPixmap& pix, int direction = 1);
    
    
    void setBackdropFadeAnchor(QWidget* anchor);
    
    QPixmap backdropRegion(const QRect& region, qreal devicePixelRatio);
    bool isBackdropTransitionRunning() const;
    qreal backdropTransitionProgress() const { return m_transitionProgress; }
    int backdropTransitionDirection() const { return m_transitionDirection; }
    QRectF backdropTransitionBounds() const;
    QSize backdropSnapshotSize() const;
    void finishBackdropTransition();

    
    QColor baseColor() const;
    void setBaseColor(const QColor& color);

    QColor gradientColor() const;
    void setGradientColor(const QColor& color);
    QColor pageShadowColor() const { return m_pageShadowColor; }
    void setPageShadowColor(const QColor& color);

Q_SIGNALS:
    void backdropFrameChanged();

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void paintBackdrop(QPainter& painter);
    int paintBackdropLayers(QPainter& painter);
    QPixmap m_backdrop;
    QPixmap m_scaledBackdrop;
    int m_cachedWidth;
    QPixmap m_previousBackdrop;
    QPixmap m_scaledPreviousBackdrop;
    QVariantAnimation* m_transition = nullptr;
    qreal m_transitionProgress = 1.0;
    int m_transitionDirection = 1;
    QPointer<QWidget> m_fadeAnchor;
    QColor m_pageShadowColor;

    
    QColor m_baseColor;
    QColor m_gradientColor;
};

#endif 
