#include "danmakuservice.h"

#include "../../api/networkmanager.h"
#include "../../config/config_keys.h"
#include "../../config/configstore.h"
#include "../manager/servermanager.h"
#include "dandanplayprovider.h"
#include "danmakuasscomposer.h"
#include "danmakucachestore.h"
#include "danmakusettings.h"
#include "danmuapiprovider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <QPromise>
#include <QRegularExpression>
#include <QSet>
#include <QSharedPointer>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QtConcurrent/QtConcurrent>
#include <algorithm>
#include <exception>
#include <limits>
#include <qcorofuture.h>
#include <stdexcept>
#include <utility>

namespace
{

constexpr auto kLocalDanmakuProvider = "local-file";
constexpr auto kDanmakuSourceModePreferOnline = "prefer-online";
constexpr auto kDanmakuSourceModePreferLocal = "prefer-local";
constexpr auto kDanmakuSourceModeOnlineOnly = "online-only";
constexpr auto kDanmakuSourceModeLocalOnly = "local-only";
constexpr int kDanmakuAssRenderVersion = 6;

QStringList parseBlockedKeywords(const QString &value)
{
    QString normalized = value;
    normalized.replace('\n', ',');
    QStringList parts = normalized.split(',', Qt::SkipEmptyParts);
    for (QString &part : parts)
    {
        part = part.trimmed();
    }
    parts.removeAll(QString());
    parts.removeDuplicates();
    return parts;
}

QString providerKey(const QString &serverId, const char *baseKey)
{
    return ConfigKeys::forServer(serverId, baseKey);
}

QString preferredProviderTitle(const DanmakuMatchCandidate &candidate)
{
    return candidate.displayText().trimmed();
}

QString safeDirectoryComponent(QString value)
{
    value = value.trimmed();
    value.replace(QRegularExpression(QStringLiteral(R"([^A-Za-z0-9._-])")), QStringLiteral("_"));
    return value.isEmpty() ? QStringLiteral("default") : value;
}

QString legacyLocalDanmakuRootPath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/danmaku/local");
}

QString preferredLocalDanmakuRootPath()
{
    const QString appDir = QDir::cleanPath(QCoreApplication::applicationDirPath());
    if (appDir.isEmpty())
    {
        return legacyLocalDanmakuRootPath();
    }

    return QDir(appDir).filePath(QStringLiteral("qEmby-data/danmaku/local"));
}

QString legacyLocalDanmakuDirectoryPath(QString serverId)
{
    return QDir(legacyLocalDanmakuRootPath()).filePath(safeDirectoryComponent(serverId));
}

bool copyDirectoryRecursively(const QString &sourcePath, const QString &targetPath)
{
    const QDir sourceDir(sourcePath);
    if (!sourceDir.exists())
    {
        return false;
    }

    if (!QDir().mkpath(targetPath))
    {
        return false;
    }

    const QFileInfoList entries = sourceDir.entryInfoList(QDir::NoDotAndDotDot | QDir::AllEntries);
    for (const QFileInfo &entry : entries)
    {
        const QString sourceEntryPath = entry.absoluteFilePath();
        const QString targetEntryPath = QDir(targetPath).filePath(entry.fileName());

        if (entry.isDir())
        {
            if (!copyDirectoryRecursively(sourceEntryPath, targetEntryPath))
            {
                return false;
            }
            continue;
        }

        if (QFile::exists(targetEntryPath))
        {
            continue;
        }
        if (!QFile::copy(sourceEntryPath, targetEntryPath))
        {
            return false;
        }
    }
    return true;
}

QString normalizedSourceMode(QString value)
{
    value = value.trimmed().toLower();
    if (value == QLatin1String(kDanmakuSourceModePreferLocal) || value == QLatin1String(kDanmakuSourceModeOnlineOnly) ||
        value == QLatin1String(kDanmakuSourceModeLocalOnly))
    {
        return value;
    }
    return QString::fromLatin1(kDanmakuSourceModePreferLocal);
}

QString cacheScopeForConfig(const DanmakuProviderConfig &config)
{
    const QString endpointId = config.endpointId.trimmed();
    if (!endpointId.isEmpty())
    {
        return endpointId;
    }
    return config.baseUrl.trimmed().toLower();
}

QString endpointDisplayName(const DanmakuProviderConfig &config)
{
    const QString endpointName = config.endpointName.trimmed();
    if (!endpointName.isEmpty())
    {
        return endpointName;
    }

    const QString baseUrl = config.baseUrl.trimmed();
    return baseUrl.isEmpty() ? config.provider.trimmed() : baseUrl;
}

DanmakuProviderConfig providerConfigFromServerDefinition(const DanmakuServerDefinition &server, const QString &serverId,
                                                         bool allowLegacyCredentialFallback)
{
    auto *store = ConfigStore::instance();
    DanmakuProviderConfig config;
    config.provider =
        server.provider.trimmed().isEmpty() ? QStringLiteral("dandanplay") : server.provider.trimmed().toLower();
    config.baseUrl =
        server.baseUrl.trimmed().isEmpty() ? QStringLiteral("https://api.dandanplay.net") : server.baseUrl.trimmed();
    config.endpointId = server.id.trimmed();
    config.endpointName = server.displayName();
    config.enabled = server.enabled;
    config.appId = server.appId.trimmed();
    config.appSecret = server.appSecret.trimmed();
    config.accessToken = server.accessToken.trimmed();

    if (server.builtIn)
    {
        config.appId = server.appId.trimmed();
        config.appSecret = server.appSecret.trimmed();
    }
    else if (allowLegacyCredentialFallback)
    {
        if (config.appId.isEmpty())
        {
            config.appId = store->get<QString>(providerKey(serverId, ConfigKeys::DanmakuProviderAppId));
        }
        if (config.appSecret.isEmpty())
        {
            config.appSecret = store->get<QString>(providerKey(serverId, ConfigKeys::DanmakuProviderAppSecret));
        }
    }

    config.contentScope = server.contentScope.trimmed().toLower();
    config.withRelated = store->get<bool>(providerKey(serverId, ConfigKeys::DanmakuWithRelated), true);
    config.cacheHours =
        store->get<QString>(providerKey(serverId, ConfigKeys::DanmakuCacheHours), QStringLiteral("24")).toInt();
    return config;
}

void applyEndpointMetadata(DanmakuMatchCandidate *candidate, const DanmakuProviderConfig &config)
{
    if (!candidate)
    {
        return;
    }

    if (candidate->cacheScope.trimmed().isEmpty())
    {
        candidate->cacheScope = cacheScopeForConfig(config);
    }
    if (candidate->endpointId.trimmed().isEmpty())
    {
        candidate->endpointId = config.endpointId.trimmed();
    }
    if (candidate->endpointName.trimmed().isEmpty())
    {
        candidate->endpointName = endpointDisplayName(config);
    }
}

bool candidateBelongsToConfig(const DanmakuMatchCandidate &candidate, const DanmakuProviderConfig &config)
{
    const QString endpointId = candidate.endpointId.trimmed();
    if (!endpointId.isEmpty())
    {
        return endpointId == config.endpointId.trimmed();
    }

    const QString cacheScope = candidate.cacheScope.trimmed();
    if (cacheScope.isEmpty())
    {
        return false;
    }
    if (cacheScope == cacheScopeForConfig(config))
    {
        return true;
    }
    const QString legacyBaseUrlScope = config.baseUrl.trimmed().toLower();
    return !legacyBaseUrlScope.isEmpty() && cacheScope == legacyBaseUrlScope;
}

QString localCandidateTitle(const QFileInfo &fileInfo)
{
    QString title = fileInfo.completeBaseName().trimmed();
    if (title.endsWith(QStringLiteral(".danmaku"), Qt::CaseInsensitive))
    {
        title.chop(QStringLiteral(".danmaku").size());
    }
    return title.trimmed().isEmpty() ? fileInfo.fileName() : title.trimmed();
}

QString firstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString &value : values)
    {
        if (!value.trimmed().isEmpty())
        {
            return value.trimmed();
        }
    }
    return {};
}

QString jsonStringField(const QJsonObject &obj, std::initializer_list<const char *> keys)
{
    for (const char *key : keys)
    {
        const QString value = obj.value(QLatin1String(key)).toVariant().toString().trimmed();
        if (!value.isEmpty())
        {
            return value;
        }
    }
    return {};
}

int jsonIntField(const QJsonObject &obj, std::initializer_list<const char *> keys, int defaultValue = 0)
{
    for (const char *key : keys)
    {
        const QJsonValue value = obj.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull())
        {
            return value.toVariant().toInt();
        }
    }
    return defaultValue;
}

qint64 jsonLongField(const QJsonObject &obj, std::initializer_list<const char *> keys, qint64 defaultValue = 0)
{
    for (const char *key : keys)
    {
        const QJsonValue value = obj.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull())
        {
            return value.toVariant().toLongLong();
        }
    }
    return defaultValue;
}

QColor parseColor(const QJsonValue &value)
{
    if (value.isString())
    {
        const QColor color(value.toString());
        if (color.isValid())
        {
            return color;
        }

        bool ok = false;
        const uint rgb = value.toString().toUInt(&ok, 10);
        if (ok)
        {
            return QColor::fromRgb(rgb);
        }
    }

    bool ok = false;
    const uint rgb = value.toVariant().toUInt(&ok);
    return ok ? QColor::fromRgb(rgb) : QColor(Qt::white);
}

