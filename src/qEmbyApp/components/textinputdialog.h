#ifndef TEXTINPUTDIALOG_H
#define TEXTINPUTDIALOG_H

#include "moderndialogbase.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;

class TextInputDialog : public ModernDialogBase {
    Q_OBJECT

public:
    explicit TextInputDialog(QWidget *parent = nullptr);

    void setPromptText(const QString &text);
    void setPlaceholderText(const QString &text);
    void setText(const QString &text);
    void setConfirmButtonText(const QString &text);

    QString text() const;

    static QString getText(QWidget *parent,
                           const QString &title,
                           const QString &promptText,
                           const QString &initialText = QString(),
                           const QString &placeholderText = QString(),
                           const QString &confirmText = QString(),
                           bool *ok = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private:
    void updateConfirmState();

    QLabel *m_promptLabel;
    QLineEdit *m_lineEdit;
    QPushButton *m_confirmButton;
};

#endif 
