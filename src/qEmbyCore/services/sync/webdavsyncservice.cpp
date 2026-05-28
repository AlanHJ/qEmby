#include "webdavsyncservice.h"

#include "../../api/webdav/webdavclient.h"
#include "../../config/configstore.h"
#include "../../utils/securesecretbox.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QTimeZone>

#include <algorithm>
#include <memory>
#include <stdexcept>
#include <utility>

namespace
{

constexpr char kLegacySnapshotPrefix[] = "qemby-config-";
constexpr char kLegacySnapshotSuffix[] = ".bin";
constexpr char kSnapshotPrefix[] = "qEmby-";
constexpr char kSnapshotSuffix[] = "-config.bin";
constexpr char kDefaultSnapshotTag[] = "snapshot";

QString sanitizeNameSegment(const QString &s, const QString &fallback)
{
    const QString trimmed = s.trimmed();
    QString out;
    out.reserve(trimmed.size());
    for (QChar c : trimmed)
    {
        if (c.isLetterOrNumber() || c == QLatin1Char('_') ||
            c == QLatin1Char('-') || c == QLatin1Char('.'))
        {
            out.append(c);
        }
        else if (c.isSpace())
        {
            out.append(QLatin1Char('_'));
        }
        else
        {
            out.append(QLatin1Char('_'));
        }
    }
    if (out.isEmpty())
    {
        out = fallback;
    }
    return out;
}

bool looksEncryptedPayload(const QByteArray &bytes)
{
    return bytes.size() >= 4 && bytes.left(4) == QByteArrayLiteral("QSB1");
}


QString serversJsonPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(dir).filePath(QStringLiteral("servers.json"));
}

QJsonArray readServersJson()
{
    QFile f(serversJsonPath());
    if (!f.open(QIODevice::ReadOnly))
    {
        return QJsonArray();
    }
    const QByteArray bytes = f.readAll();
    f.close();
    return QJsonDocument::fromJson(bytes).array();
}

bool writeServersJson(const QJsonArray &arr)
{
    const QString path = serversJsonPath();
    const QString dir = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dir))
    {
        qWarning() << "[WebdavSyncService] failed to create dir for servers.json:" << dir;
        return false;
    }

    const QByteArray payload = QJsonDocument(arr).toJson(QJsonDocument::Indented);

    const QString tmpPath = path + QStringLiteral(".tmp");
    {
        QFile out(tmpPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning() << "[WebdavSyncService] failed to open" << tmpPath
                       << "|" << out.errorString();
            return false;
        }
        if (out.write(payload) != payload.size())
        {
            out.close();
            QFile::remove(tmpPath);
            return false;
        }
        out.close();
    }

    QFile::remove(path);
    if (!QFile::rename(tmpPath, path))
    {
        if (!QFile::copy(tmpPath, path))
        {
            qWarning() << "[WebdavSyncService] writeServersJson rename/copy failed";
            return false;
        }
        QFile::remove(tmpPath);
    }
    return true;
}


QSet<QString> idSetOfArray(const QJsonArray &arr)
{
    QSet<QString> ids;
    for (const QJsonValue &v : arr)
    {
        const QString id = v.toObject().value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
        {
            ids.insert(id);
        }
    }
    return ids;
}



QVariant jsonValueToVariant(const QJsonValue &v)
{
    switch (v.type())
    {
    case QJsonValue::Bool:
        return v.toBool();
    case QJsonValue::Double: {
        const double d = v.toDouble();
        const qlonglong i = static_cast<qlonglong>(d);
        if (static_cast<double>(i) == d)
        {
            return QVariant::fromValue(i);
        }
        return d;
    }
    case QJsonValue::String:
        return v.toString();
    case QJsonValue::Array:
        return QString::fromUtf8(QJsonDocument(v.toArray()).toJson(QJsonDocument::Compact));
    case QJsonValue::Object:
        return QString::fromUtf8(QJsonDocument(v.toObject()).toJson(QJsonDocument::Compact));
    case QJsonValue::Null:
    case QJsonValue::Undefined:
    default:
        return QVariant();
    }
}

bool keyMatchesIncludedNamespace(const QString &key)
{
    for (const QString &ns : ConfigBundle::includedNamespaces())
    {
        if (key.startsWith(ns))
        {
            return true;
        }
    }
    return false;
}

} 





