#ifndef SETTINGSPAGEBASE_H
#define SETTINGSPAGEBASE_H

#include <QWidget>
#include <QVBoxLayout>
#include <QString>

class QEmbyCore;
class QLabel;

class SettingsPageBase : public QWidget {
    Q_OBJECT
public:
    explicit SettingsPageBase(QEmbyCore* core, const QString& pageTitle, QWidget* parent = nullptr);
    ~SettingsPageBase() override = default;


protected:
    QEmbyCore* m_core;          
    QVBoxLayout* m_mainLayout;  
};

#endif 
