#include "danmuapiprovider.h"

#include <QColor>
#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <cmath>
#include <exception>
#include <stdexcept>
#include <utility>

namespace {

constexpr int kDanmuApiRequestTimeoutMs = 20000;

struct SearchHint {
    QString subject;
    int seasonNumber = -1;
    int episodeNumber = -1;
};

NetworkRequestOptions requestOptions()
{
    NetworkRequestOptions options;
    options.timeoutMs = kDanmuApiRequestTimeoutMs;
    return options;
}

QMap<QString, QString> requestHeaders()
{
    return {{QStringLiteral("Accept"), QStringLiteral("application/json")},
            {QStringLiteral("User-Agent"),
             QStringLiteral("qEmby/1.0 (Danmaku)")}};
}

QString stringField(const QJsonObject &object,
                    std::initializer_list<const char *> keys)
{
    for (const char *key : keys) {
        const QString value =
            object.value(QLatin1String(key)).toVariant().toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }
    return {};
}

int intField(const QJsonObject &object,
             std::initializer_list<const char *> keys,
             int fallback = -1)
{
    for (const char *key : keys) {
        const QJsonValue value = object.value(QLatin1String(key));
        if (!value.isUndefined() && !value.isNull()) {
            bool ok = false;
            const int parsed = value.toVariant().toString().toInt(&ok);
            if (ok) {
                return parsed;
            }
        }
    }
    return fallback;
}

void ensureSuccessfulResponse(const QJsonObject &response,
                              const QString &operation)
{
    if (!response.contains(QStringLiteral("success")) ||
        response.value(QStringLiteral("success")).toBool(true)) {
        return;
    }
    const QString message = stringField(
        response, {"errorMessage", "message", "error"});
    throw std::runtime_error(
        QStringLiteral("%1: %2")
            .arg(operation,
                 message.isEmpty() ? QStringLiteral("request rejected")
                                   : message)
            .toStdString());
}

QString buildUrl(const DanmakuProviderConfig &config,
                 const QString &apiPath,
                 const QList<QPair<QString, QString>> &queryItems = {})
{
    QUrl url = QUrl::fromUserInput(config.baseUrl.trimmed());
    QString path = url.path();
    while (path.endsWith('/') && path.size() > 1) {
        path.chop(1);
    }

    const QString token = config.accessToken.trimmed();
    if (!token.isEmpty()) {
        const QString lastSegment = path.section('/', -1);
        if (lastSegment != token) {
            if (!path.endsWith('/')) {
                path.append('/');
            }
            path.append(token);
        }
    }

    if (!path.endsWith('/')) {
        path.append('/');
    }
    path.append(apiPath.startsWith('/') ? apiPath.mid(1) : apiPath);
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

QString normalizedTitle(QString value)
{
    value = value.trimmed().toLower();
    value.remove(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N}]+)")));
    return value;
}

double titleScore(const QString &lhs, const QString &rhs)
{
    const QString left = normalizedTitle(lhs);
    const QString right = normalizedTitle(rhs);
    if (left.isEmpty() || right.isEmpty()) {
        return 0.0;
    }
    if (left == right) {
        return 1.0;
    }
    if (left.contains(right) || right.contains(left)) {
        return 0.82;
    }
    if (left.size() < 2 || right.size() < 2) {
        return 0.0;
    }

    QSet<QString> leftPairs;
    QSet<QString> rightPairs;
    for (int i = 0; i + 1 < left.size(); ++i) {
        leftPairs.insert(left.mid(i, 2));
    }
    for (int i = 0; i + 1 < right.size(); ++i) {
        rightPairs.insert(right.mid(i, 2));
    }
    int common = 0;
    for (const QString &pair : std::as_const(leftPairs)) {
        if (rightPairs.contains(pair)) {
            ++common;
        }
    }
    return (2.0 * common) / (leftPairs.size() + rightPairs.size());
}

