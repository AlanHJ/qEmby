#ifndef WEBDAVPASSPHRASEPROMPT_H
#define WEBDAVPASSPHRASEPROMPT_H

#include "moderndialogbase.h"

class QAction;
class QLabel;
class QLineEdit;
class QPushButton;












class WebdavPassphrasePrompt : public ModernDialogBase
{
    Q_OBJECT
public:
    explicit WebdavPassphrasePrompt(QWidget *parent = nullptr);

    void setPromptText(const QString &text);
    void setPlaceholderText(const QString &text);
    void setConfirmButtonText(const QString &text);

    QString passphrase() const;

    
    static QString getPassphrase(QWidget *parent, const QString &title,
                                 const QString &promptText, const QString &confirmText,
                                 bool *ok);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onToggleVisibility();
    void onTextChanged();

private:
    QLabel *m_promptLabel = nullptr;
    QLineEdit *m_lineEdit = nullptr;
    QAction *m_togglePwdAction = nullptr;
    QPushButton *m_confirmBtn = nullptr;
    QPushButton *m_cancelBtn = nullptr;
};

#endif 
