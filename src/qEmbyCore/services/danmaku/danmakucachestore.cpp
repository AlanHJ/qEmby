#include "danmakucachestore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

QJsonObject candidateToJson(const DanmakuMatchCandidate &candidate)
{
    QJsonObject obj;
    obj["provider"] = candidate.provider;
    obj["cacheScope"] = candidate.cacheScope;
    obj["endpointId"] = candidate.endpointId;
    obj["endpointName"] = candidate.endpointName;
    obj["targetId"] = candidate.targetId;
    obj["title"] = candidate.title;
    obj["subtitle"] = candidate.subtitle;
    obj["seasonNumber"] = candidate.seasonNumber;
    obj["episodeNumber"] = candidate.episodeNumber;
    obj["durationMs"] = QString::number(candidate.durationMs);
    obj["score"] = candidate.score;
    obj["matchReason"] = candidate.matchReason;
    obj["commentCount"] = candidate.commentCount;
    return obj;
}

DanmakuMatchCandidate candidateFromJson(const QJsonObject &obj)
{
    DanmakuMatchCandidate candidate;
    candidate.provider = obj["provider"].toString();
    candidate.cacheScope = obj["cacheScope"].toString();
    candidate.endpointId = obj["endpointId"].toString();
    candidate.endpointName = obj["endpointName"].toString();
    candidate.targetId = obj["targetId"].toString();
    candidate.title = obj["title"].toString();
    candidate.subtitle = obj["subtitle"].toString();
    candidate.seasonNumber = obj["seasonNumber"].toInt(-1);
    candidate.episodeNumber = obj["episodeNumber"].toInt(-1);
    candidate.durationMs = obj["durationMs"].toVariant().toLongLong();
    candidate.score = obj["score"].toDouble();
    candidate.matchReason = obj["matchReason"].toString();
    candidate.commentCount = obj["commentCount"].toInt();
    return candidate;
}

QJsonObject commentToJson(const DanmakuComment &comment)
{
    QJsonObject obj;
    obj["timeMs"] = QString::number(comment.timeMs);
    obj["mode"] = comment.mode;
    obj["color"] = comment.color.name(QColor::HexRgb);
    obj["fontLevel"] = comment.fontLevel;
    obj["sender"] = comment.sender;
    obj["text"] = comment.text;
    obj["createdAt"] = comment.createdAt.toString(Qt::ISODate);
    return obj;
}

DanmakuComment commentFromJson(const QJsonObject &obj)
{
    DanmakuComment comment;
    comment.timeMs = obj["timeMs"].toVariant().toLongLong();
    comment.mode = obj["mode"].toInt(1);
    comment.color = QColor(obj["color"].toString(QStringLiteral("#FFFFFF")));
    comment.fontLevel = obj["fontLevel"].toInt(25);
    comment.sender = obj["sender"].toString();
    comment.text = obj["text"].toString();
    comment.createdAt =
        QDateTime::fromString(obj["createdAt"].toString(), Qt::ISODate);
    return comment;
}

bool ensureParentDir(const QString &path)
{
    return QDir().mkpath(QFileInfo(path).absolutePath());
}

QString commentCacheKey(const QString &provider,
                        const QString &cacheScope,
                        const QString &targetId)
{
    const QByteArray rawKey =
        QStringLiteral("%1|%2|%3").arg(provider, cacheScope, targetId).toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(rawKey, QCryptographicHash::Sha1).toHex());
}

} 

