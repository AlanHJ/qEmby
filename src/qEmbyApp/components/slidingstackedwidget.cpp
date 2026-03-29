#include "slidingstackedwidget.h"
#include <QWidget>
#include "../../qEmbyCore/config/configstore.h"
#include "config/config_keys.h"

SlidingStackedWidget::SlidingStackedWidget(QWidget *parent)
    : QStackedWidget(parent),
    m_speed(350), 
    m_animationType(QEasingCurve::OutCubic), 
    m_isAnimating(false),
    m_nextIndex(0)
{
    m_animGroup = new QParallelAnimationGroup(this);
    connect(m_animGroup, &QParallelAnimationGroup::finished, this, &SlidingStackedWidget::animationDoneSlot);
}

void SlidingStackedWidget::setSpeed(int speed)
{
    m_speed = speed;
}

void SlidingStackedWidget::setEasingCurve(QEasingCurve::Type curveType)
{
    m_animationType = curveType;
}

void SlidingStackedWidget::slideInWgt(QWidget *widget, SlideDirection direction)
{
    int idx = indexOf(widget);
    if (idx != -1) {
        slideInIdx(idx, direction);
    }
}

void SlidingStackedWidget::slideInIdx(int idx, SlideDirection direction)
{
    if (idx < 0 || idx >= count())
        return;

    
    if (currentIndex() == idx && !m_isAnimating)
        return;

    
    
    if (m_isAnimating) {
        m_animGroup->stop();           
        animationDoneSlot();           
    }

    
    bool reduceAnimations = ConfigStore::instance()->get<bool>(ConfigKeys::UiAnimations, false);
    if (reduceAnimations) {
        m_nextIndex = idx;
        setCurrentIndex(idx);
        QWidget *w = widget(idx);
        if (w) w->raise();
        return;
    }

    m_isAnimating = true;
    m_nextIndex = idx;

    int now = currentIndex();
    QWidget *widgetNow = widget(now);
    QWidget *widgetNext = widget(idx);

    
    if (!widgetNow || !widgetNext) {
        m_isAnimating = false;
        if (widgetNext) {
            setCurrentIndex(idx);
        }
        return;
    }

    int width = this->width();
    int height = this->height();

    int offsetX = 0;
    int offsetY = 0;

    SlideDirection actualDirection = direction;
    if (direction == Automatic) {
        if (now < idx)
            actualDirection = RightToLeft;
        else
            actualDirection = LeftToRight;
    }

    if (actualDirection == RightToLeft) {
        offsetX = width;
        offsetY = 0;
    } else if (actualDirection == LeftToRight) {
        offsetX = -width;
        offsetY = 0;
    } else if (actualDirection == BottomToTop) {
        offsetX = 0;
        offsetY = height;
    } else if (actualDirection == TopToBottom) {
        offsetX = 0;
        offsetY = -height;
    }

    
    widgetNext->setGeometry(0, 0, width, height);

    QPoint pNext = widgetNext->pos();
    QPoint pNow = widgetNow->pos();

    
    widgetNext->move(pNext.x() + offsetX, pNext.y() + offsetY);
    widgetNext->show();
    widgetNext->raise(); 

    
    QPropertyAnimation *animNow = new QPropertyAnimation(widgetNow, "pos");
    animNow->setDuration(m_speed);
    animNow->setEasingCurve(m_animationType);
    animNow->setStartValue(pNow);
    animNow->setEndValue(QPoint(pNow.x() - offsetX, pNow.y() - offsetY));

    
    QPropertyAnimation *animNext = new QPropertyAnimation(widgetNext, "pos");
    animNext->setDuration(m_speed);
    animNext->setEasingCurve(m_animationType);
    animNext->setStartValue(widgetNext->pos());
    animNext->setEndValue(pNext);

    m_animGroup->clear();
    m_animGroup->addAnimation(animNow);
    m_animGroup->addAnimation(animNext);
    m_animGroup->start();
}

void SlidingStackedWidget::animationDoneSlot()
{
    
    if (m_nextIndex >= 0 && m_nextIndex < count()) {
        setCurrentIndex(m_nextIndex);
        QWidget *w = widget(m_nextIndex);
        if (w) {
            w->raise();
        }
    }
    m_isAnimating = false;
}
