#ifndef SLIDINGSTACKEDWIDGET_H
#define SLIDINGSTACKEDWIDGET_H

#include <QStackedWidget>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QEasingCurve>


class SlidingStackedWidget : public QStackedWidget
{
    Q_OBJECT
public:
    enum SlideDirection {
        LeftToRight,
        RightToLeft,
        TopToBottom,
        BottomToTop,
        Automatic 
    };

    explicit SlidingStackedWidget(QWidget *parent = nullptr);

    void setSpeed(int speed);
    void setEasingCurve(QEasingCurve::Type curveType);

    
    void slideInIdx(int idx, SlideDirection direction = Automatic);
    void slideInWgt(QWidget *widget, SlideDirection direction = Automatic);

private Q_SLOTS:
    void animationDoneSlot();

private:
    int m_speed;
    QEasingCurve::Type m_animationType;
    QParallelAnimationGroup *m_animGroup;
    bool m_isAnimating;
    int m_nextIndex;
};

#endif 
