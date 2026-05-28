#include "configbundle.h"

#include "../../config/configstore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>
#include <QSysInfo>
#include <QVariant>

namespace
{


constexpr char kKSchemaVersion[] = "schemaVersion";
constexpr char kKAppVersion[] = "appVersion";
constexpr char kKDeviceName[] = "deviceName";
constexpr char kKOsName[] = "osName";
constexpr char kKOsPretty[] = "osPretty";
constexpr char kKExportedAt[] = "exportedAt";


constexpr char kKMetadata[] = "metadata";
constexpr char kKServers[] = "servers";
constexpr char kKConfigEntries[] = "configEntries";


QString serversJsonPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("servers.json"));
}

QString compactProductType(QString type)
{
    type = type.trimmed().toLower();
    if (type.isEmpty())
    {
        return QStringLiteral("OS");
    }
    if (type == QStringLiteral("ubuntu"))
        return QStringLiteral("Ubuntu");
    if (type == QStringLiteral("debian"))
        return QStringLiteral("Debian");
    if (type == QStringLiteral("arch"))
        return QStringLiteral("Arch");
    if (type == QStringLiteral("fedora"))
        return QStringLiteral("Fedora");
    if (type == QStringLiteral("centos"))
        return QStringLiteral("CentOS");
    if (type == QStringLiteral("opensuse"))
        return QStringLiteral("openSUSE");
    if (type == QStringLiteral("manjaro"))
        return QStringLiteral("Manjaro");
    if (type == QStringLiteral("rhel"))
        return QStringLiteral("RHEL");
    if (type == QStringLiteral("linux"))
        return QStringLiteral("Linux");
    type[0] = type.at(0).toUpper();
    return type;
}

int windowsBuildNumber()
{
    const QStringList parts = QSysInfo::kernelVersion().split(QLatin1Char('.'));
    bool ok = false;
    const int build = parts.size() >= 3 ? parts.at(2).toInt(&ok) : 0;
    return ok ? build : 0;
}


QString readableOsName()
{
    const QString type = QSysInfo::productType().toLower();
    const QString version = QSysInfo::productVersion().trimmed();
    if (type == QStringLiteral("windows"))
    {
        const int build = windowsBuildNumber();
        if (build >= 22000)
            return QStringLiteral("Win11");
        if (build > 0)
            return QStringLiteral("Win10");
        return version.isEmpty() ? QStringLiteral("Windows")
                                 : QStringLiteral("Windows%1").arg(version);
    }
    if (type == QStringLiteral("macos") || type == QStringLiteral("osx"))
        return version.isEmpty() ? QStringLiteral("macOS")
                                 : QStringLiteral("macOS%1").arg(version);

    const QString name = compactProductType(type);
    return version.isEmpty() ? name : name + version;
}



QJsonValue variantToJson(const QVariant &v)
{
    if (!v.isValid() || v.isNull()) {
        return QJsonValue::Null;
    }

    switch (static_cast<QMetaType::Type>(v.userType())) {
    case QMetaType::Bool:
        return v.toBool();
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
    case QMetaType::ULongLong:
        return v.toLongLong();
    case QMetaType::Double:
    case QMetaType::Float:
        return v.toDouble();
    case QMetaType::QStringList: {
        QJsonArray arr;
        for (const QString &s : v.toStringList()) {
            arr.append(s);
        }
        return arr;
    }
    case QMetaType::QByteArray:
        
        {
            const QByteArray ba = v.toByteArray();
            const QJsonDocument tryDoc = QJsonDocument::fromJson(ba);
            if (!tryDoc.isNull()) {
                if (tryDoc.isObject()) return tryDoc.object();
                if (tryDoc.isArray()) return tryDoc.array();
            }
            return QString::fromUtf8(ba);
        }
    case QMetaType::QString:
    default:
        return v.toString();
    }
}




