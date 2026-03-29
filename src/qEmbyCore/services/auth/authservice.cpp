#include "authservice.h"
#include <QJsonObject>
#include <QUuid>
#include <QUrl>
#include <qcoronetwork.h>
#include <stdexcept>

AuthService::AuthService(NetworkManager* networkManager, ServerManager* serverManager, QObject *parent)
    : QObject(parent), m_networkManager(networkManager), m_serverManager(serverManager)
{
}

QCoro::Task<ServerProfile> AuthService::login(const QString& serverUrl,
                                              const QString& username,
                                              const QString& password,
                                              bool ignoreSslVerification)
{
    QString cleanUrl = serverUrl;
    if (cleanUrl.endsWith('/')) {
        cleanUrl.chop(1);
    }

    NetworkRequestOptions requestOptions;
    requestOptions.ignoreSslErrors = ignoreSslVerification;

    qDebug() << "[AuthService] Login start"
             << "| url:" << cleanUrl
             << "| ignoreSslVerification:" << ignoreSslVerification;

    
    
    QJsonObject infoResp = co_await m_networkManager->get(
        cleanUrl + "/System/Info/Public", QMap<QString, QString>(),
        requestOptions);

    
    ServerProfile tempProfile;
    tempProfile.url = cleanUrl;
    tempProfile.deviceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    tempProfile.ignoreSslVerification = ignoreSslVerification;

    
    QString productName = infoResp["ProductName"].toString();
    if (productName.contains("Jellyfin", Qt::CaseInsensitive)) {
        tempProfile.type = ServerProfile::Jellyfin;
    } else {
        tempProfile.type = ServerProfile::Emby;
    }

    
    QString realName = infoResp["ServerName"].toString();
    if (!realName.isEmpty()) {
        tempProfile.name = realName;
    } else {
        QUrl parsedUrl(cleanUrl);
        tempProfile.name = parsedUrl.host() + (parsedUrl.port() != -1 ? ":" + QString::number(parsedUrl.port()) : "");
    }

    
    ApiClient tempClient(tempProfile, m_networkManager);
    QJsonObject payload;
    payload["Username"] = username;
    payload["Pw"] = password;

    
    QJsonObject authResp = co_await tempClient.post("/Users/AuthenticateByName", payload);

    
    tempProfile.accessToken = authResp["AccessToken"].toString();

    QJsonObject userObj = authResp["User"].toObject();
    tempProfile.userId = userObj["Id"].toString();
    tempProfile.userName = userObj["Name"].toString();

    QJsonObject policyObj = userObj["Policy"].toObject();
    tempProfile.isAdmin = policyObj["IsAdministrator"].toBool();

    
    try {
        
        QString highResIconPath = (tempProfile.type == ServerProfile::Jellyfin) 
                                ? "/web/icon-transparent.png" 
                                : "/web/touchicon.png";

        const QByteArray highResIconBytes = co_await m_networkManager->getBytes(
            tempProfile.url + highResIconPath, QMap<QString, QString>(),
            requestOptions);
        if (!highResIconBytes.isEmpty()) {
            tempProfile.iconBase64 =
                QString::fromUtf8(highResIconBytes.toBase64());
        } else {
            const QByteArray fallbackIconBytes =
                co_await m_networkManager->getBytes(
                    tempProfile.url + "/web/favicon.ico",
                    QMap<QString, QString>(), requestOptions);
            if (!fallbackIconBytes.isEmpty()) {
                tempProfile.iconBase64 =
                    QString::fromUtf8(fallbackIconBytes.toBase64());
            }
        }
    } catch (...) {
        
    }

    
    m_serverManager->addServer(tempProfile);
    m_serverManager->setActiveServer(tempProfile.id);

    
    co_return tempProfile;
}

void AuthService::logout()
{
    
    m_serverManager->clearActiveSession();

    
    Q_EMIT userLoggedOut();
}

QCoro::Task<ServerProfile> AuthService::validateSession(const QString& serverId)
{
    
    QList<ServerProfile> servers = m_serverManager->servers();
    ServerProfile targetProfile;
    bool found = false;
    for (const auto& p : servers) {
        if (p.id == serverId) {
            targetProfile = p;
            found = true;
            break;
        }
    }

    if (!found) {
        throw std::runtime_error(tr("本地存储中找不到该服务器配置。").toStdString());
    }

    
    ApiClient tempClient(targetProfile, m_networkManager);

    
    
    co_await tempClient.get("/System/Info");

    
    m_serverManager->setActiveServer(targetProfile.id);

    co_return targetProfile;
}
