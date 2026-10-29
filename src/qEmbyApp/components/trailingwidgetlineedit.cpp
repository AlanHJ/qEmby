#include "trailingwidgetlineedit.h"

#include <QEvent>
#include <QScopedValueRollback>
#include <QStyle>
#include <QStyleOptionFrame>

TrailingWidgetLineEdit::TrailingWidgetLineEdit(QWidget *parent)
    : QLineEdit(parent)
{
}

void TrailingWidgetLineEdit::setTrailingWidget(QWidget *widget)
{
    if (m_trailingWidget == widget)
        return;

    if (m_trailingWidget) {
        m_trailingWidget->removeEventFilter(this);
        m_trailingWidget->hide();
        m_trailingWidget->deleteLater();
    }

    m_trailingWidget = widget;
    if (m_trailingWidget) {
        m_trailingWidget->setParent(this);
        m_trailingWidget->ensurePolished();
        m_trailingWidget->installEventFilter(this);
        m_trailingWidget->show();
    }
    updateTrailingWidgetGeometry();
}

bool TrailingWidgetLineEdit::event(QEvent *event)
{
    const bool handled = QLineEdit::event(event);
    switch (event->type()) {
    case QEvent::Resize:
    case QEvent::Show:
    case QEvent::StyleChange:
    case QEvent::FontChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::LayoutRequest:
        updateTrailingWidgetGeometry();
        break;
    default:
        break;
    }
    return handled;
}

bool TrailingWidgetLineEdit::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_trailingWidget) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Hide:
        case QEvent::ShowToParent:
        case QEvent::HideToParent:
        case QEvent::LayoutRequest:
        case QEvent::FontChange:
        case QEvent::StyleChange:
            updateTrailingWidgetGeometry();
            break;
        default:
            break;
        }
    }
    return QLineEdit::eventFilter(watched, event);
}

void TrailingWidgetLineEdit::updateTrailingWidgetGeometry()
{
    if (m_updatingGeometry)
        return;
    const QScopedValueRollback<bool> guard(m_updatingGeometry, true);

    QMargins margins;
    if (m_trailingWidget && !m_trailingWidget->isHidden()) {
        
        QStyleOptionFrame option;
        initStyleOption(&option);
        const QRect contentRect = style()->subElementRect(
            QStyle::SE_LineEditContents, &option, this);
        
        
        const QSize availableSize(qMax(0, contentRect.width()),
                                  qMax(0, height() - 2 * option.lineWidth));
        const QSize widgetSize = m_trailingWidget->sizeHint()
            .expandedTo(m_trailingWidget->minimumSizeHint())
            .boundedTo(availableSize);
        const QRect logicalRect(contentRect.right() - widgetSize.width() + 1,
                                (height() - widgetSize.height()) / 2,
                                widgetSize.width(), widgetSize.height());
        m_trailingWidget->setGeometry(
            QStyle::visualRect(layoutDirection(), contentRect, logicalRect));

        const int reservedWidth = widgetSize.width() + 8;
        if (isRightToLeft())
            margins.setLeft(reservedWidth);
        else
            margins.setRight(reservedWidth);
    }

    if (textMargins() != margins)
        setTextMargins(margins);
}
