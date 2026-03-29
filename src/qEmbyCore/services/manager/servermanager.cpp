#include "servermanager.h"
#include "../../api/embywebsocket.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QFile>
#include <QStandardPaths>
#include <QDir>

ServerManager::ServerManager(NetworkManager* nm, QObject* parent)
    : QObject(parent), m_network(nm) {
    loadSettings();
}

void ServerManager::addServer(const ServerProfile& profile) {
    
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i].url == profile.url && m_servers[i].userId == profile.userId) {
            m_servers[i] = profile;
            saveSettings();
            Q_EMIT serversChanged();
            return;
        }
    }
    m_servers.append(profile);
    saveSettings();
    Q_EMIT serversChanged();
}

void ServerManager::removeServer(const QString& id) {
    for (int i = 0; i < m_servers.size(); ++i) {
        if (m_servers[i].id == id) {
            
            m_servers.removeAt(i);

            
            saveSettings();
            Q_EMIT serversChanged();

            
            if (m_activeProfile.id == id) {
                
                disconnectWebSocket();
                
                m_activeProfile = ServerProfile();
                m_activeClient.reset();

                
                if (!m_servers.isEmpty()) {
                    setActiveServer(m_servers.first().id);
                } else {
                    
                    Q_EMIT activeServerChanged(m_activeProfile);
                }
            }
            break; 
        }
    }
}

void ServerManager::setActiveServer(const QString& id) {
    for (const auto& profile : m_servers) {
        if (profile.id == id) {
            m_activeProfile = profile;
            
            m_activeClient = QSharedPointer<ApiClient>::create(profile, m_network);
            Q_EMIT activeServerChanged(m_activeProfile);
            return;
        }
    }
}





void ServerManager::connectWebSocket()
{
    
    if (m_activeProfile.url.isEmpty() || m_activeProfile.accessToken.isEmpty()) return;

    
    if (m_activeWebSocket && m_activeWebSocket->isConnected()) return;

    
    if (m_activeWebSocket) {
        m_activeWebSocket->disconnectFromServer();
        m_activeWebSocket->deleteLater();
        m_activeWebSocket = nullptr;
    }

    
    m_activeWebSocket = new EmbyWebSocket(m_activeProfile, this);
    m_activeWebSocket->connectToServer();
}

void ServerManager::disconnectWebSocket()
{
    if (m_activeWebSocket) {
        m_activeWebSocket->disconnectFromServer();
        m_activeWebSocket->deleteLater();
        m_activeWebSocket = nullptr;
    }
}

EmbyWebSocket* ServerManager::activeWebSocket() const
{
    return m_activeWebSocket;
}





void ServerManager::saveSettings() {
    
    QJsonArray array;
    for (const auto& p : m_servers) {
        QJsonObject obj;
        obj["id"] = p.id;
        obj["name"] = p.name;
        obj["url"] = p.url;
        obj["type"] = (p.type == ServerProfile::Emby ? "Emby" : "Jellyfin");
        obj["ignoreSslVerification"] = p.ignoreSslVerification;
        obj["userId"] = p.userId;
        obj["userName"] = p.userName;
        obj["accessToken"] = p.accessToken;
        obj["deviceId"] = p.deviceId;
        obj["isAdmin"] = p.isAdmin;
        obj["iconBase64"] = p.iconBase64;
        array.append(obj);
    }

    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(path);
    QFile file(path + "/servers.json");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(array).toJson());
    }
}

void ServerManager::loadSettings() {
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QFile file(path + "/servers.json");
    if (!file.open(QIODevice::ReadOnly)) return;

    QJsonArray array = QJsonDocument::fromJson(file.readAll()).array();
    m_servers.clear();
    for (auto val : array) {
        QJsonObject obj = val.toObject();
        ServerProfile p;
        p.id = obj["id"].toString();
        p.name = obj["name"].toString();
        p.url = obj["url"].toString();
        p.type = (obj["type"].toString() == "Emby" ? ServerProfile::Emby : ServerProfile::Jellyfin);
        p.ignoreSslVerification = obj["ignoreSslVerification"].toBool(false);
        p.userId = obj["userId"].toString();
        p.userName = obj["userName"].toString();
        p.accessToken = obj["accessToken"].toString();
        p.deviceId = obj["deviceId"].toString();
        p.isAdmin = obj["isAdmin"].toBool();
        p.iconBase64 = obj["iconBase64"].toString();
        m_servers.append(p);
    }

    
    if (!m_servers.isEmpty()) {
        setActiveServer(m_servers.first().id);
    }
}

void ServerManager::clearActiveSession()
{
    disconnectWebSocket();
    m_activeProfile = ServerProfile();
    m_activeClient.reset();
    Q_EMIT activeServerChanged(m_activeProfile);
}

