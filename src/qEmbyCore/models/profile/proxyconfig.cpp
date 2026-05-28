#include "proxyconfig.h"

#include <QNetworkProxyFactory>
#include <QNetworkProxyQuery>
#include <QString>

QNetworkProxy ProxyConfig::toQNetworkProxy() const {
    switch (mode) {
    case None:
        return QNetworkProxy(QNetworkProxy::NoProxy);

    case System: {
        
        const auto proxies =
            QNetworkProxyFactory::systemProxyForQuery(QNetworkProxyQuery());
        if (proxies.isEmpty()) {
            return QNetworkProxy(QNetworkProxy::NoProxy);
        }
        return proxies.first();
    }

    case Custom: {
        if (host.trimmed().isEmpty() || port == 0) {
            return QNetworkProxy(QNetworkProxy::NoProxy);
        }
        const QNetworkProxy::ProxyType qtType =
            (type == Socks5) ? QNetworkProxy::Socks5Proxy
                             : QNetworkProxy::HttpProxy;
        return QNetworkProxy(qtType, host.trimmed(), port,
                             username, password);
    }
    }
    return QNetworkProxy(QNetworkProxy::NoProxy);
}

QJsonObject ProxyConfig::toJson() const {
    QJsonObject obj;
    obj["mode"] = static_cast<int>(mode);
    obj["type"] = static_cast<int>(type);
    obj["host"] = host;
    obj["port"] = static_cast<int>(port);
    obj["username"] = username;
    obj["password"] = password;
    obj["bypassLocalhost"] = bypassLocalhost;
    return obj;
}

ProxyConfig ProxyConfig::fromJson(const QJsonObject& obj) {
    ProxyConfig cfg;
    if (obj.isEmpty()) {
        return cfg;
    }
    cfg.mode = static_cast<Mode>(obj.value("mode").toInt(0));
    cfg.type = static_cast<Type>(obj.value("type").toInt(0));
    cfg.host = obj.value("host").toString();
    const int parsedPort = obj.value("port").toInt(0);
    cfg.port = (parsedPort > 0 && parsedPort <= 65535)
                   ? static_cast<quint16>(parsedPort)
                   : 0;
    cfg.username = obj.value("username").toString();
    cfg.password = obj.value("password").toString();
    cfg.bypassLocalhost = obj.value("bypassLocalhost").toBool(true);
    return cfg;
}

QString ProxyConfig::summary() const {
    switch (mode) {
    case None:
        return QStringLiteral("none");
    case System:
        return QStringLiteral("system");
    case Custom: {
        const QString typeLabel =
            (type == Socks5) ? QStringLiteral("socks5") : QStringLiteral("http");
        const QString credLabel =
            username.isEmpty() ? QString() : QStringLiteral(" auth=***");
        return QStringLiteral("custom %1://%2:%3%4")
            .arg(typeLabel,
                 host.trimmed(),
                 QString::number(port),
                 credLabel);
    }
    }
    return QStringLiteral("unknown");
}

bool ProxyConfig::operator==(const ProxyConfig& other) const {
    return mode == other.mode && type == other.type && host == other.host &&
           port == other.port && username == other.username &&
           password == other.password &&
           bypassLocalhost == other.bypassLocalhost;
}
