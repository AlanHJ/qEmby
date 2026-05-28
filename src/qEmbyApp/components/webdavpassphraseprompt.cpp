#include "webdavpassphraseprompt.h"

#include "../managers/thememanager.h"

#include <QAction>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

WebdavPassphrasePrompt::WebdavPassphrasePrompt(QWidget *parent)
    : ModernDialogBase(parent)
{
    setMinimumWidth(420);

    m_promptLabel = new QLabel(this);
    m_promptLabel->setObjectName("dialog-text");
    m_promptLabel->setWordWrap(true);
    contentLayout()->addWidget(m_promptLabel);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setEchoMode(QLineEdit::Password);
    m_lineEdit->setClearButtonEnabled(false);

    
    
    m_togglePwdAction = new QAction(
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/dark/eye.svg")),
        tr("Show Password"), this);
    m_togglePwdAction->setCheckable(true);
    m_togglePwdAction->setToolTip(tr("Toggle passphrase visibility"));
    m_lineEdit->addAction(m_togglePwdAction, QLineEdit::TrailingPosition);
    contentLayout()->addWidget(m_lineEdit);
    contentLayout()->addSpacing(16);

    auto *buttons = new QHBoxLayout();
    buttons->addStretch();
    buttons->setSpacing(12);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    m_cancelBtn->setObjectName("dialog-btn-cancel");
    m_cancelBtn->setCursor(Qt::PointingHandCursor);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    buttons->addWidget(m_cancelBtn);

    m_confirmBtn = new QPushButton(tr("OK"), this);
    m_confirmBtn->setObjectName("dialog-btn-primary");
    m_confirmBtn->setCursor(Qt::PointingHandCursor);
    connect(m_confirmBtn, &QPushButton::clicked, this, &QDialog::accept);
    buttons->addWidget(m_confirmBtn);

    contentLayout()->addLayout(buttons);

    connect(m_lineEdit, &QLineEdit::textChanged, this,
            &WebdavPassphrasePrompt::onTextChanged);
    connect(m_lineEdit, &QLineEdit::returnPressed, this, [this]()
            {
                if (m_confirmBtn->isEnabled())
                    accept();
            });
    connect(m_togglePwdAction, &QAction::toggled, this,
            &WebdavPassphrasePrompt::onToggleVisibility);

    onTextChanged();
}

void WebdavPassphrasePrompt::setPromptText(const QString &text)
{
    m_promptLabel->setText(text);
}

void WebdavPassphrasePrompt::setPlaceholderText(const QString &text)
{
    m_lineEdit->setPlaceholderText(text);
}

void WebdavPassphrasePrompt::setConfirmButtonText(const QString &text)
{
    m_confirmBtn->setText(text);
}

QString WebdavPassphrasePrompt::passphrase() const
{
    return m_lineEdit->text();
}

QString WebdavPassphrasePrompt::getPassphrase(QWidget *parent, const QString &title,
                                              const QString &promptText,
                                              const QString &confirmText, bool *ok)
{
    WebdavPassphrasePrompt dlg(parent);
    dlg.setTitle(title);
    dlg.setPromptText(promptText);
    dlg.setPlaceholderText(WebdavPassphrasePrompt::tr("Sync passphrase"));
    if (!confirmText.isEmpty())
    {
        dlg.setConfirmButtonText(confirmText);
    }

    const bool accepted = (dlg.exec() == QDialog::Accepted);
    if (ok)
    {
        *ok = accepted;
    }
    return accepted ? dlg.passphrase() : QString();
}

void WebdavPassphrasePrompt::showEvent(QShowEvent *event)
{
    ModernDialogBase::showEvent(event);
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void WebdavPassphrasePrompt::onToggleVisibility()
{
    if (!m_togglePwdAction || !m_lineEdit)
        return;
    const bool visible = m_togglePwdAction->isChecked();
    m_lineEdit->setEchoMode(visible ? QLineEdit::Normal : QLineEdit::Password);
    m_togglePwdAction->setIcon(ThemeManager::getAdaptiveIcon(
        visible ? QStringLiteral(":/svg/dark/eye-off.svg")
                : QStringLiteral(":/svg/dark/eye.svg")));
    m_togglePwdAction->setText(visible ? tr("Hide Password") : tr("Show Password"));
}

void WebdavPassphrasePrompt::onTextChanged()
{
    m_confirmBtn->setEnabled(!m_lineEdit->text().isEmpty());
}