int extractSeasonNumber(const QString &text)
{
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(
            QStringLiteral(R"((?:^|[^A-Za-z0-9])S\s*0*(\d{1,2})(?:[^A-Za-z0-9]|$))"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"(第\s*0*(\d{1,2})\s*[季部期])")),
        QRegularExpression(
            QStringLiteral(R"((?:Season\s*|)(\d{1,2})(?:st|nd|rd|th)?\s*(?:Season|期))"),
            QRegularExpression::CaseInsensitiveOption)};
    for (const QRegularExpression &pattern : patterns) {
        const QRegularExpressionMatch match = pattern.match(text);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
    }
    return 0;
}

int extractEpisodeNumber(const QString &text)
{
    static const QList<QRegularExpression> patterns = {
        QRegularExpression(
            QStringLiteral(R"((?:^|[^A-Za-z0-9])S\s*0*\d{1,2}\s*E\s*0*(\d{1,4})(?:[^A-Za-z0-9]|$))"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"(第\s*0*(\d{1,4})\s*[话話集期])")),
        QRegularExpression(
            QStringLiteral(R"((?:^|[^A-Za-z0-9])(?:EP?|Episode)\s*[._-]?\s*0*(\d{1,4})(?:[^A-Za-z0-9]|$))"),
            QRegularExpression::CaseInsensitiveOption)};
    for (const QRegularExpression &pattern : patterns) {
        const QRegularExpressionMatch match = pattern.match(text);
        if (match.hasMatch()) {
            return match.captured(1).toInt();
        }
    }
    bool ok = false;
    const int number = text.trimmed().toInt(&ok);
    return ok ? number : 0;
}

SearchHint parseSearchHint(const QString &input,
                           const DanmakuMediaContext &context)
{
    SearchHint hint;
    hint.subject = input.trimmed();
    hint.seasonNumber = context.seasonNumber;
    hint.episodeNumber = context.episodeNumber;

    static const QList<QRegularExpression> patterns = {
        QRegularExpression(
            QStringLiteral(R"((?:^|[\s._-])S\s*0*(\d{1,2})\s*E\s*0*(\d{1,4})(?=$|[\s._-]))"),
            QRegularExpression::CaseInsensitiveOption),
        QRegularExpression(
            QStringLiteral(R"(第\s*0*(\d{1,2})\s*季\s*第?\s*0*(\d{1,4})\s*[话話集期])"))};
    for (const QRegularExpression &pattern : patterns) {
        const QRegularExpressionMatch match = pattern.match(hint.subject);
        if (!match.hasMatch()) {
            continue;
        }
        hint.seasonNumber = match.captured(1).toInt();
        hint.episodeNumber = match.captured(2).toInt();
        hint.subject.remove(match.capturedStart(), match.capturedLength());
        hint.subject = hint.subject.trimmed();
        break;
    }
    if (hint.subject.isEmpty()) {
        hint.subject = context.isEpisode() ? context.seriesName.trimmed()
                                           : context.title.trimmed();
    }
    return hint;
}

double candidateScore(const DanmakuMediaContext &context,
                      const DanmakuMatchCandidate &candidate,
                      const QString &query)
{
    const QString subject =
        context.isEpisode() ? context.seriesName : context.title;
    const QString candidateSubject = candidate.subtitle.isEmpty()
                                         ? candidate.title
                                         : candidate.subtitle;
    double score = titleScore(subject, candidateSubject) * 55.0;
    score += titleScore(query, candidateSubject) * 20.0;
    score += titleScore(context.title, candidate.title) * 8.0;
    if (context.isEpisode() && context.episodeNumber > 0 &&
        candidate.episodeNumber == context.episodeNumber) {
        score += 24.0;
    }
    if (context.isEpisode() && context.seasonNumber > 0) {
        if (candidate.seasonNumber == context.seasonNumber) {
            score += 24.0;
        } else if (candidate.seasonNumber > 0) {
            score -= 35.0;
        } else if (context.seasonNumber == 1) {
            score += 5.0;
        }
    }
    return score;
}

