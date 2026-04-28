#include "dandanplayprovider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <stdexcept>
#include <utility>

namespace {

QString firstNonEmpty(std::initializer_list<QString> values)
{
    for (const QString &value : values) {
        if (!value.trimmed().isEmpty()) {
            return value.trimmed();
        }
    }
    return {};
}

QString stringField(const QJsonObject &obj,
                    std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value = obj.value(QLatin1String(key)).toVariant().toString();
        if (!value.trimmed().isEmpty()) {
            return value.trimmed();
        }
    }
    return {};
}

int intField(const QJsonObject &obj,
             std::initializer_list<const char *> keys,
             int defaultValue = 0)
{
    for (const char *key : keys) {
        const QJsonValue value = obj.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull()) {
            return value.toVariant().toInt();
        }
    }
    return defaultValue;
}

qint64 longField(const QJsonObject &obj,
                 std::initializer_list<const char *> keys,
                 qint64 defaultValue = 0)
{
    for (const char *key : keys) {
        const QJsonValue value = obj.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull()) {
            return value.toVariant().toLongLong();
        }
    }
    return defaultValue;
}

QColor parseColor(const QJsonValue &value)
{
    if (value.isString()) {
        const QColor color(value.toString());
        if (color.isValid()) {
            return color;
        }
        bool ok = false;
        const uint rgb = value.toString().toUInt(&ok, 10);
        if (ok) {
            return QColor::fromRgb(rgb);
        }
    }

    bool ok = false;
    const uint rgb = value.toVariant().toUInt(&ok);
    if (ok) {
        return QColor::fromRgb(rgb);
    }
    return QColor(Qt::white);
}

