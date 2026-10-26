#ifndef HTTPRANGEUTILS_H
#define HTTPRANGEUTILS_H

#include <QByteArray>
#include <QNetworkRequest>
#include <QString>
#include <QUrl>
#include <optional>

namespace HttpRangeUtils {
constexpr qint64 BlockBytes = 1024 * 1024;
struct Range {
    qint64 first = 0;
    qint64 last = -1;
    qint64 total = -1;
    bool partial = false;
};
std::optional<Range> requestRange(const QByteArray &value);
std::optional<Range> contentRange(const QByteArray &value);
QString blockKey(const QNetworkRequest &request, qint64 first);
QByteArray strongValidator(const QByteArray &etag, const QByteArray &modified, const QByteArray &date);
bool isPlaylist(const QUrl &url, const QByteArray &contentType = {});
bool forbidsCaching(const QByteArray &cacheControl);
int freshnessSeconds(const QByteArray &cacheControl, const QByteArray &age);
}

#endif
