#ifndef DANMAKUSERVERDIALOG_H
#define DANMAKUSERVERDIALOG_H

#include "moderndialogbase.h"

#include "models/danmaku/danmakumodels.h"

#include <QList>
#include <QString>

class QListWidget;
class QLineEdit;
class QLabel;
class QPushButton;
class QEvent;
class QShowEvent;

class DanmakuServerDialog : public ModernDialogBase
{
    Q_OBJECT
public:
    explicit DanmakuServerDialog(QWidget *parent = nullptr);

    void setServers(QList<DanmakuServerDefinition> servers,
                    QString selectedServerId = QString());
    QList<DanmakuServerDefinition> servers() const;
    QString selectedServerId() const;

protected:
    void accept() override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    int currentServerIndex() const;
    int editingServerIndex() const;
    int enabledServerCount() const;
    int preferredServerIndex(int preferredIndex) const;
    void removeServer(int index);
    void selectServer(int index);
    void refreshServerList();
    void refreshServerListItem(int index);
    void syncServerListItemSize(int index);
    void syncAllServerListItemSizes();
    void loadCurrentServer();
    void updateSelectionState();
    void updateEditorState();
    void updateServerEnabled(int index, bool enabled);
    void updateCurrentServerName(const QString &name);
    void updateCurrentServerUrl(const QString &baseUrl);
    void updateCurrentServerDescription(const QString &description);
    void updateCurrentServerAppId(const QString &appId);
    void updateCurrentServerAppSecret(const QString &appSecret);
    void updateActionState();

    static QString createServerId();
    static QString normalizeBaseUrl(QString baseUrl);

    QList<DanmakuServerDefinition> m_servers;
    QString m_selectedServerId;
    QString m_editingServerId;
    bool m_loading = false;

    QListWidget *m_serverList = nullptr;
    QLineEdit *m_nameEdit = nullptr;
    QLineEdit *m_baseUrlEdit = nullptr;
    QLabel *m_descriptionLabel = nullptr;
    QLineEdit *m_descriptionEdit = nullptr;
    QLabel *m_appIdLabel = nullptr;
    QLineEdit *m_appIdEdit = nullptr;
    QLabel *m_appSecretLabel = nullptr;
    QLineEdit *m_appSecretEdit = nullptr;
    QPushButton *m_addButton = nullptr;
};

#endif 