QString providerIdValue(const QVariantMap &providerIds,
                        std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value =
            providerIds.value(QString::fromLatin1(key)).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

QString normalizedHost(const QString &baseUrl)
{
    const QUrl url = QUrl::fromUserInput(baseUrl.trimmed());
    return url.host().trimmed().toLower();
}

bool isOfficialDandanplayEndpoint(const DanmakuProviderConfig &config)
{
    return normalizedHost(config.baseUrl) == QStringLiteral("api.dandanplay.net");
}

QString missingCredentialsMessage()
{
    return QCoreApplication::translate(
        "DandanplayProvider",
        "DandanPlay Open API now requires App ID and App Secret. Configure them in Danmaku Server settings.");
}

bool isMissingCredentialsErrorMessage(const QString &message)
{
    return message.trimmed() == missingCredentialsMessage();
}

void ensureOfficialAuthentication(const DanmakuProviderConfig &config,
                                  const QString &apiPath)
{
    if (!isOfficialDandanplayEndpoint(config)) {
        return;
    }

    const QString appId = config.appId.trimmed();
    const QString appSecret = config.appSecret.trimmed();
    if (!appId.isEmpty() && !appSecret.isEmpty()) {
        return;
    }

    qWarning().noquote()
        << "[Danmaku][DandanPlay] Missing App credentials for official endpoint"
        << "| path:" << apiPath
        << "| baseUrl:" << config.baseUrl;
    throw std::runtime_error(missingCredentialsMessage().toStdString());
}

QMap<QString, QString> buildHeaders(const DanmakuProviderConfig &config,
                                    const QString &apiPath)
{
    QMap<QString, QString> headers;
    headers.insert(QStringLiteral("Accept"), QStringLiteral("application/json"));
    headers.insert(QStringLiteral("User-Agent"),
                   QStringLiteral("qEmby/1.0 (Danmaku)"));

    const QString appId = config.appId.trimmed();
    const QString appSecret = config.appSecret.trimmed();
    if (appId.isEmpty() || appSecret.isEmpty()) {
        return headers;
    }

    const QString unixTimestamp =
        QString::number(QDateTime::currentSecsSinceEpoch());
    const QByteArray raw = (appId + unixTimestamp + apiPath + appSecret)
                               .toUtf8();
    const QString signature = QString::fromLatin1(
        QCryptographicHash::hash(raw, QCryptographicHash::Sha256).toBase64());

    headers.insert(QStringLiteral("X-AppId"), appId);
    headers.insert(QStringLiteral("X-Timestamp"), unixTimestamp);
    headers.insert(QStringLiteral("X-Signature"), signature);
    return headers;
}

QString buildUrl(const DanmakuProviderConfig &config,
                 const QString &apiPath,
                 const QList<QPair<QString, QString>> &queryItems)
{
    QUrl url(config.baseUrl.trimmed().isEmpty()
                 ? QStringLiteral("https://api.dandanplay.net")
                 : config.baseUrl.trimmed());
    QString path = url.path();
    if (!path.endsWith('/')) {
        path.append('/');
    }
    const QString normalizedApiPath =
        apiPath.startsWith('/') ? apiPath.mid(1) : apiPath;
    path += normalizedApiPath;
    url.setPath(path);

    QUrlQuery query;
    for (const auto &item : queryItems) {
        if (!item.second.trimmed().isEmpty()) {
            query.addQueryItem(item.first, item.second.trimmed());
        }
    }
    url.setQuery(query);
    return url.toString();
}

double titleScore(const QString &lhs, const QString &rhs)
{
    if (lhs.isEmpty() || rhs.isEmpty()) {
        return 0.0;
    }

    const QString rawLhs = lhs.trimmed().toLower();
    const QString rawRhs = rhs.trimmed().toLower();
    if (rawLhs == rawRhs) {
        return 1.0;
    }

    auto cleaned = [](const QString &s) -> QString {
        QString result = s;
        const int fromIdx = result.lastIndexOf(QStringLiteral(" from "));
        if (fromIdx > 0) {
            result = result.left(fromIdx).trimmed();
        }
        int pos = 0;
        while (pos < result.size()) {
            const QChar ch = result.at(pos);
            if (ch == QChar(0x3010) || ch == QLatin1Char('[')) {
                const QChar closeChar =
                    ch == QChar(0x3010) ? QChar(0x3011) : QLatin1Char(']');
                const int closeIdx = result.indexOf(closeChar, pos + 1);
                if (closeIdx > pos) {
                    result.remove(pos, closeIdx - pos + 1);
                    continue;
                }
            }
            ++pos;
        }
        result = result.trimmed();
        for (int i = result.size() - 1; i >= 0; --i) {
            const QChar ch = result.at(i);
            if (ch == QLatin1Char(')') || ch == QChar(0xFF09)) {
                const QChar openChar =
                    ch == QLatin1Char(')') ? QLatin1Char('(') : QChar(0xFF08);
                const int openIdx = result.lastIndexOf(openChar, i - 1);
                if (openIdx >= 0 && i - openIdx == 5) {
                    bool isYear = true;
                    for (int j = openIdx + 1; j < i; ++j) {
                        if (!result.at(j).isDigit()) {
                            isYear = false;
                            break;
                        }
                    }
                    if (isYear) {
                        result.remove(openIdx, i - openIdx + 1);
                        result = result.trimmed();
                    }
                }
                break;
            }
            if (!ch.isSpace()) {
                break;
            }
        }
        return result.trimmed();
    };

    const QString cleanLhs = cleaned(rawLhs);
    const QString cleanRhs = cleaned(rawRhs);
    if (!cleanLhs.isEmpty() && !cleanRhs.isEmpty() &&
        cleanLhs == cleanRhs) {
        return 0.98;
    }

    const QString &shorter =
        cleanLhs.size() <= cleanRhs.size() ? cleanLhs : cleanRhs;
    const QString &longer =
        cleanLhs.size() <= cleanRhs.size() ? cleanRhs : cleanLhs;
    if (!shorter.isEmpty() && !longer.isEmpty()) {
        if (longer.startsWith(shorter)) {
            const double ratio = shorter.size() /
                                 static_cast<double>(longer.size());
            return 0.70 + ratio * 0.22;
        }
        if (longer.contains(shorter)) {
            const double ratio = shorter.size() /
                                 static_cast<double>(longer.size());
            return 0.30 + ratio * 0.30;
        }
    }

    if (rawLhs.contains(rawRhs) || rawRhs.contains(rawLhs)) {
        return 0.45;
    }

    int overlap = 0;
    for (const QChar ch : cleanLhs.isEmpty() ? rawLhs : cleanLhs) {
        const QString &target = cleanRhs.isEmpty() ? rawRhs : cleanRhs;
        if (!ch.isSpace() && target.contains(ch)) {
            ++overlap;
        }
    }
    const int base = cleanLhs.isEmpty() ? rawLhs.size()
                                        : cleanLhs.size();
    return base == 0
               ? 0.0
               : qBound(0.0, overlap / static_cast<double>(base), 0.35);
}

int extractYear(const QString &title)
{
    const int parenIdx = title.lastIndexOf('(');
    if (parenIdx < 0) {
        return 0;
    }
    const int closeIdx = title.indexOf(')', parenIdx + 1);
    if (closeIdx < 0 || closeIdx - parenIdx != 5) {
        return 0;
    }
    bool ok = false;
    const int year =
        title.mid(parenIdx + 1, 4).toInt(&ok);
    return (ok && year >= 1900 && year <= 2100) ? year : 0;
}

double computeScore(const DanmakuMediaContext &context,
                    const DanmakuMatchCandidate &candidate,
                    const QString &queryKeyword)
{
    double score = 0.0;
    const QString subjectTitle =
        context.isEpisode() ? context.seriesName : context.title;
    const QString &candidateSubject = candidate.subtitle.isEmpty()
                                          ? candidate.title
                                          : candidate.subtitle;
    score += titleScore(subjectTitle, candidateSubject) * 55.0;
    score += titleScore(context.originalTitle, candidateSubject) * 18.0;
    score += titleScore(context.title, candidate.title) * 18.0;
    score += titleScore(queryKeyword, candidate.displayText()) * 12.0;

    if (context.isEpisode() && context.episodeNumber > 0 &&
        candidate.episodeNumber > 0 &&
        context.episodeNumber == candidate.episodeNumber) {
        score += 24.0;
    }

    if (context.durationMs > 0 && candidate.durationMs > 0) {
        const qint64 diff = std::llabs(context.durationMs - candidate.durationMs);
        if (diff <= 30 * 1000) {
            score += 12.0;
        } else if (diff <= 90 * 1000) {
            score += 6.0;
        }
    }

    if (candidate.commentCount > 0) {
        score += std::min(10.0, candidate.commentCount / 80.0);
    }

    if (context.productionYear > 0) {
        const int candidateYear = extractYear(candidateSubject);
        if (candidateYear > 0) {
            const int yearDiff = std::abs(context.productionYear - candidateYear);
            if (yearDiff == 0) {
                score += 18.0;
            } else if (yearDiff <= 1) {
                score += 8.0;
            } else if (yearDiff <= 3) {
                score += 3.0;
            }
        }
    }

    return score;
}

QList<DanmakuMatchCandidate> parseSearchResponse(const QJsonObject &response,
                                                 const DanmakuMediaContext &context,
                                                 const QString &queryKeyword)
{
    QList<DanmakuMatchCandidate> candidates;
    auto parseArray = [&](const QJsonArray &animeArray) {
        for (const QJsonValue &animeValue : animeArray) {
            const QJsonObject animeObj = animeValue.toObject();
            const QString animeTitle = firstNonEmpty(
                {stringField(animeObj, {"animeTitle", "title", "name"}),
                 stringField(animeObj, {"animeTitleCN", "animeTitleJP"})});

            QJsonArray episodes = animeObj.value(QStringLiteral("episodes")).toArray();
            if (episodes.isEmpty() &&
                !animeObj.value(QStringLiteral("episodeId")).isUndefined()) {
                episodes.append(animeObj);
            }
            if (episodes.isEmpty() &&
                (!animeObj.value(QStringLiteral("animeId")).isUndefined() ||
                 !animeObj.value(QStringLiteral("bangumiId")).isUndefined())) {
                QJsonObject fallbackEpisodeObj = animeObj;
                const QString animeIdStr = stringField(
                    animeObj, {"animeId", "bangumiId"});
                if (!animeIdStr.isEmpty()) {
                    fallbackEpisodeObj.insert(
                        QStringLiteral("episodeId"), animeIdStr);
                }
                if (fallbackEpisodeObj.value(
                        QStringLiteral("episodeTitle")).isUndefined()) {
                    fallbackEpisodeObj.insert(
                        QStringLiteral("episodeTitle"), animeTitle);
                }
                episodes.append(fallbackEpisodeObj);
            }

            for (const QJsonValue &episodeValue : episodes) {
                const QJsonObject episodeObj = episodeValue.toObject();
                DanmakuMatchCandidate candidate;
                candidate.provider = QStringLiteral("dandanplay");
                candidate.targetId =
                    stringField(episodeObj, {"episodeId", "id", "episodeID"});
                candidate.title = stringField(
                    episodeObj, {"episodeTitle", "title", "name", "episodeName"});
                candidate.subtitle = animeTitle;
                candidate.episodeNumber =
                    intField(episodeObj, {"episodeNumber", "episode", "sort"}, -1);
                candidate.durationMs = longField(
                    episodeObj, {"durationMs", "duration", "videoDuration"}, 0);
                if (candidate.durationMs > 0 && candidate.durationMs < 1000) {
                    candidate.durationMs *= 1000;
                }
                candidate.commentCount = intField(
                    episodeObj, {"commentCount", "comments", "danmakuCount"}, 0);
                candidate.score = computeScore(context, candidate, queryKeyword);
                candidate.matchReason = QStringLiteral("search");
                if (candidate.isValid()) {
                    candidates.append(candidate);
                }
            }
        }
    };

    parseArray(response.value(QStringLiteral("animes")).toArray());
    if (!candidates.isEmpty()) {
        return candidates;
    }

    parseArray(response.value(QStringLiteral("data")).toArray());
    return candidates;
}

QList<DanmakuComment> parseCommentsResponse(const QJsonObject &response)
{
    QList<DanmakuComment> comments;
    QJsonArray array = response.value(QStringLiteral("comments")).toArray();
    if (array.isEmpty()) {
        array = response.value(QStringLiteral("data")).toArray();
    }
    if (array.isEmpty()) {
        array = response.value(QStringLiteral("result")).toArray();
    }

    comments.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject obj = value.toObject();
        DanmakuComment comment;
        comment.text = firstNonEmpty(
            {stringField(obj, {"m", "text", "comment", "content"})});

        const QString p = stringField(obj, {"p"});
        if (!p.isEmpty()) {
            const QStringList parts = p.split(',', Qt::KeepEmptyParts);
            if (!parts.isEmpty()) {
                comment.timeMs =
                    static_cast<qint64>(parts[0].toDouble() * 1000.0);
            }
            if (parts.size() > 1) {
                bool ok = false;
                const int parsedMode = parts[1].toInt(&ok);
                if (ok) {
                    comment.mode = parsedMode;
                }
            }
            if (parts.size() > 2) {
                comment.color = parseColor(parts[2]);
            }
            if (parts.size() > 3) {
                bool ok = false;
                const int parsedFontLevel = parts[3].toInt(&ok);
                if (ok) {
                    comment.fontLevel = parsedFontLevel;
                }
            }
        } else {
            comment.timeMs = longField(obj, {"timeMs", "time", "position"}, 0);
            if (comment.timeMs > 0 && comment.timeMs < 1000) {
                comment.timeMs *= 1000;
            }
            comment.mode = intField(obj, {"mode", "positionType"}, 1);
            comment.color = parseColor(obj.value(QStringLiteral("color")));
            comment.fontLevel = intField(obj, {"size", "fontLevel"}, 25);
        }

        comment.sender =
            stringField(obj, {"sender", "user", "author", "nickname"});
        const QString createdAtStr =
            stringField(obj, {"dateTime", "createdAt", "date", "timeStamp"});
        comment.createdAt =
            QDateTime::fromString(createdAtStr, Qt::ISODate);

        if (comment.isValid()) {
            comments.append(comment);
        }
    }
    return comments;
}

