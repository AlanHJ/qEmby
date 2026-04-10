#include "danmakucachestore.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace {

QJsonObject candidateToJson(const DanmakuMatchCandidate &candidate)
{
    QJsonObject obj;
    obj["provider"] = candidate.provider;
    obj["cacheScope"] = candidate.cacheScope;
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
                                  bool *manualOverride) const
{
    if (!candidate) {
        return false;
    }

    QFile file(matchesFilePath(context.serverId));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    const QJsonObject root = doc.object();
    const QJsonObject entry = root.value(context.cacheKey()).toObject();
    if (entry.isEmpty()) {
        return false;
    }

    *candidate = candidateFromJson(entry.value("candidate").toObject());
    if (manualOverride) {
        *manualOverride = entry.value("manualOverride").toBool(false);
    }
    return candidate->isValid();
}

void DanmakuCacheStore::saveMatch(const DanmakuMediaContext &context,
                                  const DanmakuMatchCandidate &candidate,
                                  bool manualOverride) const
{
    const QString filePath = matchesFilePath(context.serverId);
    ensureParentDir(filePath);

    QJsonObject root;
    QFile file(filePath);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        root = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }

    QJsonObject entry;
    entry["candidate"] = candidateToJson(candidate);
    entry["manualOverride"] = manualOverride;
    entry["updatedAt"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    root[context.cacheKey()] = entry;

    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
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
