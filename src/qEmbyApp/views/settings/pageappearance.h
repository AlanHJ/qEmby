#ifndef PAGE_APPEARANCE_H
#define PAGE_APPEARANCE_H

#include "settingspagebase.h"

class PageAppearance : public SettingsPageBase {
    Q_OBJECT
public:
    explicit PageAppearance(QEmbyCore* core, QWidget* parent = nullptr);
};

#endif 
