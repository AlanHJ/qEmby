#include "dandanplayprovider.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDebug>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QRegularExpression>
#include <QSet>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
#include <QUrlQuery>
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <limits>
#include <stdexcept>
#include <utility>
#include <QtConcurrent/QtConcurrent>
#include <qcorofuture.h>

namespace {

constexpr qint64 kDandanplayHashSampleBytes = 16LL * 1024 * 1024;
constexpr int kDanmakuRequestTimeoutMs = 10000;

struct MediaFingerprint {
    QString hash;
    qint64 fileSize = 0;

    bool isValid() const
    {
        return hash.size() == 32 && fileSize > 0;
    }
};

NetworkRequestOptions danmakuRequestOptions()
{
    NetworkRequestOptions options;
    options.timeoutMs = kDanmakuRequestTimeoutMs;
    return options;
}

QCoro::Task<MediaFingerprint> resolveMediaFingerprint(
    NetworkManager *networkManager,
    DanmakuMediaContext context)
{
    MediaFingerprint fingerprint;
    const QString providedHash = context.fileHash.trimmed().toLower();
    if (providedHash.size() == 32 && context.fileSize > 0) {
        fingerprint.hash = providedHash;
        fingerprint.fileSize = context.fileSize;
        co_return fingerprint;
    }

    const QString localPath = context.path.trimmed();
    const QFileInfo localInfo(localPath);
    if (!localPath.isEmpty() && localInfo.exists() && localInfo.isFile()) {
        auto hashFuture = QtConcurrent::run(
            [localPath]() -> MediaFingerprint {
                MediaFingerprint result;
                QFile file(localPath);
                if (!file.open(QIODevice::ReadOnly)) {
                    return result;
                }
                result.fileSize = file.size();
                const QByteArray sample = file.read(kDandanplayHashSampleBytes);
                const qint64 expectedBytes =
                    qMin(result.fileSize, kDandanplayHashSampleBytes);
                if (result.fileSize <= 0 || sample.size() != expectedBytes) {
                    return {};
                }
                result.hash = QString::fromLatin1(
                    QCryptographicHash::hash(sample, QCryptographicHash::Md5)
                        .toHex());
                return result;
            });
        fingerprint = co_await hashFuture;
        co_return fingerprint;
    }

    if (!networkManager || context.mediaUrl.trimmed().isEmpty() ||
        context.fileSize <= 0) {
        co_return fingerprint;
    }
    if (context.durationMs >= 10 * 60 * 1000 &&
        context.fileSize < kDandanplayHashSampleBytes) {
        qWarning().noquote()
            << "[Danmaku][DandanPlay] Ignoring implausible remote media size"
            << "| mediaId:" << context.mediaId
            << "| fileSize:" << context.fileSize
            << "| durationMs:" << context.durationMs;
        co_return fingerprint;
    }

    const qint64 expectedBytes =
        qMin(context.fileSize, kDandanplayHashSampleBytes);
    try {
        const QByteArray sample = co_await networkManager->getBytesLimited(
            context.mediaUrl, {}, expectedBytes, danmakuRequestOptions());
        if (sample.size() != expectedBytes) {
            co_return fingerprint;
        }
        fingerprint.fileSize = context.fileSize;
        fingerprint.hash = QString::fromLatin1(
            QCryptographicHash::hash(sample, QCryptographicHash::Md5).toHex());
    } catch (const std::exception &e) {
        qWarning().noquote()
            << "[Danmaku][DandanPlay] Media fingerprint unavailable"
            << "| mediaId:" << context.mediaId
            << "| expectedBytes:" << expectedBytes
            << "| error:" << e.what();
    }
    co_return fingerprint;
}

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

bool isExplicitlyNonAnime(const DanmakuMediaContext &context,
                          const DanmakuProviderConfig &config)
{
    if (config.contentScope.compare(QStringLiteral("anime"),
                                    Qt::CaseInsensitive) != 0 ||
        context.genres.isEmpty()) {
        return false;
    }
    for (const QString &genre : context.genres) {
        const QString normalized = genre.trimmed().toLower();
        if (normalized == QLatin1String("animation") ||
            normalized == QLatin1String("anime") ||
            normalized.contains(QStringLiteral("动画")) ||
            normalized.contains(QStringLiteral("動漫")) ||
            normalized.contains(QStringLiteral("アニメ"))) {
            return false;
        }
    }
    return true;
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

QList<NetworkJsonGetRequest> buildEpisodeSearchRequests(
    const DanmakuProviderConfig &config,
    const QStringList &keywords,
    const QString &episodeParameter,
    const QString &tmdbId,
    bool useV2Engine,
    bool isEpisode)
{
    const QString apiPath = QStringLiteral("/api/v2/search/episodes");
    QList<NetworkJsonGetRequest> requests;
    requests.reserve(keywords.size());
    for (const QString &keyword : keywords) {
        QList<QPair<QString, QString>> queryItems = {
            {QStringLiteral("anime"), keyword},
            {QStringLiteral("v2"),
             useV2Engine ? QStringLiteral("true")
                         : QStringLiteral("false")}};
        if (!episodeParameter.isEmpty()) {
            queryItems.append(
                {QStringLiteral("episode"), episodeParameter});
        }
        if (!tmdbId.isEmpty()) {
            queryItems.append({QStringLiteral("tmdbId"), tmdbId});
            queryItems.append(
                {QStringLiteral("tmdbIdType"),
                 isEpisode ? QStringLiteral("0") : QStringLiteral("1")});
        }
        requests.append({buildUrl(config, apiPath, queryItems),
                         buildHeaders(config, apiPath),
                         danmakuRequestOptions()});
    }
    return requests;
}

QList<NetworkJsonGetRequest> buildAnimeSearchRequests(
    const DanmakuProviderConfig &config,
    const QStringList &keywords,
    bool useV2Engine)
{
    const QString apiPath = QStringLiteral("/api/v2/search/anime");
    QList<NetworkJsonGetRequest> requests;
    requests.reserve(keywords.size());
    for (const QString &keyword : keywords) {
        const QList<QPair<QString, QString>> queryItems = {
            {QStringLiteral("keyword"), keyword},
            {QStringLiteral("v2"),
             useV2Engine ? QStringLiteral("true")
                         : QStringLiteral("false")}};
        requests.append({buildUrl(config, apiPath, queryItems),
                         buildHeaders(config, apiPath),
                         danmakuRequestOptions()});
    }
    return requests;
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

    auto comparable = [](QString value) {
        value.remove(QRegularExpression(QStringLiteral(R"([^\p{L}\p{N}]+)")));
        return value;
    };
    const QString comparableLhs = comparable(
        cleanLhs.isEmpty() ? rawLhs : cleanLhs);
    const QString comparableRhs = comparable(
        cleanRhs.isEmpty() ? rawRhs : cleanRhs);
    if (comparableLhs.size() < 2 || comparableRhs.size() < 2) {
        return comparableLhs == comparableRhs ? 1.0 : 0.0;
    }

    auto bigrams = [](const QString &value) {
        QSet<QString> result;
        for (int i = 0; i + 1 < value.size(); ++i) {
            result.insert(value.mid(i, 2));
        }
        return result;
    };
    const QSet<QString> lhsBigrams = bigrams(comparableLhs);
    const QSet<QString> rhsBigrams = bigrams(comparableRhs);
    int common = 0;
    for (const QString &gram : lhsBigrams) {
        if (rhsBigrams.contains(gram)) {
            ++common;
        }
    }
    const double dice =
        (2.0 * common) / (lhsBigrams.size() + rhsBigrams.size());
    return qBound(0.0, dice * 0.55, 0.55);
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


int chineseDigitsToInt(const QString &digits)
{
    if (digits.isEmpty()) {
        return 0;
    }

    bool ok = false;
    const int direct = digits.toInt(&ok);
    if (ok && direct > 0) {
        return direct;
    }

    static const QHash<QChar, int> digitMap = {
        {QChar(0x4E00), 1}, {QChar(0x4E8C), 2}, {QChar(0x4E09), 3},
        {QChar(0x56DB), 4}, {QChar(0x4E94), 5}, {QChar(0x516D), 6},
        {QChar(0x4E03), 7}, {QChar(0x516B), 8}, {QChar(0x4E5D), 9}};
    static const QChar shi(0x5341); 

    int total = 0;
    int currentDigit = 0;
    for (const QChar ch : digits) {
        if (ch == shi) {
            total += currentDigit == 0 ? 10 : currentDigit * 10;
            currentDigit = 0;
        } else if (digitMap.contains(ch)) {
            currentDigit = digitMap.value(ch);
        }
    }
    total += currentDigit;
    return total;
}


QString chineseOrdinalString(int value)
{
    if (value <= 0) {
        return {};
    }

    static const QChar digits[] = {
        QChar(0x4E00), QChar(0x4E8C), QChar(0x4E09),
        QChar(0x56DB), QChar(0x4E94), QChar(0x516D),
        QChar(0x4E03), QChar(0x516B), QChar(0x4E5D)};
    static const QChar shi(0x5341);

    if (value < 10) {
        return QString(digits[value - 1]);
    }
    if (value == 10) {
        return QString(shi);
    }
    if (value < 20) {
        return QString(shi) + digits[value - 10 - 1];
    }
    if (value < 100) {
        const int tens = value / 10;
        const int ones = value % 10;
        QString result = QString(digits[tens - 1]) + shi;
        if (ones > 0) {
            result += digits[ones - 1];
        }
        return result;
    }
    return QString::number(value);
}





int extractSeasonNumber(const QString &title)
{
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    static const QRegularExpression cnPattern(
        QStringLiteral(R"(第\s*([\x{4E00}-\x{9FFF}0-9]+)\s*[季部])"));
    QRegularExpressionMatch match = cnPattern.match(trimmed);
    if (match.hasMatch()) {
        const int parsed = chineseDigitsToInt(match.captured(1).trimmed());
        if (parsed > 0) {
            return parsed;
        }
    }

    static const QRegularExpression enPattern(
        QStringLiteral(R"((?:^|[^A-Za-z])(?:Season|S)\s*0*(\d{1,2})(?:[^A-Za-z0-9]|$))"),
        QRegularExpression::CaseInsensitiveOption);
    match = enPattern.match(trimmed);
    if (match.hasMatch()) {
        const int parsed = match.captured(1).toInt();
        if (parsed > 0) {
            return parsed;
        }
    }

    static const QRegularExpression ordinalSeasonPattern(
        QStringLiteral(R"((?:^|[^A-Za-z0-9])0*(\d{1,2})(?:st|nd|rd|th)?\s*(?:Season|期)(?:[^A-Za-z0-9]|$))"),
        QRegularExpression::CaseInsensitiveOption);
    match = ordinalSeasonPattern.match(trimmed);
    if (match.hasMatch()) {
        const int parsed = match.captured(1).toInt();
        if (parsed > 0) {
            return parsed;
        }
    }

    static const QRegularExpression romanPattern(
        QStringLiteral(R"((?:^|\s)(VIII|VII|VI|IV|IX|III|II|V|X)\s*$)"));
    match = romanPattern.match(trimmed);
    if (match.hasMatch()) {
        const QString roman = match.captured(1).toUpper();
        static const QHash<QString, int> romanMap = {
            {QStringLiteral("II"), 2},   {QStringLiteral("III"), 3},
            {QStringLiteral("IV"), 4},   {QStringLiteral("V"), 5},
            {QStringLiteral("VI"), 6},   {QStringLiteral("VII"), 7},
            {QStringLiteral("VIII"), 8}, {QStringLiteral("IX"), 9},
            {QStringLiteral("X"), 10}};
        const int parsed = romanMap.value(roman, 0);
        if (parsed > 1) {
            return parsed;
        }
    }

    return 0;
}

int extractEpisodeNumber(const QString &title)
{
    const QString trimmed = title.trimmed();
    if (trimmed.isEmpty()) {
        return 0;
    }

    static const QRegularExpression seasonEpisodePattern(
        QStringLiteral(R"((?:^|[^A-Za-z0-9])S\s*0*\d{1,2}\s*E\s*0*(\d{1,4})(?:[^A-Za-z0-9]|$))"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = seasonEpisodePattern.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }

    static const QRegularExpression chineseEpisodePattern(
        QStringLiteral(R"(第\s*0*(\d{1,4})\s*[话話集期])"));
    match = chineseEpisodePattern.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }

    static const QRegularExpression englishEpisodePattern(
        QStringLiteral(R"((?:^|[^A-Za-z0-9])(?:EP?|Episode)\s*[._-]?\s*0*(\d{1,4})(?:[^A-Za-z0-9]|$))"),
        QRegularExpression::CaseInsensitiveOption);
    match = englishEpisodePattern.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(1).toInt();
    }

    static const QRegularExpression numberOnlyPattern(
        QStringLiteral(R"(^\s*0*(\d{1,4})(?:\s|$|[._-])?)"));
    match = numberOnlyPattern.match(trimmed);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

struct ManualSearchHint {
    QString keyword;
    int seasonNumber = -1;
    int episodeNumber = -1;
    bool hasExplicitEpisode = false;
};

ManualSearchHint parseManualSearchHint(const QString &input)
{
    ManualSearchHint hint;
    hint.keyword = input.trimmed();

    static const QRegularExpression seasonEpisodePattern(
        QStringLiteral(R"((?:^|[\s._-])S\s*0*(\d{1,2})\s*E\s*0*(\d{1,4})(?=$|[\s._-]))"),
        QRegularExpression::CaseInsensitiveOption);
    QRegularExpressionMatch match = seasonEpisodePattern.match(hint.keyword);
    if (match.hasMatch()) {
        hint.seasonNumber = match.captured(1).toInt();
        hint.episodeNumber = match.captured(2).toInt();
        hint.hasExplicitEpisode = hint.episodeNumber > 0;
        hint.keyword.remove(match.capturedStart(), match.capturedLength());
    } else {
        static const QRegularExpression chineseSeasonEpisodePattern(
            QStringLiteral(R"(第\s*0*(\d{1,2})\s*季\s*第?\s*0*(\d{1,4})\s*[话話集期])"));
        match = chineseSeasonEpisodePattern.match(hint.keyword);
        if (match.hasMatch()) {
            hint.seasonNumber = match.captured(1).toInt();
            hint.episodeNumber = match.captured(2).toInt();
            hint.hasExplicitEpisode = hint.episodeNumber > 0;
            hint.keyword.remove(match.capturedStart(), match.capturedLength());
        }
    }

    hint.keyword.replace(QRegularExpression(QStringLiteral(R"([\s._-]{2,})")),
                         QStringLiteral(" "));
    hint.keyword = hint.keyword.trimmed();
    return hint;
}

bool isClearlyEpisodicAnimeType(const QString &type)
{
    const QString normalized = type.trimmed().toLower();
    return normalized == QLatin1String("tvseries") ||
           normalized == QLatin1String("jpdrama") ||
           normalized == QLatin1String("tmdbtv");
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
    score += titleScore(queryKeyword, candidate.displayText()) * 6.0;

    if (context.isEpisode() && context.episodeNumber > 0 &&
        candidate.episodeNumber > 0 &&
        context.episodeNumber == candidate.episodeNumber) {
        score += 24.0;
    }

    
    
    
    if (context.isEpisode() && context.seasonNumber > 0) {
        const int candidateSeason = extractSeasonNumber(candidateSubject);
        if (candidateSeason > 0) {
            if (candidateSeason == context.seasonNumber) {
                score += 30.0; 
            } else {
                score -= 30.0; 
            }
        } else if (context.seasonNumber == 1) {
            
            score += 6.0;
        } else {
            
            score -= 8.0;
        }
    }

    if (context.durationMs > 0 && candidate.durationMs > 0) {
        const qint64 diff = std::llabs(context.durationMs - candidate.durationMs);
        if (diff <= 30 * 1000) {
            score += 12.0;
        } else if (diff <= 90 * 1000) {
            score += 6.0;
        } else if (diff > std::max<qint64>(5 * 60 * 1000,
                                           context.durationMs * 18 / 100)) {
            score -= 22.0;
        }
    }

    if (candidate.commentCount > 0) {
        score += std::min(4.0, candidate.commentCount / 200.0);
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
            } else {
                score -= 20.0;
            }
        }
    }

    return score;
}

qint64 normalizeCandidateDurationMs(qint64 value, qint64 expectedDurationMs)
{
    if (value <= 0) {
        return 0;
    }
    if (value <= 24 * 60 * 60) {
        const qint64 secondsValue = value * 1000;
        if (expectedDurationMs <= 0 ||
            std::llabs(secondsValue - expectedDurationMs) <
                std::llabs(value - expectedDurationMs)) {
            return secondsValue;
        }
    }
    return value;
}

QList<DanmakuMatchCandidate> parseSearchResponse(
    const QJsonObject &response,
    const DanmakuMediaContext &context,
    const QString &queryKeyword,
    int requestedEpisodeNumber,
    bool excludeClearlyEpisodicWorks)
{
    QList<DanmakuMatchCandidate> candidates;
    auto parseArray = [&](const QJsonArray &animeArray) {
        for (const QJsonValue &animeValue : animeArray) {
            const QJsonObject animeObj = animeValue.toObject();
            const QString animeType =
                stringField(animeObj, {"type", "animeType"});
            if (excludeClearlyEpisodicWorks &&
                isClearlyEpisodicAnimeType(animeType)) {
                continue;
            }
            const QString animeTitle = firstNonEmpty(
                {stringField(animeObj, {"animeTitle", "title", "name"}),
                 stringField(animeObj, {"animeTitleCN", "animeTitleJP"})});

            QJsonArray episodes = animeObj.value(QStringLiteral("episodes")).toArray();
            if (episodes.isEmpty() &&
                !animeObj.value(QStringLiteral("episodeId")).isUndefined()) {
                episodes.append(animeObj);
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
                candidate.seasonNumber = extractSeasonNumber(animeTitle);
                candidate.episodeNumber = intField(
                    episodeObj, {"episodeNumber", "episode", "sort"}, -1);
                if (candidate.episodeNumber <= 0) {
                    candidate.episodeNumber =
                        extractEpisodeNumber(candidate.title);
                }
                if (candidate.episodeNumber <= 0 &&
                    requestedEpisodeNumber > 0) {
                    
                    
                    
                    candidate.episodeNumber = requestedEpisodeNumber;
                }
                candidate.durationMs = longField(
                    episodeObj, {"durationMs", "duration", "videoDuration"}, 0);
                candidate.durationMs = normalizeCandidateDurationMs(
                    candidate.durationMs, context.durationMs);
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

struct EpisodeSearchBatchOutcome {
    QList<DanmakuMatchCandidate> candidates;
    QString lastError;
    bool hadSuccessfulResponse = false;
};

EpisodeSearchBatchOutcome processEpisodeSearchResponses(
    const QList<NetworkJsonResult> &responses,
    const QStringList &requestKeywords,
    DanmakuMediaContext context,
    int requestedEpisodeNumber,
    QString stage,
    bool tmdbConstrained,
    bool excludeClearlyEpisodicWorks)
{
    EpisodeSearchBatchOutcome outcome;
    for (int i = 0; i < responses.size(); ++i) {
        const QString keyword = requestKeywords.value(i);
        const NetworkJsonResult &response = responses.at(i);
        if (!response.succeeded()) {
            outcome.lastError = response.errorMessage;
            qWarning().noquote()
                << "[Danmaku][DandanPlay] Episode search failed"
                << "| stage:" << stage
                << "| keyword:" << keyword
                << "| error:" << response.errorMessage;
            continue;
        }

        outcome.hadSuccessfulResponse = true;
        QList<DanmakuMatchCandidate> candidates =
            parseSearchResponse(response.object, context, keyword,
                                requestedEpisodeNumber,
                                excludeClearlyEpisodicWorks);
        if (context.isEpisode()) {
            candidates.erase(
                std::remove_if(
                    candidates.begin(), candidates.end(),
                    [&context](const DanmakuMatchCandidate &candidate) {
                        if (context.episodeNumber > 0 &&
                            candidate.episodeNumber > 0 &&
                            candidate.episodeNumber != context.episodeNumber) {
                            return true;
                        }
                        if (context.seasonNumber > 0 &&
                            candidate.seasonNumber > 0 &&
                            candidate.seasonNumber != context.seasonNumber) {
                            return true;
                        }
                        return context.seasonNumber > 1 &&
                               candidate.seasonNumber <= 0;
                    }),
                candidates.end());
        }
        if (tmdbConstrained) {
            for (DanmakuMatchCandidate &candidate : candidates) {
                candidate.matchReason = QStringLiteral("tmdb");
                candidate.score = qMax(candidate.score, 160.0);
            }
        }
        if (!candidates.isEmpty()) {
            outcome.candidates.append(candidates);
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Episode search hit"
                << "| stage:" << stage
                << "| keyword:" << keyword
                << "| count:" << candidates.size();
        }
        if (response.object.value(QStringLiteral("hasMore")).toBool()) {
            qWarning().noquote()
                << "[Danmaku][DandanPlay] Search result truncated by server"
                << "| stage:" << stage
                << "| keyword:" << keyword
                << "| returnedCount:" << candidates.size();
        }
    }
    return outcome;
}

QStringList parseAnimeSearchTitles(const QJsonObject &response,
                                   const QString &queryKeyword)
{
    QStringList titles;
    auto parseArray = [&titles, &queryKeyword](const QJsonArray &animeArray) {
        for (const QJsonValue &animeValue : animeArray) {
            const QJsonObject animeObj = animeValue.toObject();
            const QString title = firstNonEmpty(
                {stringField(animeObj, {"animeTitle", "title", "name"}),
                 stringField(animeObj, {"animeTitleCN", "animeTitleJP"})});
            if (title.isEmpty() || titles.contains(title)) {
                continue;
            }

            if (titleScore(queryKeyword, title) < 0.30) {
                continue;
            }
            titles.append(title);
            if (titles.size() >= 6) {
                break;
            }
        }
    };

    parseArray(response.value(QStringLiteral("animes")).toArray());
    if (titles.size() < 6) {
        parseArray(response.value(QStringLiteral("data")).toArray());
    }
    return titles;
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
    const QString &fileName,
    bool hashMatchRequest)
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
        candidate.seasonNumber = extractSeasonNumber(candidate.subtitle);
        candidate.episodeNumber =
            intField(matchObj, {"episodeNumber", "episode", "sort"}, -1);
        if (candidate.episodeNumber <= 0) {
            candidate.episodeNumber = extractEpisodeNumber(candidate.title);
        }
        candidate.durationMs = longField(
            matchObj, {"durationMs", "duration", "videoDuration"}, 0);
        candidate.durationMs = normalizeCandidateDurationMs(
            candidate.durationMs, context.durationMs);
        candidate.commentCount = intField(
            matchObj, {"commentCount", "comments", "danmakuCount"}, 0);

        if (candidate.title.isEmpty()) {
            candidate.title = candidate.subtitle;
        }

        candidate.score = computeScore(context, candidate, fileName);
        if (isMatched && hashMatchRequest) {
            candidate.score = qMax(candidate.score, 200.0);
            candidate.matchReason = QStringLiteral("hash");
        } else if (isMatched) {
            
            
            candidate.score += 8.0;
            candidate.matchReason = QStringLiteral("filename");
        } else {
            candidate.matchReason = QStringLiteral("match-candidate");
        }
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

QCoro::Task<DanmakuMediaContext> DandanplayProvider::enrichMediaFingerprint(
    DanmakuMediaContext context) const
{
    if (!context.fileHash.trimmed().isEmpty() && context.fileSize > 0) {
        co_return context;
    }
    const MediaFingerprint fingerprint =
        co_await resolveMediaFingerprint(m_networkManager, context);
    if (fingerprint.isValid()) {
        context.fileHash = fingerprint.hash;
        context.fileSize = fingerprint.fileSize;
    }
    co_return context;
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
    const bool isManualSearch = !manualKeyword.trimmed().isEmpty();
    const ManualSearchHint manualHint =
        isManualSearch ? parseManualSearchHint(manualKeyword)
                       : ManualSearchHint{};
    DanmakuMediaContext searchContext = context;
    if (isManualSearch && manualHint.hasExplicitEpisode) {
        searchContext.seasonNumber = manualHint.seasonNumber;
        searchContext.episodeNumber = manualHint.episodeNumber;
    }

    
    
    
    
    
    
    
    QStringList keywords;
    QString manualSubject;
    if (isManualSearch) {
        manualSubject = firstNonEmpty(
            {manualHint.keyword,
             searchContext.isEpisode() ? searchContext.seriesName
                                       : searchContext.title});
        
        
        
        if (searchContext.isEpisode()) {
            searchContext.seriesName = manualSubject;
        } else {
            searchContext.title = manualSubject;
        }
        searchContext.originalTitle.clear();
        if (searchContext.isEpisode() && searchContext.seasonNumber > 1 &&
            extractSeasonNumber(manualSubject) <= 0) {
            const QString ordinal =
                chineseOrdinalString(searchContext.seasonNumber);
            if (!ordinal.isEmpty()) {
                keywords << QStringLiteral("%1 第%2季")
                                .arg(manualSubject, ordinal);
            }
        }
        keywords << manualSubject;
    } else if (context.isEpisode()) {
        const QString trimmedSeries = context.seriesName.trimmed();
        const QString trimmedOriginal = context.originalTitle.trimmed();
        const QString trimmedTitle = context.title.trimmed();
        if (!trimmedSeries.isEmpty()) {
            
            if (context.seasonNumber > 1) {
                const QString ordinal = chineseOrdinalString(context.seasonNumber);
                if (!ordinal.isEmpty()) {
                    keywords << QStringLiteral("%1 第%2季")
                                    .arg(trimmedSeries, ordinal);
                }
            }
            keywords << trimmedSeries;
            if (!trimmedOriginal.isEmpty() && trimmedOriginal != trimmedSeries) {
                keywords << trimmedOriginal;
            }
        } else {
            
            keywords << trimmedTitle << trimmedOriginal;
        }
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

    const QString currentSubject =
        context.isEpisode() ? context.seriesName.trimmed()
                            : context.title.trimmed();
    const bool manualTargetsCurrentMedia =
        isManualSearch && !manualSubject.isEmpty() &&
        titleScore(manualSubject, currentSubject) >= 0.92;
    
    
    
    const QString tmdbId =
        (!isManualSearch || manualTargetsCurrentMedia)
            ? providerIdValue(context.providerIds,
                              {"Tmdb", "tmdb", "TMDb", "tmdbid"})
            : QString();
    const bool strongLookupOnly =
        manualKeyword.trimmed().isEmpty() &&
        isExplicitlyNonAnime(context, config);

    const QString filePath = context.path.trimmed();
    QString normalizedFilePath = filePath;
    const QUrl filePathUrl = QUrl::fromUserInput(normalizedFilePath);
    if (filePathUrl.scheme().compare(QStringLiteral("http"),
                                     Qt::CaseInsensitive) == 0 ||
        filePathUrl.scheme().compare(QStringLiteral("https"),
                                     Qt::CaseInsensitive) == 0) {
        normalizedFilePath = filePathUrl.path();
    }
    normalizedFilePath.replace(QLatin1Char('\\'), QLatin1Char('/'));
    const QString fileName = firstNonEmpty(
        {context.fileName, filePath.isEmpty()
                               ? QString()
                               : QFileInfo(normalizedFilePath)
                                     .completeBaseName()
                                     .trimmed()});

    qDebug().noquote()
        << "[Danmaku][DandanPlay] Search start"
        << "| media:" << context.displayTitle()
        << "| mediaId:" << context.mediaId
        << "| isEpisode:" << context.isEpisode()
        << "| seriesName:" << context.seriesName.trimmed()
        << "| seasonNumber:" << context.seasonNumber
        << "| episodeNumber:" << context.episodeNumber
        << "| requestedSeason:" << searchContext.seasonNumber
        << "| requestedEpisode:" << searchContext.episodeNumber
        << "| explicitManualEpisode:" << manualHint.hasExplicitEpisode
        << "| manualTargetsCurrentMedia:" << manualTargetsCurrentMedia
        << "| tmdbConstraint:" << !tmdbId.isEmpty()
        << "| contentScope:" << config.contentScope
        << "| fileName:" << fileName
        << "| keywords:" << keywords.join(QStringLiteral(" | "));

    MediaFingerprint fingerprint;
    if (manualKeyword.trimmed().isEmpty()) {
        fingerprint = co_await resolveMediaFingerprint(m_networkManager, context);
    }
    if (manualKeyword.trimmed().isEmpty() &&
        (!fileName.isEmpty() || fingerprint.isValid())) {
        try {
            const bool useHashMatch = fingerprint.isValid();
            const QString matchPath = QStringLiteral("/api/v2/match");
            ensureOfficialAuthentication(config, matchPath);
            QJsonObject payload;
            payload.insert(QStringLiteral("fileName"), fileName);
            payload.insert(QStringLiteral("fileHash"), fingerprint.hash);
            payload.insert(QStringLiteral("fileSize"), fingerprint.fileSize);
            payload.insert(
                QStringLiteral("videoDuration"),
                context.durationMs > 0
                    ? static_cast<int>(qMin<qint64>(
                          context.durationMs / 1000,
                          std::numeric_limits<int>::max()))
                    : 0);
            payload.insert(QStringLiteral("matchMode"),
                           useHashMatch
                               ? (fileName.isEmpty()
                                      ? QStringLiteral("hashOnly")
                                      : QStringLiteral("hashAndFileName"))
                               : QStringLiteral("fileNameOnly"));
            const QString matchUrl = buildUrl(config, matchPath, {});
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Match request"
                << "| fileName:" << fileName
                << "| hashAvailable:" << useHashMatch
                << "| fileSize:" << fingerprint.fileSize
                << "| url:" << matchUrl;
            const QJsonObject matchResponse = co_await m_networkManager->post(
                matchUrl, buildHeaders(config, matchPath), payload,
                danmakuRequestOptions());
            hadSuccessfulSearchResponse = true;
            const QList<DanmakuMatchCandidate> matchCandidates =
                parseMatchResponse(matchResponse, context, fileName,
                                   useHashMatch);
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Match result"
                << "| fileName:" << fileName
                << "| isMatched:"
                << matchResponse.value(QStringLiteral("isMatched")).toBool()
                << "| count:" << matchCandidates.size();
            if (!matchCandidates.isEmpty()) {
                if (matchCandidates.first().isHashMatch()) {
                    allCandidates.append(matchCandidates);
                    qDebug().noquote()
                        << "[Danmaku][DandanPlay] Verified hash match found,"
                           " skipping title searches"
                        << "| mediaId:" << context.mediaId
                        << "| targetId:" << matchCandidates.first().targetId;
                    co_return deduplicateCandidates(allCandidates);
                }
                if (!strongLookupOnly) {
                    allCandidates.append(matchCandidates);
                }
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

    if (strongLookupOnly && tmdbId.isEmpty()) {
        qDebug().noquote()
            << "[Danmaku][DandanPlay] Skipping fuzzy title search for non-anime media"
            << "| mediaId:" << context.mediaId;
        co_return deduplicateCandidates(allCandidates);
    }

    QString requestedEpisodeParameter;
    int requestedEpisodeNumber = -1;
    if ((searchContext.isEpisode() || manualHint.hasExplicitEpisode) &&
        searchContext.episodeNumber > 0) {
        requestedEpisodeNumber = searchContext.episodeNumber;
        requestedEpisodeParameter =
            searchContext.seasonNumber == 0
                ? QStringLiteral("S%1").arg(searchContext.episodeNumber)
                : QString::number(searchContext.episodeNumber);
    } else if (!isManualSearch && !searchContext.isEpisode()) {
        requestedEpisodeNumber = 1;
        requestedEpisodeParameter = QStringLiteral("1");
    }

    const bool excludeClearlyEpisodicWorks =
        !isManualSearch && !searchContext.isEpisode();

    bool titleSearchFoundCandidates = false;

    const QString episodeSearchPath =
        QStringLiteral("/api/v2/search/episodes");
    const QString animeSearchPath = QStringLiteral("/api/v2/search/anime");

    if (!keywords.isEmpty()) {
        ensureOfficialAuthentication(config, episodeSearchPath);
        const QList<NetworkJsonGetRequest> requests =
            buildEpisodeSearchRequests(
                config, keywords, requestedEpisodeParameter, tmdbId, true,
                context.isEpisode());
        qDebug().noquote()
            << "[Danmaku][DandanPlay] Parallel episode search"
            << "| stage: v2-direct"
            << "| requestCount:" << requests.size()
            << "| episodeParameter:" << requestedEpisodeParameter
            << "| tmdbConstraint:" << !tmdbId.isEmpty()
            << "| keywords:" << keywords.join(QStringLiteral(" | "));
        const QList<NetworkJsonResult> responses =
            co_await m_networkManager->getBatch(requests);
        const EpisodeSearchBatchOutcome outcome =
            processEpisodeSearchResponses(
                responses, keywords, searchContext, requestedEpisodeNumber,
                QStringLiteral("v2-direct"), !tmdbId.isEmpty(),
                excludeClearlyEpisodicWorks);
        hadSuccessfulSearchResponse |= outcome.hadSuccessfulResponse;
        if (!outcome.lastError.isEmpty()) {
            lastSearchError = outcome.lastError;
        }
        if (!outcome.candidates.isEmpty()) {
            titleSearchFoundCandidates = true;
            allCandidates.append(outcome.candidates);
        }
    }

    QStringList canonicalTitles;
    if (!titleSearchFoundCandidates && !keywords.isEmpty()) {
        ensureOfficialAuthentication(config, animeSearchPath);
        const QList<NetworkJsonGetRequest> animeRequests =
            buildAnimeSearchRequests(config, keywords, true);
        qDebug().noquote()
            << "[Danmaku][DandanPlay] Anime title discovery"
            << "| stage: v2-anime"
            << "| requestCount:" << animeRequests.size()
            << "| keywords:" << keywords.join(QStringLiteral(" | "));
        const QList<NetworkJsonResult> animeResponses =
            co_await m_networkManager->getBatch(animeRequests);
        for (int i = 0; i < animeResponses.size(); ++i) {
            const NetworkJsonResult &response = animeResponses.at(i);
            const QString keyword = keywords.value(i);
            if (!response.succeeded()) {
                lastSearchError = response.errorMessage;
                qWarning().noquote()
                    << "[Danmaku][DandanPlay] Anime title discovery failed"
                    << "| keyword:" << keyword
                    << "| error:" << response.errorMessage;
                continue;
            }
            hadSuccessfulSearchResponse = true;
            canonicalTitles.append(
                parseAnimeSearchTitles(response.object, keyword));
        }
        canonicalTitles.removeDuplicates();
        if (canonicalTitles.size() > 8) {
            canonicalTitles = canonicalTitles.mid(0, 8);
        }

        if (!canonicalTitles.isEmpty()) {
            const QList<NetworkJsonGetRequest> canonicalRequests =
                buildEpisodeSearchRequests(
                    config, canonicalTitles, requestedEpisodeParameter, {},
                    true, context.isEpisode());
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Canonical title episode search"
                << "| stage: v2-anime-episodes"
                << "| requestCount:" << canonicalRequests.size()
                << "| episodeParameter:" << requestedEpisodeParameter
                << "| titles:"
                << canonicalTitles.join(QStringLiteral(" | "));
            const QList<NetworkJsonResult> canonicalResponses =
                co_await m_networkManager->getBatch(canonicalRequests);
            const EpisodeSearchBatchOutcome outcome =
                processEpisodeSearchResponses(
                    canonicalResponses, canonicalTitles, searchContext,
                    requestedEpisodeNumber,
                    QStringLiteral("v2-anime-episodes"), false,
                    excludeClearlyEpisodicWorks);
            hadSuccessfulSearchResponse |= outcome.hadSuccessfulResponse;
            if (!outcome.lastError.isEmpty()) {
                lastSearchError = outcome.lastError;
            }
            if (!outcome.candidates.isEmpty()) {
                titleSearchFoundCandidates = true;
                allCandidates.append(outcome.candidates);
            }
        }
    }

    if (!titleSearchFoundCandidates && !keywords.isEmpty()) {
        QStringList legacyKeywords = keywords;
        legacyKeywords.append(canonicalTitles);
        legacyKeywords.removeDuplicates();
        if (legacyKeywords.size() > 8) {
            legacyKeywords = legacyKeywords.mid(0, 8);
        }
        const QList<NetworkJsonGetRequest> legacyRequests =
            buildEpisodeSearchRequests(
                config, legacyKeywords, requestedEpisodeParameter, {}, false,
                context.isEpisode());
        qDebug().noquote()
            << "[Danmaku][DandanPlay] Legacy episode search fallback"
            << "| stage: legacy-episodes"
            << "| requestCount:" << legacyRequests.size()
            << "| episodeParameter:" << requestedEpisodeParameter
            << "| keywords:"
            << legacyKeywords.join(QStringLiteral(" | "));
        const QList<NetworkJsonResult> legacyResponses =
            co_await m_networkManager->getBatch(legacyRequests);
        const EpisodeSearchBatchOutcome outcome =
            processEpisodeSearchResponses(
                legacyResponses, legacyKeywords, searchContext,
                requestedEpisodeNumber, QStringLiteral("legacy-episodes"),
                false, excludeClearlyEpisodicWorks);
        hadSuccessfulSearchResponse |= outcome.hadSuccessfulResponse;
        if (!outcome.lastError.isEmpty()) {
            lastSearchError = outcome.lastError;
        }
        if (!outcome.candidates.isEmpty()) {
            titleSearchFoundCandidates = true;
            allCandidates.append(outcome.candidates);
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

    
    if (!deduplicated.isEmpty()) {
        const int topN = std::min<int>(deduplicated.size(), 5);
        for (int i = 0; i < topN; ++i) {
            const DanmakuMatchCandidate &c = deduplicated.at(i);
            const QString candidateSubject =
                c.subtitle.isEmpty() ? c.title : c.subtitle;
            const int candidateSeason = extractSeasonNumber(candidateSubject);
            qDebug().noquote()
                << "[Danmaku][DandanPlay] Search top candidate"
                << "| rank:" << (i + 1)
                << "| title:" << c.title
                << "| subtitle:" << c.subtitle
                << "| candidateSeason:" << candidateSeason
                << "| candidateEpisode:" << c.episodeNumber
                << "| seasonHit:"
                << (context.seasonNumber > 0 && candidateSeason > 0 &&
                    candidateSeason == context.seasonNumber)
                << "| episodeHit:"
                << (context.episodeNumber > 0 && c.episodeNumber > 0 &&
                    c.episodeNumber == context.episodeNumber)
                << "| score:" << c.score
                << "| matchReason:" << c.matchReason
                << "| commentCount:" << c.commentCount;
        }
    }
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
        co_await m_networkManager->get(url, buildHeaders(config, apiPath),
                                      danmakuRequestOptions());
    comments = parseCommentsResponse(response);
    qDebug().noquote()
        << "[Danmaku][DandanPlay] Fetch comments result"
        << "| targetId:" << candidate.targetId
        << "| count:" << comments.size();
    co_return comments;
}
