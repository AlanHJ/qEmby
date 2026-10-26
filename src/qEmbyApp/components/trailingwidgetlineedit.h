#ifndef TRAILINGWIDGETLINEEDIT_H
#define TRAILINGWIDGETLINEEDIT_H

#include <QLineEdit>
#include <QPointer>


class TrailingWidgetLineEdit : public QLineEdit
{
    Q_OBJECT

public:
    explicit TrailingWidgetLineEdit(QWidget *parent = nullptr);

    
    void setTrailingWidget(QWidget *widget);

protected:
    bool event(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void updateTrailingWidgetGeometry();

    QPointer<QWidget> m_trailingWidget;
    bool m_updatingGeometry = false;
};

#endif 