bool DanmakuCacheStore::loadMatch(const DanmakuMediaContext &context,
                                  DanmakuMatchCandidate *candidate,
                                  bool *manualOverride,
                                  int automaticMaxAgeHours) const
{
    if (!candidate) {
        return false;
    }

    QJsonObject entry;
    bool loadedLegacyEntry = false;
    QFile entryFile(matchEntryFilePath(context));
    if (entryFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        entry = QJsonDocument::fromJson(entryFile.readAll()).object();
    } else {
        
        QFile legacyFile(matchesFilePath(context.serverId));
        if (!legacyFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return false;
        }
        const QJsonObject legacyRoot =
            QJsonDocument::fromJson(legacyFile.readAll()).object();
        entry = legacyRoot.value(context.cacheKey()).toObject();
        loadedLegacyEntry = !entry.isEmpty();
    }
    if (entry.isEmpty()) {
        return false;
    }

    const bool isManual = entry.value("manualOverride").toBool(false);
    const qint64 cachedMediaFileSize =
        entry.value("mediaFileSize").toVariant().toLongLong();
    const QDateTime cachedMediaModifiedAt = QDateTime::fromString(
        entry.value("mediaModifiedAt").toString(), Qt::ISODate);
    if (context.fileSize > 0 && cachedMediaFileSize > 0 &&
        context.fileSize != cachedMediaFileSize) {
        return false;
    }
    if (context.mediaModifiedAt.isValid() && cachedMediaModifiedAt.isValid() &&
        context.mediaModifiedAt != cachedMediaModifiedAt) {
        return false;
    }
    if (!isManual) {
        const int algorithmVersion = entry.value("algorithmVersion").toInt(0);
        const QDateTime updatedAt = QDateTime::fromString(
            entry.value("updatedAt").toString(), Qt::ISODate);
        const qint64 maximumAgeSeconds =
            static_cast<qint64>(automaticMaxAgeHours) * 3600;
        if (algorithmVersion != CurrentMatchAlgorithmVersion ||
            !updatedAt.isValid() || automaticMaxAgeHours <= 0 ||
            updatedAt.secsTo(QDateTime::currentDateTimeUtc()) >
                maximumAgeSeconds) {
            return false;
        }
    }

    *candidate = candidateFromJson(entry.value("candidate").toObject());
    if (manualOverride) {
        *manualOverride = isManual;
    }
    const bool valid = candidate->isValid();
    if (valid && loadedLegacyEntry && isManual) {
        saveMatch(context, *candidate, true);
    }
    return valid;
}

void DanmakuCacheStore::saveMatch(const DanmakuMediaContext &context,
                                  const DanmakuMatchCandidate &candidate,
                                  bool manualOverride) const
{
    const QString filePath = matchEntryFilePath(context);
    ensureParentDir(filePath);
    QJsonObject entry;
    entry["candidate"] = candidateToJson(candidate);
    entry["manualOverride"] = manualOverride;
    entry["algorithmVersion"] = CurrentMatchAlgorithmVersion;
    entry["mediaFileSize"] = QString::number(context.fileSize);
    entry["mediaModifiedAt"] = context.mediaModifiedAt.toString(Qt::ISODate);
    entry["updatedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    QSaveFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

void DanmakuCacheStore::removeMatch(const DanmakuMediaContext &context) const
{
    const QString filePath = matchEntryFilePath(context);
    if (!filePath.isEmpty() && QFile::exists(filePath) &&
        !QFile::remove(filePath)) {
        qWarning().noquote()
            << "[Danmaku][Cache] Failed to remove match entry"
            << "| mediaId:" << context.mediaId
            << "| sourceId:" << context.mediaSourceId;
    }
}

bool DanmakuCacheStore::loadFingerprint(const DanmakuMediaContext &context,
                                        QString *fileHash,
                                        int maxAgeHours) const
{
    if (!fileHash || context.fileSize <= 0) {
        return false;
    }
    QFile file(fingerprintEntryFilePath(context));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QJsonObject entry = QJsonDocument::fromJson(file.readAll()).object();
    const QDateTime updatedAt = QDateTime::fromString(
        entry.value("updatedAt").toString(), Qt::ISODate);
    const qint64 cachedSize =
        entry.value("fileSize").toVariant().toLongLong();
    const QString cachedHash = entry.value("fileHash").toString().trimmed();
    const QDateTime cachedMediaModifiedAt = QDateTime::fromString(
        entry.value("mediaModifiedAt").toString(), Qt::ISODate);
    if (cachedSize != context.fileSize || cachedHash.size() != 32 ||
        !updatedAt.isValid() || maxAgeHours <= 0 ||
        updatedAt.secsTo(QDateTime::currentDateTimeUtc()) >
            static_cast<qint64>(maxAgeHours) * 3600) {
        return false;
    }
    if (context.mediaModifiedAt.isValid() && cachedMediaModifiedAt.isValid() &&
        context.mediaModifiedAt != cachedMediaModifiedAt) {
        return false;
    }
    *fileHash = cachedHash;
    return true;
}

void DanmakuCacheStore::saveFingerprint(const DanmakuMediaContext &context,
                                        const QString &fileHash) const
{
    const QString normalizedHash = fileHash.trimmed().toLower();
    if (context.fileSize <= 0 || normalizedHash.size() != 32) {
        return;
    }
    const QString filePath = fingerprintEntryFilePath(context);
    ensureParentDir(filePath);
    QJsonObject entry;
    entry["fileSize"] = QString::number(context.fileSize);
    entry["fileHash"] = normalizedHash;
    entry["updatedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    entry["mediaModifiedAt"] = context.mediaModifiedAt.toString(Qt::ISODate);
    QSaveFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact));
        file.commit();
    }
}