double titleScore(const QString &lhs, const QString &rhs)
{
    if (lhs.isEmpty() || rhs.isEmpty())
    {
        return 0.0;
    }

    const QString normalizedLhs = lhs.trimmed().toLower();
    const QString normalizedRhs = rhs.trimmed().toLower();
    if (normalizedLhs == normalizedRhs)
    {
        return 1.0;
    }
    if (normalizedLhs.contains(normalizedRhs) || normalizedRhs.contains(normalizedLhs))
    {
        return 0.78;
    }

    auto comparable = [](QString value)
    {
        value.remove(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N}]+)")));
        return value;
    };
    const QString lhsValue = comparable(normalizedLhs);
    const QString rhsValue = comparable(normalizedRhs);
    if (lhsValue.size() < 2 || rhsValue.size() < 2)
    {
        return lhsValue == rhsValue ? 1.0 : 0.0;
    }
    auto bigrams = [](const QString &value)
    {
        QSet<QString> result;
        for (int i = 0; i + 1 < value.size(); ++i)
        {
            result.insert(value.mid(i, 2));
        }
        return result;
    };
    const QSet<QString> lhsBigrams = bigrams(lhsValue);
    const QSet<QString> rhsBigrams = bigrams(rhsValue);
    int common = 0;
    for (const QString &gram : lhsBigrams)
    {
        if (rhsBigrams.contains(gram))
        {
            ++common;
        }
    }
    return (2.0 * common) / static_cast<double>(lhsBigrams.size() + rhsBigrams.size());
}

