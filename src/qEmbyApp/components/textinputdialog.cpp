#include "textinputdialog.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QShowEvent>
#include <QVBoxLayout>

TextInputDialog::TextInputDialog(QWidget *parent)
    : ModernDialogBase(parent) {
    setMinimumWidth(420);

    m_promptLabel = new QLabel(this);
    m_promptLabel->setObjectName("dialog-text");
    m_promptLabel->setWordWrap(true);
    contentLayout()->addWidget(m_promptLabel);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setClearButtonEnabled(true);
    contentLayout()->addWidget(m_lineEdit);
    contentLayout()->addSpacing(24);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->setSpacing(12);

    auto *cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName("dialog-btn-cancel");
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_confirmButton = new QPushButton(this);
    m_confirmButton->setObjectName("dialog-btn-primary");
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    connect(m_confirmButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_confirmButton);

    contentLayout()->addLayout(buttonLayout);

    connect(m_lineEdit, &QLineEdit::textChanged, this,
            [this]() { updateConfirmState(); });
    connect(m_lineEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_confirmButton->isEnabled()) {
            accept();
        }
    });

    updateConfirmState();
}

void TextInputDialog::setPromptText(const QString &text) {
    m_promptLabel->setText(text);
}

void TextInputDialog::setPlaceholderText(const QString &text) {
    m_lineEdit->setPlaceholderText(text);
}

void TextInputDialog::setText(const QString &text) {
    m_lineEdit->setText(text);
    updateConfirmState();
}

void TextInputDialog::setConfirmButtonText(const QString &text) {
    m_confirmButton->setText(text);
}

QString TextInputDialog::text() const {
    return m_lineEdit->text();
}

QString TextInputDialog::getText(QWidget *parent, const QString &title,
                                 const QString &promptText,
                                 const QString &initialText,
                                 const QString &placeholderText,
                                 const QString &confirmText, bool *ok) {
    TextInputDialog dialog(parent);
    dialog.setTitle(title);
    dialog.setPromptText(promptText);
    dialog.setText(initialText);
    dialog.setPlaceholderText(placeholderText);
    dialog.setConfirmButtonText(confirmText.isEmpty() ? dialog.tr("OK")
                                                      : confirmText);

    const bool accepted = (dialog.exec() == QDialog::Accepted);
    if (ok) {
        *ok = accepted;
    }

    return accepted ? dialog.text() : QString();
}

void TextInputDialog::showEvent(QShowEvent *event) {
    ModernDialogBase::showEvent(event);
    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void TextInputDialog::updateConfirmState() {
    m_confirmButton->setEnabled(!m_lineEdit->text().trimmed().isEmpty());
}
