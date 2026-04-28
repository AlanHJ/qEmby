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

namespace
{

constexpr auto kOfficialDandanplayServerId = "default";
constexpr auto kOfficialDandanplayProvider = "dandanplay";
constexpr auto kOfficialDandanplayName = "DandanPlay Open API";
constexpr auto kOfficialDandanplayBaseUrl = "https://api.dandanplay.net";
constexpr auto kOfficialDandanplayContentScope = "anime";


constexpr auto kBuiltInOfficialDandanplayAppId = "wyptw278x3";
constexpr auto kBuiltInOfficialDandanplayAppSecret = "ApQZbydpeafcDTe8pBX3EJxifAuhfkSn";

QString settingKey(const QString &serverId, const char *baseKey)
{
    return ConfigKeys::forServer(serverId.trimmed(), baseKey);
}

QString normalizeBaseUrl(QString baseUrl)
{
    baseUrl = baseUrl.trimmed();
    while (baseUrl.endsWith('/') && !baseUrl.endsWith(QStringLiteral("://")))
    {
        baseUrl.chop(1);
    }
    return baseUrl;
}

QString normalizedHost(const QString &baseUrl)
{
    const QUrl url = QUrl::fromUserInput(baseUrl.trimmed());
    return url.host().trimmed().toLower();
}

bool isOfficialDandanplayEndpoint(const QString &baseUrl)
{
    return normalizedHost(baseUrl) == QStringLiteral("api.dandanplay.net");
}

bool isBuiltInOfficialDandanplayServer(const DanmakuServerDefinition &server)
{
    return server.builtIn ||
           (server.id == QLatin1String(kOfficialDandanplayServerId) && isOfficialDandanplayEndpoint(server.baseUrl));
}

QString defaultServerName(const QString &baseUrl)
{
    const QString normalized = normalizeBaseUrl(baseUrl);
    if (normalized.isEmpty() || normalized == QString::fromLatin1(kOfficialDandanplayBaseUrl))
    {
        return QString::fromLatin1(kOfficialDandanplayName);
    }

    const QUrl url = QUrl::fromUserInput(normalized);
    if (!url.host().trimmed().isEmpty())
    {
        return url.host().trimmed();
    }
    return normalized;
}

DanmakuServerDefinition makeBuiltInOfficialDandanplayServer()
{
    DanmakuServerDefinition server;
    server.id = QString::fromLatin1(kOfficialDandanplayServerId);
    server.name = QString::fromLatin1(kOfficialDandanplayName);
    server.provider = QString::fromLatin1(kOfficialDandanplayProvider);
    server.baseUrl = QString::fromLatin1(kOfficialDandanplayBaseUrl);
    server.appId = QString::fromLatin1(kBuiltInOfficialDandanplayAppId);
    server.appSecret = QString::fromLatin1(kBuiltInOfficialDandanplayAppSecret);
    server.contentScope = QString::fromLatin1(kOfficialDandanplayContentScope);
    server.builtIn = true;
    server.enabled = true;
    return server;
}

void applyBuiltInOfficialDandanplayDefaults(DanmakuServerDefinition *server)
{
    if (!server || !isBuiltInOfficialDandanplayServer(*server))
    {
        return;
    }

    const DanmakuServerDefinition builtIn = makeBuiltInOfficialDandanplayServer();
    server->builtIn = true;
    server->id = builtIn.id;
    server->baseUrl = builtIn.baseUrl;
    if (server->provider.trimmed().isEmpty())
    {
        server->provider = builtIn.provider;
    }
    if (server->name.trimmed().isEmpty())
    {
        server->name = builtIn.name;
    }
    if (server->appId.trimmed().isEmpty())
    {
        server->appId = builtIn.appId;
    }
    if (server->appSecret.trimmed().isEmpty())
    {
        server->appSecret = builtIn.appSecret;
    }
    if (server->contentScope.trimmed().isEmpty())
    {
        server->contentScope = builtIn.contentScope;
    }
}

DanmakuServerDefinition legacyServerDefinition(const QString &serverId)
{
    auto *store = ConfigStore::instance();

    DanmakuServerDefinition server = makeBuiltInOfficialDandanplayServer();
    server.provider = store->get<QString>(settingKey(serverId, ConfigKeys::DanmakuProvider), server.provider);
    server.baseUrl =
        normalizeBaseUrl(store->get<QString>(settingKey(serverId, ConfigKeys::DanmakuProviderBaseUrl), server.baseUrl));
    const QString legacyAppId = store->get<QString>(settingKey(serverId, ConfigKeys::DanmakuProviderAppId));
    const QString legacyAppSecret = store->get<QString>(settingKey(serverId, ConfigKeys::DanmakuProviderAppSecret));
    const bool hasLegacyAppId = !legacyAppId.trimmed().isEmpty();
    const bool hasLegacyAppSecret = !legacyAppSecret.trimmed().isEmpty();
    const bool officialEndpoint = isOfficialDandanplayEndpoint(server.baseUrl);
    if (!officialEndpoint && hasLegacyAppId)
    {
        server.appId = legacyAppId.trimmed();
    }
    if (!officialEndpoint && hasLegacyAppSecret)
    {
        server.appSecret = legacyAppSecret.trimmed();
    }
    server.name = defaultServerName(server.baseUrl);
    if (!officialEndpoint)
    {
        server.contentScope.clear();
        if (!hasLegacyAppId)
        {
            server.appId.clear();
        }
        if (!hasLegacyAppSecret)
        {
            server.appSecret.clear();
        }
        server.builtIn = false;
    }
    applyBuiltInOfficialDandanplayDefaults(&server);
    server.enabled = true;
    return server;
}

int firstEnabledServerIndex(const QList<DanmakuServerDefinition> &servers)
{
    for (int index = 0; index < servers.size(); ++index)
    {
        if (servers.at(index).enabled)
        {
            return index;
        }
    }
    return -1;
}

QList<DanmakuServerDefinition> normalizedServers(QList<DanmakuServerDefinition> servers)
{
    QSet<QString> usedIds;
    QList<DanmakuServerDefinition> normalized;
    normalized.reserve(servers.size());

    for (DanmakuServerDefinition &server : servers)
    {
        server.id = server.id.trimmed();
        server.name = server.name.trimmed();
        server.provider = server.provider.trimmed();
        server.baseUrl = normalizeBaseUrl(server.baseUrl);
        server.description = server.description.trimmed();
        server.appId = server.appId.trimmed();
        server.appSecret = server.appSecret.trimmed();
        server.contentScope = server.contentScope.trimmed().toLower();
        server.builtIn = isBuiltInOfficialDandanplayServer(server);

        if (server.provider.isEmpty())
        {
            server.provider = QString::fromLatin1(kOfficialDandanplayProvider);
        }
        if (server.id.isEmpty())
        {
            server.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        }
        applyBuiltInOfficialDandanplayDefaults(&server);
        if (server.name.isEmpty())
        {
            server.name = defaultServerName(server.baseUrl);
        }
        if (server.baseUrl.isEmpty() || usedIds.contains(server.id))
        {
            continue;
        }

        usedIds.insert(server.id);
        normalized.append(server);
    }

    bool hasBuiltInOfficialServer = false;
    for (const DanmakuServerDefinition &server : std::as_const(normalized))
    {
        if (isBuiltInOfficialDandanplayServer(server))
        {
            hasBuiltInOfficialServer = true;
            break;
        }
    }
    if (!hasBuiltInOfficialServer)
    {
        normalized.prepend(makeBuiltInOfficialDandanplayServer());
    }
    return normalized;
}

QList<DanmakuServerDefinition> parseServers(const QString &json)
{
    QList<DanmakuServerDefinition> servers;
    const QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isArray())
    {
        return servers;
    }

