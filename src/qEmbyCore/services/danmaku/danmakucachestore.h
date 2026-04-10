#ifndef DANMAKUCACHESTORE_H
#define DANMAKUCACHESTORE_H

#include "../../models/danmaku/danmakumodels.h"

#include <QString>

class DanmakuCacheStore
{
public:
    DanmakuCacheStore() = default;

    bool loadMatch(const DanmakuMediaContext &context,
                   DanmakuMatchCandidate *candidate,
                   bool *manualOverride) const;
    void saveMatch(const DanmakuMediaContext &context,
                   const DanmakuMatchCandidate &candidate,
                   bool manualOverride) const;

    QList<DanmakuComment> loadComments(const QString &provider,
                                       const QString &cacheScope,
                                       const QString &targetId,
                                       int maxAgeHours) const;
    void saveComments(const QString &provider,
                      const QString &cacheScope,
                      const QString &targetId,
                      const QString &sourceTitle,
                      const QList<DanmakuComment> &comments) const;

    bool loadAssPath(const QString &assKey,
                     QString *path,
                     int maxAgeHours) const;
    QString saveAssFile(const QString &assKey, const QString &content) const;

    void clearAll() const;

private:
    QString baseDirPath() const;
    QString matchesFilePath(const QString &serverId) const;
    QString commentsFilePath(const QString &provider,
                             const QString &cacheScope,
                             const QString &targetId) const;
    QString assFilePath(const QString &assKey) const;
};

#endif 
