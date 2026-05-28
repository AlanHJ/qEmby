#ifndef MODERNMENUSCROLLBAR_H
#define MODERNMENUSCROLLBAR_H

#include <QScrollBar>
#include <QPaintEvent>


















class ModernMenuScrollBar : public QScrollBar {
    Q_OBJECT

public:
    explicit ModernMenuScrollBar(Qt::Orientation orientation, QWidget *parent = nullptr);
    explicit ModernMenuScrollBar(QWidget *parent = nullptr);
    ~ModernMenuScrollBar() override = default;

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    
    
    QRect computeHandleRect() const;

    bool m_hovered = false;
};

#endif 
