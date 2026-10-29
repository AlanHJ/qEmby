#ifndef CAROUSELNAVIGATIONBUTTON_H
#define CAROUSELNAVIGATIONBUTTON_H

#include <QColor>
#include <QPushButton>

class QPropertyAnimation;
class QShowEvent;
class QHideEvent;



class CarouselNavigationButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(QColor symbolColor READ symbolColor WRITE setSymbolColor)
    Q_PROPERTY(QColor activeSymbolColor READ activeSymbolColor WRITE setActiveSymbolColor)
    Q_PROPERTY(QColor disabledSymbolColor READ disabledSymbolColor WRITE setDisabledSymbolColor)
    Q_PROPERTY(qreal rotationAngle READ rotationAngle WRITE setRotationAngle)
public:
    enum class Symbol { Previous, Next, Dot, Refresh };

    explicit CarouselNavigationButton(Symbol symbol, QWidget* parent = nullptr);
    QColor symbolColor() const { return m_symbolColor; }
    void setSymbolColor(const QColor& color);
    QColor activeSymbolColor() const { return m_activeSymbolColor; }
    void setActiveSymbolColor(const QColor& color);
    QColor disabledSymbolColor() const { return m_disabledSymbolColor; }
    void setDisabledSymbolColor(const QColor& color);
    void setSpinning(bool spinning);
    qreal rotationAngle() const { return m_rotationAngle; }
    void setRotationAngle(qreal angle);

protected:
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    Symbol m_symbol;
    QColor m_symbolColor;
    QColor m_activeSymbolColor;
    QColor m_disabledSymbolColor;
    QPropertyAnimation* m_rotationAnimation = nullptr;
    qreal m_rotationAngle = 0.0;
    bool m_spinning = false;
};

#endif 