int yearHint(const QString &title)
{
    static const QRegularExpression yearPattern(QStringLiteral(R"((?:^|[^0-9])((?:19|20)[0-9]{2})(?:[^0-9]|$))"));
    const QRegularExpressionMatch match = yearPattern.match(title);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

bool isAnimationGenre(const QString &genre)
{
    const QString normalized = genre.trimmed().toLower();
    return normalized == QLatin1String("animation") || normalized == QLatin1String("anime") ||
           normalized.contains(QStringLiteral("动画")) || normalized.contains(QStringLiteral("動漫")) ||
           normalized.contains(QStringLiteral("アニメ"));
}

bool contentScopeAllowsContext(const DanmakuProviderConfig &config, const DanmakuMediaContext &context)
{
    if (config.contentScope.compare(QStringLiteral("anime"), Qt::CaseInsensitive) != 0 || context.genres.isEmpty())
    {
        return true;
    }
    if (std::any_of(context.genres.cbegin(), context.genres.cend(), isAnimationGenre))
    {
        return true;
    }

    const bool hasTmdbId = !context.providerIds.value(QStringLiteral("Tmdb")).toString().trimmed().isEmpty() ||
                           !context.providerIds.value(QStringLiteral("TMDb")).toString().trimmed().isEmpty() ||
                           !context.providerIds.value(QStringLiteral("tmdb")).toString().trimmed().isEmpty();
    const bool localFileAccessible = !context.path.trimmed().isEmpty() && QFileInfo(context.path.trimmed()).isFile();
    const bool remoteSampleAvailable = context.fileSize > 0 && !context.mediaUrl.trimmed().isEmpty();
    const bool canUseFileFingerprint = localFileAccessible || remoteSampleAvailable;
    
    
    
    return hasTmdbId || canUseFileFingerprint;
}

double bestCandidateTitleScore(const DanmakuMediaContext &context, const DanmakuMatchCandidate &candidate)
{
    const QString subject = context.isEpisode() ? context.seriesName : context.title;
    const QString candidateSubject = candidate.subtitle.isEmpty() ? candidate.title : candidate.subtitle;
    return std::max({titleScore(subject, candidateSubject), titleScore(context.originalTitle, candidateSubject),
                      titleScore(context.title, candidate.title)});
}

QString candidateWorkTitle(const DanmakuMatchCandidate &candidate)
{
    QString title = candidate.subtitle.trimmed().isEmpty()
                        ? candidate.title.trimmed()
                        : candidate.subtitle.trimmed();
    if (!candidate.subtitle.trimmed().isEmpty())
    {
        return title;
    }

    static const QRegularExpression seasonEpisodeSuffix(
        QStringLiteral(R"((?:\s|[-_.])S\s*\d{1,2}\s*E\s*\d{1,4}.*$)"),
        QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression chineseEpisodeSuffix(
        QStringLiteral(R"(\s*[-:：]?\s*第\s*\d{1,4}\s*[话話集期].*$)"));
    title.remove(seasonEpisodeSuffix);
    title.remove(chineseEpisodeSuffix);
    return title.trimmed();
}

int candidateMatchPriority(const DanmakuMatchCandidate &candidate)
{
    if (candidate.isHashMatch())
    {
        return 0;
    }
    if (candidate.isProviderIdMatch())
    {
        return 1;
    }
    if (candidate.matchReason.compare(QStringLiteral("filename"), Qt::CaseInsensitive) == 0)
    {
        return 2;
    }
    return 3;
}

void sortDanmakuCandidates(QList<DanmakuMatchCandidate> &candidates,
                           const DanmakuMediaContext &context,
                           const QString &manualKeyword)
{
    const QString query = manualKeyword.trimmed();
    const bool manualBrowsing = !query.isEmpty();
    QHash<QString, double> workBestScores;
    QHash<QString, int> workBestCommentCounts;
    for (const DanmakuMatchCandidate &candidate : std::as_const(candidates))
    {
        const QString workKey =
            candidateWorkTitle(candidate).simplified().toCaseFolded();
        if (!workBestScores.contains(workKey))
        {
            workBestScores.insert(workKey, candidate.score);
        }
        else
        {
            workBestScores[workKey] =
                std::max(workBestScores.value(workKey), candidate.score);
        }
        workBestCommentCounts.insert(
            workKey, std::max(workBestCommentCounts.value(workKey),
                              candidate.commentCount));
    }

    std::stable_sort(
        candidates.begin(), candidates.end(),
        [&context, query, manualBrowsing, workBestScores,
         workBestCommentCounts](const DanmakuMatchCandidate &lhs,
                                const DanmakuMatchCandidate &rhs)
        {
            const int lhsPriority = candidateMatchPriority(lhs);
            const int rhsPriority = candidateMatchPriority(rhs);
            if (lhsPriority != rhsPriority)
            {
                return lhsPriority < rhsPriority;
            }

            const QString lhsWorkTitle = candidateWorkTitle(lhs);
            const QString rhsWorkTitle = candidateWorkTitle(rhs);
            const QString lhsWorkKey =
                lhsWorkTitle.simplified().toCaseFolded();
            const QString rhsWorkKey =
                rhsWorkTitle.simplified().toCaseFolded();
            const double lhsRelevance = manualBrowsing
                                            ? titleScore(query, lhsWorkTitle)
                                            : bestCandidateTitleScore(context, lhs);
            const double rhsRelevance = manualBrowsing
                                            ? titleScore(query, rhsWorkTitle)
                                            : bestCandidateTitleScore(context, rhs);
            if (std::abs(lhsRelevance - rhsRelevance) > 0.0001)
            {
                return lhsRelevance > rhsRelevance;
            }

            const bool sameWork =
                lhsWorkKey == rhsWorkKey;
            if (!sameWork)
            {
                const double lhsWorkScore = workBestScores.value(lhsWorkKey);
                const double rhsWorkScore = workBestScores.value(rhsWorkKey);
                if (!qFuzzyCompare(lhsWorkScore, rhsWorkScore))
                {
                    return lhsWorkScore > rhsWorkScore;
                }
                const int lhsWorkComments =
                    workBestCommentCounts.value(lhsWorkKey);
                const int rhsWorkComments =
                    workBestCommentCounts.value(rhsWorkKey);
                if (lhsWorkComments != rhsWorkComments)
                {
                    return lhsWorkComments > rhsWorkComments;
                }
                return QString::compare(lhsWorkTitle, rhsWorkTitle,
                                        Qt::CaseInsensitive) < 0;
            }

            if (manualBrowsing &&
                (lhs.episodeNumber > 0 || rhs.episodeNumber > 0))
            {
                const int lhsSeason = lhs.seasonNumber > 0 ? lhs.seasonNumber : 1;
                const int rhsSeason = rhs.seasonNumber > 0 ? rhs.seasonNumber : 1;
                if (lhsSeason != rhsSeason)
                {
                    return lhsSeason < rhsSeason;
                }

                const int lhsEpisode = lhs.episodeNumber > 0
                                           ? lhs.episodeNumber
                                           : std::numeric_limits<int>::max();
                const int rhsEpisode = rhs.episodeNumber > 0
                                           ? rhs.episodeNumber
                                           : std::numeric_limits<int>::max();
                if (lhsEpisode != rhsEpisode)
                {
                    return lhsEpisode < rhsEpisode;
                }
            }

            if (!qFuzzyCompare(lhs.score, rhs.score))
            {
                return lhs.score > rhs.score;
            }
            if (lhs.commentCount != rhs.commentCount)
            {
                return lhs.commentCount > rhs.commentCount;
            }
            const int titleComparison =
                QString::compare(lhs.title, rhs.title, Qt::CaseInsensitive);
            if (titleComparison != 0)
            {
                return titleComparison < 0;
            }
            const int endpointComparison = QString::compare(
                lhs.endpointName, rhs.endpointName, Qt::CaseInsensitive);
            if (endpointComparison != 0)
            {
                return endpointComparison < 0;
            }
            return lhs.targetId < rhs.targetId;
        });
}

bool isPlausibleOnlineCandidate(const DanmakuMediaContext &context, const DanmakuMatchCandidate &candidate)
{
    if (!candidate.isValid())
    {
        return false;
    }
    if (candidate.isHashMatch() || candidate.isProviderIdMatch())
    {
        return true;
    }

    const double minimumTitleScore = context.isEpisode() ? 0.48 : 0.58;
    if (bestCandidateTitleScore(context, candidate) < minimumTitleScore)
    {
        return false;
    }

    if (context.isEpisode() && context.episodeNumber > 0 && candidate.episodeNumber > 0 &&
        context.episodeNumber != candidate.episodeNumber)
    {
        return false;
    }
    if (context.isEpisode() && context.seasonNumber > 0 && candidate.seasonNumber > 0 &&
        context.seasonNumber != candidate.seasonNumber)
    {
        return false;
    }

    const QString candidateSubject = candidate.subtitle.isEmpty() ? candidate.title : candidate.subtitle;
    const int candidateYear = yearHint(candidateSubject);
    if (!context.isEpisode() && context.productionYear > 0 && candidateYear > 0 &&
        std::abs(context.productionYear - candidateYear) > 1)
    {
        return false;
    }

    if (context.durationMs > 0 && candidate.durationMs > 0)
    {
        const qint64 allowedDifference = std::max<qint64>(5 * 60 * 1000, context.durationMs * 18 / 100);
        if (std::llabs(context.durationMs - candidate.durationMs) > allowedDifference)
        {
            return false;
        }
    }
    return true;
}

double onlineConfidenceThreshold(const DanmakuMediaContext &context)
{
    return context.isEpisode() ? 72.0 : 62.0;
}

void parseSeasonEpisodeHint(const QString &text, int *seasonNumber, int *episodeNumber)
{
    if (!seasonNumber || !episodeNumber)
    {
        return;
    }

    *seasonNumber = -1;
    *episodeNumber = -1;

    const QString normalized = text.trimmed();
    const QRegularExpression seasonEpisodePattern(
        QStringLiteral(R"((?:^|[^A-Za-z0-9])S(\d{1,2})\s*E(\d{1,3})(?:[^A-Za-z0-9]|$))"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = seasonEpisodePattern.match(normalized);
    if (match.hasMatch())
    {
        *seasonNumber = match.captured(1).toInt();
        *episodeNumber = match.captured(2).toInt();
        return;
    }

    const QRegularExpression episodeOnlyPattern(QStringLiteral(R"(第\s*0*(\d{1,3})\s*[话集])"));
    match = episodeOnlyPattern.match(normalized);
    if (match.hasMatch())
    {
        *episodeNumber = match.captured(1).toInt();
    }
}

double computeLocalCandidateScore(const DanmakuMediaContext &context, const QString &candidateTitle,
                                  int candidateSeason, int candidateEpisode, const QString &manualKeyword)
{
    double score = 0.0;
    const QString subjectTitle = context.isEpisode() ? context.seriesName : context.title;
    score += titleScore(subjectTitle, candidateTitle) * 55.0;
    score += titleScore(context.originalTitle, candidateTitle) * 18.0;
    score += titleScore(context.title, candidateTitle) * 18.0;
    score += titleScore(manualKeyword, candidateTitle) * 12.0;

    if (!context.path.trimmed().isEmpty())
    {
        const QString fileStem = QFileInfo(context.path.trimmed()).completeBaseName().trimmed();
        score += titleScore(fileStem, candidateTitle) * 14.0;
    }

    if (context.isEpisode() && context.episodeNumber > 0 && candidateEpisode > 0 &&
        context.episodeNumber == candidateEpisode)
    {
        score += 24.0;
    }
    if (context.isEpisode() && context.seasonNumber > 0 && candidateSeason > 0 &&
        context.seasonNumber == candidateSeason)
    {
        score += 8.0;
    }
    return score;
}

int countAssDialogueLines(const QByteArray &rawData)
{
    int dialogueCount = 0;
    const QList<QByteArray> lines = rawData.split('\n');
    for (QByteArray line : lines)
    {
        line = line.trimmed();
        if (line.toLower().startsWith("dialogue:"))
        {
            ++dialogueCount;
        }
    }
    return dialogueCount;
}

int countAssDialogueLinesFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        return 0;
    }
    return countAssDialogueLines(file.readAll());
}

void updateCommentCountFromAss(DanmakuLoadResult *result)
{
    if (!result || result->commentCount > 0 || result->assFilePath.isEmpty())
    {
        return;
    }

    result->commentCount = countAssDialogueLinesFromFile(result->assFilePath);
}

bool isSupportedLocalDanmakuFile(const QFileInfo &fileInfo)
{
    if (!fileInfo.isFile())
    {
        return false;
    }

    const QString suffix = fileInfo.suffix().trimmed().toLower();
    return suffix == QLatin1String("ass") || suffix == QLatin1String("json") || suffix == QLatin1String("xml");
}

QList<DanmakuMatchCandidate> searchLocalDanmakuCandidates(const QString &directoryPath,
                                                          const DanmakuMediaContext &context,
                                                          const QString &manualKeyword)
{
    struct IndexedLocalFile
    {
        QString path;
        QString title;
        int seasonNumber = -1;
        int episodeNumber = -1;
    };
    struct LocalIndexCacheEntry
    {
        QDateTime indexedAt;
        QList<IndexedLocalFile> files;
    };
    static QMutex indexMutex;
    static QHash<QString, LocalIndexCacheEntry> indexCache;

    QList<DanmakuMatchCandidate> candidates;
    const QDir dir(directoryPath);
    if (!dir.exists())
    {
        return candidates;
    }

    QList<IndexedLocalFile> indexedFiles;
    bool indexCacheHit = false;
    {
        QMutexLocker locker(&indexMutex);
        const auto existing = indexCache.constFind(directoryPath);
        if (existing != indexCache.constEnd() && existing->indexedAt.secsTo(QDateTime::currentDateTimeUtc()) < 60)
        {
            indexedFiles = existing->files;
            indexCacheHit = true;
        }
    }

    if (!indexCacheHit)
    {
        QDirIterator iterator(directoryPath, QDir::Files, QDirIterator::Subdirectories);
        while (iterator.hasNext())
        {
            iterator.next();
            const QFileInfo fileInfo = iterator.fileInfo();
            if (!isSupportedLocalDanmakuFile(fileInfo))
            {
                continue;
            }
            IndexedLocalFile indexed;
            indexed.path = fileInfo.canonicalFilePath();
            if (indexed.path.isEmpty())
            {
                indexed.path = fileInfo.absoluteFilePath();
            }
            indexed.title = localCandidateTitle(fileInfo);
            parseSeasonEpisodeHint(indexed.title, &indexed.seasonNumber, &indexed.episodeNumber);
            indexedFiles.append(indexed);
        }
        QMutexLocker locker(&indexMutex);
        indexCache.insert(directoryPath, {QDateTime::currentDateTimeUtc(), indexedFiles});
    }

    candidates.reserve(indexedFiles.size());
    for (const IndexedLocalFile &indexed : std::as_const(indexedFiles))
    {
        DanmakuMatchCandidate candidate;
        candidate.provider = QString::fromLatin1(kLocalDanmakuProvider);
        candidate.cacheScope = QDir::fromNativeSeparators(directoryPath);
        candidate.targetId = indexed.path;
        candidate.title = indexed.title;
        candidate.seasonNumber = indexed.seasonNumber;
        candidate.episodeNumber = indexed.episodeNumber;
        candidate.matchReason = QStringLiteral("local-file");
        candidate.score = computeLocalCandidateScore(context, candidate.title, candidate.seasonNumber,
                                                     candidate.episodeNumber, manualKeyword.trimmed());
        if (candidate.isValid())
        {
            candidates.append(candidate);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const DanmakuMatchCandidate &lhs, const DanmakuMatchCandidate &rhs)
              {
                  if (!qFuzzyCompare(lhs.score, rhs.score))
                  {
                      return lhs.score > rhs.score;
                  }
                  return lhs.title < rhs.title;
              });
    return candidates;
}

bool parsePField(QString p, qint64 *timeMs, int *mode, QColor *color, int *fontLevel, QDateTime *createdAt)
{
    if (!timeMs || !mode || !color || !fontLevel || !createdAt)
    {
        return false;
    }

    const QStringList parts = p.split(',', Qt::KeepEmptyParts);
    if (parts.isEmpty())
    {
        return false;
    }

    *timeMs = static_cast<qint64>(parts[0].toDouble() * 1000.0);
    if (parts.size() > 1)
    {
        bool ok = false;
        const int parsedMode = parts[1].toInt(&ok);
        if (ok)
        {
            *mode = parsedMode;
        }
    }

    const bool looksLikeBilibiliLayout = parts.size() >= 8;
    if (looksLikeBilibiliLayout)
    {
        if (parts.size() > 2)
        {
            bool ok = false;
            const int parsedFontLevel = parts[2].toInt(&ok);
            if (ok)
            {
                *fontLevel = parsedFontLevel;
            }
        }
        if (parts.size() > 3)
        {
            *color = parseColor(parts[3]);
        }
        if (parts.size() > 4)
        {
            bool ok = false;
            const qint64 timestamp = parts[4].toLongLong(&ok);
            if (ok && timestamp > 0)
            {
                *createdAt = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC);
            }
        }
        return true;
    }

    if (parts.size() > 2)
    {
        *color = parseColor(parts[2]);
    }
    if (parts.size() > 3)
    {
        bool ok = false;
        const int parsedFontLevel = parts[3].toInt(&ok);
        if (ok)
        {
            *fontLevel = parsedFontLevel;
        }
    }
    return true;
}

DanmakuComment commentFromJsonObject(const QJsonObject &obj)
{
    DanmakuComment comment;
    comment.text = firstNonEmpty({jsonStringField(obj, {"m", "text", "comment", "content"})});

    const QString p = jsonStringField(obj, {"p"});
    if (!p.isEmpty())
    {
        parsePField(p, &comment.timeMs, &comment.mode, &comment.color, &comment.fontLevel, &comment.createdAt);
    }
    else
    {
        comment.timeMs = jsonLongField(obj, {"timeMs", "time", "position"}, 0);
        if (comment.timeMs > 0 && comment.timeMs < 1000)
        {
            comment.timeMs *= 1000;
        }
        comment.mode = jsonIntField(obj, {"mode", "positionType"}, 1);
        comment.color = parseColor(obj.value(QStringLiteral("color")));
        comment.fontLevel = jsonIntField(obj, {"size", "fontLevel"}, 25);

        const QString createdAtString = jsonStringField(obj, {"dateTime", "createdAt", "date", "timeStamp"});
        if (!createdAtString.isEmpty())
        {
            comment.createdAt = QDateTime::fromString(createdAtString, Qt::ISODate);
            if (!comment.createdAt.isValid())
            {
                bool ok = false;
                const qint64 timestamp = createdAtString.toLongLong(&ok);
                if (ok && timestamp > 0)
                {
                    comment.createdAt = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC);
                }
            }
        }
        else
        {
            const QJsonValue createdAtValue = obj.value(QStringLiteral("timeStamp"));
            const qint64 timestamp = createdAtValue.toVariant().toLongLong();
            if (timestamp > 0)
            {
                comment.createdAt = QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::UTC);
            }
        }
    }

    comment.sender = jsonStringField(obj, {"sender", "user", "author", "nickname"});
    return comment;
}

