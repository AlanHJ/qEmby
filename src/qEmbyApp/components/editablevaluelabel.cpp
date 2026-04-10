#include "editablevaluelabel.h"

#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QStackedLayout>

EditableValueLabel::EditableValueLabel(QString textObjectName, QWidget *parent)
    : QWidget(parent)
{
    m_layout = new QStackedLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);
    m_layout->setStackingMode(QStackedLayout::StackOne);

    m_label = new QLabel(this);
    m_label->setObjectName(textObjectName);
    m_label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_label->setCursor(Qt::IBeamCursor);
    m_label->installEventFilter(this);
    m_layout->addWidget(m_label);

    m_editor = new QLineEdit(this);
    m_editor->setObjectName(textObjectName);
    m_editor->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_editor->setFrame(false);
    m_editor->installEventFilter(this);
    m_layout->addWidget(m_editor);
    m_layout->setCurrentWidget(m_label);

    connect(m_editor, &QLineEdit::editingFinished, this, [this]() {
        if (m_isEditing) {
            finishEditing(true);
        }
    });
}

QString EditableValueLabel::text() const
{
    return m_label ? m_label->text() : QString();
}

bool EditableValueLabel::isEditing() const
{
    return m_isEditing;
}

void EditableValueLabel::setText(const QString &text)
{
    if (!m_label) {
        return;
    }

    m_label->setText(text);
    if (!m_isEditing && m_editor) {
        m_editor->setText(text);
    }
}

void EditableValueLabel::setAlignment(Qt::Alignment alignment)
{
    if (m_label) {
        m_label->setAlignment(alignment);
    }
    if (m_editor) {
        m_editor->setAlignment(alignment);
    }
}

bool EditableValueLabel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_label && event->type() == QEvent::MouseButtonDblClick) {
        beginEditing();
        return true;
    }

    if (watched == m_editor && event->type() == QEvent::KeyPress) {
        auto *keyEvent = static_cast<QKeyEvent *>(event);
        if (keyEvent->key() == Qt::Key_Return ||
            keyEvent->key() == Qt::Key_Enter) {
            finishEditing(true);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Escape) {
            finishEditing(false);
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

void EditableValueLabel::beginEditing()
{
    if (!m_label || !m_editor || m_isEditing) {
        return;
    }

    m_isEditing = true;
    m_editor->setText(m_label->text());
    m_layout->setCurrentWidget(m_editor);
    m_editor->setFocus();
    m_editor->selectAll();
}

void EditableValueLabel::finishEditing(bool accept)
{
    if (!m_label || !m_editor || !m_isEditing) {
        return;
    }

    const QString submittedText = m_editor->text().trimmed();
    m_isEditing = false;
    m_layout->setCurrentWidget(m_label);

    if (accept) {
        emit textSubmitted(submittedText);
    } else {
        m_editor->setText(m_label->text());
    }
}
