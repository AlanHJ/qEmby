#include "danmakusettings.h"

#include "../../config/config_keys.h"
#include "../../config/configstore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <QUuid>
#include <utility>

namespace {

QString settingKey(const QString &serverId, const char *baseKey)
{
    return ConfigKeys::forServer(serverId.trimmed(), baseKey);
}

QString normalizeBaseUrl(QString baseUrl)
{
    baseUrl = baseUrl.trimmed();
    while (baseUrl.endsWith('/') && !baseUrl.endsWith(QStringLiteral("://"))) {
        baseUrl.chop(1);
    }
    return baseUrl;
}

QString defaultServerName(const QString &baseUrl)
{
    const QString normalized = normalizeBaseUrl(baseUrl);
    if (normalized.isEmpty() ||
        normalized == QStringLiteral("https://api.dandanplay.net")) {
        return QStringLiteral("DandanPlay Open API");
    }

    const QUrl url = QUrl::fromUserInput(normalized);
    if (!url.host().trimmed().isEmpty()) {
        return url.host().trimmed();
    }
    return normalized;
}

DanmakuServerDefinition defaultServerDefinition()
{
    DanmakuServerDefinition server;
    server.id = QStringLiteral("default");
    server.name = QStringLiteral("DandanPlay Open API");
    server.provider = QStringLiteral("dandanplay");
    server.baseUrl = QStringLiteral("https://api.dandanplay.net");
    server.enabled = true;
    return server;
}

DanmakuServerDefinition legacyServerDefinition(const QString &serverId)
{
    auto *store = ConfigStore::instance();

    DanmakuServerDefinition server;
    server.id = QStringLiteral("legacy");
    server.provider = store->get<QString>(
        settingKey(serverId, ConfigKeys::DanmakuProvider),
        QStringLiteral("dandanplay"));
    server.baseUrl = normalizeBaseUrl(store->get<QString>(
        settingKey(serverId, ConfigKeys::DanmakuProviderBaseUrl),
        QStringLiteral("https://api.dandanplay.net")));
    server.appId = store->get<QString>(
        settingKey(serverId, ConfigKeys::DanmakuProviderAppId));
    server.appSecret = store->get<QString>(
        settingKey(serverId, ConfigKeys::DanmakuProviderAppSecret));
    server.name = defaultServerName(server.baseUrl);
    server.enabled = true;
    return server;
}

int firstEnabledServerIndex(const QList<DanmakuServerDefinition> &servers)
{
    for (int index = 0; index < servers.size(); ++index) {
        if (servers.at(index).enabled) {
            return index;
        }
    }
    return -1;
}

QList<DanmakuServerDefinition> normalizedServers(
    QList<DanmakuServerDefinition> servers)
{
    QSet<QString> usedIds;
    QList<DanmakuServerDefinition> normalized;
    normalized.reserve(servers.size());

    for (DanmakuServerDefinition &server : servers) {
        server.id = server.id.trimmed();
        server.name = server.name.trimmed();
        server.provider = server.provider.trimmed();
        server.baseUrl = normalizeBaseUrl(server.baseUrl);
        server.appId = server.appId.trimmed();
        server.appSecret = server.appSecret.trimmed();

        if (server.provider.isEmpty()) {
            server.provider = QStringLiteral("dandanplay");
        }
        if (server.id.isEmpty()) {
            server.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        if (server.name.isEmpty()) {
            server.name = defaultServerName(server.baseUrl);
        }
        if (server.baseUrl.isEmpty() || usedIds.contains(server.id)) {
            continue;
        }

        usedIds.insert(server.id);
        normalized.append(server);
    }

    if (normalized.isEmpty()) {
        normalized.append(defaultServerDefinition());
    }
    return normalized;
}

QList<DanmakuServerDefinition> parseServers(const QString &json)
{
    QList<DanmakuServerDefinition> servers;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray()) {
        return servers;
    }

    const QJsonArray array = document.array();
    servers.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        DanmakuServerDefinition server;
        server.id = object.value(QStringLiteral("id")).toString().trimmed();
        server.name = object.value(QStringLiteral("name")).toString().trimmed();
        server.provider =
            object.value(QStringLiteral("provider")).toString().trimmed();
        server.baseUrl =
            object.value(QStringLiteral("baseUrl")).toString().trimmed();
        server.appId =
            object.value(QStringLiteral("appId")).toString().trimmed();
        server.appSecret =
            object.value(QStringLiteral("appSecret")).toString().trimmed();
        server.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        servers.append(server);
    }
    return normalizedServers(servers);
}

QJsonArray toJsonArray(const QList<DanmakuServerDefinition> &servers)
{
    QJsonArray array;
    for (const DanmakuServerDefinition &server : servers) {
        QJsonObject object;
        object.insert(QStringLiteral("id"), server.id);
        object.insert(QStringLiteral("name"), server.name);
        object.insert(QStringLiteral("provider"), server.provider);
        object.insert(QStringLiteral("baseUrl"), server.baseUrl);
        object.insert(QStringLiteral("appId"), server.appId);
        object.insert(QStringLiteral("appSecret"), server.appSecret);
        object.insert(QStringLiteral("enabled"), server.enabled);
        array.append(object);
    }
    return array;
}

} 