QList<DanmakuComment> parseLocalJsonComments(const QByteArray &rawData)
{
    QList<DanmakuComment> comments;
    const QJsonDocument document = QJsonDocument::fromJson(rawData);
    if (document.isNull())
    {
        return comments;
    }

    QJsonArray array;
    if (document.isArray())
    {
        array = document.array();
    }
    else
    {
        const QJsonObject root = document.object();
        array = root.value(QStringLiteral("comments")).toArray();
        if (array.isEmpty())
        {
            array = root.value(QStringLiteral("data")).toArray();
        }
        if (array.isEmpty())
        {
            array = root.value(QStringLiteral("result")).toArray();
        }
    }

    comments.reserve(array.size());
    for (const QJsonValue &value : array)
    {
        const DanmakuComment comment = commentFromJsonObject(value.toObject());
        if (comment.isValid())
        {
            comments.append(comment);
        }
    }
    return comments;
}

QList<DanmakuComment> parseLocalXmlComments(const QByteArray &rawData)
{
    QList<DanmakuComment> comments;
    QXmlStreamReader reader(rawData);
    while (!reader.atEnd())
    {
        reader.readNext();
        if (!reader.isStartElement() || reader.name() != QLatin1String("d"))
        {
            continue;
        }

        DanmakuComment comment;
        const QString p = reader.attributes().value(QStringLiteral("p")).toString();
        parsePField(p, &comment.timeMs, &comment.mode, &comment.color, &comment.fontLevel, &comment.createdAt);
        comment.text = reader.readElementText().trimmed();
        if (comment.isValid())
        {
            comments.append(comment);
        }
    }
    return comments;
}

} 

DanmakuService::DanmakuService(NetworkManager *networkManager, ServerManager *serverManager, QObject *parent)
    : QObject(parent), m_networkManager(networkManager), m_serverManager(serverManager),
      m_dandanplayProvider(new DandanplayProvider(networkManager)),
      m_danmuApiProvider(new DanmuApiProvider(networkManager)), m_cacheStore(new DanmakuCacheStore())
{
}

DanmakuService::~DanmakuService()
{
    delete m_dandanplayProvider;
    delete m_danmuApiProvider;
    delete m_cacheStore;
}

DanmakuMatchCandidate DanmakuService::createLocalFileCandidate(QString filePath)
{
    DanmakuMatchCandidate candidate;
    const QFileInfo fileInfo(filePath.trimmed());
    if (!fileInfo.exists() || !fileInfo.isFile() || !isSupportedLocalDanmakuFile(fileInfo))
    {
        return candidate;
    }

    candidate.provider = QString::fromLatin1(kLocalDanmakuProvider);
    candidate.targetId = fileInfo.canonicalFilePath();
    if (candidate.targetId.isEmpty())
    {
        candidate.targetId = fileInfo.absoluteFilePath();
    }
    candidate.cacheScope = QDir::fromNativeSeparators(fileInfo.absolutePath());
    candidate.title = localCandidateTitle(fileInfo);
    candidate.matchReason = QStringLiteral("manual-local-file");
    parseSeasonEpisodeHint(candidate.title, &candidate.seasonNumber, &candidate.episodeNumber);
    return candidate;
}

QString DanmakuService::localDanmakuDirectoryPath(QString serverId)
{
    return QDir(preferredLocalDanmakuRootPath()).filePath(safeDirectoryComponent(serverId));
}

bool DanmakuService::ensureLocalDanmakuDirectory(QString serverId)
{
    const QString targetPath = localDanmakuDirectoryPath(serverId);
    if (QDir(targetPath).exists())
    {
        return true;
    }

    const QString legacyPath = legacyLocalDanmakuDirectoryPath(serverId);
    if (QDir(legacyPath).exists())
    {
        const QString targetParentPath = QFileInfo(targetPath).absolutePath();
        if (QDir().mkpath(targetParentPath) && QDir().rename(legacyPath, targetPath))
        {
            qDebug().noquote() << "[Danmaku][Service] Migrated local danmaku directory"
                               << "| from:" << legacyPath << "| to:" << targetPath;
            return true;
        }

        if (copyDirectoryRecursively(legacyPath, targetPath))
        {
            qDebug().noquote() << "[Danmaku][Service] Copied legacy local danmaku directory"
                               << "| from:" << legacyPath << "| to:" << targetPath;
            return true;
        }

        qWarning().noquote() << "[Danmaku][Service] Failed to migrate legacy local danmaku directory"
                             << "| from:" << legacyPath << "| to:" << targetPath;
    }

    const bool created = QDir().mkpath(targetPath);
    if (!created)
    {
        qWarning().noquote() << "[Danmaku][Service] Failed to create local danmaku directory"
                             << "| path:" << targetPath;
    }
    return created;
}

DanmakuRenderOptions DanmakuService::renderOptions() const
{
    DanmakuRenderOptions options;
    auto *store = ConfigStore::instance();
    options.enabled = store->get<bool>(ConfigKeys::PlayerDanmakuEnabled, true);
    options.opacity = store->get<QString>(ConfigKeys::PlayerDanmakuOpacity, QStringLiteral("72")).toDouble() / 100.0;
    options.fontScale =
        store->get<QString>(ConfigKeys::PlayerDanmakuFontScale, QStringLiteral("100")).toDouble() / 100.0;
    options.fontWeight = store->get<QString>(ConfigKeys::PlayerDanmakuFontWeight, QStringLiteral("400")).toInt();
    options.outlineSize =
        store->get<QString>(ConfigKeys::PlayerDanmakuOutlineSize, QStringLiteral("30")).toDouble() / 10.0;
    options.shadowOffset =
        store->get<QString>(ConfigKeys::PlayerDanmakuShadowOffset, QStringLiteral("10")).toDouble() / 10.0;
    options.areaPercent = store->get<QString>(ConfigKeys::PlayerDanmakuAreaPercent, QStringLiteral("70")).toInt();
    options.density = store->get<QString>(ConfigKeys::PlayerDanmakuDensity, QStringLiteral("100")).toInt();
    options.speedScale =
        store->get<QString>(ConfigKeys::PlayerDanmakuSpeedScale, QStringLiteral("50")).toDouble() / 100.0;
    options.offsetMs = store->get<QString>(ConfigKeys::PlayerDanmakuOffsetMs, QStringLiteral("0")).toInt();
    options.hideScroll = store->get<bool>(ConfigKeys::PlayerDanmakuHideScroll, false);
    options.hideTop = store->get<bool>(ConfigKeys::PlayerDanmakuHideTop, false);
    options.hideBottom = store->get<bool>(ConfigKeys::PlayerDanmakuHideBottom, false);
    options.dualSubtitle = store->get<bool>(ConfigKeys::PlayerDanmakuDualSubtitle, true);
    options.blockedKeywords = parseBlockedKeywords(store->get<QString>(ConfigKeys::PlayerDanmakuBlockedKeywords));
    return options;
}

DanmakuProviderConfig DanmakuService::providerConfig(QString serverId) const
{
    DanmakuProviderConfig config;
    if (!m_serverManager)
    {
        return config;
    }

    if (serverId.trimmed().isEmpty())
    {
        serverId = m_serverManager->activeProfile().id;
    }
    const DanmakuServerDefinition selectedServer = DanmakuSettings::selectedServer(serverId);
    return providerConfigFromServerDefinition(selectedServer, serverId, true);
}

QList<DanmakuProviderConfig> DanmakuService::enabledProviderConfigs(QString serverId) const
{
    QList<DanmakuProviderConfig> configs;
    if (!m_serverManager)
    {
        return configs;
    }

    serverId = serverId.trimmed();
    if (serverId.isEmpty())
    {
        serverId = m_serverManager->activeProfile().id;
    }

    const QList<DanmakuServerDefinition> servers = DanmakuSettings::loadServers(serverId);
    const QString selectedId = DanmakuSettings::selectedServerId(serverId);
    auto appendServerConfig = [&configs, &serverId](const DanmakuServerDefinition &server)
    {
        if (!server.enabled || !server.isValid())
        {
            return;
        }
        configs.append(providerConfigFromServerDefinition(server, serverId, false));
    };

    for (const DanmakuServerDefinition &server : servers)
    {
        if (!selectedId.isEmpty() && server.id == selectedId)
        {
            appendServerConfig(server);
            break;
        }
    }

    for (const DanmakuServerDefinition &server : servers)
    {
        if (!selectedId.isEmpty() && server.id == selectedId)
        {
            continue;
        }
        appendServerConfig(server);
    }
    return configs;
}