    const QJsonArray array = document.array();
    servers.reserve(array.size());
    for (const QJsonValue &value : array)
    {
        const QJsonObject object = value.toObject();
        DanmakuServerDefinition server;
        server.id = object.value(QStringLiteral("id")).toString().trimmed();
        server.name = object.value(QStringLiteral("name")).toString().trimmed();
        server.provider = object.value(QStringLiteral("provider")).toString().trimmed();
        server.baseUrl = object.value(QStringLiteral("baseUrl")).toString().trimmed();
        server.description = object.value(QStringLiteral("description")).toString().trimmed();
        server.appId = object.value(QStringLiteral("appId")).toString().trimmed();
        server.appSecret = object.value(QStringLiteral("appSecret")).toString().trimmed();
        server.contentScope = object.value(QStringLiteral("contentScope")).toString().trimmed();
        server.builtIn = object.value(QStringLiteral("builtIn")).toBool(false);
        server.enabled = object.value(QStringLiteral("enabled")).toBool(true);
        servers.append(server);
    }
    return normalizedServers(servers);
}

QJsonArray toJsonArray(const QList<DanmakuServerDefinition> &servers)
{
    QJsonArray array;
    for (const DanmakuServerDefinition &server : servers)
    {
        DanmakuServerDefinition savedServer = server;
        if (isBuiltInOfficialDandanplayServer(savedServer))
        {
            savedServer.builtIn = true;
            savedServer.description.clear();
            savedServer.appId.clear();
            savedServer.appSecret.clear();
        }

        QJsonObject object;
        object.insert(QStringLiteral("id"), savedServer.id);
        object.insert(QStringLiteral("name"), savedServer.name);
        object.insert(QStringLiteral("provider"), savedServer.provider);
        object.insert(QStringLiteral("baseUrl"), savedServer.baseUrl);
        object.insert(QStringLiteral("description"), savedServer.description);
        object.insert(QStringLiteral("appId"), savedServer.appId);
        object.insert(QStringLiteral("appSecret"), savedServer.appSecret);
        object.insert(QStringLiteral("contentScope"), savedServer.contentScope);
        object.insert(QStringLiteral("builtIn"), savedServer.builtIn);
        object.insert(QStringLiteral("enabled"), savedServer.enabled);
        array.append(object);
    }
    return array;
}

} 

