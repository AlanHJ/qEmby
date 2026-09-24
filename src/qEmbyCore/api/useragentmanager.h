#ifndef USERAGENTMANAGER_H
#define USERAGENTMANAGER_H

#include "../qEmbyCore_global.h"
#include "../models/profile/serverprofile.h"
#include "../models/profile/useragentconfig.h"

#include <QObject>
#include <QList>
#include <QString>
#include <QUrl>

class QNetworkRequest;
class ServerManager;

class QEMBYCORE_EXPORT UserAgentManager : public QObject {
    Q_OBJECT
public:
    static UserAgentManager *instance();

    void attachServerManager(ServerManager *serverManager);

    UserAgentConfig globalConfig() const;
    void setGlobalConfig(const UserAgentConfig &config);

    UserAgentConfig resolveForUrl(const QUrl &url) const;
    UserAgentConfig resolveForServer(const ServerProfile &profile) const;
    UserAgentConfig resolveForServerId(const QString &serverId) const;

    void applyToRequest(QNetworkRequest &request) const;
    void applyToRequest(QNetworkRequest &request,
                        const ServerProfile &profile) const;
    void applyToRequest(QNetworkRequest &request,
                        const QString &serverId) const;

Q_SIGNALS:
    void userAgentChanged();

private:
    explicit UserAgentManager(QObject *parent = nullptr);
    Q_DISABLE_COPY(UserAgentManager)

    const ServerProfile *findServerForUrl(const QUrl &url) const;
    static void applyConfig(QNetworkRequest &request,
                            const UserAgentConfig &config);

    ServerManager *m_serverManager = nullptr;
    mutable QList<ServerProfile> m_serverCache;
    bool m_updatingGlobalConfig = false;
};

#endif 