QList<DanmakuMatchCandidate> parseEpisodeSearchResponse(
    const QJsonObject &response,
    const DanmakuMediaContext &context,
    const QString &query,
    int requestedEpisode)
{
    QList<DanmakuMatchCandidate> candidates;
    const QJsonArray animes = response.value(QStringLiteral("animes")).toArray();
    for (const QJsonValue &animeValue : animes) {
        const QJsonObject anime = animeValue.toObject();
        const QString animeTitle =
            stringField(anime, {"animeTitle", "title", "name"});
        const int animeSeason = extractSeasonNumber(animeTitle);
        const QJsonArray episodes =
            anime.value(QStringLiteral("episodes")).toArray();
        for (const QJsonValue &episodeValue : episodes) {
            const QJsonObject episode = episodeValue.toObject();
            DanmakuMatchCandidate candidate;
            candidate.provider = QStringLiteral("danmu_api");
            candidate.targetId =
                stringField(episode, {"episodeId", "id", "episodeID"});
            candidate.title = stringField(
                episode, {"episodeTitle", "title", "name"});
            candidate.subtitle = animeTitle;
            candidate.seasonNumber = animeSeason;
            candidate.episodeNumber = intField(
                episode, {"episodeNumber", "episode", "sort"}, -1);
            if (candidate.episodeNumber <= 0) {
                candidate.episodeNumber = extractEpisodeNumber(candidate.title);
            }
            if (candidate.episodeNumber <= 0 && requestedEpisode > 0) {
                candidate.episodeNumber = requestedEpisode;
            }
            candidate.matchReason = QStringLiteral("search");
            candidate.score = candidateScore(context, candidate, query);
            if (candidate.isValid()) {
                candidates.append(candidate);
            }
        }
    }
    return candidates;
}

QList<DanmakuMatchCandidate> parseMatchResponse(
    const QJsonObject &response,
    const DanmakuMediaContext &context,
    const QString &fileName)
{
    QList<DanmakuMatchCandidate> candidates;
    const bool matched = response.value(QStringLiteral("isMatched")).toBool();
    const QJsonArray matches = response.value(QStringLiteral("matches")).toArray();
    for (const QJsonValue &value : matches) {
        const QJsonObject match = value.toObject();
        DanmakuMatchCandidate candidate;
        candidate.provider = QStringLiteral("danmu_api");
        candidate.targetId =
            stringField(match, {"episodeId", "id", "episodeID"});
        candidate.title = stringField(
            match, {"episodeTitle", "title", "name"});
        candidate.subtitle = stringField(
            match, {"animeTitle", "animeName"});
        candidate.seasonNumber = extractSeasonNumber(candidate.subtitle);
        candidate.episodeNumber = extractEpisodeNumber(candidate.title);
        candidate.matchReason = matched ? QStringLiteral("filename")
                                        : QStringLiteral("match-candidate");
        candidate.score = candidateScore(context, candidate, fileName);
        if (matched) {
            candidate.score += 10.0;
        }
        if (candidate.isValid()) {
            candidates.append(candidate);
        }
    }
    return candidates;
}

QList<DanmakuMatchCandidate> deduplicate(
    const QList<DanmakuMatchCandidate> &input)
{
    QHash<QString, DanmakuMatchCandidate> best;
    for (const DanmakuMatchCandidate &candidate : input) {
        const auto existing = best.constFind(candidate.targetId);
        if (existing == best.constEnd() || candidate.score > existing->score) {
            best.insert(candidate.targetId, candidate);
        }
    }
    QList<DanmakuMatchCandidate> result = best.values();
    std::sort(result.begin(), result.end(),
              [](const DanmakuMatchCandidate &left,
                 const DanmakuMatchCandidate &right) {
                  return left.score > right.score;
              });
    return result;
}

QColor parseColor(const QString &value)
{
    bool ok = false;
    const uint rgb = value.toUInt(&ok, 10);
    if (ok) {
        return QColor::fromRgb(rgb);
    }
    const QColor color(value);
    return color.isValid() ? color : QColor(Qt::white);
}

