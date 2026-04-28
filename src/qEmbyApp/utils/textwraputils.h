#ifndef TEXTWRAPUTILS_H
#define TEXTWRAPUTILS_H

#include <QSize>
#include <QString>

class QAbstractItemView;
class QFont;
class QEvent;

namespace TextWrapUtils {

QString wrapPlainText(const QString& text, const QFont& font, int maxWidth);
QSize measureWrappedPlainText(const QString& text, const QFont& font,
                              int maxWidth);
bool showWrappedMediaItemToolTip(QAbstractItemView* view, QEvent* event,
                                 int displayTimeMs = 15000);

} 

#endif 