DanmakuProviderConfig DanmakuService::providerConfigForCandidate(QString serverId,
                                                                 const DanmakuMatchCandidate &candidate) const
{
    const QList<DanmakuProviderConfig> configs = enabledProviderConfigs(serverId);
    for (const DanmakuProviderConfig &config : configs)
    {
        if (candidateBelongsToConfig(candidate, config))
        {
            return config;
        }
    }
    return providerConfig(serverId);
}

bool DanmakuService::autoMatchEnabled(const QString &serverId) const
{
    QString resolvedServerId = serverId.trimmed();
    if (resolvedServerId.isEmpty() && m_serverManager)
    {
        resolvedServerId = m_serverManager->activeProfile().id;
    }

    return ConfigStore::instance()->get<bool>(providerKey(resolvedServerId, ConfigKeys::DanmakuAutoMatch), true);
}

QCoro::Task<DanmakuLoadResult> DanmakuService::prepareDanmaku(DanmakuMediaContext context, QString manualKeyword)
{
    DanmakuLoadResult loadResult;
    const DanmakuRenderOptions options = renderOptions();
    qDebug().noquote() << "[Danmaku][Service] Prepare start"
                       << "| media:" << context.displayTitle() << "| mediaId:" << context.mediaId
                       << "| sourceId:" << context.mediaSourceId << "| serverId:" << context.serverId
                       << "| localDir:" << localDanmakuDirectoryPath(context.serverId)
                       << "| manualKeyword:" << manualKeyword.trimmed();
    if (!options.enabled)
    {
        qDebug() << "[Danmaku][Service] Danmaku disabled globally, skip prepare";
        co_return loadResult;
    }

    const DanmakuProviderConfig config = providerConfig(context.serverId);
    const QList<DanmakuProviderConfig> enabledConfigs = enabledProviderConfigs(context.serverId);
    qDebug().noquote() << "[Danmaku][Service] Provider config"
                       << "| provider:" << config.provider << "| endpointId:" << config.endpointId
                       << "| endpointName:" << config.endpointName << "| enabled:" << config.enabled
                       << "| contentScope:" << config.contentScope << "| withRelated:" << config.withRelated
                       << "| cacheHours:" << config.cacheHours << "| enabledProviderCount:" << enabledConfigs.size()
                       << "| hasAppId:" << !config.appId.trimmed().isEmpty()
                       << "| hasAppSecret:" << !config.appSecret.trimmed().isEmpty()
                       << "| hasAccessToken:" << !config.accessToken.trimmed().isEmpty();

    DanmakuMatchResult matchResult = co_await resolveMatch(context, manualKeyword);
    loadResult.matchResult = matchResult;
    loadResult.needManualMatch = !matchResult.matched;
    if (!matchResult.matched)
    {
        qDebug().noquote() << "[Danmaku][Service] No match resolved"
                           << "| mediaId:" << context.mediaId << "| candidateCount:" << matchResult.candidates.size()
                           << "| manualKeyword:" << manualKeyword.trimmed();
        co_return loadResult;
    }

    qDebug().noquote() << "[Danmaku][Service] Match selected"
                       << "| provider:" << matchResult.selected.provider
                       << "| targetId:" << matchResult.selected.targetId
                       << "| title:" << preferredProviderTitle(matchResult.selected)
                       << "| score:" << matchResult.selected.score << "| cacheHit:" << matchResult.cacheHit
                       << "| manualOverride:" << matchResult.manualOverride;

    std::exception_ptr staleTargetException;
    try
    {
        loadResult = co_await prepareDanmakuForCandidate(context, matchResult.selected);
    }
    catch (const std::exception &e)
    {
        const QString errorMessage = QString::fromUtf8(e.what());
        const bool staleDanmuApiTarget = matchResult.selected.provider == QLatin1String("danmu_api") &&
                                         errorMessage.contains(QStringLiteral("HTTP 404"), Qt::CaseInsensitive);
        if (!staleDanmuApiTarget)
        {
            throw;
        }
        staleTargetException = std::current_exception();
        qWarning().noquote() << "[Danmaku][Service] DanmuApi target expired, rematching once"
                             << "| mediaId:" << context.mediaId << "| endpointId:" << matchResult.selected.endpointId
                             << "| targetId:" << matchResult.selected.targetId;
    }

    
    
    if (staleTargetException)
    {
        m_cacheStore->removeMatch(context);
        DanmakuMatchResult refreshedMatch = co_await resolveMatch(context, context.displayTitle());
        if (!refreshedMatch.matched || !refreshedMatch.selected.isValid())
        {
            std::rethrow_exception(staleTargetException);
        }
        loadResult = co_await prepareDanmakuForCandidate(context, refreshedMatch.selected);
        matchResult = refreshedMatch;
    }
    loadResult.matchResult = matchResult;
    if (!loadResult.success && loadResult.commentCount <= 0 &&
        matchResult.selected.provider != QLatin1String(kLocalDanmakuProvider))
    {
        loadResult.needManualMatch = true;
    }
    co_return loadResult;
}

