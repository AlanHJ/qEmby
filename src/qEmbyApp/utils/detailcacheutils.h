#ifndef DETAILCACHEUTILS_H
#define DETAILCACHEUTILS_H

#include "models/media/mediaitem.h"

#include <QImage>
#include <QList>
#include <QString>
#include <QtGlobal>
#include <optional>

namespace DetailCacheUtils {

struct DetailCacheEntry {
  MediaItem item;
  MediaItem playableItem;
  QList<MediaItem> seasons;
  QList<MediaItem> seasonEpisodes;
  QList<MediaItem> similarItems;
  QList<MediaItem> collections;
  QList<MediaItem> additionalParts;
  QString fingerprint;
  QString sectionsFingerprint;
  QString seasonId;
  int seasonIndex = -1;
  bool playableItemFromServer = false;
  bool hasPlayableItem = false;
  bool hasSeasons = false;
  bool hasSeasonEpisodes = false;
  bool hasSimilarItems = false;
  bool hasCollections = false;
  bool hasAdditionalParts = false;
  qint64 savedAtMs = 0;
  QString filePath;
};

std::optional<DetailCacheEntry> load(const QString &serverId,
                                     const QString &userId,
                                     const QString &itemId);
bool save(const QString &serverId, const QString &userId,
          const MediaItem &item, QString *errorString = nullptr);
bool save(const QString &serverId, const QString &userId,
          const DetailCacheEntry &entry, QString *errorString = nullptr);
QString fingerprint(const MediaItem &item);
QString sectionsFingerprint(const DetailCacheEntry &entry);
QString cacheFilePath(const QString &serverId, const QString &userId,
                      const QString &itemId);
QString imageCacheFilePath(const QString &serverId, const QString &userId,
                           const QString &ownerItemId, const QString &role,
                           const QString &imageItemId,
                           const QString &imageType,
                           const QString &imageTag, int maxWidth);
std::optional<QImage> loadImage(const QString &serverId, const QString &userId,
                                const QString &ownerItemId,
                                const QString &role,
                                const QString &imageItemId,
                                const QString &imageType,
                                const QString &imageTag, int maxWidth);
bool saveImage(const QString &serverId, const QString &userId,
               const QString &ownerItemId, const QString &role,
               const QString &imageItemId, const QString &imageType,
               const QString &imageTag, int maxWidth, const QImage &image,
               QString *errorString = nullptr);

} 

#endif 
