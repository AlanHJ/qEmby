#include "proxymanager.h"

#include "../config/config_keys.h"
#include "../config/configstore.h"
#include "../services/manager/servermanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QUrl>
#include <QtGlobal>

namespace {




class ProxyManagerFactory : public QNetworkProxyFactory {
public:
    QList<QNetworkProxy> queryProxy(
        const QNetworkProxyQuery& query) override {
        if (auto* mgr = ProxyManager::instance()) {
            return mgr->queryProxy(query);
        }
        return {QNetworkProxy(QNetworkProxy::NoProxy)};
    }
};

bool g_factoryInstalled = false;

}  

ProxyManager* ProxyManager::instance() {
    
    static ProxyManager* s_instance = new ProxyManager(qApp);
    return s_instance;
}

void ProxyManager::installApplicationFactory() {
    if (g_factoryInstalled) {
        return;
    }
    
    (void)instance();
    QNetworkProxyFactory::setApplicationProxyFactory(new ProxyManagerFactory());
    g_factoryInstalled = true;
    qInfo() << "[ProxyManager] applicationProxyFactory installed";
}

ProxyManager::ProxyManager(QObject* parent) : QObject(parent) {
    
    auto* cfg = ConfigStore::instance();
    connect(cfg, &ConfigStore::valueChanged, this,
            [this](const QString& key, const QVariant&) {
                if (key.startsWith(QStringLiteral("network/"))) {
                    onConfigStoreChanged(key);
                }
            });
}

ProxyManager::~ProxyManager() = default;

void ProxyManager::attachServerManager(ServerManager* sm) {
    if (m_serverManager == sm) {
        return;
    }
    if (m_serverManager) {
        
        disconnect(m_serverManager, nullptr, this, nullptr);
    }
    m_serverManager = sm;
    if (!m_serverManager) {
        m_serverCache.clear();
        return;
    }

    m_serverCache = m_serverManager->servers();

    connect(m_serverManager, &ServerManager::serversChanged, this, [this]() {
        m_serverCache = m_serverManager->servers();
        qDebug() << "[ProxyManager] server list refreshed | count:"
                 << m_serverCache.size();
        Q_EMIT proxyChanged();
    });
    connect(m_serverManager, &ServerManager::serverProxyChanged, this,
            [this](const QString& id) {
                m_serverCache = m_serverManager->servers();
                qDebug() << "[ProxyManager] server proxy changed | id:" << id;
                Q_EMIT proxyChanged();
            });

    qInfo() << "[ProxyManager] attached to ServerManager"
            << "| servers:" << m_serverCache.size();
}

QNetworkProxy ProxyManager::resolveForUrl(const QUrl& url) const {
    const QString host = url.host(QUrl::FullyDecoded);

    if (const ServerProfile* profile = findServerByHost(host)) {
        if (profile->useGlobalProxy) {
            const auto cfg = globalConfig();
            qDebug() << "[ProxyManager] resolved"
                     << "| host:" << host
                     << "| source: server-uses-global"
                     << "| serverId:" << profile->id
                     << "| proxy:" << cfg.summary();
            return cfg.toQNetworkProxy();
        }
        
        if (profile->proxy.mode == ProxyConfig::Custom &&
            profile->proxy.bypassLocalhost && isLoopbackOrLocal(host)) {
            qDebug() << "[ProxyManager] bypass localhost"
                     << "| host:" << host
                     << "| serverId:" << profile->id;
            return QNetworkProxy(QNetworkProxy::NoProxy);
        }
        qDebug() << "[ProxyManager] resolved"
                 << "| host:" << host
                 << "| source: server"
                 << "| serverId:" << profile->id
                 << "| proxy:" << profile->proxy.summary();
        return profile->proxy.toQNetworkProxy();
    }

    
    const auto cfg = globalConfig();
    if (cfg.mode == ProxyConfig::Custom && cfg.bypassLocalhost &&
        isLoopbackOrLocal(host)) {
        qDebug() << "[ProxyManager] bypass localhost (global) | host:" << host;
        return QNetworkProxy(QNetworkProxy::NoProxy);
    }
    qDebug() << "[ProxyManager] resolved"
             << "| host:" << host
             << "| source: global"
             << "| proxy:" << cfg.summary();
    return cfg.toQNetworkProxy();
}

QNetworkProxy ProxyManager::resolveForServer(
    const ServerProfile& profile) const {
    if (profile.useGlobalProxy) {
        const auto cfg = globalConfig();
        qDebug() << "[ProxyManager] resolveForServer"
                 << "| id:" << profile.id
                 << "| source: server-uses-global"
                 << "| proxy:" << cfg.summary();
        return cfg.toQNetworkProxy();
    }
    qDebug() << "[ProxyManager] resolveForServer"
             << "| id:" << profile.id
             << "| source: server"
             << "| proxy:" << profile.proxy.summary();
    return profile.proxy.toQNetworkProxy();
}

QNetworkProxy ProxyManager::resolveForServerId(const QString& serverId) const {
    if (serverId.isEmpty()) {
        const auto cfg = globalConfig();
        qDebug() << "[ProxyManager] resolveForServerId"
                 << "| serverId: <empty>"
                 << "| source: global"
                 << "| proxy:" << cfg.summary();
        return cfg.toQNetworkProxy();
    }
    
    for (const auto& profile : m_serverCache) {
        if (profile.id == serverId) {
            return resolveForServer(profile);
        }
    }
    
    const auto cfg = globalConfig();
    qDebug() << "[ProxyManager] resolveForServerId"
             << "| serverId:" << serverId
             << "| source: global (server not found)"
             << "| proxy:" << cfg.summary();
    return cfg.toQNetworkProxy();
}