WebdavSyncService::WebdavSyncService(WebdavProfileStore *store, QObject *parent)
    : QObject(parent), m_store(store)
{
}

WebdavSyncService::~WebdavSyncService() = default;





QString WebdavSyncService::proposedSnapshotFileName(const ConfigBundleMetadata &metadata,
                                                    QString customTag)
{
    const qint64 exportedAt = metadata.exportedAt > 0
                                  ? metadata.exportedAt
                                  : QDateTime::currentMSecsSinceEpoch();
    const QString version = sanitizeNameSegment(metadata.appVersion, QStringLiteral("dev"));
    const QString osName = sanitizeNameSegment(metadata.osName, QStringLiteral("OS"));
    const QString device = sanitizeNameSegment(metadata.deviceName, QStringLiteral("device"));
    const QString tag = sanitizeNameSegment(customTag, QString::fromLatin1(kDefaultSnapshotTag));
    const QString ts = QDateTime::fromMSecsSinceEpoch(exportedAt, Qt::LocalTime)
                           .toString(QStringLiteral("yyyy-MM-dd-HH-mm-ss"));

    return QString::fromLatin1(kSnapshotPrefix) + version + QLatin1Char('-') + osName +
           QLatin1Char('-') + device + QLatin1Char('-') + ts + QLatin1Char('-') + tag +
           QString::fromLatin1(kSnapshotSuffix);
}

bool WebdavSyncService::isQEmbySnapshotName(const QString &fileName)
{
    const bool isNewName = fileName.startsWith(QString::fromLatin1(kSnapshotPrefix)) &&
                           fileName.endsWith(QString::fromLatin1(kSnapshotSuffix));
    const bool isLegacyName =
        fileName.startsWith(QString::fromLatin1(kLegacySnapshotPrefix)) &&
        fileName.endsWith(QString::fromLatin1(kLegacySnapshotSuffix));
    return isNewName || isLegacyName;
}

void WebdavSyncService::parseSnapshotName(const QString &fileName,
                                          QString &appVersionOut,
                                          QString &osNameOut,
                                          QString &deviceHintOut,
                                          QString &customTagOut,
                                          QDateTime &createdAtOut)
{
    appVersionOut.clear();
    osNameOut.clear();
    deviceHintOut.clear();
    customTagOut.clear();
    createdAtOut = QDateTime();

    if (!isQEmbySnapshotName(fileName))
    {
        return;
    }

    if (fileName.startsWith(QString::fromLatin1(kSnapshotPrefix)) &&
        fileName.endsWith(QString::fromLatin1(kSnapshotSuffix)))
    {
        const int prefixLen = static_cast<int>(sizeof(kSnapshotPrefix)) - 1;
        const int suffixLen = static_cast<int>(sizeof(kSnapshotSuffix)) - 1;
        const QString core = fileName.mid(prefixLen, fileName.size() - prefixLen - suffixLen);

        static const QRegularExpression tsPattern(QStringLiteral(
            "-(\\d{4}-\\d{2}-\\d{2}(?: \\d{2}:\\d{2}:\\d{2}|-\\d{2}-\\d{2}-\\d{2}))-"));
        const QRegularExpressionMatch match = tsPattern.match(core);
        if (!match.hasMatch())
        {
            deviceHintOut = core;
            return;
        }

        const QString beforeTs = core.left(match.capturedStart(0));
        customTagOut = core.mid(match.capturedEnd(0));
        const int firstDash = beforeTs.indexOf(QLatin1Char('-'));
        const int secondDash = firstDash >= 0
                                   ? beforeTs.indexOf(QLatin1Char('-'), firstDash + 1)
                                   : -1;
        if (firstDash >= 0)
        {
            appVersionOut = beforeTs.left(firstDash);
        }
        if (secondDash >= 0)
        {
            osNameOut = beforeTs.mid(firstDash + 1, secondDash - firstDash - 1);
            deviceHintOut = beforeTs.mid(secondDash + 1);
        }
        else
        {
            deviceHintOut = beforeTs;
        }

        const QString timestampText = match.captured(1);
        const QString timestampFormat = timestampText.contains(QLatin1Char(':'))
                                            ? QStringLiteral("yyyy-MM-dd HH:mm:ss")
                                            : QStringLiteral("yyyy-MM-dd-HH-mm-ss");
        const QDateTime parsed = QDateTime::fromString(timestampText, timestampFormat);
        if (parsed.isValid())
        {
            createdAtOut = QDateTime(parsed.date(), parsed.time(),
                                     QTimeZone::systemTimeZone());
        }
        return;
    }

    const int prefixLen = static_cast<int>(sizeof(kLegacySnapshotPrefix)) - 1;
    const int suffixLen = static_cast<int>(sizeof(kLegacySnapshotSuffix)) - 1;
    const QString core = fileName.mid(prefixLen, fileName.size() - prefixLen - suffixLen);

    
    const int lastDash = core.lastIndexOf(QLatin1Char('-'));
    const int secondLastDash = lastDash >= 0
                                   ? core.lastIndexOf(QLatin1Char('-'), lastDash - 1)
                                   : -1;
    if (secondLastDash < 0)
    {
        deviceHintOut = core;
        return;
    }

    const QString tsPart = core.mid(secondLastDash + 1);
    deviceHintOut = core.left(secondLastDash);

    QDateTime parsed = QDateTime::fromString(tsPart, QStringLiteral("yyyyMMdd-HHmmss"));
    if (parsed.isValid())
    {
        parsed.setTimeZone(QTimeZone(QTimeZone::UTC));
        createdAtOut = parsed;
    }
}