QCoro::Task<DanmakuLoadResult> DanmakuService::prepareDanmakuForCandidate(DanmakuMediaContext context,
                                                                          DanmakuMatchCandidate candidate)
{
    DanmakuLoadResult loadResult;
    const DanmakuRenderOptions options = renderOptions();
    if (!candidate.isValid() || !options.enabled)
    {
        co_return loadResult;
    }

    const DanmakuProviderConfig config = providerConfigForCandidate(context.serverId, candidate);
    if (candidate.provider != QLatin1String(kLocalDanmakuProvider))
    {
        applyEndpointMetadata(&candidate, config);
    }
    loadResult.provider = candidate.provider;
    loadResult.sourceServerId = candidate.endpointId;
    loadResult.sourceServerName = candidate.endpointName;
    loadResult.sourceTitle = preferredProviderTitle(candidate);

    qDebug().noquote() << "[Danmaku][Service] Prepare candidate start"
                       << "| mediaId:" << context.mediaId << "| provider:" << candidate.provider
                       << "| endpointId:" << candidate.endpointId << "| endpointName:" << candidate.endpointName
                       << "| targetId:" << candidate.targetId << "| title:" << loadResult.sourceTitle;

    if (candidate.provider == QLatin1String(kLocalDanmakuProvider))
    {
        const QFileInfo localFileInfo(candidate.targetId);
        if (!localFileInfo.exists() || !localFileInfo.isFile())
        {
            qWarning().noquote() << "[Danmaku][Service] Local danmaku file missing"
                                 << "| path:" << candidate.targetId;
            co_return loadResult;
        }

        const QString localFilePath = localFileInfo.canonicalFilePath().isEmpty() ? localFileInfo.absoluteFilePath()
                                                                                  : localFileInfo.canonicalFilePath();
        const QString suffix = localFileInfo.suffix().trimmed().toLower();
        if (suffix == QLatin1String("ass"))
        {
            loadResult.success = true;
            loadResult.assFilePath = localFilePath;
            updateCommentCountFromAss(&loadResult);
            qDebug().noquote() << "[Danmaku][Service] Using local ASS file directly"
                               << "| path:" << localFilePath << "| commentCount:" << loadResult.commentCount;
            co_return loadResult;
        }

        const QString assKey = assCacheKey(candidate, options);
        QString cachedAssPath;
        if (m_cacheStore->loadAssPath(assKey, &cachedAssPath, config.cacheHours))
        {
            loadResult.success = true;
            loadResult.assFilePath = cachedAssPath;
            updateCommentCountFromAss(&loadResult);
            qDebug().noquote() << "[Danmaku][Service] Local ASS cache hit"
                               << "| assKey:" << assKey << "| path:" << cachedAssPath
                               << "| commentCount:" << loadResult.commentCount;
            co_return loadResult;
        }

        
        struct LocalParseResult
        {
            QList<DanmakuComment> comments;
            QString assPath;
            QString errorMessage;
        };

        auto localParseFuture = QtConcurrent::run(
            [localFilePath, suffix, options, assKey]() -> LocalParseResult
            {
                LocalParseResult result;
                QFile localFile(localFilePath);
                if (!localFile.open(QIODevice::ReadOnly | QIODevice::Text))
                {
                    result.errorMessage = localFile.errorString();
                    return result;
                }

                const QByteArray rawData = localFile.readAll();
                if (suffix == QLatin1String("json"))
                {
                    result.comments = parseLocalJsonComments(rawData);
                }
                else if (suffix == QLatin1String("xml"))
                {
                    result.comments = parseLocalXmlComments(rawData);
                }

                if (!result.comments.isEmpty())
                {
                    const QString assContent = DanmakuAssComposer::composeAss(result.comments, options);
                    DanmakuCacheStore store;
                    result.assPath = store.saveAssFile(assKey, assContent);
                }
                return result;
            });
        LocalParseResult parseResult = co_await localParseFuture;

        if (!parseResult.errorMessage.isEmpty())
        {
            qWarning().noquote() << "[Danmaku][Service] Failed to open local danmaku file"
                                 << "| path:" << localFilePath << "| error:" << parseResult.errorMessage;
            throw std::runtime_error(parseResult.errorMessage.toStdString());
        }

        qDebug().noquote() << "[Danmaku][Service] Parsed local danmaku file"
                           << "| path:" << localFilePath << "| suffix:" << suffix
                           << "| count:" << parseResult.comments.size();

        if (parseResult.comments.isEmpty())
        {
            qWarning().noquote() << "[Danmaku][Service] Local danmaku file contains no comments"
                                 << "| path:" << localFilePath;
            co_return loadResult;
        }

        loadResult.comments = parseResult.comments;
        loadResult.commentCount = parseResult.comments.size();
        if (!parseResult.assPath.isEmpty())
        {
            loadResult.assFilePath = parseResult.assPath;
            loadResult.success = true;
            qDebug().noquote() << "[Danmaku][Service] Generated ASS file from local danmaku"
                               << "| assKey:" << assKey << "| commentCount:" << parseResult.comments.size()
                               << "| path:" << parseResult.assPath;
        }
        else
        {
            qWarning().noquote() << "[Danmaku][Service] Failed to save ASS file for local danmaku"
                                 << "| assKey:" << assKey << "| path:" << localFilePath;
        }

        qDebug().noquote() << "[Danmaku][Service] Prepare candidate finished"
                           << "| mediaId:" << context.mediaId << "| success:" << loadResult.success
                           << "| commentCount:" << loadResult.commentCount << "| assPath:" << loadResult.assFilePath;
        co_return loadResult;
    }

    if (!config.enabled)
    {
        qDebug().noquote() << "[Danmaku][Service] Online provider disabled, skip remote candidate"
                           << "| provider:" << candidate.provider << "| targetId:" << candidate.targetId;
        co_return loadResult;
    }

    const QString assKey = assCacheKey(candidate, options);
    QString cachedAssPath;
    if (m_cacheStore->loadAssPath(assKey, &cachedAssPath, config.cacheHours))
    {
        loadResult.success = true;
        loadResult.assFilePath = cachedAssPath;
        updateCommentCountFromAss(&loadResult);
        qDebug().noquote() << "[Danmaku][Service] ASS cache hit"
                           << "| assKey:" << assKey << "| path:" << cachedAssPath
                           << "| commentCount:" << loadResult.commentCount;
    }

    
    const QString cProvider = candidate.provider;
    const QString cCacheScope = candidate.cacheScope;
    const QString cTargetId = candidate.targetId;
    const int cCacheHours = config.cacheHours;
    auto commentCacheFuture = QtConcurrent::run(
        [cProvider, cCacheScope, cTargetId, cCacheHours]()
        {
            DanmakuCacheStore store;
            return store.loadComments(cProvider, cCacheScope, cTargetId, cCacheHours);
        });
    QList<DanmakuComment> comments = co_await commentCacheFuture;
    qDebug().noquote() << "[Danmaku][Service] Comment cache"
                       << "| provider:" << candidate.provider << "| cacheScope:" << candidate.cacheScope
                       << "| targetId:" << candidate.targetId << "| count:" << comments.size();

    if (comments.isEmpty())
    {
        try
        {
            if (config.provider == QStringLiteral("dandanplay"))
            {
                qDebug().noquote() << "[Danmaku][Service] Fetching comments from provider"
                                   << "| provider:" << config.provider << "| endpointId:" << config.endpointId
                                   << "| endpointName:" << config.endpointName << "| targetId:" << candidate.targetId;
                comments = co_await m_dandanplayProvider->fetchComments(candidate, config);
            }
            else if (config.provider == QStringLiteral("danmu_api"))
            {
                qDebug().noquote() << "[Danmaku][Service] Fetching comments from provider"
                                   << "| provider:" << config.provider << "| endpointId:" << config.endpointId
                                   << "| endpointName:" << config.endpointName << "| targetId:" << candidate.targetId;
                comments = co_await m_danmuApiProvider->fetchComments(candidate, config);
            }
            if (!comments.isEmpty())
            {
                
                const QString sProvider = candidate.provider;
                const QString sCacheScope = candidate.cacheScope;
                const QString sTargetId = candidate.targetId;
                const QString sSourceTitle = loadResult.sourceTitle;
                (void)QtConcurrent::run(
                    [sProvider, sCacheScope, sTargetId, sSourceTitle, savedComments = comments]()
                    {
                        DanmakuCacheStore store;
                        store.saveComments(sProvider, sCacheScope, sTargetId, sSourceTitle, savedComments);
                    });
                qDebug().noquote() << "[Danmaku][Service] Saved comments to cache"
                                   << "| provider:" << candidate.provider << "| cacheScope:" << candidate.cacheScope
                                   << "| targetId:" << candidate.targetId << "| count:" << comments.size();
            }
        }
        catch (const std::exception &e)
        {
            qWarning().noquote() << "[Danmaku][Service] Fetch comments failed"
                                 << "| provider:" << config.provider << "| targetId:" << candidate.targetId
                                 << "| error:" << e.what();
            if (loadResult.hasRenderableTrack())
            {
                updateCommentCountFromAss(&loadResult);
                qWarning().noquote() << "[Danmaku][Service] Reusing cached ASS after fetch failure"
                                     << "| path:" << loadResult.assFilePath
                                     << "| commentCount:" << loadResult.commentCount;
                co_return loadResult;
            }
            throw;
        }
    }

    if (comments.isEmpty())
    {
        if (loadResult.hasRenderableTrack())
        {
            updateCommentCountFromAss(&loadResult);
            qDebug().noquote() << "[Danmaku][Service] Reusing cached ASS without comment payload"
                               << "| path:" << loadResult.assFilePath << "| commentCount:" << loadResult.commentCount;
            co_return loadResult;
        }

        qDebug().noquote() << "[Danmaku][Service] No comments available for selected candidate"
                           << "| provider:" << candidate.provider << "| targetId:" << candidate.targetId;
        co_return loadResult;
    }

    loadResult.commentCount = comments.size();
    loadResult.comments = comments;

    if (!loadResult.success)
    {
        
        auto assComposeFuture = QtConcurrent::run(
            [comments = std::move(comments), options, assKey]()
            {
                const QString assContent = DanmakuAssComposer::composeAss(comments, options);
                DanmakuCacheStore store;
                return store.saveAssFile(assKey, assContent);
            });
        const QString assPath = co_await assComposeFuture;
        if (assPath.isEmpty())
        {
            qWarning().noquote() << "[Danmaku][Service] Failed to save ASS file"
                                 << "| assKey:" << assKey;
            co_return loadResult;
        }
        loadResult.assFilePath = assPath;
        loadResult.success = true;
        qDebug().noquote() << "[Danmaku][Service] Generated ASS file"
                           << "| assKey:" << assKey << "| commentCount:" << loadResult.commentCount
                           << "| path:" << assPath;
    }

    qDebug().noquote() << "[Danmaku][Service] Prepare candidate finished"
                       << "| mediaId:" << context.mediaId << "| success:" << loadResult.success
                       << "| commentCount:" << loadResult.commentCount << "| assPath:" << loadResult.assFilePath;
    co_return loadResult;
}

QCoro::Task<QList<DanmakuMatchCandidate>> DanmakuService::searchCandidates(DanmakuMediaContext context,
                                                                           QString manualKeyword)
{
    const QList<DanmakuMatchCandidate> candidates =
        co_await searchCandidatesForConfig(context, providerConfig(context.serverId), manualKeyword);
    co_return candidates;
}

QCoro::Task<QList<DanmakuMatchCandidate>> DanmakuService::searchCandidatesForConfig(DanmakuMediaContext context,
                                                                                    DanmakuProviderConfig config,
                                                                                    QString manualKeyword)
{
    QList<DanmakuMatchCandidate> candidates;
    if (!config.enabled)
    {
        qDebug().noquote() << "[Danmaku][Service] Online provider disabled, skip search"
                           << "| mediaId:" << context.mediaId << "| provider:" << config.provider
                           << "| endpointId:" << config.endpointId;
        co_return candidates;
    }

    if (manualKeyword.trimmed().isEmpty() && !contentScopeAllowsContext(config, context))
    {
        qDebug().noquote() << "[Danmaku][Service] Provider content scope rejected media"
                           << "| mediaId:" << context.mediaId << "| itemType:" << context.itemType
                           << "| contentScope:" << config.contentScope
                           << "| genres:" << context.genres.join(QStringLiteral(", "));
        co_return candidates;
    }

    if (config.provider == QStringLiteral("dandanplay"))
    {
        candidates = co_await m_dandanplayProvider->searchCandidates(context, config, manualKeyword);
    }
    else if (config.provider == QStringLiteral("danmu_api"))
    {
        candidates = co_await m_danmuApiProvider->searchCandidates(context, config, manualKeyword);
    }
    const QString cacheScope = cacheScopeForConfig(config);
    for (DanmakuMatchCandidate &candidate : candidates)
    {
        if (candidate.cacheScope.trimmed().isEmpty())
        {
            candidate.cacheScope = cacheScope;
        }
        applyEndpointMetadata(&candidate, config);
    }
    qDebug().noquote() << "[Danmaku][Service] Search candidates"
                       << "| mediaId:" << context.mediaId << "| provider:" << config.provider
                       << "| endpointId:" << config.endpointId << "| endpointName:" << config.endpointName
                       << "| cacheScope:" << cacheScope << "| manualKeyword:" << manualKeyword.trimmed()
                       << "| count:" << candidates.size();
    co_return candidates;
}

