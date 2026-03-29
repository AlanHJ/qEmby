#ifndef PAGE_PLAYER_H
#define PAGE_PLAYER_H

#include "settingspagebase.h"

class PagePlayer : public SettingsPageBase {
    Q_OBJECT
public:
    explicit PagePlayer(QEmbyCore* core, QWidget* parent = nullptr);
};

#endif 