WebdavClient *WebdavSyncService::buildClient()
{
    if (!m_store)
    {
        throw std::runtime_error(
            tr("Internal error: WebDAV profile store is unavailable.")
                .toUtf8()
                .toStdString());
    }
    if (!m_store->hasProfile())
    {
        throw std::runtime_error(
            tr("WebDAV profile is not configured. Please save a profile first.")
                .toUtf8()
                .toStdString());
    }

    WebdavProfile profile = m_store->profile();
    if (!profile.isValid())
    {
        throw std::runtime_error(
            tr("WebDAV profile is incomplete. Please fill in URL, username and password.")
                .toUtf8()
                .toStdString());
    }

    
    return new WebdavClient(profile, nullptr);
}





QCoro::Task<bool> WebdavSyncService::testConnection()
{
    qDebug() << "[WebdavSyncService] testConnection START";
    std::unique_ptr<WebdavClient> client(buildClient());

    
    const bool ok = co_await client->ensureRootDir();
    qDebug() << "[WebdavSyncService] testConnection DONE | ok:" << ok;
    co_return ok;
}

QCoro::Task<QString> WebdavSyncService::uploadSnapshot(QString customTag, bool encrypt,
                                                       QString passphrase)
{
    qDebug() << "[WebdavSyncService] uploadSnapshot START"
             << "| encrypt:" << encrypt
             << "| customTag:" << customTag;
    if (encrypt && passphrase.isEmpty())
    {
        throw std::runtime_error(
            tr("Passphrase cannot be empty.").toUtf8().toStdString());
    }

    std::unique_ptr<WebdavClient> client(buildClient());
    co_await client->ensureRootDir();

    ConfigBundle bundle = ConfigBundle::collectFromLocal();
    QByteArray plain = bundle.serialize(false);
    if (plain.isEmpty())
    {
        throw std::runtime_error(
            tr("There is no local configuration to upload.")
                .toUtf8()
                .toStdString());
    }

    QByteArray payload;
    if (encrypt)
    {
        payload = SecureSecretBox::encryptWithPassphrase(plain, passphrase);
        SecureSecretBox::secureZero(plain);
        if (payload.isEmpty())
        {
            throw std::runtime_error(
                tr("Failed to encrypt the snapshot.")
                    .toUtf8()
                    .toStdString());
        }
    }
    else
    {
        payload = std::move(plain);
    }

    const QString fileName = proposedSnapshotFileName(bundle.metadata, customTag);
    qDebug() << "[WebdavSyncService] uploadSnapshot uploading"
             << "| fileName:" << fileName
             << "| encrypted:" << encrypt
             << "| size:" << payload.size();

    co_await client->putFile(fileName, payload,
                             QStringLiteral("application/octet-stream"));
    SecureSecretBox::secureZero(payload);

    if (m_store)
    {
        m_store->updateLastSyncAt(QDateTime::currentMSecsSinceEpoch());
    }
    emit snapshotListChanged();

    qDebug() << "[WebdavSyncService] uploadSnapshot DONE | fileName:" << fileName;
    co_return fileName;
}

