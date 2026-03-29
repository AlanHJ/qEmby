#ifndef SERVERMANAGER_H
#define SERVERMANAGER_H

#include "../../qEmbyCore_global.h"
#include "../../models/profile/serverprofile.h"
#include "../../api/apiclient.h"
#include "../../api/networkmanager.h"
#include <QObject>
#include <QList>
#include <QSharedPointer>

class EmbyWebSocket;

class QEMBYCORE_EXPORT ServerManager : public QObject {
    Q_OBJECT
public:
    explicit ServerManager(NetworkManager* nm, QObject* parent = nullptr);

    
    void addServer(const ServerProfile& profile);
    void removeServer(const QString& id);
    void setActiveServer(const QString& id);

    
    QList<ServerProfile> servers() const { return m_servers; }
    ServerProfile activeProfile() const { return m_activeProfile; }
    ApiClient* activeClient() const { return m_activeClient.data(); }

    
    void connectWebSocket();
    void disconnectWebSocket();
    EmbyWebSocket* activeWebSocket() const;

    
    void loadSettings();
    void saveSettings();

    void clearActiveSession();
Q_SIGNALS:
    void serversChanged();
    void activeServerChanged(const ServerProfile& profile);

private:
    NetworkManager* m_network;
    QList<ServerProfile> m_servers;
    ServerProfile m_activeProfile;
    QSharedPointer<ApiClient> m_activeClient; 
    EmbyWebSocket* m_activeWebSocket = nullptr; 
};

#endif
