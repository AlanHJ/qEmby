#ifndef PAGE_GENERAL_H
#define PAGE_GENERAL_H

#include "settingspagebase.h"

class PageGeneral : public SettingsPageBase {
    Q_OBJECT
public:
    explicit PageGeneral(QEmbyCore* core, QWidget* parent = nullptr);
};

#endif 