QList<DanmakuComment> DanmakuCacheStore::loadComments(const QString &provider,
                                                      const QString &cacheScope,
                                                      const QString &targetId,
                                                      int maxAgeHours) const
{
    QList<DanmakuComment> comments;
    QFile file(commentsFilePath(provider, cacheScope, targetId));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return comments;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    const QDateTime fetchedAt = QDateTime::fromString(
        root.value("fetchedAt").toString(), Qt::ISODate);
    if (!fetchedAt.isValid() ||
        fetchedAt.secsTo(QDateTime::currentDateTimeUtc()) >
            static_cast<qint64>(maxAgeHours) * 3600) {
        return comments;
    }

    const QJsonArray arr = root.value("comments").toArray();
    comments.reserve(arr.size());
    for (const QJsonValue &value : arr) {
        const DanmakuComment comment = commentFromJson(value.toObject());
        if (comment.isValid()) {
            comments.append(comment);
        }
    }
    return comments;
}

void DanmakuCacheStore::saveComments(const QString &provider,
                                     const QString &cacheScope,
                                     const QString &targetId,
                                     const QString &sourceTitle,
                                     const QList<DanmakuComment> &comments) const
{
    const QString filePath = commentsFilePath(provider, cacheScope, targetId);
    ensureParentDir(filePath);

    QJsonArray arr;
    for (const DanmakuComment &comment : comments) {
        arr.append(commentToJson(comment));
    }

    QJsonObject root;
    root["provider"] = provider;
    root["cacheScope"] = cacheScope;
    root["targetId"] = targetId;
    root["sourceTitle"] = sourceTitle;
    root["fetchedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root["comments"] = arr;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    }
}

bool DanmakuCacheStore::loadAssPath(const QString &assKey,
                                    QString *path,
                                    int maxAgeHours) const
{
    if (!path) {
        return false;
    }

    const QString filePath = assFilePath(assKey);
    QFileInfo info(filePath);
    if (!info.exists()) {
        return false;
    }

    const qint64 ageSeconds = info.lastModified().secsTo(QDateTime::currentDateTime());
    if (ageSeconds > static_cast<qint64>(maxAgeHours) * 3600) {
        return false;
    }

    *path = filePath;
    return true;
}

QString DanmakuCacheStore::saveAssFile(const QString &assKey,
                                       const QString &content) const
{
    const QString filePath = assFilePath(assKey);
    if (!ensureParentDir(filePath)) {
        return {};
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return {};
    }

    file.write(content.toUtf8());
    return filePath;
}

void DanmakuCacheStore::clearAll() const
{
    QDir dir(baseDirPath());
    if (dir.exists()) {
        dir.removeRecursively();
    }
}

QString DanmakuCacheStore::baseDirPath() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
           QStringLiteral("/danmaku");
}

QString DanmakuCacheStore::matchesFilePath(const QString &serverId) const
{
    return baseDirPath() + QStringLiteral("/matches/%1.json").arg(serverId);
}

QString DanmakuCacheStore::matchEntryFilePath(
    const DanmakuMediaContext &context) const
{
    const QByteArray rawKey =
        QStringLiteral("%1|%2").arg(context.serverId, context.cacheKey()).toUtf8();
    const QString hashedKey = QString::fromLatin1(
        QCryptographicHash::hash(rawKey, QCryptographicHash::Sha1).toHex());
    return baseDirPath() +
           QStringLiteral("/matches-v2/%1/%2.json")
               .arg(context.serverId, hashedKey);
}

QString DanmakuCacheStore::fingerprintEntryFilePath(
    const DanmakuMediaContext &context) const
{
    const QByteArray rawKey =
        QStringLiteral("%1|%2").arg(context.serverId, context.cacheKey()).toUtf8();
    const QString hashedKey = QString::fromLatin1(
        QCryptographicHash::hash(rawKey, QCryptographicHash::Sha1).toHex());
    return baseDirPath() +
           QStringLiteral("/fingerprints/%1/%2.json")
               .arg(context.serverId, hashedKey);
}

QString DanmakuCacheStore::commentsFilePath(const QString &provider,
                                            const QString &cacheScope,
                                            const QString &targetId) const
{
    const QString hashedKey = commentCacheKey(provider, cacheScope, targetId);
    return baseDirPath() +
           QStringLiteral("/comments/%1/%2.json").arg(provider, hashedKey);
}

QString DanmakuCacheStore::assFilePath(const QString &assKey) const
{
    return baseDirPath() + QStringLiteral("/ass/%1.ass").arg(assKey);
}
