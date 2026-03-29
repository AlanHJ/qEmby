#ifndef DETAILCONTENTWIDGET_H
#define DETAILCONTENTWIDGET_H

#include <QWidget>
#include <QPixmap>
#include <QColor>

class DetailContentWidget : public QWidget {
    Q_OBJECT
    Q_PROPERTY(QColor baseColor READ baseColor WRITE setBaseColor)
    Q_PROPERTY(QColor gradientColor READ gradientColor WRITE setGradientColor)

public:
    explicit DetailContentWidget(QWidget* parent = nullptr);

    
    void setBackdrop(const QPixmap& pix);

    
    QColor baseColor() const;
    void setBaseColor(const QColor& color);

    QColor gradientColor() const;
    void setGradientColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPixmap m_backdrop;
    QPixmap m_scaledBackdrop;
    int m_cachedWidth;

    
    QColor m_baseColor;
    QColor m_gradientColor;
};

#endif 
