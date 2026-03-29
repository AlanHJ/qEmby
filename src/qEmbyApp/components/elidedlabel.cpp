#include "elidedlabel.h"
#include <QFontMetrics>

ElidedLabel::ElidedLabel(QWidget* parent) : QLabel(parent)
{
    
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void ElidedLabel::setFullText(const QString& text)
{
    m_fullText = text;
    setToolTip(text); 
    updateElidedText();
}

void ElidedLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);
    updateElidedText();
}

QSize ElidedLabel::sizeHint() const
{
    
    
    return minimumSizeHint();
}

QSize ElidedLabel::minimumSizeHint() const
{
    QFontMetrics fm(font());
    return QSize(fm.horizontalAdvance("...") + 2, QLabel::minimumSizeHint().height());
}

void ElidedLabel::updateElidedText()
{
    if (m_fullText.isEmpty()) {
        if (text() != "") setText("");
        return;
    }

    QFontMetrics fm(font());
    QString elided = fm.elidedText(m_fullText, Qt::ElideRight, width() - 2);

    if (text() != elided) {
        setText(elided);
    }
}