QCoro::Task<QList<DanmakuService::ProviderSearchOutcome>>
DanmakuService::searchProvidersInParallel(DanmakuMediaContext context, QList<DanmakuProviderConfig> configs,
                                          QString manualKeyword)
{
    if (configs.isEmpty())
    {
        co_return QList<ProviderSearchOutcome>{};
    }

    const bool needsDandanFingerprint =
        manualKeyword.trimmed().isEmpty() &&
        std::any_of(configs.cbegin(), configs.cend(), [](const DanmakuProviderConfig &config)
                    { return config.enabled && config.provider == QLatin1String("dandanplay"); });
    if (needsDandanFingerprint && m_dandanplayProvider)
    {
        QString cachedFingerprint;
        if (m_cacheStore && m_cacheStore->loadFingerprint(context, &cachedFingerprint))
        {
            context.fileHash = cachedFingerprint;
            qDebug().noquote() << "[Danmaku][Service] Media fingerprint cache hit"
                               << "| mediaId:" << context.mediaId << "| sourceId:" << context.mediaSourceId
                               << "| fileSize:" << context.fileSize;
        }
        else
        {
            context = co_await m_dandanplayProvider->enrichMediaFingerprint(context);
            if (m_cacheStore && !context.fileHash.isEmpty())
            {
                m_cacheStore->saveFingerprint(context, context.fileHash);
            }
        }
    }

    struct SharedSearchState
    {
        QPromise<QList<ProviderSearchOutcome>> promise;
        QList<ProviderSearchOutcome> outcomes;
        int remaining = 0;
    };
    const auto state = QSharedPointer<SharedSearchState>::create();
    state->remaining = configs.size();
    QFuture<QList<ProviderSearchOutcome>> completionFuture = state->promise.future();
    state->promise.start();

    for (const DanmakuProviderConfig &config : std::as_const(configs))
    {
        QCoro::connect(searchProviderSafely(context, config, manualKeyword), this,
                       [state](ProviderSearchOutcome outcome)
                       {
                           state->outcomes.append(std::move(outcome));
                           --state->remaining;
                           if (state->remaining == 0)
                           {
                               state->promise.addResult(state->outcomes);
                               state->promise.finish();
                           }
                       });
    }

    const QList<ProviderSearchOutcome> outcomes = co_await completionFuture;
    co_return outcomes;
}

QCoro::Task<DanmakuService::ProviderSearchOutcome>
DanmakuService::searchProviderSafely(DanmakuMediaContext context, DanmakuProviderConfig config, QString manualKeyword)
{
    ProviderSearchOutcome outcome;
    outcome.config = config;
    try
    {
        outcome.candidates = co_await searchCandidatesForConfig(context, config, manualKeyword);
    }
    catch (const std::exception &e)
    {
        outcome.errorMessage = QString::fromUtf8(e.what()).trimmed();
    }
    co_return outcome;
}

QCoro::Task<QList<DanmakuMatchCandidate>> DanmakuService::searchAllCandidates(DanmakuMediaContext context,
                                                                              QString manualKeyword)
{
    QList<DanmakuMatchCandidate> aggregatedCandidates;
    const QString trimmedManualKeyword = manualKeyword.trimmed();
    const QList<DanmakuProviderConfig> onlineProviders = enabledProviderConfigs(context.serverId);
    const QString sourceMode = normalizedSourceMode(
        ConfigStore::instance()->get<QString>(providerKey(context.serverId, ConfigKeys::DanmakuSourceMode),
                                              QString::fromLatin1(kDanmakuSourceModePreferLocal)));
    const bool allowLocal = sourceMode != QLatin1String(kDanmakuSourceModeOnlineOnly);
    const bool allowOnline = sourceMode != QLatin1String(kDanmakuSourceModeLocalOnly) && !onlineProviders.isEmpty();
    const QStringList sourceOrder = (sourceMode == QLatin1String(kDanmakuSourceModePreferLocal) ||
                                     sourceMode == QLatin1String(kDanmakuSourceModeLocalOnly))
                                        ? QStringList{QStringLiteral("local"), QStringLiteral("online")}
                                        : QStringList{QStringLiteral("online"), QStringLiteral("local")};

    std::exception_ptr remoteSearchException;
    QString remoteSearchError;

    qDebug().noquote() << "[Danmaku][Service] Search all candidates"
                       << "| mediaId:" << context.mediaId << "| sourceMode:" << sourceMode
                       << "| manualKeyword:" << trimmedManualKeyword << "| allowLocal:" << allowLocal
                       << "| allowOnline:" << allowOnline << "| enabledProviderCount:" << onlineProviders.size();

    for (const QString &source : sourceOrder)
    {
        if (source == QLatin1String("local"))
        {
            if (!allowLocal)
            {
                continue;
            }

            const QString localDirectory = localDanmakuDirectoryPath(context.serverId);
            auto localSearchFuture = QtConcurrent::run(
                [localDirectory, context, trimmedManualKeyword]()
                { return searchLocalDanmakuCandidates(localDirectory, context, trimmedManualKeyword); });
            const QList<DanmakuMatchCandidate> localCandidates = co_await localSearchFuture;
            aggregatedCandidates.append(localCandidates);
            qDebug().noquote() << "[Danmaku][Service] Search all local candidates"
                               << "| mediaId:" << context.mediaId << "| count:" << localCandidates.size();
            continue;
        }

        if (!allowOnline)
        {
            continue;
        }

        const QList<ProviderSearchOutcome> outcomes =
            co_await searchProvidersInParallel(context, onlineProviders, trimmedManualKeyword);
        for (const ProviderSearchOutcome &outcome : outcomes)
        {
            if (outcome.errorMessage.isEmpty())
            {
                aggregatedCandidates.append(outcome.candidates);
                qDebug().noquote() << "[Danmaku][Service] Search all online candidates"
                                   << "| mediaId:" << context.mediaId << "| endpointId:" << outcome.config.endpointId
                                   << "| endpointName:" << outcome.config.endpointName
                                   << "| count:" << outcome.candidates.size();
            }
            else
            {
                remoteSearchError = outcome.errorMessage;
                remoteSearchException = std::make_exception_ptr(std::runtime_error(remoteSearchError.toStdString()));
                qWarning().noquote() << "[Danmaku][Service] Search all online candidates failed"
                                     << "| mediaId:" << context.mediaId << "| endpointId:" << outcome.config.endpointId
                                     << "| endpointName:" << outcome.config.endpointName
                                     << "| error:" << remoteSearchError;
            }
        }
    }

    if (aggregatedCandidates.isEmpty() && remoteSearchException)
    {
        std::rethrow_exception(remoteSearchException);
    }

    if (!aggregatedCandidates.isEmpty() && remoteSearchException)
    {
        qWarning().noquote() << "[Danmaku][Service] Search all candidates keeping partial results"
                             << "| mediaId:" << context.mediaId << "| candidateCount:" << aggregatedCandidates.size()
                             << "| error:" << remoteSearchError;
    }
    sortDanmakuCandidates(aggregatedCandidates, context,
                          trimmedManualKeyword);

    co_return aggregatedCandidates;
}

