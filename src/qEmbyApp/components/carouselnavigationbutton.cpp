#include "carouselnavigationbutton.h"

#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>
#include <QPolygonF>
#include <QShowEvent>
#include <QHideEvent>
#include <QEasingCurve>
#include <QStyle>
#include <QStyleOptionButton>

CarouselNavigationButton::CarouselNavigationButton(Symbol symbol, QWidget* parent)
    : QPushButton(parent), m_symbol(symbol),
      m_symbolColor(palette().color(QPalette::ButtonText)),
      m_activeSymbolColor(palette().color(QPalette::HighlightedText)),
      m_disabledSymbolColor(palette().color(QPalette::Disabled, QPalette::ButtonText))
{
    setAttribute(Qt::WA_LayoutUsesWidgetRect, true);
    setAutoDefault(false);
    if (m_symbol == Symbol::Refresh) {
        
        m_rotationAnimation = new QPropertyAnimation(this, "rotationAngle", this);
        m_rotationAnimation->setStartValue(0.0);
        m_rotationAnimation->setEndValue(360.0);
        m_rotationAnimation->setDuration(900);
        m_rotationAnimation->setEasingCurve(QEasingCurve::Linear);
        connect(m_rotationAnimation, &QAbstractAnimation::finished, this,
                [this]() { setRotationAngle(0.0); });
    }
}

void CarouselNavigationButton::setSymbolColor(const QColor& color)
{
    if (m_symbolColor == color) return;
    m_symbolColor = color;
    update();
}

void CarouselNavigationButton::setActiveSymbolColor(const QColor& color)
{
    if (m_activeSymbolColor == color) return;
    m_activeSymbolColor = color;
    update();
}

void CarouselNavigationButton::setDisabledSymbolColor(const QColor& color)
{
    if (m_disabledSymbolColor == color) return;
    m_disabledSymbolColor = color;
    update();
}

void CarouselNavigationButton::setRotationAngle(qreal angle)
{
    m_rotationAngle = angle;
    update();
}

void CarouselNavigationButton::setSpinning(bool spinning)
{
    if (!m_rotationAnimation || m_spinning == spinning) return;
    m_spinning = spinning;
    if (spinning) {
        m_rotationAnimation->setLoopCount(-1);
        if (isVisible() && m_rotationAnimation->state() != QAbstractAnimation::Running)
            m_rotationAnimation->start();
    } else if (m_rotationAnimation->state() == QAbstractAnimation::Running) {
        
        
        m_rotationAnimation->setLoopCount(m_rotationAnimation->currentLoop() + 1);
    } else {
        m_rotationAnimation->stop();
        setRotationAngle(0.0);
    }
    update();
}

void CarouselNavigationButton::showEvent(QShowEvent* event)
{
    QPushButton::showEvent(event);
    if (m_rotationAnimation && m_spinning) {
        m_rotationAnimation->setLoopCount(-1);
        m_rotationAnimation->start();
    }
}

void CarouselNavigationButton::hideEvent(QHideEvent* event)
{
    if (m_rotationAnimation) {
        m_rotationAnimation->stop();
        setRotationAngle(0.0);
    }
    QPushButton::hideEvent(event);
}

void CarouselNavigationButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QStyleOptionButton option;
    initStyleOption(&option);
    QPainter painter(this);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QPointF center = QRectF(contentsRect()).center();
    
    const qreal symbolScale = qMin(qreal(1.0), contentsRect().height() / 24.0);
    painter.translate(center);
    painter.scale(symbolScale, symbolScale);
    
    const QColor color = m_spinning ? m_activeSymbolColor : !isEnabled() ? m_disabledSymbolColor
        : (isChecked() || underMouse() || hasFocus() ? m_activeSymbolColor : m_symbolColor);
    if (m_symbol == Symbol::Dot) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(color);
        const qreal radius = isChecked() ? 2.0 : 1.5;
        painter.drawEllipse(QPointF(0.0, 0.0), radius, radius);
        return;
    }

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (m_symbol == Symbol::Refresh) {
        
        
        painter.rotate(m_rotationAngle);
        const QRectF ring(-5.0, -5.0, 10.0, 10.0);
        QPainterPath refresh;
        refresh.arcMoveTo(ring, -15.0);
        refresh.arcTo(ring, -15.0, -300.0);
        const QPointF tip = refresh.currentPosition();
        refresh.moveTo(tip + QPointF(-2.5, 0.0));
        refresh.lineTo(tip);
        refresh.lineTo(tip + QPointF(0.0, -2.5));
        painter.drawPath(refresh);
        return;
    }
    const qreal direction = m_symbol == Symbol::Previous ? -1.0 : 1.0;
    const QPolygonF chevron {
        QPointF(-direction * 1.5, -3.0),
        QPointF(direction * 1.5, 0.0),
        QPointF(-direction * 1.5, 3.0)
    };
    painter.drawPolyline(chevron);
}