QCoro::Task<QList<WebdavSnapshot>> WebdavSyncService::listSnapshots()
{
    qDebug() << "[WebdavSyncService] listSnapshots START";
    std::unique_ptr<WebdavClient> client(buildClient());
    co_await client->ensureRootDir();

    const QList<WebdavEntry> entries = co_await client->list(QString());

    QList<WebdavSnapshot> snapshots;
    for (const WebdavEntry &e : entries)
    {
        if (e.isCollection)
        {
            continue;
        }
        if (!isQEmbySnapshotName(e.displayName))
        {
            continue;
        }

        WebdavSnapshot s;
        s.fileName = e.displayName;
        s.mtime = e.lastModified;
        s.size = e.contentLength;
        parseSnapshotName(s.fileName, s.appVersion, s.osName, s.deviceHint,
                          s.customTag, s.createdAt);
        if (!s.createdAt.isValid())
        {
            s.createdAt = s.mtime;
        }
        snapshots.append(s);
    }

    std::sort(snapshots.begin(), snapshots.end(),
              [](const WebdavSnapshot &a, const WebdavSnapshot &b)
              { return a.createdAt > b.createdAt; });

    qDebug() << "[WebdavSyncService] listSnapshots DONE"
             << "| total:" << entries.size()
             << "| matched:" << snapshots.size();
    co_return snapshots;
}

QCoro::Task<ConfigBundle> WebdavSyncService::downloadSnapshot(QString fileName, QString passphrase)
{
    qDebug() << "[WebdavSyncService] downloadSnapshot START | fileName:" << fileName;
    if (!isQEmbySnapshotName(fileName))
    {
        throw std::runtime_error(
            tr("Selected file is not a valid qEmby snapshot.")
                .toUtf8()
                .toStdString());
    }

    std::unique_ptr<WebdavClient> client(buildClient());
    QByteArray payload = co_await client->getFile(fileName);

    if (!looksEncryptedPayload(payload))
    {
        auto plainBundleOpt = ConfigBundle::deserialize(payload);
        if (!plainBundleOpt.has_value())
        {
            SecureSecretBox::secureZero(payload);
            throw std::runtime_error(
                tr("Snapshot is not a valid qEmby configuration bundle.")
                    .toUtf8()
                    .toStdString());
        }

        SecureSecretBox::secureZero(payload);
        qDebug() << "[WebdavSyncService] downloadSnapshot DONE"
                 << "| encrypted:" << false
                 << "| servers:" << plainBundleOpt->servers.size()
                 << "| entries:" << plainBundleOpt->configEntries.size()
                 << "| fromHost:" << plainBundleOpt->metadata.deviceName
                 << "| fromOs:" << plainBundleOpt->metadata.osPretty
                 << "| fromApp:" << plainBundleOpt->metadata.appVersion;
        co_return plainBundleOpt.value();
    }

    if (passphrase.isEmpty())
    {
        SecureSecretBox::secureZero(payload);
        throw WebdavPassphraseRequiredError(
            tr("Passphrase is required for encrypted snapshots.")
                .toUtf8()
                .toStdString());
    }

    auto plainOpt = SecureSecretBox::decryptWithPassphrase(payload, passphrase);
    SecureSecretBox::secureZero(payload);
    if (!plainOpt.has_value())
    {
        throw std::runtime_error(
            tr("Failed to decrypt the snapshot. The passphrase may be wrong or the file is corrupted.")
                .toUtf8()
                .toStdString());
    }

    QByteArray plain = plainOpt.value();
    auto bundleOpt = ConfigBundle::deserialize(plain);
    SecureSecretBox::secureZero(plain);
    if (!bundleOpt.has_value())
    {
        throw std::runtime_error(
            tr("Snapshot is not a valid qEmby configuration bundle.")
                .toUtf8()
                .toStdString());
    }

    qDebug() << "[WebdavSyncService] downloadSnapshot DONE"
             << "| encrypted:" << true
             << "| servers:" << bundleOpt->servers.size()
             << "| entries:" << bundleOpt->configEntries.size()
             << "| fromHost:" << bundleOpt->metadata.deviceName
             << "| fromOs:" << bundleOpt->metadata.osPretty
             << "| fromApp:" << bundleOpt->metadata.appVersion;
    co_return bundleOpt.value();
}