QVariant jsonToVariant(const QJsonValue &v)
{
    switch (v.type()) {
    case QJsonValue::Bool:
        return v.toBool();
    case QJsonValue::Double: {
        const double d = v.toDouble();
        const qlonglong i = static_cast<qlonglong>(d);
        if (static_cast<double>(i) == d) {
            return QVariant::fromValue(i);
        }
        return d;
    }
    case QJsonValue::String:
        return v.toString();
    case QJsonValue::Array:
        return QString::fromUtf8(
            QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    case QJsonValue::Object:
        return QString::fromUtf8(
            QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    case QJsonValue::Null:
    case QJsonValue::Undefined:
    default:
        return QVariant();
    }
}

} 





QJsonObject ConfigBundleMetadata::toJson() const
{
    QJsonObject obj;
    obj.insert(QString::fromLatin1(kKSchemaVersion), schemaVersion);
    obj.insert(QString::fromLatin1(kKAppVersion), appVersion);
    obj.insert(QString::fromLatin1(kKDeviceName), deviceName);
    obj.insert(QString::fromLatin1(kKOsName), osName);
    obj.insert(QString::fromLatin1(kKOsPretty), osPretty);
    obj.insert(QString::fromLatin1(kKExportedAt), exportedAt);
    return obj;
}

ConfigBundleMetadata ConfigBundleMetadata::fromJson(const QJsonObject &obj)
{
    ConfigBundleMetadata m;
    m.schemaVersion = obj.value(QString::fromLatin1(kKSchemaVersion)).toInt(1);
    m.appVersion = obj.value(QString::fromLatin1(kKAppVersion)).toString();
    m.deviceName = obj.value(QString::fromLatin1(kKDeviceName)).toString();
    m.osName = obj.value(QString::fromLatin1(kKOsName)).toString();
    m.osPretty = obj.value(QString::fromLatin1(kKOsPretty)).toString();
    m.exportedAt = static_cast<qint64>(obj.value(QString::fromLatin1(kKExportedAt)).toDouble(0.0));
    return m;
}

ConfigBundleMetadata ConfigBundleMetadata::current()
{
    ConfigBundleMetadata m;
    m.schemaVersion = 1;
    m.appVersion = QCoreApplication::applicationVersion();
    if (m.appVersion.isEmpty()) {
        m.appVersion = QStringLiteral("dev");
    }
    m.deviceName = QSysInfo::machineHostName();
    m.osName = readableOsName();
    m.osPretty = QSysInfo::prettyProductName();
    m.exportedAt = QDateTime::currentMSecsSinceEpoch();
    return m;
}





const QStringList &ConfigBundle::includedNamespaces()
{
    static const QStringList list = {
        QStringLiteral("general/"),
        QStringLiteral("network/"),
        QStringLiteral("appearance/"),
        QStringLiteral("library/"),
        QStringLiteral("player/"),
        QStringLiteral("danmaku/"),
        QStringLiteral("ext_player/"),
        QStringLiteral("search/"),
        QStringLiteral("sort/"),  
        QStringLiteral("view/"),  
        QStringLiteral("server/"),
        
        
        QStringLiteral("app/"),
    };
    return list;
}

bool ConfigBundle::shouldExcludeKey(const QString &key)
{
    
    static const QStringList exactBlacklist = {
        QStringLiteral("general/last_selected_server_id"),
        QStringLiteral("app/last_selected_server_id"),
        QStringLiteral("appearance/startup_window_state"),
        QStringLiteral("ext_player/detected_list"),
        QStringLiteral("player/last_subtitle_dir"),
    };
    if (exactBlacklist.contains(key)) {
        return true;
    }

    
    static const QStringList prefixBlacklist = {
        QStringLiteral("cache/"),
        QStringLiteral("player/external_subtitle/"),
        QStringLiteral("download/"),
    };
    for (const QString &prefix : prefixBlacklist) {
        if (key.startsWith(prefix)) {
            return true;
        }
    }
    return false;
}





QJsonObject ConfigBundle::toJson() const
{
    QJsonObject root;
    root.insert(QString::fromLatin1(kKMetadata), metadata.toJson());
    root.insert(QString::fromLatin1(kKServers), servers);
    root.insert(QString::fromLatin1(kKConfigEntries), configEntries);
    return root;
}

QByteArray ConfigBundle::serialize(bool indented) const
{
    return QJsonDocument(toJson()).toJson(indented ? QJsonDocument::Indented
                                                   : QJsonDocument::Compact);
}

ConfigBundle ConfigBundle::fromJson(const QJsonObject &obj)
{
    ConfigBundle b;
    b.metadata = ConfigBundleMetadata::fromJson(
        obj.value(QString::fromLatin1(kKMetadata)).toObject());
    b.servers = obj.value(QString::fromLatin1(kKServers)).toArray();
    b.configEntries = obj.value(QString::fromLatin1(kKConfigEntries)).toObject();
    return b;
}

std::optional<ConfigBundle> ConfigBundle::deserialize(const QByteArray &bytes)
{
    QJsonParseError err{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "[ConfigBundle] deserialize parse failed:" << err.errorString();
        return std::nullopt;
    }
    return fromJson(doc.object());
}



ConfigBundle ConfigBundle::collectFromLocal()
{
    ConfigBundle b;
    b.metadata = ConfigBundleMetadata::current();

    
    {
        QFile f(serversJsonPath());
        if (f.open(QIODevice::ReadOnly)) {
            const QByteArray bytes = f.readAll();
            f.close();
            const QJsonDocument doc = QJsonDocument::fromJson(bytes);
            if (doc.isArray()) {
                b.servers = doc.array();
            } else {
                qWarning() << "[ConfigBundle] servers.json is not array, skipped";
            }
        } else {
            qDebug() << "[ConfigBundle] servers.json not present at"
                     << serversJsonPath();
        }
    }

    
    ConfigStore *store = ConfigStore::instance();
    if (store) {
        const QStringList allKeys = store->allKeys();
        const QStringList &nsList = includedNamespaces();

        for (const QString &key : allKeys) {
            
            bool inWhitelist = false;
            for (const QString &ns : nsList) {
                if (key.startsWith(ns)) {
                    inWhitelist = true;
                    break;
                }
            }
            if (!inWhitelist) {
                continue;
            }
            if (shouldExcludeKey(key)) {
                continue;
            }

            const QVariant val = store->get<QVariant>(key);
            b.configEntries.insert(key, variantToJson(val));
        }
    }

    qDebug() << "[ConfigBundle] collectFromLocal"
             << "| servers:" << b.servers.size()
             << "| entries:" << b.configEntries.size()
             << "| host:" << b.metadata.deviceName
             << "| os:" << b.metadata.osPretty
             << "| app:" << b.metadata.appVersion;
    return b;
}
