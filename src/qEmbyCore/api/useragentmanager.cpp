#include "useragentmanager.h"

#include "../config/config_keys.h"
#include "../config/configstore.h"
#include "../services/manager/servermanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QNetworkRequest>

UserAgentManager *UserAgentManager::instance() {
    static UserAgentManager *manager = new UserAgentManager(qApp);
    return manager;
}

UserAgentManager::UserAgentManager(QObject *parent) : QObject(parent) {
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &) {
                if (m_updatingGlobalConfig) {
                    return;
                }
                if (key == QLatin1String(ConfigKeys::UserAgentEnabled) ||
                    key == QLatin1String(ConfigKeys::UserAgentValue)) {
                    qInfo() << "[UserAgentManager] global configuration changed"
                            << "| enabled:" << globalConfig().enabled
                            << "| hasValue:"
                            << !globalConfig().value.trimmed().isEmpty();
                    Q_EMIT userAgentChanged();
                }
            });
}

void UserAgentManager::attachServerManager(ServerManager *serverManager) {
    if (m_serverManager == serverManager) {
        return;
    }
    if (m_serverManager) {
        disconnect(m_serverManager, nullptr, this, nullptr);
    }
    m_serverManager = serverManager;
    m_serverCache = m_serverManager ? m_serverManager->servers()
                                    : QList<ServerProfile>{};
    if (!m_serverManager) {
        return;
    }
    connect(m_serverManager, &ServerManager::serversChanged, this, [this]() {
        m_serverCache = m_serverManager->servers();
    });
    connect(m_serverManager, &ServerManager::serverUserAgentChanged, this,
            [this](const QString &serverId) {
                m_serverCache = m_serverManager->servers();
                qInfo() << "[UserAgentManager] server configuration changed"
                        << "| serverId:" << serverId;
                Q_EMIT userAgentChanged();
            });
}

UserAgentConfig UserAgentManager::globalConfig() const {
    auto *store = ConfigStore::instance();
    UserAgentConfig config;
    config.enabled =
        store->get<bool>(ConfigKeys::UserAgentEnabled, false);
    config.value = store->get<QString>(ConfigKeys::UserAgentValue);
    return config;
}

void UserAgentManager::setGlobalConfig(const UserAgentConfig &config) {
    UserAgentConfig normalized = config;
    normalized.value = normalized.value.trimmed();
    if (globalConfig() == normalized) {
        return;
    }
    auto *store = ConfigStore::instance();
    m_updatingGlobalConfig = true;
    store->set(ConfigKeys::UserAgentValue, normalized.value);
    store->set(ConfigKeys::UserAgentEnabled, normalized.enabled);
    m_updatingGlobalConfig = false;
    qInfo() << "[UserAgentManager] global configuration updated"
            << "| enabled:" << normalized.enabled
            << "| hasValue:" << !normalized.value.isEmpty();
    Q_EMIT userAgentChanged();
}

UserAgentConfig UserAgentManager::resolveForUrl(const QUrl &url) const {
    if (const ServerProfile *profile = findServerForUrl(url)) {
        return resolveForServer(*profile);
    }
    return globalConfig();
}

UserAgentConfig
UserAgentManager::resolveForServer(const ServerProfile &profile) const {
    return profile.useGlobalUserAgent ? globalConfig() : profile.userAgent;
}

UserAgentConfig
UserAgentManager::resolveForServerId(const QString &serverId) const {
    for (const ServerProfile &profile : m_serverCache) {
        if (profile.id == serverId) {
            return resolveForServer(profile);
        }
    }
    return globalConfig();
}

void UserAgentManager::applyToRequest(QNetworkRequest &request) const {
    applyConfig(request, resolveForUrl(request.url()));
}

void UserAgentManager::applyToRequest(QNetworkRequest &request,
                                      const ServerProfile &profile) const {
    applyConfig(request, resolveForServer(profile));
}

void UserAgentManager::applyToRequest(QNetworkRequest &request,
                                      const QString &serverId) const {
    applyConfig(request, resolveForServerId(serverId));
}

const ServerProfile *UserAgentManager::findServerForUrl(const QUrl &url) const {
    const QString normalizedHost = url.host().trimmed().toLower();
    const QString normalizedScheme = url.scheme().trimmed().toLower();
    if (normalizedHost.isEmpty() || normalizedScheme.isEmpty()) {
        return nullptr;
    }
    const auto effectivePort = [](const QUrl &candidate) {
        if (candidate.port() >= 0) {
            return candidate.port();
        }
        return candidate.scheme().compare(QStringLiteral("https"),
                                          Qt::CaseInsensitive) == 0
                   ? 443
                   : 80;
    };
    const int requestPort = effectivePort(url);
    const ServerProfile *match = nullptr;
    for (const ServerProfile &profile : m_serverCache) {
        const QUrl profileUrl(profile.url);
        if (profileUrl.host().toLower() == normalizedHost &&
            profileUrl.scheme().toLower() == normalizedScheme &&
            effectivePort(profileUrl) == requestPort) {
            
            
            if (match) {
                return nullptr;
            }
            match = &profile;
        }
    }
    return match;
}

void UserAgentManager::applyConfig(QNetworkRequest &request,
                                   const UserAgentConfig &config) {
    if (!config.isEffective()) {
        return;
    }
    const QString value = config.value.trimmed();
    if (value.contains(QLatin1Char('\r')) ||
        value.contains(QLatin1Char('\n'))) {
        qWarning() << "[UserAgentManager] ignored invalid User-Agent value "
                      "containing line breaks";
        return;
    }
    request.setRawHeader(QByteArrayLiteral("User-Agent"),
                         value.toUtf8());
}
