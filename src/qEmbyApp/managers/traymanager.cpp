#include "traymanager.h"
#include <QApplication>
#include <QIcon>
#include "config/configstore.h"
#include "config/config_keys.h"

TrayManager::TrayManager(QObject *parent)
    : QObject(parent),
    m_trayIcon(new QSystemTrayIcon(this)),
    m_trayMenu(new QMenu())
{
    m_trayIcon->setIcon(QIcon(":/svg/qemby_logo.svg"));
    m_trayIcon->setToolTip(qApp->applicationName());

    setupMenu();
    m_trayIcon->setContextMenu(m_trayMenu);

    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &TrayManager::onTrayActivated);
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this, &TrayManager::onConfigChanged);

    
    connect(ThemeManager::instance(), &ThemeManager::themeChanged, this, &TrayManager::onThemeChanged);

    bool isTrayEnabled = ConfigStore::instance()->get<bool>(ConfigKeys::CloseToTray, false);
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_trayIcon->setVisible(isTrayEnabled);
        QApplication::setQuitOnLastWindowClosed(!isTrayEnabled);
    }

    
    updateIcons();
}

TrayManager::~TrayManager() {
    if (m_trayMenu) {
        m_trayMenu->deleteLater();
    }
}

void TrayManager::setupMenu() {
    
    m_trayMenu->setObjectName("modern-tray-menu");

    
    m_trayMenu->setWindowFlags(m_trayMenu->windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    m_trayMenu->setAttribute(Qt::WA_TranslucentBackground);

    
    m_showAction = new QAction(tr("Show qEmby"), this);
    connect(m_showAction, &QAction::triggered, this, &TrayManager::showMainRequested);

    m_quitAction = new QAction(tr("Quit"), this);
    connect(m_quitAction, &QAction::triggered, this, &TrayManager::quitRequested);

    m_trayMenu->addAction(m_showAction);
    m_trayMenu->addSeparator();
    m_trayMenu->addAction(m_quitAction);
}

void TrayManager::updateIcons() {
    
    
    
    m_showAction->setIcon(ThemeManager::getAdaptiveIcon(":/svg/dark/window-pop.svg"));
    m_quitAction->setIcon(ThemeManager::getAdaptiveIcon(":/svg/dark/logout.svg"));
}

void TrayManager::onThemeChanged(ThemeManager::Theme theme) {
    Q_UNUSED(theme); 
    
    updateIcons();
}

void TrayManager::onTrayActivated(QSystemTrayIcon::ActivationReason reason) {
    if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick) {
        emit showMainRequested();
    }
}

void TrayManager::onConfigChanged(const QString &key, const QVariant &value) {
    if (key == ConfigKeys::CloseToTray) {
        bool enabled = value.toBool();
        if (QSystemTrayIcon::isSystemTrayAvailable()) {
            m_trayIcon->setVisible(enabled);
            QApplication::setQuitOnLastWindowClosed(!enabled);
        }
    }
}
