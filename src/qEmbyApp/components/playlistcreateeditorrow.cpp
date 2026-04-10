#include "playlistcreateeditorrow.h"

#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

PlaylistCreateEditorRow::PlaylistCreateEditorRow(QWidget *parent)
    : QWidget(parent) {
    setObjectName("PlaylistInlineCreateRow");
    setAttribute(Qt::WA_StyledBackground, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 3, 4, 3);
    layout->setSpacing(2);

    m_lineEdit = new QLineEdit(this);
    m_lineEdit->setObjectName("PlaylistInlineCreateEdit");
    m_lineEdit->setClearButtonEnabled(false);
    m_lineEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_lineEdit->setFixedHeight(22);
    layout->addWidget(m_lineEdit, 1);

    m_confirmButton = new QPushButton(this);
    m_confirmButton->setObjectName("PlaylistInlineCreateConfirmBtn");
    m_confirmButton->setCursor(Qt::PointingHandCursor);
    m_confirmButton->setFixedSize(18, 18);
    layout->addWidget(m_confirmButton);

    m_cancelButton = new QPushButton(this);
    m_cancelButton->setObjectName("PlaylistInlineCreateCancelBtn");
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    m_cancelButton->setFixedSize(18, 18);
    layout->addWidget(m_cancelButton);

    connect(m_lineEdit, &QLineEdit::textChanged, this,
            [this](const QString &text) {
                updateConfirmButtonState();
                emit textChanged(text);
            });
    connect(m_lineEdit, &QLineEdit::returnPressed, this, [this]() {
        const QString trimmedText = m_lineEdit->text().trimmed();
        if (!trimmedText.isEmpty()) {
            emit confirmed(trimmedText);
        }
    });
    connect(m_confirmButton, &QPushButton::clicked, this, [this]() {
        const QString trimmedText = m_lineEdit->text().trimmed();
        if (!trimmedText.isEmpty()) {
            emit confirmed(trimmedText);
        }
    });
    connect(m_cancelButton, &QPushButton::clicked, this,
            &PlaylistCreateEditorRow::cancelled);

    updateConfirmButtonState();
}

QString PlaylistCreateEditorRow::text() const {
    return m_lineEdit ? m_lineEdit->text() : QString();
}

void PlaylistCreateEditorRow::setText(const QString &text) {
    if (!m_lineEdit) {
        return;
    }

    const bool changed = (m_lineEdit->text() != text);
    m_lineEdit->setText(text);
    if (!changed) {
        updateConfirmButtonState();
    }
}

void PlaylistCreateEditorRow::setPlaceholderText(const QString &text) {
    if (m_lineEdit) {
        m_lineEdit->setPlaceholderText(text);
    }
}

void PlaylistCreateEditorRow::focusEditor() {
    if (!m_lineEdit) {
        return;
    }

    m_lineEdit->setFocus();
    m_lineEdit->selectAll();
}

void PlaylistCreateEditorRow::updateConfirmButtonState() {
    if (m_confirmButton && m_lineEdit) {
        m_confirmButton->setEnabled(!m_lineEdit->text().trimmed().isEmpty());
    }
}