ProxyConfig ProxyManager::globalConfig() const {
    auto* cfg = ConfigStore::instance();
    ProxyConfig p;
    p.mode = static_cast<ProxyConfig::Mode>(
        cfg->get<int>(ConfigKeys::ProxyMode, ProxyConfig::None));
    p.type = static_cast<ProxyConfig::Type>(
        cfg->get<int>(ConfigKeys::ProxyType, ProxyConfig::Http));
    p.host = cfg->get<QString>(ConfigKeys::ProxyHost);
    const int parsedPort = cfg->get<int>(ConfigKeys::ProxyPort, 0);
    p.port = (parsedPort > 0 && parsedPort <= 65535)
                 ? static_cast<quint16>(parsedPort)
                 : 0;
    p.username = cfg->get<QString>(ConfigKeys::ProxyUsername);
    p.password = cfg->get<QString>(ConfigKeys::ProxyPassword);
    p.bypassLocalhost =
        cfg->get<bool>(ConfigKeys::ProxyBypassLocalhost, true);
    return p;
}

void ProxyManager::setGlobalConfig(const ProxyConfig& cfg) {
    auto* store = ConfigStore::instance();
    store->set(ConfigKeys::ProxyMode, static_cast<int>(cfg.mode));
    store->set(ConfigKeys::ProxyType, static_cast<int>(cfg.type));
    store->set(ConfigKeys::ProxyHost, cfg.host);
    store->set(ConfigKeys::ProxyPort, static_cast<int>(cfg.port));
    store->set(ConfigKeys::ProxyUsername, cfg.username);
    store->set(ConfigKeys::ProxyPassword, cfg.password);
    store->set(ConfigKeys::ProxyBypassLocalhost, cfg.bypassLocalhost);
    qInfo() << "[ProxyManager] global proxy updated | proxy:" << cfg.summary();
    
    
}

QString ProxyManager::toMpvHttpProxy(const QNetworkProxy& proxy) {
    if (proxy.type() == QNetworkProxy::NoProxy) {
        return {};
    }
    
    
    
    
    
    
    
    if (proxy.type() == QNetworkProxy::Socks5Proxy) {
        qWarning() << "[ProxyManager] SOCKS5 proxy cannot be used with MPV player"
                   << "| host:" << proxy.hostName()
                   << "| port:" << proxy.port()
                   << "| reason: MPV/FFmpeg limitation (http-proxy option only supports HTTP proxies)"
                   << "| fallback: direct connection";
        return {};
    }
    if (proxy.type() != QNetworkProxy::HttpProxy) {
        qWarning() << "[ProxyManager] unsupported proxy type for MPV"
                   << "| type:" << proxy.type()
                   << "| fallback: direct connection";
        return {};
    }
    const QString host = proxy.hostName().trimmed();
    if (host.isEmpty() || proxy.port() == 0) {
        qWarning() << "[ProxyManager] invalid HTTP proxy configuration for MPV"
                   << "| host:" << host
                   << "| port:" << proxy.port();
        return {};
    }
    QUrl proxyUrl;
    proxyUrl.setScheme(QStringLiteral("http"));
    proxyUrl.setHost(host);
    proxyUrl.setPort(proxy.port());
    if (!proxy.user().isEmpty()) {
        proxyUrl.setUserName(proxy.user());
        proxyUrl.setPassword(proxy.password());
    }
    return proxyUrl.toString(QUrl::FullyEncoded);
}

QList<QNetworkProxy> ProxyManager::queryProxy(
    const QNetworkProxyQuery& query) const {
    const QUrl url = query.url();
    
    if (!url.isValid() || url.host().isEmpty()) {
        const auto cfg = globalConfig();
        return {cfg.toQNetworkProxy()};
    }
    return {resolveForUrl(url)};
}

const ServerProfile* ProxyManager::findServerByHost(const QString& host) const {
    if (host.isEmpty()) {
        return nullptr;
    }
    const QString needle = host.toLower();
    for (const auto& p : m_serverCache) {
        const QUrl u(p.url, QUrl::TolerantMode);
        if (!u.isValid()) {
            continue;
        }
        const QString serverHost = u.host(QUrl::FullyDecoded).toLower();
        if (!serverHost.isEmpty() && serverHost == needle) {
            return &p;  
        }
    }
    return nullptr;
}

bool ProxyManager::isLoopbackOrLocal(const QString& host) {
    if (host.isEmpty()) {
        return false;
    }
    const QString lower = host.toLower();
    if (lower == QStringLiteral("localhost") ||
        lower.endsWith(QStringLiteral(".localhost"))) {
        return true;
    }
    QHostAddress addr(host);
    if (addr.isNull()) {
        return false;
    }
    if (addr.isLoopback() || addr.isLinkLocal()) {
        return true;
    }
    
    if (addr.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 v4 = addr.toIPv4Address();
        if ((v4 & 0xff000000u) == 0x0a000000u) return true;            
        if ((v4 & 0xfff00000u) == 0xac100000u) return true;            
        if ((v4 & 0xffff0000u) == 0xc0a80000u) return true;            
    }
    return false;
}

void ProxyManager::onConfigStoreChanged(const QString& key) {
    qDebug() << "[ProxyManager] config changed | key:" << key
             << "| proxy:" << globalConfig().summary();
    Q_EMIT proxyChanged();
}