QList<DanmakuServerDefinition> DanmakuSettings::loadServers(QString serverId)
{
    serverId = serverId.trimmed();
    if (serverId.isEmpty()) {
        return {defaultServerDefinition()};
    }

    const QString storedJson = ConfigStore::instance()->get<QString>(
        settingKey(serverId, ConfigKeys::DanmakuServers));
    if (!storedJson.trimmed().isEmpty()) {
        QList<DanmakuServerDefinition> servers =
            parseServers(storedJson);
        if (!servers.isEmpty()) {
            auto *store = ConfigStore::instance();
            const QString legacySelectedId = store->get<QString>(
                settingKey(serverId, ConfigKeys::DanmakuSelectedServer));
            const QString legacyAppId = store->get<QString>(
                settingKey(serverId, ConfigKeys::DanmakuProviderAppId));
            const QString legacyAppSecret = store->get<QString>(
                settingKey(serverId, ConfigKeys::DanmakuProviderAppSecret));

            if (!legacyAppId.trimmed().isEmpty() ||
                !legacyAppSecret.trimmed().isEmpty()) {
                int selectedIndex = 0;
                for (int index = 0; index < servers.size(); ++index) {
                    if (servers.at(index).id == legacySelectedId) {
                        selectedIndex = index;
                        break;
                    }
                }

                DanmakuServerDefinition &selectedServer =
                    servers[selectedIndex];
                if (selectedServer.appId.trimmed().isEmpty()) {
                    selectedServer.appId = legacyAppId.trimmed();
                }
                if (selectedServer.appSecret.trimmed().isEmpty()) {
                    selectedServer.appSecret = legacyAppSecret.trimmed();
                }
            }
            return servers;
        }
    }

    return normalizedServers({legacyServerDefinition(serverId)});
}

DanmakuServerDefinition DanmakuSettings::selectedServer(QString serverId)
{
    const QList<DanmakuServerDefinition> servers = loadServers(serverId);
    const QString selectedId = selectedServerId(serverId);
    for (const DanmakuServerDefinition &server : servers) {
        if (server.id == selectedId) {
            return server;
        }
    }

    const int enabledIndex = firstEnabledServerIndex(servers);
    if (enabledIndex >= 0) {
        return servers.at(enabledIndex);
    }
    return servers.isEmpty() ? defaultServerDefinition() : servers.first();
}

QString DanmakuSettings::selectedServerId(QString serverId)
{
    const QList<DanmakuServerDefinition> servers = loadServers(serverId);
    if (servers.isEmpty()) {
        return QString();
    }

    serverId = serverId.trimmed();
    if (serverId.isEmpty()) {
        const int enabledIndex = firstEnabledServerIndex(servers);
        return enabledIndex >= 0 ? servers.at(enabledIndex).id
                                 : servers.first().id;
    }

    const QString selectedId = ConfigStore::instance()->get<QString>(
        settingKey(serverId, ConfigKeys::DanmakuSelectedServer));
    for (const DanmakuServerDefinition &server : servers) {
        if (server.id == selectedId && server.enabled) {
            return selectedId;
        }
    }

    const int enabledIndex = firstEnabledServerIndex(servers);
    if (enabledIndex >= 0) {
        return servers.at(enabledIndex).id;
    }
    return servers.first().id;
}

void DanmakuSettings::saveServers(QString serverId,
                                  QList<DanmakuServerDefinition> servers,
                                  QString selectedServerId)
{
    serverId = serverId.trimmed();
    if (serverId.isEmpty()) {
        return;
    }

    servers = normalizedServers(std::move(servers));
    if (selectedServerId.trimmed().isEmpty()) {
        const int enabledIndex = firstEnabledServerIndex(servers);
        selectedServerId =
            enabledIndex >= 0 ? servers.at(enabledIndex).id : servers.first().id;
    }

    bool hasSelectedServer = false;
    for (const DanmakuServerDefinition &server : servers) {
        if (server.id == selectedServerId && server.enabled) {
            hasSelectedServer = true;
            break;
        }
    }
    if (!hasSelectedServer) {
        const int enabledIndex = firstEnabledServerIndex(servers);
        selectedServerId =
            enabledIndex >= 0 ? servers.at(enabledIndex).id : servers.first().id;
    }

    auto *store = ConfigStore::instance();
    store->set(settingKey(serverId, ConfigKeys::DanmakuServers),
               QString::fromUtf8(
                   QJsonDocument(toJsonArray(servers))
                       .toJson(QJsonDocument::Compact)));
    store->set(settingKey(serverId, ConfigKeys::DanmakuSelectedServer),
               selectedServerId);

    DanmakuServerDefinition selected = servers.first();
    for (const DanmakuServerDefinition &server : std::as_const(servers)) {
        if (server.id == selectedServerId) {
            selected = server;
            break;
        }
    }
    store->set(settingKey(serverId, ConfigKeys::DanmakuProvider),
               selected.provider);
    store->set(settingKey(serverId, ConfigKeys::DanmakuProviderBaseUrl),
               selected.baseUrl);
    store->set(settingKey(serverId, ConfigKeys::DanmakuProviderAppId),
               selected.appId);
    store->set(settingKey(serverId, ConfigKeys::DanmakuProviderAppSecret),
               selected.appSecret);
}