QCoro::Task<DanmakuMatchResult> DanmakuService::resolveMatch(DanmakuMediaContext context, QString manualKeyword)
{
    DanmakuMatchResult result;
    const QString trimmedManualKeyword = manualKeyword.trimmed();
    const QList<DanmakuProviderConfig> onlineProviders = enabledProviderConfigs(context.serverId);
    const QString sourceMode = normalizedSourceMode(
        ConfigStore::instance()->get<QString>(providerKey(context.serverId, ConfigKeys::DanmakuSourceMode),
                                              QString::fromLatin1(kDanmakuSourceModePreferLocal)));
    const auto cachedCandidateAvailable = [](const DanmakuMatchCandidate &candidate)
    {
        if (!candidate.isValid())
        {
            return false;
        }
        if (candidate.provider == QLatin1String(kLocalDanmakuProvider))
        {
            return QFileInfo::exists(candidate.targetId);
        }
        return true;
    };
    const auto sourceModeAllowsCandidate = [sourceMode, onlineProviders](const DanmakuMatchCandidate &candidate)
    {
        const bool isLocalCandidate = candidate.provider == QLatin1String(kLocalDanmakuProvider);
        if (isLocalCandidate)
        {
            return sourceMode != QLatin1String(kDanmakuSourceModeOnlineOnly);
        }
        if (sourceMode == QLatin1String(kDanmakuSourceModeLocalOnly) || onlineProviders.isEmpty())
        {
            return false;
        }
        if (candidate.endpointId.trimmed().isEmpty() && candidate.cacheScope.trimmed().isEmpty())
        {
            return true;
        }
        for (const DanmakuProviderConfig &config : onlineProviders)
        {
            if (candidateBelongsToConfig(candidate, config))
            {
                return true;
            }
        }
        return false;
    };

    DanmakuMatchCandidate cachedCandidate;
    bool manualOverride = false;
    const bool hasCachedMatch = m_cacheStore->loadMatch(context, &cachedCandidate, &manualOverride);
    const bool cachedCandidateTrusted = manualOverride || cachedCandidate.isHashMatch() ||
                                        (isPlausibleOnlineCandidate(context, cachedCandidate) &&
                                         cachedCandidate.score >= onlineConfidenceThreshold(context));
    if (trimmedManualKeyword.isEmpty() && hasCachedMatch && cachedCandidateAvailable(cachedCandidate) &&
        sourceModeAllowsCandidate(cachedCandidate) &&
        (cachedCandidate.provider == QLatin1String(kLocalDanmakuProvider) || cachedCandidateTrusted))
    {
        result.matched = true;
        result.cacheHit = true;
        result.manualOverride = manualOverride;
        result.selected = cachedCandidate;
        qDebug().noquote() << "[Danmaku][Service] Match cache hit"
                           << "| mediaId:" << context.mediaId << "| matched:" << result.matched
                           << "| manualOverride:" << result.manualOverride
                           << "| endpointId:" << result.selected.endpointId
                           << "| endpointName:" << result.selected.endpointName
                           << "| targetId:" << result.selected.targetId;
        if (result.matched)
        {
            co_return result;
        }
    }

    if (trimmedManualKeyword.isEmpty() && !autoMatchEnabled(context.serverId))
    {
        qDebug().noquote() << "[Danmaku][Service] Auto match disabled"
                           << "| mediaId:" << context.mediaId << "| serverId:" << context.serverId;
        co_return result;
    }

    const bool allowLocal = sourceMode != QLatin1String(kDanmakuSourceModeOnlineOnly);
    const bool allowOnline = sourceMode != QLatin1String(kDanmakuSourceModeLocalOnly) && !onlineProviders.isEmpty();
    const QStringList sourceOrder = (sourceMode == QLatin1String(kDanmakuSourceModePreferLocal) ||
                                     sourceMode == QLatin1String(kDanmakuSourceModeLocalOnly))
                                        ? QStringList{QStringLiteral("local"), QStringLiteral("online")}
                                        : QStringList{QStringLiteral("online"), QStringLiteral("local")};

    qDebug().noquote() << "[Danmaku][Service] Resolve match"
                       << "| mediaId:" << context.mediaId << "| sourceMode:" << sourceMode
                       << "| allowLocal:" << allowLocal << "| allowOnline:" << allowOnline
                       << "| enabledProviderCount:" << onlineProviders.size();

    QList<DanmakuMatchCandidate> aggregatedCandidates;
    std::exception_ptr remoteSearchException;
    QString remoteSearchError;

    const auto appendCandidates = [&aggregatedCandidates](const QList<DanmakuMatchCandidate> &candidates)
    { aggregatedCandidates.append(candidates); };
    const auto trySelectLocalCandidate = [this, &context,
                                          &result](const QList<DanmakuMatchCandidate> &candidates) -> bool
    {
        if (candidates.isEmpty())
        {
            return false;
        }

        DanmakuMatchCandidate selected;
        for (const DanmakuMatchCandidate &candidate : candidates)
        {
            if (isPlausibleOnlineCandidate(context, candidate))
            {
                selected = candidate;
                break;
            }
        }
        if (!selected.isValid())
        {
            qDebug().noquote() << "[Danmaku][Service] Local candidates rejected by metadata constraints"
                               << "| mediaId:" << context.mediaId << "| candidateCount:" << candidates.size();
            return false;
        }
        const double threshold = context.isEpisode() ? 44.0 : 36.0;
        const bool allowSingleCandidateFallback = candidates.size() == 1 && selected.score >= 24.0;
        qDebug().noquote() << "[Danmaku][Service] Local candidates discovered"
                           << "| mediaId:" << context.mediaId << "| count:" << candidates.size()
                           << "| topTargetId:" << selected.targetId << "| topScore:" << selected.score
                           << "| threshold:" << threshold;

        if (selected.score >= threshold || allowSingleCandidateFallback)
        {
            result.matched = true;
            result.selected = selected;
            result.manualOverride = false;
            m_cacheStore->saveMatch(context, selected, false);
            qDebug().noquote() << "[Danmaku][Service] Local match selected"
                               << "| mediaId:" << context.mediaId << "| targetId:" << selected.targetId
                               << "| score:" << selected.score << "| manualOverride:" << result.manualOverride;
            return true;
        }
        return false;
    };
    const auto trySelectOnlineCandidate = [this, &context,
                                           &result](const QList<DanmakuMatchCandidate> &candidates) -> bool
    {
        if (candidates.isEmpty())
        {
            return false;
        }

        DanmakuMatchCandidate selected;
        for (const DanmakuMatchCandidate &candidate : candidates)
        {
            if (isPlausibleOnlineCandidate(context, candidate))
            {
                selected = candidate;
                break;
            }
        }
        if (!selected.isValid())
        {
            qDebug().noquote() << "[Danmaku][Service] Online candidates rejected by metadata constraints"
                               << "| mediaId:" << context.mediaId << "| candidateCount:" << candidates.size();
            return false;
        }

        const double threshold = onlineConfidenceThreshold(context);
        if (!selected.isHashMatch() && selected.score < threshold)
        {
            qDebug().noquote() << "[Danmaku][Service] Best plausible online match below confidence threshold"
                               << "| mediaId:" << context.mediaId << "| endpointId:" << selected.endpointId
                               << "| endpointName:" << selected.endpointName << "| targetId:" << selected.targetId
                               << "| score:" << selected.score << "| threshold:" << threshold;
            return false;
        }

        result.matched = true;
        result.selected = selected;
        result.manualOverride = false;
        m_cacheStore->saveMatch(context, selected, false);
        qDebug().noquote() << "[Danmaku][Service] Auto match selected"
                           << "| hashVerified:" << selected.isHashMatch() << "| mediaId:" << context.mediaId
                           << "| endpointId:" << selected.endpointId << "| endpointName:" << selected.endpointName
                           << "| targetId:" << selected.targetId << "| score:" << selected.score;
        return true;
    };

    for (const QString &source : sourceOrder)
    {
        if (source == QLatin1String("local"))
        {
            if (!allowLocal)
            {
                continue;
            }

            const QString localDirectory = localDanmakuDirectoryPath(context.serverId);
            auto localSearchFuture = QtConcurrent::run(
                [localDirectory, context, trimmedManualKeyword]()
                { return searchLocalDanmakuCandidates(localDirectory, context, trimmedManualKeyword); });
            const QList<DanmakuMatchCandidate> localCandidates = co_await localSearchFuture;
            appendCandidates(localCandidates);
            if (trySelectLocalCandidate(localCandidates))
            {
                result.candidates = aggregatedCandidates;
                co_return result;
            }
            continue;
        }

        if (!allowOnline)
        {
            continue;
        }

        QList<DanmakuMatchCandidate> onlineCandidates;
        const QList<ProviderSearchOutcome> outcomes =
            co_await searchProvidersInParallel(context, onlineProviders, trimmedManualKeyword);
        for (const ProviderSearchOutcome &outcome : outcomes)
        {
            if (outcome.errorMessage.isEmpty())
            {
                onlineCandidates.append(outcome.candidates);
                qDebug().noquote() << "[Danmaku][Service] Resolve online candidates"
                                   << "| mediaId:" << context.mediaId << "| endpointId:" << outcome.config.endpointId
                                   << "| endpointName:" << outcome.config.endpointName
                                   << "| count:" << outcome.candidates.size();
            }
            else
            {
                remoteSearchError = outcome.errorMessage;
                remoteSearchException = std::make_exception_ptr(std::runtime_error(remoteSearchError.toStdString()));
                qWarning().noquote() << "[Danmaku][Service] Search candidates failed"
                                     << "| mediaId:" << context.mediaId << "| endpointId:" << outcome.config.endpointId
                                     << "| endpointName:" << outcome.config.endpointName
                                     << "| sourceMode:" << sourceMode << "| manualKeyword:" << trimmedManualKeyword
                                     << "| error:" << remoteSearchError;
            }
        }

        sortDanmakuCandidates(onlineCandidates, context,
                              trimmedManualKeyword);
        appendCandidates(onlineCandidates);
        if (trySelectOnlineCandidate(onlineCandidates))
        {
            result.candidates = aggregatedCandidates;
            co_return result;
        }
    }

    result.candidates = aggregatedCandidates;
    if (aggregatedCandidates.isEmpty())
    {
        if (remoteSearchException)
        {
            std::rethrow_exception(remoteSearchException);
        }
    }
    else if (remoteSearchException)
    {
        qWarning().noquote() << "[Danmaku][Service] Keep available candidates after remote search failure"
                             << "| mediaId:" << context.mediaId << "| candidateCount:" << aggregatedCandidates.size()
                             << "| error:" << remoteSearchError;
    }

    co_return result;
}

void DanmakuService::saveManualMatch(const DanmakuMediaContext &context, const DanmakuMatchCandidate &candidate)
{
    if (!candidate.isValid())
    {
        return;
    }
    m_cacheStore->saveMatch(context, candidate, true);
    qDebug().noquote() << "[Danmaku][Service] Manual match saved"
                       << "| mediaId:" << context.mediaId << "| endpointId:" << candidate.endpointId
                       << "| endpointName:" << candidate.endpointName << "| targetId:" << candidate.targetId;
}

void DanmakuService::clearCache()
{
    m_cacheStore->clearAll();
    qDebug() << "[Danmaku][Service] Cache cleared";
}

QString DanmakuService::assCacheKey(const DanmakuMatchCandidate &candidate, const DanmakuRenderOptions &options) const
{
    QJsonObject obj;
    obj["assRenderVersion"] = kDanmakuAssRenderVersion;
    obj["provider"] = candidate.provider;
    obj["cacheScope"] = candidate.cacheScope;
    obj["endpointId"] = candidate.endpointId;
    obj["targetId"] = candidate.targetId;
    obj["opacity"] = options.opacity;
    obj["fontScale"] = options.fontScale;
    obj["areaPercent"] = options.areaPercent;
    obj["density"] = options.density;
    obj["speedScale"] = options.speedScale;
    obj["offsetMs"] = options.offsetMs;
    obj["hideScroll"] = options.hideScroll;
    obj["hideTop"] = options.hideTop;
    obj["hideBottom"] = options.hideBottom;
    if (candidate.provider == QLatin1String(kLocalDanmakuProvider))
    {
        const QFileInfo fileInfo(candidate.targetId);
        obj["localPath"] = QDir::fromNativeSeparators(
            fileInfo.canonicalFilePath().isEmpty() ? fileInfo.absoluteFilePath() : fileInfo.canonicalFilePath());
        obj["localSize"] = QString::number(fileInfo.size());
        obj["localModifiedAt"] = QString::number(fileInfo.lastModified().toMSecsSinceEpoch());
    }
    QJsonArray blocked;
    for (const QString &keyword : options.blockedKeywords)
    {
        blocked.append(keyword);
    }
    obj["blockedKeywords"] = blocked;

    return QString::fromLatin1(
        QCryptographicHash::hash(QJsonDocument(obj).toJson(QJsonDocument::Compact), QCryptographicHash::Sha1).toHex());
}
