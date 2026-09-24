#ifndef USERAGENTSETTINGSDIALOG_H
#define USERAGENTSETTINGSDIALOG_H

#include "moderndialogbase.h"
#include "models/profile/useragentconfig.h"

class QLabel;
class QLineEdit;
class QPushButton;
class ModernSwitch;
class ServerManager;

class UserAgentSettingsDialog : public ModernDialogBase {
    Q_OBJECT
public:
    enum class Scope { Global, PerServer };

    static UserAgentSettingsDialog *createForGlobal(QWidget *parent = nullptr);
    static UserAgentSettingsDialog *createForServer(
        ServerManager *serverManager, const QString &serverId,
        QWidget *parent = nullptr);
    static UserAgentSettingsDialog *createForDraft(
        const UserAgentConfig &initial, bool initialUseGlobal,
        QWidget *parent = nullptr);

    UserAgentConfig resultConfig() const { return m_resultConfig; }
    bool resultUseGlobal() const { return m_resultUseGlobal; }

private:
    explicit UserAgentSettingsDialog(Scope scope, QWidget *parent = nullptr);

    void buildUi();
    void loadInitialValues();
    void applyConfigToUi(const UserAgentConfig &config);
    UserAgentConfig collectFromUi() const;
    void onUseGlobalToggled(bool checked);
    void onSaveClicked();

    Scope m_scope;
    ServerManager *m_serverManager = nullptr;
    QString m_serverId;
    UserAgentConfig m_initialConfig;
    bool m_initialUseGlobal = false;
    bool m_isDraft = false;
    UserAgentConfig m_resultConfig;
    bool m_resultUseGlobal = false;

    ModernSwitch *m_useGlobalSwitch = nullptr;
    ModernSwitch *m_enabledSwitch = nullptr;
    QLineEdit *m_valueEdit = nullptr;
    QLabel *m_scopeHint = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_cancelButton = nullptr;
};

#endif 
