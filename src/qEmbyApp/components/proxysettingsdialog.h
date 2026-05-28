#ifndef PROXYSETTINGSDIALOG_H
#define PROXYSETTINGSDIALOG_H

#include "moderndialogbase.h"
#include "models/profile/proxyconfig.h"
#include <QString>

class QFrame;
class QLabel;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QWidget;
class ModernComboBox;
class ModernSwitch;
class ServerManager;
















class ProxySettingsDialog : public ModernDialogBase {
    Q_OBJECT
public:
    enum class Scope { Global, PerServer };

    
    static ProxySettingsDialog* createForGlobal(QWidget* parent = nullptr);

    
    
    static ProxySettingsDialog* createForServer(ServerManager* sm,
                                                const QString& serverId,
                                                QWidget* parent = nullptr);

    
    
    static ProxySettingsDialog* createForDraft(const ProxyConfig& initial,
                                               bool initialUseGlobal,
                                               QWidget* parent = nullptr);

    
    ProxyConfig resultConfig() const { return m_resultConfig; }
    bool        resultUseGlobal() const { return m_resultUseGlobal; }

protected:
    explicit ProxySettingsDialog(Scope scope, QWidget* parent = nullptr);

private:
    void buildUi();
    void loadInitialValues();
    void wireSignals();
    void updateCustomPanelVisibility();
    void updateFormEnabled();
    void onSaveClicked();
    void onTestClicked();
    void onUseGlobalToggled(bool checked);
    void persistGlobal(const ProxyConfig& cfg);
    void persistServer(const ProxyConfig& cfg, bool useGlobal);

    
    ProxyConfig collectFromUi() const;

    
    
    void applyConfigToUi(const ProxyConfig& cfg);

    
    bool validateForm(QString* errorMessage) const;

    
    Scope        m_scope;
    QString      m_serverId;       
    bool         m_isDraft = false;  
    ServerManager* m_serverManager = nullptr;  
    ProxyConfig  m_initialConfig;
    bool         m_initialUseGlobal = false;

    ProxyConfig  m_resultConfig;
    bool         m_resultUseGlobal = false;

    
    ModernSwitch*    m_useGlobalSwitch = nullptr;  
    QLabel*          m_useGlobalHint = nullptr;
    QWidget*         m_formContainer = nullptr;    
    ModernComboBox*  m_modeCombo = nullptr;
    QLabel*          m_typeLabel = nullptr;        
    ModernComboBox*  m_typeCombo = nullptr;        
    QWidget*         m_customPanel = nullptr;      
    QLineEdit*       m_hostEdit = nullptr;
    QLineEdit*       m_portEdit = nullptr;
    QLineEdit*       m_usernameEdit = nullptr;
    QLineEdit*       m_passwordEdit = nullptr;
    ModernSwitch*    m_bypassSwitch = nullptr;

    
    QPushButton*     m_testBtn = nullptr;
    QLineEdit*       m_testUrlEdit = nullptr;      
    QLabel*          m_testStatusLabel = nullptr;

    
    QPushButton*     m_saveBtn = nullptr;
    QPushButton*     m_cancelBtn = nullptr;
};

#endif  
