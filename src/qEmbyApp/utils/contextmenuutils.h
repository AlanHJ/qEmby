#ifndef CONTEXTMENUUTILS_H
#define CONTEXTMENUUTILS_H

class QObject;
class QEvent;

namespace ContextMenuUtils {

bool showStyledTextContextMenu(QObject* watched, QEvent* event);
bool showStyledLabelContextMenu(QObject* watched, QEvent* event);

} 

#endif 