QList<DanmakuMatchCandidate> deduplicateCandidates(
    const QList<DanmakuMatchCandidate> &candidates)
{
    QHash<QString, DanmakuMatchCandidate> bestById;
    for (const DanmakuMatchCandidate &candidate : candidates) {
        const auto existing = bestById.constFind(candidate.targetId);
        if (existing == bestById.constEnd() ||
            candidate.score > existing->score) {
            bestById.insert(candidate.targetId, candidate);
        }
    }

    QList<DanmakuMatchCandidate> deduplicated = bestById.values();
    std::sort(deduplicated.begin(), deduplicated.end(),
              [](const DanmakuMatchCandidate &lhs,
                 const DanmakuMatchCandidate &rhs) {
                  if (!qFuzzyCompare(lhs.score, rhs.score)) {
                      return lhs.score > rhs.score;
                  }
                  return lhs.commentCount > rhs.commentCount;
              });
    return deduplicated;
}

QList<DanmakuMatchCandidate> parseMatchResponse(
    const QJsonObject &response,
    const DanmakuMediaContext &context,
    const QString &fileName)
{
    QList<DanmakuMatchCandidate> candidates;
    const bool isMatched = response.value(QStringLiteral("isMatched")).toBool();
    QJsonArray matches = response.value(QStringLiteral("matches")).toArray();
    if (matches.isEmpty()) {
        matches = response.value(QStringLiteral("animes")).toArray();
    }
    if (matches.isEmpty()) {
        matches = response.value(QStringLiteral("data")).toArray();
    }

    for (const QJsonValue &matchValue : matches) {
        const QJsonObject matchObj = matchValue.toObject();
        DanmakuMatchCandidate candidate;
        candidate.provider = QStringLiteral("dandanplay");
        candidate.targetId =
            stringField(matchObj, {"episodeId", "id", "episodeID"});
        candidate.title = stringField(
            matchObj, {"episodeTitle", "title", "name", "episodeName"});
        candidate.subtitle = stringField(
            matchObj, {"animeTitle", "animeTitleCN", "animeName"});
        candidate.episodeNumber =
            intField(matchObj, {"episodeNumber", "episode", "sort"}, -1);
        candidate.durationMs = longField(
            matchObj, {"durationMs", "duration", "videoDuration"}, 0);
        if (candidate.durationMs > 0 && candidate.durationMs < 1000) {
            candidate.durationMs *= 1000;
        }
        candidate.commentCount = intField(
            matchObj, {"commentCount", "comments", "danmakuCount"}, 0);

        if (candidate.targetId.isEmpty()) {
            const QString animeIdStr =
                stringField(matchObj, {"animeId", "bangumiId"});
            if (!animeIdStr.isEmpty()) {
                candidate.targetId = animeIdStr;
            }
        }
        if (candidate.title.isEmpty()) {
            candidate.title = candidate.subtitle;
        }

        candidate.score = computeScore(context, candidate, fileName);
        if (isMatched) {
            candidate.score += 30.0;
        }
        candidate.matchReason = QStringLiteral("match");
        if (candidate.isValid()) {
            candidates.append(candidate);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
              [](const DanmakuMatchCandidate &lhs,
                 const DanmakuMatchCandidate &rhs) {
                  return lhs.score > rhs.score;
              });
    return candidates;
}

} 

