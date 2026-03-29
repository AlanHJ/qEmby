#include "tagpresetoptionrow.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QMouseEvent>

TagPresetOptionRow::TagPresetOptionRow(const QString &text, QWidget *parent)
    : QFrame(parent) {
    setObjectName("TagPresetOptionRow");
    setFrameShape(QFrame::NoFrame);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_checkBox = new QCheckBox(text, this);
    m_checkBox->setObjectName("TagPresetCheckbox");
    m_checkBox->setAttribute(Qt::WA_TransparentForMouseEvents);
    m_checkBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    layout->addWidget(m_checkBox);
}

QCheckBox *TagPresetOptionRow::checkBox() const {
    return m_checkBox;
}

QString TagPresetOptionRow::text() const {
    return m_checkBox ? m_checkBox->text() : QString();
}

void TagPresetOptionRow::setChecked(bool checked) {
    if (m_checkBox) {
        m_checkBox->setChecked(checked);
    }
}

void TagPresetOptionRow::mouseReleaseEvent(QMouseEvent *event) {
    if (!m_checkBox || !isEnabled() || !m_checkBox->isEnabled() ||
        event->button() != Qt::LeftButton) {
        QFrame::mouseReleaseEvent(event);
        return;
    }

    if (rect().contains(event->pos())) {
        m_checkBox->setFocus(Qt::MouseFocusReason);
        m_checkBox->click();
        event->accept();
        return;
    }

    QFrame::mouseReleaseEvent(event);
}
