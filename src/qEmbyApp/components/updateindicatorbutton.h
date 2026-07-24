#ifndef UPDATEINDICATORBUTTON_H
#define UPDATEINDICATORBUTTON_H

#include <QPushButton>

class QPropertyAnimation;
class QHideEvent;
class QShowEvent;

class UpdateIndicatorButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal arrowProgress READ arrowProgress WRITE setArrowProgress)

public:
    explicit UpdateIndicatorButton(QWidget *parent = nullptr);

    qreal arrowProgress() const;
    void setArrowProgress(qreal progress);

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    qreal m_arrowProgress = 0.0;
    QPropertyAnimation *m_arrowAnimation = nullptr;
};

#endif 
