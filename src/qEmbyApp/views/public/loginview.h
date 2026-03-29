#ifndef LOGINVIEW_H
#define LOGINVIEW_H

#include <QWidget>
#include <qcorotask.h>
#include "../../managers/thememanager.h" 

class QLineEdit;
class QPushButton;
class QLabel;
class QStackedWidget;
class QVBoxLayout;
class QEmbyCore;
class QResizeEvent;
class QAction; 
class QUrl;


class LoadingOverlay;
class ModernComboBox;
class ModernSwitch;
class ServerWheelView;

class LoginView : public QWidget
{
    Q_OBJECT
public:
    explicit LoginView(QEmbyCore* core, QWidget *parent = nullptr);

Q_SIGNALS:
    void loginCompleted();

protected:
    void showEvent(QShowEvent *event) override;
    
    void resizeEvent(QResizeEvent *event) override;

private Q_SLOTS:
    
    QCoro::Task<void> onLoginClicked();
    QCoro::Task<void> onServerCardClicked(const QString& serverId);

    void showAddPage();
    void showListPage();
    void onRemoveServerClicked(const QString& serverId);
    void onEditServerClicked(const QString& serverId);
    
    
    void onThemeChanged(ThemeManager::Theme theme);

private:
    void updateSslOptionsVisibility();
    void syncProtocolSelectionFromUrlText(const QString& text);
    QUrl buildNormalizedServerUrl(QString* errorMessage) const;
    void applyServerUrlToForm(const QUrl& url, bool ignoreSslVerification);
    QString displayServerAddress(const QUrl& url) const;

    QEmbyCore* m_core;

    QStackedWidget* m_pageSwitcher;

    QWidget* m_listPage;
    QWidget* m_addPage;

    
    ServerWheelView* m_wheelView;

    ModernComboBox* m_protocolInput;
    QLineEdit* m_serverAddressInput;
    QLineEdit* m_portInput = nullptr;
    QWidget* m_sslOptionsRow = nullptr;
    ModernSwitch* m_ignoreSslSwitch = nullptr;

    QLineEdit* m_usernameInput;
    QLineEdit* m_passwordInput;
    QPushButton* m_loginButton;
    QLabel* m_errorLabel;
    
    
    QAction* m_togglePwdAction = nullptr;

    QString m_editingServerId;
    
    
    LoadingOverlay* m_loadingOverlay = nullptr;

    bool m_autoLoginAttempted = false;

    void setupUi();
    void setupListPage();
    void setupAddPage();
    void refreshServerList();
    
    
    QString getThemeSvgPath(const QString& iconName) const;
};

#endif 
