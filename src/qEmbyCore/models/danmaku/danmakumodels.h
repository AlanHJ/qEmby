#ifndef DANMAKUMODELS_H
#define DANMAKUMODELS_H

#include "../../qEmbyCore_global.h"

#include <QColor>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <QVariantMap>

struct QEMBYCORE_EXPORT DanmakuServerDefinition {
    QString id;
    QString name;
    QString provider = QStringLiteral("dandanplay");
    QString baseUrl = QStringLiteral("https://api.dandanplay.net");
    QString appId;
    QString appSecret;
    bool enabled = true;

    bool isValid() const {
        return !id.trimmed().isEmpty() && !name.trimmed().isEmpty() &&
               !baseUrl.trimmed().isEmpty();
    }

    QString displayName() const {
        return name.trimmed().isEmpty() ? baseUrl.trimmed() : name.trimmed();
    }
};
Q_DECLARE_METATYPE(DanmakuServerDefinition)

struct QEMBYCORE_EXPORT DanmakuProviderConfig {
    QString provider = QStringLiteral("dandanplay");
    QString baseUrl = QStringLiteral("https://api.dandanplay.net");
    QString endpointId;
    QString endpointName;
    QString appId;
    QString appSecret;
    bool withRelated = true;
    int cacheHours = 24;
};
Q_DECLARE_METATYPE(DanmakuProviderConfig)

struct QEMBYCORE_EXPORT DanmakuMediaContext {
    QString serverId;
    QString mediaId;
    QString mediaSourceId;
    QString itemType;
    QString title;
    QString originalTitle;
    QString seriesName;
    int productionYear = 0;
    int seasonNumber = -1;
    int episodeNumber = -1;
    qint64 durationMs = 0;
    QString path;
    QVariantMap providerIds;

    QString cacheKey() const {
        return mediaSourceId.isEmpty() ? mediaId
                                       : QStringLiteral("%1:%2")
                                             .arg(mediaId, mediaSourceId);
    }

    bool isEpisode() const {
        return itemType.compare(QStringLiteral("Episode"),
                                Qt::CaseInsensitive) == 0;
    }

    QString displayTitle() const {
        if (isEpisode() && !seriesName.isEmpty()) {
            return QStringLiteral("%1 S%2E%3")
                .arg(seriesName)
                .arg(seasonNumber, 2, 10, QChar('0'))
                .arg(episodeNumber, 2, 10, QChar('0'));
        }
        return title;
    }
};
Q_DECLARE_METATYPE(DanmakuMediaContext)

struct QEMBYCORE_EXPORT DanmakuMatchCandidate {
    QString provider;
    QString cacheScope;
    QString targetId;
    QString title;
    QString subtitle;
    int seasonNumber = -1;
    int episodeNumber = -1;
    qint64 durationMs = 0;
    double score = 0.0;
    QString matchReason;
    int commentCount = 0;

    bool isValid() const {
        return !provider.isEmpty() && !targetId.isEmpty();
    }

    QString displayText() const {
        return subtitle.isEmpty() ? title
                                  : QStringLiteral("%1 - %2")
                                        .arg(subtitle, title);
    }
};
Q_DECLARE_METATYPE(DanmakuMatchCandidate)

struct QEMBYCORE_EXPORT DanmakuMatchResult {
    bool matched = false;
    bool cacheHit = false;
    bool manualOverride = false;
    DanmakuMatchCandidate selected;
    QList<DanmakuMatchCandidate> candidates;

    bool needManualMatch() const {
        return !matched && !candidates.isEmpty();
    }
};
Q_DECLARE_METATYPE(DanmakuMatchResult)

struct QEMBYCORE_EXPORT DanmakuComment {
    qint64 timeMs = 0;
    int mode = 1;
    QColor color = QColor(Qt::white);
    int fontLevel = 25;
    QString sender;
    QString text;
    QDateTime createdAt;

    bool isValid() const {
        return !text.trimmed().isEmpty() && timeMs >= 0;
    }
};
Q_DECLARE_METATYPE(DanmakuComment)

struct QEMBYCORE_EXPORT DanmakuRenderOptions {
    bool enabled = true;
    double opacity = 0.72;
    double fontScale = 1.0;
    int areaPercent = 70;
    int density = 100;
    double speedScale = 1.0;
    int offsetMs = 0;
    bool hideScroll = false;
    bool hideTop = false;
    bool hideBottom = false;
    bool dualSubtitle = true;
    QStringList blockedKeywords;
};
Q_DECLARE_METATYPE(DanmakuRenderOptions)

struct QEMBYCORE_EXPORT DanmakuLoadResult {
    bool success = false;
    QString assFilePath;
    int commentCount = 0;
    QString provider;
    QString sourceTitle;
    bool needManualMatch = false;
    DanmakuMatchResult matchResult;

    bool hasComments() const {
        return success && commentCount > 0 && !assFilePath.isEmpty();
    }

    bool hasRenderableTrack() const {
        return success && !assFilePath.isEmpty();
    }
};
Q_DECLARE_METATYPE(DanmakuLoadResult)

#endif 
