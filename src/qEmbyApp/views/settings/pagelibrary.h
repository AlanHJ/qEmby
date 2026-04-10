#ifndef PAGE_LIBRARY_H
#define PAGE_LIBRARY_H

#include "settingspagebase.h"

class DashboardSectionOrderWidget;
class QLabel;
class SettingsSubPanel;

class PageLibrary : public SettingsPageBase {
    Q_OBJECT
public:
    explicit PageLibrary(QEmbyCore* core, QWidget* parent = nullptr);

private:
    void updateCacheSizes();

    QLabel *m_dataCacheSizeLabel = nullptr;
    QLabel *m_imgCacheSizeLabel = nullptr;
    SettingsSubPanel* m_homeSectionOrderPanel = nullptr;
    DashboardSectionOrderWidget* m_homeSectionOrderWidget = nullptr;
};

#endif 