QList<DanmakuComment> parseComments(const QJsonObject &response)
{
    QList<DanmakuComment> comments;
    const QJsonArray array = response.value(QStringLiteral("comments")).toArray();
    comments.reserve(array.size());
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        DanmakuComment comment;
        comment.text = stringField(object, {"m", "text", "content"});
        const QString p = stringField(object, {"p"});
        if (!p.isEmpty()) {
            const QStringList parts = p.split(',', Qt::KeepEmptyParts);
            if (!parts.isEmpty()) {
                comment.timeMs =
                    static_cast<qint64>(parts.at(0).toDouble() * 1000.0);
            }
            if (parts.size() > 1) {
                comment.mode = parts.at(1).toInt();
            }
            if (parts.size() > 2) {
                comment.color = parseColor(parts.at(2));
            }
            if (parts.size() > 3) {
                comment.sender = parts.at(3).trimmed();
            }
        } else {
            const double rawTime =
                object.value(QStringLiteral("time")).toVariant().toDouble();
            if (rawTime > 0.0 && rawTime < 1000.0) {
                comment.timeMs = static_cast<qint64>(rawTime * 1000.0);
            } else {
                comment.timeMs = static_cast<qint64>(rawTime);
            }
            comment.mode = intField(object, {"mode", "positionType"}, 1);
            comment.color = parseColor(
                object.value(QStringLiteral("color")).toVariant().toString());
        }
        if (comment.isValid()) {
            comments.append(comment);
        }
    }
    return comments;
}

} 

DanmuApiProvider::DanmuApiProvider(NetworkManager *networkManager)
    : m_networkManager(networkManager)
{
}

QCoro::Task<QList<DanmakuMatchCandidate>> DanmuApiProvider::searchCandidates(
    DanmakuMediaContext context,
    DanmakuProviderConfig config,
    QString manualKeyword) const
{
    QList<DanmakuMatchCandidate> allCandidates;
    if (!m_networkManager || config.baseUrl.trimmed().isEmpty()) {
        co_return allCandidates;
    }

    const bool manual = !manualKeyword.trimmed().isEmpty();
    const SearchHint hint = parseSearchHint(
        manual ? manualKeyword
               : (context.isEpisode() ? context.seriesName : context.title),
        context);
    DanmakuMediaContext searchContext = context;
    if (searchContext.isEpisode()) {
        searchContext.seriesName = hint.subject;
        searchContext.seasonNumber = hint.seasonNumber;
        searchContext.episodeNumber = hint.episodeNumber;
    } else if (manual) {
        searchContext.title = hint.subject;
        searchContext.originalTitle.clear();
    }

    QString fileName = context.fileName.trimmed();
    if (fileName.isEmpty() && !context.path.trimmed().isEmpty()) {
        fileName = QFileInfo(context.path.trimmed()).completeBaseName().trimmed();
    }
    if (!manual && !fileName.isEmpty()) {
        try {
            const QString path = QStringLiteral("/api/v2/match");
            const QString url = buildUrl(config, path);
            const QMap<QString, QString> headers = requestHeaders();
            const QJsonObject payload{
                {QStringLiteral("fileName"), fileName}};
            const NetworkRequestOptions options = requestOptions();
            const QJsonObject response = co_await m_networkManager->post(
                url, headers, payload, options);
            ensureSuccessfulResponse(response, QStringLiteral("match"));
            const QList<DanmakuMatchCandidate> matchCandidates =
                parseMatchResponse(response, context, fileName);
            allCandidates.append(matchCandidates);
            qDebug().noquote()
                << "[Danmaku][DanmuApi] Filename match"
                << "| endpointId:" << config.endpointId
                << "| mediaId:" << context.mediaId
                << "| matched:"
                << response.value(QStringLiteral("isMatched")).toBool()
                << "| count:" << matchCandidates.size();
            const double directMatchThreshold =
                context.isEpisode() ? 72.0 : 62.0;
            if (response.value(QStringLiteral("isMatched")).toBool() &&
                !matchCandidates.isEmpty() &&
                matchCandidates.first().score >= directMatchThreshold) {
                co_return deduplicate(allCandidates);
            }
        } catch (const std::exception &e) {
            qWarning().noquote()
                << "[Danmaku][DanmuApi] Filename match failed"
                << "| endpointId:" << config.endpointId
                << "| mediaId:" << context.mediaId
                << "| error:" << e.what();
        }
    }

    QString querySubject = hint.subject;
    if (searchContext.isEpisode() &&
        (hint.seasonNumber == 0 || hint.seasonNumber > 1) &&
        extractSeasonNumber(querySubject) <= 0) {
        querySubject += QStringLiteral(" S%1").arg(hint.seasonNumber, 2, 10,
                                                   QChar('0'));
    }
    QString episodeParameter;
    if (searchContext.isEpisode() && hint.episodeNumber > 0) {
        episodeParameter = QString::number(hint.episodeNumber);
    } else if (!searchContext.isEpisode()) {
        episodeParameter = QStringLiteral("movie");
    }

    const QString path = QStringLiteral("/api/v2/search/episodes");
    QJsonObject response;
    try {
        const QList<QPair<QString, QString>> queryItems{
            {QStringLiteral("anime"), querySubject},
            {QStringLiteral("episode"), episodeParameter}};
        const QString url = buildUrl(config, path, queryItems);
        const QMap<QString, QString> headers = requestHeaders();
        const NetworkRequestOptions options = requestOptions();
        response = co_await m_networkManager->get(
            url, headers, options);
        ensureSuccessfulResponse(response, QStringLiteral("episode search"));
    } catch (const std::exception &e) {
        if (allCandidates.isEmpty()) {
            throw;
        }
        qWarning().noquote()
            << "[Danmaku][DanmuApi] Metadata search failed, keeping filename candidates"
            << "| endpointId:" << config.endpointId
            << "| mediaId:" << context.mediaId
            << "| error:" << e.what();
        co_return deduplicate(allCandidates);
    }
    QList<DanmakuMatchCandidate> searchCandidates = parseEpisodeSearchResponse(
        response, searchContext, hint.subject, hint.episodeNumber);
    searchCandidates.erase(
        std::remove_if(
            searchCandidates.begin(), searchCandidates.end(),
            [&searchContext](const DanmakuMatchCandidate &candidate) {
                if (!searchContext.isEpisode()) {
                    return false;
                }
                if (searchContext.episodeNumber > 0 &&
                    candidate.episodeNumber > 0 &&
                    candidate.episodeNumber != searchContext.episodeNumber) {
                    return true;
                }
                if (searchContext.seasonNumber > 0 &&
                    candidate.seasonNumber > 0 &&
                    candidate.seasonNumber != searchContext.seasonNumber) {
                    return true;
                }
                
                
                
                return false;
            }),
        searchCandidates.end());
    allCandidates.append(searchCandidates);

    qDebug().noquote()
        << "[Danmaku][DanmuApi] Metadata search"
        << "| endpointId:" << config.endpointId
        << "| mediaId:" << context.mediaId
        << "| season:" << hint.seasonNumber
        << "| episode:" << hint.episodeNumber
        << "| movieMode:" << !searchContext.isEpisode()
        << "| count:" << searchCandidates.size();
    co_return deduplicate(allCandidates);
}

