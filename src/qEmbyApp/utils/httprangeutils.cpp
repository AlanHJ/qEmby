#include "httprangeutils.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

namespace HttpRangeUtils {
std::optional<Range> requestRange(const QByteArray &value)
{
    if (value.isEmpty()) return Range{};
    static const QRegularExpression pattern(QStringLiteral("^bytes=(\\d+)-(\\d*)$"));
    const auto match = pattern.match(QString::fromLatin1(value.trimmed()));
    if (!match.hasMatch()) return std::nullopt;
    bool ok = false;
    Range range;
    range.partial = true;
    range.first = match.captured(1).toLongLong(&ok);
    if (!ok) return std::nullopt;
    if (!match.captured(2).isEmpty()) {
        range.last = match.captured(2).toLongLong(&ok);
        if (!ok || range.last < range.first) return std::nullopt;
    }
    
    if (range.first > std::numeric_limits<qint64>::max() - BlockBytes) return std::nullopt;
    return range;
}

std::optional<Range> contentRange(const QByteArray &value)
{
    static const QRegularExpression pattern(QStringLiteral("^bytes (\\d+)-(\\d+)/(\\d+)$"));
    const auto match = pattern.match(QString::fromLatin1(value.trimmed()));
    if (!match.hasMatch()) return std::nullopt;
    bool firstOk = false, lastOk = false, totalOk = false;
    Range range{match.captured(1).toLongLong(&firstOk), match.captured(2).toLongLong(&lastOk),
                match.captured(3).toLongLong(&totalOk), true};
    if (!firstOk || !lastOk || !totalOk || range.first > range.last || range.last >= range.total ||
        range.total > std::numeric_limits<qint64>::max() - BlockBytes)
        return std::nullopt;
    return range;
}

QString blockKey(const QNetworkRequest &request, qint64 first)
{
    QCryptographicHash hash(QCryptographicHash::Sha256);
    hash.addData(request.url().toEncoded());
    auto names = request.rawHeaderList();
    std::sort(names.begin(), names.end());
    for (const auto &name : names) {
        const auto lower = name.toLower();
        if (lower == "range" || lower == "if-range") continue;
        hash.addData(QByteArray(1, '\0'));
        hash.addData(lower);
        hash.addData(QByteArray(1, '\0'));
        hash.addData(request.rawHeader(name));
    }
    return QString::fromLatin1(hash.result().toHex()) + QLatin1Char(':') + QString::number(first);
}

QByteArray strongValidator(const QByteArray &etag, const QByteArray &modified, const QByteArray &date)
{
    if (etag.startsWith('"') && etag.endsWith('"')) return etag;
    
    
    const auto changed = QDateTime::fromString(QString::fromLatin1(modified), Qt::RFC2822Date);
    const auto served = QDateTime::fromString(QString::fromLatin1(date), Qt::RFC2822Date);
    if (changed.isValid() && served.isValid() && changed.secsTo(served) >= 60) return modified;
    return {};
}

bool isPlaylist(const QUrl &url, const QByteArray &contentType)
{
    const auto path = url.path().toLower();
    const auto type = contentType.toLower();
    return path.endsWith(QLatin1String(".m3u8")) || path.endsWith(QLatin1String(".m3u")) ||
           path.endsWith(QLatin1String(".mpd")) || type.contains("mpegurl") || type.contains("dash+xml");
}

bool forbidsCaching(const QByteArray &cacheControl)
{
    const auto value = cacheControl.toLower();
    return value.contains("no-store");
}

int freshnessSeconds(const QByteArray &cacheControl, const QByteArray &age)
{
    if (cacheControl.toLower().contains("no-cache")) return 0;
    static const QRegularExpression pattern(QStringLiteral("(?:^|,)\\s*max-age\\s*=\\s*\"?(\\d+)"));
    const auto match = pattern.match(QString::fromLatin1(cacheControl.toLower()));
    
    
    qint64 seconds = 60;
    if (match.hasMatch()) {
        bool ok = false;
        seconds = match.captured(1).toLongLong(&ok);
        if (!ok) return 0;
    }
    bool ok = false;
    const qint64 currentAge = age.toLongLong(&ok);
    if (ok && currentAge > 0) seconds = qMax(qint64(0), seconds - currentAge);
    return static_cast<int>(qBound(qint64(0), seconds, qint64(60)));
}
}
