#ifndef EDITABLEVALUELABEL_H
#define EDITABLEVALUELABEL_H

#include <QString>
#include <QWidget>

class QLabel;
class QLineEdit;
class QStackedLayout;

class EditableValueLabel : public QWidget
{
    Q_OBJECT

public:
    explicit EditableValueLabel(QString textObjectName,
                                QWidget *parent = nullptr);

    QString text() const;
    bool isEditing() const;

    void setText(const QString &text);
    void setAlignment(Qt::Alignment alignment);

signals:
    void textSubmitted(const QString &text);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void beginEditing();
    void finishEditing(bool accept);

    QLabel *m_label = nullptr;
    QLineEdit *m_editor = nullptr;
    QStackedLayout *m_layout = nullptr;
    bool m_isEditing = false;
};

#endif 
