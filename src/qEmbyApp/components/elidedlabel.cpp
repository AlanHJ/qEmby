#include "elidedlabel.h"
#include "../utils/textwraputils.h"
#include <QEvent>
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

void ElidedLabel::setMaximumLines(int lines, bool reserveHeight)
{
    lines = qMax(1, lines);
    if (m_maximumLines == lines && m_reserveLineHeight == reserveHeight) return;
    const bool releaseHeight = m_reserveLineHeight && !reserveHeight;
    m_maximumLines = lines;
    m_reserveLineHeight = reserveHeight;
    if (releaseHeight) {
        setMinimumHeight(0);
        setMaximumHeight(QWIDGETSIZE_MAX);
    }
    
    setWordWrap(false);
    updateElidedText();
}

void ElidedLabel::changeEvent(QEvent* event)
{
    QLabel::changeEvent(event);
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange ||
        event->type() == QEvent::ContentsRectChange)
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
    QFontMetrics fm(font());
    if (m_reserveLineHeight) {
        const QMargins insets = contentsMargins();
        
        const int reservedHeight = fm.height() +
            (m_maximumLines - 1) * qMax(1, fm.lineSpacing()) +
            insets.top() + insets.bottom() + 2 * margin() + 2;
        if (minimumHeight() != reservedHeight || maximumHeight() != reservedHeight)
            setFixedHeight(reservedHeight);
    }
    if (m_fullText.isEmpty()) {
        if (text() != "") setText("");
        Q_EMIT elisionChanged(false);
        return;
    }

    QString elided;
    bool isElided = false;
    if (m_maximumLines == 1) {
        elided = fm.elidedText(m_fullText, Qt::ElideRight, width() - 2);
        isElided = elided != m_fullText;
    } else {
        const int availableWidth = qMax(0, contentsRect().width() - 2 * margin() - 2);
        const int availableHeight = qMax(0, contentsRect().height() - 2 * margin() - 2);
        const int completeLines = availableHeight < fm.height() ? 0
            : 1 + (availableHeight - fm.height()) / qMax(1, fm.lineSpacing());
        elided = TextWrapUtils::elideWrappedPlainText(
            m_fullText, font(), availableWidth, qMin(m_maximumLines, completeLines), &isElided);
    }

    Q_EMIT elisionChanged(isElided);
    if (text() != elided) {
        setText(elided);
    }
}
