#include "textwraputils.h"

#include <config/config_keys.h>
#include <config/configstore.h>
#include <QAbstractItemView>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QHelpEvent>
#include <QModelIndex>
#include <QSize>
#include <QStringList>
#include <QTextLayout>
#include <QTextOption>
#include <QToolTip>

namespace TextWrapUtils {

namespace {

constexpr int kToolTipEdgeMargin = 48;
constexpr int kToolTipMinTextWidth = 220;
constexpr int kToolTipMaxTextWidth = 360;

int boundedToolTipTextWidth(const QAbstractItemView* view)
{
    if (!view || !view->viewport()) {
        return kToolTipMaxTextWidth;
    }

    return qBound(kToolTipMinTextWidth,
                  view->viewport()->width() - kToolTipEdgeMargin,
                  kToolTipMaxTextWidth);
}

} 

QString wrapPlainText(const QString& text, const QFont& font, int maxWidth)
{
    if (text.isEmpty() || maxWidth <= 0) {
        return text;
    }

    QString normalized = text;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));

    QStringList wrappedLines;
    const QStringList logicalLines =
        normalized.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    for (const QString& logicalLine : logicalLines) {
        if (logicalLine.isEmpty()) {
            wrappedLines.append(QString());
            continue;
        }

        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);

        QTextLayout layout(logicalLine, font);
        layout.setTextOption(textOption);
        layout.beginLayout();
        while (true) {
            QTextLine textLine = layout.createLine();
            if (!textLine.isValid()) {
                break;
            }

            textLine.setLineWidth(maxWidth);
            wrappedLines.append(
                logicalLine.mid(textLine.textStart(), textLine.textLength()));
        }
        layout.endLayout();
    }

    return wrappedLines.join(QLatin1Char('\n'));
}

QSize measureWrappedPlainText(const QString& text, const QFont& font,
                              int maxWidth)
{
    const QString wrappedText = wrapPlainText(text, font, maxWidth);
    QFontMetrics fontMetrics(font);
    const QStringList lines =
        wrappedText.split(QLatin1Char('\n'), Qt::KeepEmptyParts);

    int maxLineWidth = 0;
    for (const QString& line : lines) {
        maxLineWidth = qMax(maxLineWidth, fontMetrics.horizontalAdvance(line));
    }

    const int lineCount = qMax(1, lines.size());
    return QSize(maxLineWidth, lineCount * fontMetrics.lineSpacing());
}

bool showWrappedMediaItemToolTip(QAbstractItemView* view, QEvent* event,
                                 int displayTimeMs)
{
    if (!view || !event || event->type() != QEvent::ToolTip) {
        return false;
    }

    if (!ConfigStore::instance()->get<bool>(ConfigKeys::ShowMediaTooltips,
                                            true)) {
        QToolTip::hideText();
        return true;
    }

    auto* helpEvent = static_cast<QHelpEvent*>(event);
    const QModelIndex index = view->indexAt(helpEvent->pos());
    const QString tooltip = index.data(Qt::ToolTipRole).toString().trimmed();

    if (!tooltip.isEmpty()) {
        const QString wrappedTooltip = wrapPlainText(
            tooltip, QToolTip::font(), boundedToolTipTextWidth(view));
        QToolTip::showText(helpEvent->globalPos(), wrappedTooltip,
                           view->viewport(), view->visualRect(index),
                           displayTimeMs);
    } else {
        QToolTip::hideText();
    }

    return true;
}

} 