DandanplayProvider::DandanplayProvider(NetworkManager *networkManager)
    : m_networkManager(networkManager)
{
}

QCoro::Task<QList<DanmakuMatchCandidate>> DandanplayProvider::searchCandidates(
    DanmakuMediaContext context,
    DanmakuProviderConfig config,
    QString manualKeyword) const
{
    QList<DanmakuMatchCandidate> allCandidates;
    if (!m_networkManager) {
        co_return allCandidates;
    }

    bool hadSuccessfulSearchResponse = false;
    QString lastSearchError;

    QStringList keywords;
    if (!manualKeyword.trimmed().isEmpty()) {
        keywords << manualKeyword.trimmed();
    } else if (context.isEpisode()) {
        keywords << context.seriesName << context.title << context.originalTitle;
    } else {
        keywords << context.title << context.originalTitle;
    }
    QStringList normalizedKeywords;
    normalizedKeywords.reserve(keywords.size());
    for (const QString &keyword : std::as_const(keywords)) {
        const QString trimmed = keyword.trimmed();
        if (!trimmed.isEmpty()) {
            normalizedKeywords.append(trimmed);
        }
    }
    keywords = normalizedKeywords;
    keywords.removeDuplicates();

    const QString tmdbId = providerIdValue(
        context.providerIds, {"Tmdb", "tmdb", "TMDb", "tmdbid"});

    const QString filePath = context.path.trimmed();
    const QString fileName =
        filePath.isEmpty() ? QString()
                           : QFileInfo(filePath).fileName().trimmed();

    qDebug().noquote()
        << "[Danmaku][DandanPlay] Search start"
        << "| media:" << context.displayTitle()
        << "| mediaId:" << context.mediaId
        << "| isEpisode:" << context.isEpisode()
        << "| contentScope:" << config.contentScope
        << "| fileName:" << fileName
        << "| keywords:" << keywords.join(QStringLiteral(" | "));

    if (!fileName.isEmpty() && manualKeyword.trimmed().isEmpty()) {
        try {
            const QString matchPath = QStringLiteral("/api/v2/match");
            ensureOfficialAuthentication(config, matchPath);
            QJsonObject payload;
            payload.insert(QStringLiteral("fileName"), fileName);
            payload.insert(QStringLiteral("fileHash"), QString());
            payload.insert(QStringLiteral("fileSize"), 0);
            payload.insert(QStringLiteral("matchMode"),
                           QStringLiteral("fileNameOnly"));
            const QString matchUrl = buildUrl(config, matchPath, {});
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Match request"
                << "| fileName:" << fileName
                << "| url:" << matchUrl;
            const QJsonObject matchResponse = co_await m_networkManager->post(
                matchUrl, buildHeaders(config, matchPath), payload);
            hadSuccessfulSearchResponse = true;
            const QList<DanmakuMatchCandidate> matchCandidates =
                parseMatchResponse(matchResponse, context, fileName);
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Match result"
                << "| fileName:" << fileName
                << "| isMatched:"
                << matchResponse.value(QStringLiteral("isMatched")).toBool()
                << "| count:" << matchCandidates.size();
            if (!matchCandidates.isEmpty()) {
                allCandidates.append(matchCandidates);
            }
        } catch (const std::exception &e) {
            const QString errorMessage =
                QString::fromUtf8(e.what()).trimmed();
            if (isMissingCredentialsErrorMessage(errorMessage)) {
                throw;
            }
            lastSearchError = errorMessage;
            qWarning().noquote()
                << "[Danmaku][DandanPlay] Match request failed"
                << "| fileName:" << fileName
                << "| error:" << errorMessage;
        }
    }

    for (const QString &keyword : std::as_const(keywords)) {
        QList<DanmakuMatchCandidate> keywordCandidates;
        try {
            const QString apiPath = QStringLiteral("/api/v2/search/episodes");
            ensureOfficialAuthentication(config, apiPath);
            QList<QPair<QString, QString>> queryItems;
            queryItems.append({QStringLiteral("anime"), keyword});
            if (context.isEpisode()) {
                if (context.episodeNumber > 0) {
                    queryItems.append({QStringLiteral("episode"),
                                       QString::number(context.episodeNumber)});
                }
                if (!tmdbId.isEmpty()) {
                    queryItems.append({QStringLiteral("tmdbId"), tmdbId});
                }
            }

            const QString url = buildUrl(config, apiPath, queryItems);
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Search request"
                << "| path:" << apiPath
                << "| keyword:" << keyword
                << "| url:" << url;
            const QJsonObject response =
                co_await m_networkManager->get(url, buildHeaders(config, apiPath));
            hadSuccessfulSearchResponse = true;
            keywordCandidates =
                parseSearchResponse(response, context, keyword);
            if (!keywordCandidates.isEmpty()) {
                qDebug().noquote()
                    << "[Danmaku][DandanPlay] Search hit"
                    << "| path:" << apiPath
                    << "| keyword:" << keyword
                    << "| count:" << keywordCandidates.size();
                allCandidates.append(keywordCandidates);
            }
        } catch (const std::exception &e) {
            const QString errorMessage = QString::fromUtf8(e.what()).trimmed();
            if (isMissingCredentialsErrorMessage(errorMessage)) {
                throw;
            }
            lastSearchError = errorMessage;
            qWarning().noquote()
                << "[Danmaku][DandanPlay] Search request failed"
                << "| keyword:" << keyword
                << "| path: /api/v2/search/episodes"
                << "| error:" << errorMessage;
        }

        if (!keywordCandidates.isEmpty()) {
            continue;
        }

        const QString fallbackPath = QStringLiteral("/api/v2/search/anime");
        const QString fallbackUrl = buildUrl(
            config, fallbackPath, {{QStringLiteral("keyword"), keyword}});
        try {
            ensureOfficialAuthentication(config, fallbackPath);
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Search fallback request"
                << "| path:" << fallbackPath
                << "| keyword:" << keyword
                << "| url:" << fallbackUrl;
            const QJsonObject fallbackResponse =
                co_await m_networkManager->get(
                    fallbackUrl, buildHeaders(config, fallbackPath));
            hadSuccessfulSearchResponse = true;
            const QList<DanmakuMatchCandidate> fallbackCandidates =
                parseSearchResponse(fallbackResponse, context, keyword);
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Search fallback result"
                << "| keyword:" << keyword
                << "| count:" << fallbackCandidates.size();
            allCandidates.append(fallbackCandidates);
        } catch (const std::exception &e) {
            const QString errorMessage = QString::fromUtf8(e.what()).trimmed();
            if (isMissingCredentialsErrorMessage(errorMessage)) {
                throw;
            }
            lastSearchError = errorMessage;
            qWarning().noquote()
                << "[Danmaku][DandanPlay] Search fallback failed"
                << "| keyword:" << keyword
                << "| path:" << fallbackPath
                << "| error:" << errorMessage;
        }
    }

    if (allCandidates.isEmpty() && !hadSuccessfulSearchResponse &&
        !lastSearchError.isEmpty()) {
        throw std::runtime_error(lastSearchError.toStdString());
    }

    const QList<DanmakuMatchCandidate> deduplicated =
        deduplicateCandidates(allCandidates);
    qDebug().noquote()
        << "[Danmaku][DandanPlay] Search finished"
        << "| mediaId:" << context.mediaId
        << "| rawCount:" << allCandidates.size()
        << "| deduplicatedCount:" << deduplicated.size();
    co_return deduplicated;
}

QCoro::Task<QList<DanmakuComment>> DandanplayProvider::fetchComments(
    DanmakuMatchCandidate candidate,
    DanmakuProviderConfig config) const
{
    QList<DanmakuComment> comments;
    if (!m_networkManager || !candidate.isValid()) {
        co_return comments;
    }

    const QString apiPath =
        QStringLiteral("/api/v2/comment/%1").arg(candidate.targetId);
    ensureOfficialAuthentication(config, apiPath);
    const QString url = buildUrl(
        config, apiPath,
        {{QStringLiteral("withRelated"),
          config.withRelated ? QStringLiteral("true")
                             : QStringLiteral("false")}});
    qDebug().noquote()
        << "[Danmaku][DandanPlay] Fetch comments"
        << "| targetId:" << candidate.targetId
        << "| withRelated:" << config.withRelated
        << "| url:" << url;
    const QJsonObject response =
        co_await m_networkManager->get(url, buildHeaders(config, apiPath));
    comments = parseCommentsResponse(response);
    qDebug().noquote()
        << "[Danmaku][DandanPlay] Fetch comments result"
        << "| targetId:" << candidate.targetId
        << "| count:" << comments.size();
    co_return comments;
}