QCoro::Task<bool> WebdavSyncService::deleteSnapshot(QString fileName)
{
    qDebug() << "[WebdavSyncService] deleteSnapshot START | fileName:" << fileName;
    if (!isQEmbySnapshotName(fileName))
    {
        throw std::runtime_error(
            tr("Refused to delete a file that is not a qEmby snapshot.")
                .toUtf8()
                .toStdString());
    }

    std::unique_ptr<WebdavClient> client(buildClient());
    const bool ok = co_await client->remove(fileName);
    if (ok)
    {
        emit snapshotListChanged();
    }
    qDebug() << "[WebdavSyncService] deleteSnapshot DONE | ok:" << ok;
    co_return ok;
}





bool WebdavSyncService::applyBundle(const ConfigBundle &bundle, MergeStrategy strategy)
{
    qDebug() << "[WebdavSyncService] applyBundle START"
             << "| strategy:" << static_cast<int>(strategy)
             << "| bundleServers:" << bundle.servers.size()
             << "| bundleEntries:" << bundle.configEntries.size()
             << "| fromHost:" << bundle.metadata.deviceName;

    
    QJsonArray finalServers;
    switch (strategy)
    {
    case MergeStrategy::Replace:
        finalServers = bundle.servers;
        break;
    case MergeStrategy::Merge: {
        const QJsonArray local = readServersJson();
        const QSet<QString> bundleIds = idSetOfArray(bundle.servers);
        finalServers = bundle.servers;
        for (const QJsonValue &v : local)
        {
            const QString id = v.toObject().value(QStringLiteral("id")).toString();
            if (id.isEmpty() || !bundleIds.contains(id))
            {
                finalServers.append(v);
            }
        }
        break;
    }
    case MergeStrategy::LocalWins: {
        const QJsonArray local = readServersJson();
        const QSet<QString> localIds = idSetOfArray(local);
        finalServers = local;
        for (const QJsonValue &v : bundle.servers)
        {
            const QString id = v.toObject().value(QStringLiteral("id")).toString();
            if (!id.isEmpty() && !localIds.contains(id))
            {
                finalServers.append(v);
            }
        }
        break;
    }
    }

    if (!writeServersJson(finalServers))
    {
        qWarning() << "[WebdavSyncService] applyBundle: writeServersJson failed";
        return false;
    }

    
    ConfigStore *store = ConfigStore::instance();
    if (!store)
    {
        qWarning() << "[WebdavSyncService] applyBundle: ConfigStore singleton missing";
        return false;
    }

    if (strategy == MergeStrategy::Replace)
    {
        
        const QStringList localKeys = store->allKeys();
        for (const QString &key : localKeys)
        {
            if (!keyMatchesIncludedNamespace(key))
            {
                continue;
            }
            if (ConfigBundle::shouldExcludeKey(key))
            {
                continue;
            }
            
            if (bundle.configEntries.contains(key))
            {
                continue;
            }
            store->set(key, QVariant());
        }
    }

    
    QStringList localKeysCache;
    if (strategy == MergeStrategy::LocalWins)
    {
        localKeysCache = store->allKeys();
    }

    for (auto it = bundle.configEntries.constBegin();
         it != bundle.configEntries.constEnd(); ++it)
    {
        const QString &key = it.key();
        if (ConfigBundle::shouldExcludeKey(key))
        {
            continue;
        }

        if (strategy == MergeStrategy::LocalWins)
        {
            if (localKeysCache.contains(key))
            {
                continue; 
            }
        }

        store->set(key, jsonValueToVariant(it.value()));
    }

    
    if (m_store)
    {
        m_store->updateLastSyncAt(QDateTime::currentMSecsSinceEpoch());
    }

    emit appliedBundle(strategy);
    qDebug() << "[WebdavSyncService] applyBundle DONE"
             << "| finalServers:" << finalServers.size();
    return true;
}
