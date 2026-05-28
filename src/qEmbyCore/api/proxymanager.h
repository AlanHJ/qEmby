#ifndef PROXYMANAGER_H
#define PROXYMANAGER_H

#include "../qEmbyCore_global.h"
#include "../models/profile/proxyconfig.h"
#include "../models/profile/serverprofile.h"
#include <QHostAddress>
#include <QList>
#include <QNetworkProxy>
#include <QObject>
#include <QString>
#include <QUrl>

class ServerManager;
class QNetworkProxyQuery;



















class QEMBYCORE_EXPORT ProxyManager : public QObject {
    Q_OBJECT
public:
    
    static ProxyManager* instance();

    
    
    static void installApplicationFactory();

    
    
    void attachServerManager(ServerManager* sm);

    
    
    QNetworkProxy resolveForUrl(const QUrl& url) const;

    
    QNetworkProxy resolveForServer(const ServerProfile& profile) const;

    
    
    QNetworkProxy resolveForServerId(const QString& serverId) const;

    
    ProxyConfig globalConfig() const;
    void        setGlobalConfig(const ProxyConfig& cfg);

    
    
    
    static QString toMpvHttpProxy(const QNetworkProxy& proxy);

    
    QList<QNetworkProxy> queryProxy(const QNetworkProxyQuery& query) const;

Q_SIGNALS:
    
    
    
    void proxyChanged();

private:
    explicit ProxyManager(QObject* parent = nullptr);
    ~ProxyManager() override;
    Q_DISABLE_COPY(ProxyManager)

    
    
    const ServerProfile* findServerByHost(const QString& host) const;

    static bool isLoopbackOrLocal(const QString& host);

    void onConfigStoreChanged(const QString& key);

    ServerManager*       m_serverManager = nullptr;
    mutable QList<ServerProfile> m_serverCache;  
};

#endif  
