#ifndef PAGEABOUT_H
#define PAGEABOUT_H

#include "settingspagebase.h"
#include <QLabel>
#include <QPushButton>

class PageAbout : public SettingsPageBase
{
    Q_OBJECT

public:
    explicit PageAbout(QEmbyCore* core, QWidget *parent = nullptr);
    ~PageAbout() override = default;

private slots:
    
    void showLicensesDialog();

private:
    QLabel *m_logoLabel;
    QLabel *m_appNameLabel;
    QLabel *m_versionLabel;
    QLabel *m_authorLabel;
    QLabel *m_linkLabel;
    QLabel *m_licenseLabel;
    QPushButton *m_licenseBtn;      
};

#endif 
