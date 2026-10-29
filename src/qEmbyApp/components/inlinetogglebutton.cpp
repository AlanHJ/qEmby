#include "inlinetogglebutton.h"

InlineToggleButton::InlineToggleButton(QWidget *parent)
    : QToolButton(parent)
{
    setToolButtonStyle(Qt::ToolButtonTextOnly);
    setCheckable(true);
    setAutoRaise(true);
    setCursor(Qt::PointingHandCursor);
    
    setFocusPolicy(Qt::TabFocus);
}
