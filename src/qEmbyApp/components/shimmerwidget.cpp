#include "shimmerwidget.h"
#include "../managers/thememanager.h"
#include <QPainter>
#include <QPainterPath>
#include <QPropertyAnimation>

ShimmerWidget::ShimmerWidget(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("shimmer-widget");
    
    setAttribute(Qt::WA_TransparentForMouseEvents, true);

    m_animation = new QPropertyAnimation(this, "shimmerPhase", this);
    m_animation->setStartValue(0.0);
    m_animation->setEndValue(1.0);
    m_animation->setDuration(1800);
    m_animation->setLoopCount(-1);
    m_animation->setEasingCurve(QEasingCurve::Linear);
}

ShimmerWidget::~ShimmerWidget()
{
    if (m_animation) {
        m_animation->stop();
    }
}

void ShimmerWidget::setCardSize(const QSize& size)
{
    m_cardSize = size;
    update();
}

void ShimmerWidget::setCardSpacing(int spacing)
{
    m_cardSpacing = spacing;
    update();
}

void ShimmerWidget::setCardRadius(int radius)
{
    m_cardRadius = radius;
    update();
}

void ShimmerWidget::setShowSubtitle(bool show)
{
    m_showSubtitle = show;
    update();
}

qreal ShimmerWidget::shimmerPhase() const
{
    return m_shimmerPhase;
}

void ShimmerWidget::setShimmerPhase(qreal phase)
{
    m_shimmerPhase = phase;
    update();
}

void ShimmerWidget::startAnimation()
{
    if (m_animation && m_animation->state() != QAbstractAnimation::Running) {
        m_shimmerPhase = 0.0;
        m_animation->start();
    }
}

void ShimmerWidget::stopAnimation()
{
    if (m_animation) {
        m_animation->stop();
    }
}

bool ShimmerWidget::isAnimating() const
{
    return m_animation &&
           m_animation->state() == QAbstractAnimation::Running;
}

void ShimmerWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const bool isDark = ThemeManager::instance()->isDarkMode();

    
    const QColor baseBg = isDark ? QColor(50, 50, 55) : QColor(229, 231, 235);

    const int padding = 8;
    const int cardW = m_cardSize.width();
    const int cardH = m_cardSize.height();
    const int imgW = cardW - padding * 2;

    
    const qreal ratio =
        static_cast<qreal>(cardH) / cardW;
    const int imgH =
        (ratio < 1.0) ? qRound(imgW * 9.0 / 16.0) : qRound(imgW * 1.5);

    
    
    
    QPainterPath allShapes;
    const int maxCards = 60; 
    int count = 0;
    int y = 0;

    while (y < height() + cardH && count < maxCards) {
        int x = 0;
        while (x < width() + cardW && count < maxCards) {
            QRectF imgRect(x + padding, y + padding, imgW, imgH);
            allShapes.addRoundedRect(imgRect, m_cardRadius, m_cardRadius);

            x += cardW + m_cardSpacing;
            ++count;
        }
        y += cardH + m_cardSpacing;
    }

    
    painter.setPen(Qt::NoPen);
    painter.setBrush(baseBg);
    painter.drawPath(allShapes);

    
    
    
    const int totalW = width();
    const int shimmerW = qRound(totalW * 0.4);
    const int shimmerX =
        qRound(m_shimmerPhase * (totalW + shimmerW)) - shimmerW;

    QLinearGradient gradient(shimmerX, 0, shimmerX + shimmerW, 0);
    const QColor shimmerHi =
        isDark ? QColor(255, 255, 255, 14) : QColor(255, 255, 255, 100);
    gradient.setColorAt(0.0, Qt::transparent);
    gradient.setColorAt(0.5, shimmerHi);
    gradient.setColorAt(1.0, Qt::transparent);

    painter.save();
    painter.setClipPath(allShapes);
    painter.setBrush(gradient);
    painter.drawRect(rect());
    painter.restore();
}
