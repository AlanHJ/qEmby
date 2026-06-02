#ifndef MODERNNUMBERINPUT_H
#define MODERNNUMBERINPUT_H

#include <QSpinBox>

class ModernNumberInput : public QSpinBox
{
    Q_OBJECT

public:
    explicit ModernNumberInput(QWidget *parent = nullptr);
};

#endif 