DanmakuServerDefinition DanmakuSettings::builtInOfficialDandanplayServer()
{
    return makeBuiltInOfficialDandanplayServer();
}

QList<DanmakuServerDefinition> DanmakuSettings::loadServers(QString serverId)
{
    serverId = serverId.trimmed();
    if (serverId.isEmpty())
    {
        return {builtInOfficialDandanplayServer()};
    }

    const QString storedJson = ConfigStore::instance()->get<QString>(settingKey(serverId, ConfigKeys::DanmakuServers));
    if (!storedJson.trimmed().isEmpty())
    {
        QList<DanmakuServerDefinition> servers = parseServers(storedJson);
        if (!servers.isEmpty())
        {
            auto *store = ConfigStore::instance();
            const QString legacySelectedId =
                store->get<QString>(settingKey(serverId, ConfigKeys::DanmakuSelectedServer));
            const QString legacyAppId = store->get<QString>(settingKey(serverId, ConfigKeys::DanmakuProviderAppId));
            const QString legacyAppSecret =
                store->get<QString>(settingKey(serverId, ConfigKeys::DanmakuProviderAppSecret));

            if (!legacyAppId.trimmed().isEmpty() || !legacyAppSecret.trimmed().isEmpty())
            {
                int selectedIndex = 0;
                for (int index = 0; index < servers.size(); ++index)
                {
                    if (servers.at(index).id == legacySelectedId)
                    {
                        selectedIndex = index;
                        break;
                    }
                }

                DanmakuServerDefinition &selectedServer = servers[selectedIndex];
                if (!selectedServer.builtIn && selectedServer.appId.trimmed().isEmpty())
                {
                    selectedServer.appId = legacyAppId.trimmed();
                }
                if (!selectedServer.builtIn && selectedServer.appSecret.trimmed().isEmpty())
                {
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
    for (const DanmakuServerDefinition &server : servers)
    {
        if (server.id == selectedId)
        {
            return server;
        }
    }

    const int enabledIndex = firstEnabledServerIndex(servers);
    if (enabledIndex >= 0)
    {
        return servers.at(enabledIndex);
    }
    return servers.isEmpty() ? builtInOfficialDandanplayServer() : servers.first();
}

QString DanmakuSettings::selectedServerId(QString serverId)
{
    const QList<DanmakuServerDefinition> servers = loadServers(serverId);
    if (servers.isEmpty())
    {
        return QString();
    }

    serverId = serverId.trimmed();
    if (serverId.isEmpty())
    {
        const int enabledIndex = firstEnabledServerIndex(servers);
        return enabledIndex >= 0 ? servers.at(enabledIndex).id : QString();
    }

    const QString selectedId =
        ConfigStore::instance()->get<QString>(settingKey(serverId, ConfigKeys::DanmakuSelectedServer));
    for (const DanmakuServerDefinition &server : servers)
    {
        if (server.id == selectedId && server.enabled)
        {
            return selectedId;
        }
    }

    const int enabledIndex = firstEnabledServerIndex(servers);
    if (enabledIndex >= 0)
    {
        return servers.at(enabledIndex).id;
    }
    return QString();
}

void DanmakuSettings::saveServers(QString serverId, QList<DanmakuServerDefinition> servers, QString selectedServerId)
{
    serverId = serverId.trimmed();
    if (serverId.isEmpty())
    {
        return;
    }

    servers = normalizedServers(std::move(servers));
    if (selectedServerId.trimmed().isEmpty())
    {
        const int enabledIndex = firstEnabledServerIndex(servers);
        selectedServerId = enabledIndex >= 0 ? servers.at(enabledIndex).id : QString();
    }

    bool hasSelectedServer = false;
    if (!selectedServerId.trimmed().isEmpty())
    {
        for (const DanmakuServerDefinition &server : servers)
        {
            if (server.id == selectedServerId && server.enabled)
            {
                hasSelectedServer = true;
                break;
            }
        }
    }
    if (!hasSelectedServer)
    {
        const int enabledIndex = firstEnabledServerIndex(servers);
        selectedServerId = enabledIndex >= 0 ? servers.at(enabledIndex).id : QString();
    }

    auto *store = ConfigStore::instance();
    store->set(settingKey(serverId, ConfigKeys::DanmakuServers),
               QString::fromUtf8(QJsonDocument(toJsonArray(servers)).toJson(QJsonDocument::Compact)));
    store->set(settingKey(serverId, ConfigKeys::DanmakuSelectedServer), selectedServerId);

    DanmakuServerDefinition selected = servers.isEmpty() ? makeBuiltInOfficialDandanplayServer() : servers.first();
    if (!selectedServerId.trimmed().isEmpty())
    {
        for (const DanmakuServerDefinition &server : std::as_const(servers))
        {
            if (server.id == selectedServerId)
            {
                selected = server;
                break;
            }
        }
    }
    if (isBuiltInOfficialDandanplayServer(selected))
    {
        selected.appId.clear();
        selected.appSecret.clear();
    }
    store->set(settingKey(serverId, ConfigKeys::DanmakuProvider), selected.provider);
    store->set(settingKey(serverId, ConfigKeys::DanmakuProviderBaseUrl), selected.baseUrl);
    store->set(settingKey(serverId, ConfigKeys::DanmakuProviderAppId), selected.appId);
    store->set(settingKey(serverId, ConfigKeys::DanmakuProviderAppSecret), selected.appSecret);
}
