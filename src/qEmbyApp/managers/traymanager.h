#ifndef TRAYMANAGER_H
#define TRAYMANAGER_H

#include <QObject>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QVariant>
#include <QAction>                 
#include "thememanager.h" 

class TrayManager : public QObject {
    Q_OBJECT
public:
    explicit TrayManager(QObject *parent = nullptr);
    ~TrayManager() override;

signals:
    
    void showMainRequested();
    
    void quitRequested();

private slots:
    void onTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onConfigChanged(const QString &key, const QVariant &value);
    void onThemeChanged(ThemeManager::Theme theme); 

private:
    void setupMenu();
    void updateIcons(); 

    QSystemTrayIcon *m_trayIcon;
    QMenu *m_trayMenu;
    QAction *m_showAction; 
    QAction *m_quitAction; 
};

#endif 
