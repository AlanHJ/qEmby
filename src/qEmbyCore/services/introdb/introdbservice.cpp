#include "introdbservice.h"
#include "api/networkmanager.h"

#include <QDebug>
#include <QJsonObject>
#include <exception>

IntroDBService::IntroDBService(NetworkManager *nm, QObject *parent)
    : QObject(parent), m_nm(nm) {}

QString IntroDBService::cacheKey(const QString &imdbId, int season, int episode) {
    return QStringLiteral("%1_s%2_e%3").arg(imdbId).arg(season).arg(episode);
}

static IntroDBService::SegmentInfo parseSegment(const QJsonObject &seg) {
    IntroDBService::SegmentInfo s;
    if (seg.contains("start_sec")) {
        s.startSec = seg["start_sec"].toDouble();
    } else if (seg.contains("start_ms")) {
        s.startSec = seg["start_ms"].toDouble() / 1000.0;
    }
    if (seg.contains("end_sec")) {
        s.endSec = seg["end_sec"].toDouble();
    } else if (seg.contains("end_ms")) {
        s.endSec = seg["end_ms"].toDouble() / 1000.0;
    }
    return s;
}

QCoro::Task<IntroDBService::EpisodeSegments>
IntroDBService::fetchSegments(QString imdbId, int season, int episode) {
    imdbId = imdbId.trimmed();
    if (imdbId.isEmpty() || season <= 0 || episode <= 0 || !m_nm) {
        qWarning("IntroDB: invalid request imdb=%s S%02dE%02d networkReady=%d",
                 qPrintable(imdbId), season, episode, !m_nm.isNull());
        co_return {};
    }

    const QString key = cacheKey(imdbId, season, episode);
    if (m_cache.contains(key)) {
        const EpisodeSegments cached = m_cache.value(key);
        qInfo("IntroDB: cache hit imdb=%s S%02dE%02d fetched=%d notFound=%d",
              qPrintable(imdbId), season, episode, cached.fetched,
              cached.notFound);
        co_return m_cache[key];
    }

    QString url = QStringLiteral(
        "https://api.introdb.app/segments"
        "?imdb_id=%1&season=%2&episode=%3")
                       .arg(imdbId)
                       .arg(season)
                       .arg(episode);

    qInfo("IntroDB: fetching %s S%02dE%02d",
          qPrintable(imdbId), season, episode);

    QPointer<IntroDBService> safeThis(this);
    QPointer<NetworkManager> network = m_nm;
    EpisodeSegments segments;
    bool cacheable = false;
    try {
        if (!network) {
            co_return segments;
        }
        QJsonObject root = co_await network->get(url, {});
        if (!safeThis) {
            co_return segments;
        }
        if (root.contains("error")) {
            segments.notFound = true;
            cacheable = true;
        } else {
            if (root.contains("intro") && !root["intro"].isNull()) {
                segments.intro = parseSegment(root["intro"].toObject());
            }
            if (root.contains("outro") && !root["outro"].isNull()) {
                segments.outro = parseSegment(root["outro"].toObject());
            }
            segments.fetched = true;
            cacheable = true;
        }
    } catch (const std::exception &e) {
        const QString error = QString::fromUtf8(e.what());
        const bool isNotFound = error.contains(QStringLiteral("HTTP 404"));
        qWarning("IntroDB: request failed: %s", e.what());
        segments.notFound = isNotFound;
        cacheable = isNotFound;
    }

    if (cacheable && safeThis) {
        safeThis->m_cache[key] = segments;
    }
    co_return segments;
}

void IntroDBService::clearCache() {
    m_cache.clear();
}
