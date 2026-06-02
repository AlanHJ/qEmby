#include "modernnumberinput.h"

#include <QAbstractSpinBox>

ModernNumberInput::ModernNumberInput(QWidget *parent)
    : QSpinBox(parent)
{
    setObjectName("ModernNumberInput");
    setRange(0, 10000);
    setAlignment(Qt::AlignCenter);
    setButtonSymbols(QAbstractSpinBox::NoButtons);
    setKeyboardTracking(false);
    setFixedSize(118, 34);
}