QCoro::Task<QList<DanmakuComment>> DanmuApiProvider::fetchComments(
    DanmakuMatchCandidate candidate,
    DanmakuProviderConfig config) const
{
    if (!m_networkManager || !candidate.isValid()) {
        co_return QList<DanmakuComment>{};
    }

    const QString path =
        QStringLiteral("/api/v2/comment/%1").arg(candidate.targetId);
    const QList<QPair<QString, QString>> queryItems{
        {QStringLiteral("format"), QStringLiteral("json")},
        {QStringLiteral("duration"), QStringLiteral("true")}};
    const QString url = buildUrl(config, path, queryItems);
    const QMap<QString, QString> headers = requestHeaders();
    const NetworkRequestOptions options = requestOptions();
    const QJsonObject response = co_await m_networkManager->get(
        url, headers, options);
    ensureSuccessfulResponse(response, QStringLiteral("comments"));
    const QList<DanmakuComment> comments = parseComments(response);
    qDebug().noquote()
        << "[Danmaku][DanmuApi] Comments fetched"
        << "| endpointId:" << config.endpointId
        << "| targetId:" << candidate.targetId
        << "| videoDuration:"
        << response.value(QStringLiteral("videoDuration")).toVariant().toLongLong()
        << "| count:" << comments.size();
    co_return comments;
}
