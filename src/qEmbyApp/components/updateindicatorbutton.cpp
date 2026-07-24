#include "updateindicatorbutton.h"

#include <QHideEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPropertyAnimation>
#include <QShowEvent>
#include <QStyle>
#include <QStyleOptionButton>

UpdateIndicatorButton::UpdateIndicatorButton(QWidget *parent)
    : QPushButton(parent),
      m_arrowAnimation(new QPropertyAnimation(this, "arrowProgress", this))
{
    setFixedSize(26, 26);
    setCursor(Qt::PointingHandCursor);

    m_arrowAnimation->setStartValue(0.0);
    m_arrowAnimation->setEndValue(1.0);
    m_arrowAnimation->setDuration(980);
    m_arrowAnimation->setEasingCurve(QEasingCurve::Linear);
    m_arrowAnimation->setLoopCount(-1);
}

qreal UpdateIndicatorButton::arrowProgress() const
{
    return m_arrowProgress;
}

void UpdateIndicatorButton::setArrowProgress(qreal progress)
{
    if (qFuzzyCompare(m_arrowProgress, progress)) {
        return;
    }
    m_arrowProgress = progress;
    update();
}

void UpdateIndicatorButton::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);

    QStyleOptionButton option;
    initStyleOption(&option);
    option.text.clear();
    option.icon = QIcon();

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    style()->drawControl(QStyle::CE_PushButton, &option, &painter, this);

    const QPointF center(width() / 2.0, height() / 2.0);
    const QColor circleColor = underMouse() ? QColor(QStringLiteral("#16A34A"))
                                            : QColor(QStringLiteral("#22C55E"));
    painter.setPen(Qt::NoPen);
    painter.setBrush(circleColor);
    painter.drawEllipse(center, 7.5, 7.5);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(QStringLiteral("#FFFFFF")), 1.5,
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));

    QPainterPath iconClip;
    iconClip.addEllipse(center, 7.1, 7.1);
    painter.setClipPath(iconClip);

    auto drawArrowAt = [&painter, center](qreal y) {
        const qreal x = center.x();
        painter.drawLine(QPointF(x, y + 2.3), QPointF(x, y - 2.3));
        painter.drawLine(QPointF(x - 2.0, y - 0.3), QPointF(x, y - 2.3));
        painter.drawLine(QPointF(x + 2.0, y - 0.3), QPointF(x, y - 2.3));
    };

    
    
    
    
    constexpr qreal arrowSpacing = 14.5;
    const qreal primaryY = center.y() + arrowSpacing / 2.0 -
                           m_arrowProgress * arrowSpacing;
    drawArrowAt(primaryY - arrowSpacing);
    drawArrowAt(primaryY);
    drawArrowAt(primaryY + arrowSpacing);
    painter.setClipping(false);
}

void UpdateIndicatorButton::showEvent(QShowEvent *event)
{
    QPushButton::showEvent(event);
    if (m_arrowAnimation->state() != QAbstractAnimation::Running) {
        m_arrowAnimation->start();
    }
}

void UpdateIndicatorButton::hideEvent(QHideEvent *event)
{
    m_arrowAnimation->stop();
    m_arrowProgress = 0.0;
    QPushButton::hideEvent(event);
}
