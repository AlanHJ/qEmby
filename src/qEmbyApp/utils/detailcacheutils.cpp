#include "detailcacheutils.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSaveFile>
#include <QStandardPaths>

namespace {

constexpr int kCacheVersion = 3;

QString safeBucket(QString value, const QString &fallback) {
  value = value.trimmed();
  if (value.isEmpty())
    value = fallback;

  const QByteArray digest =
      QCryptographicHash::hash(value.toUtf8(), QCryptographicHash::Sha1)
          .toHex();
  return QString::fromLatin1(digest);
}

QJsonArray stringListToJsonArray(const QStringList &values) {
  QJsonArray array;
  for (const QString &value : values)
    array.append(value);
  return array;
}

QStringList stringListFromJsonArray(const QJsonArray &array) {
  QStringList values;
  values.reserve(array.size());
  for (const QJsonValue &value : array)
    values.append(value.toString());
  return values;
}

QJsonObject variantMapToJsonObject(const QVariantMap &values) {
  QJsonObject object;
  for (auto it = values.constBegin(); it != values.constEnd(); ++it)
    object.insert(it.key(), QJsonValue::fromVariant(it.value()));
  return object;
}

QJsonObject userDataToJson(const MediaUserDataInfo &userData) {
  QJsonObject object;
  object.insert(QStringLiteral("IsFavorite"), userData.isFavorite);
  object.insert(QStringLiteral("PlayedPercentage"), userData.playedPercentage);
  object.insert(QStringLiteral("PlaybackPositionTicks"),
                QString::number(userData.playbackPositionTicks));
  object.insert(QStringLiteral("Played"), userData.played);
  object.insert(QStringLiteral("PlayCount"), userData.playCount);
  object.insert(QStringLiteral("LastPlayedDate"), userData.lastPlayedDate);
  return object;
}

QJsonObject personToJson(const MediaPersonInfo &person) {
  QJsonObject object;
  object.insert(QStringLiteral("Id"), person.id);
  object.insert(QStringLiteral("Name"), person.name);
  object.insert(QStringLiteral("Role"), person.role);
  object.insert(QStringLiteral("Type"), person.type);
  object.insert(QStringLiteral("PrimaryImageTag"), person.primaryImageTag);
  return object;
}

QJsonObject studioToJson(const MediaStudioInfo &studio) {
  QJsonObject object;
  object.insert(QStringLiteral("Id"), studio.id);
  object.insert(QStringLiteral("Name"), studio.name);
  return object;
}

QJsonObject externalUrlToJson(const MediaExternalUrlInfo &url) {
  QJsonObject object;
  object.insert(QStringLiteral("Name"), url.name);
  object.insert(QStringLiteral("Url"), url.url);
  return object;
}

QJsonObject streamToJson(const MediaStreamInfo &stream) {
  QJsonObject object;
  object.insert(QStringLiteral("Type"), stream.type);
  object.insert(QStringLiteral("Codec"), stream.codec);
  object.insert(QStringLiteral("CodecTag"), stream.codecTag);
  object.insert(QStringLiteral("Title"), stream.title);
  object.insert(QStringLiteral("DisplayTitle"), stream.displayTitle);
  object.insert(QStringLiteral("Profile"), stream.profile);
  object.insert(QStringLiteral("Level"), stream.level);
  object.insert(QStringLiteral("Width"), stream.width);
  object.insert(QStringLiteral("Height"), stream.height);
  object.insert(QStringLiteral("AspectRatio"), stream.aspectRatio);
  object.insert(QStringLiteral("IsInterlaced"), stream.isInterlaced);
  object.insert(QStringLiteral("RealFrameRate"), stream.realFrameRate);
  object.insert(QStringLiteral("BitRate"), QString::number(stream.bitRate));
  object.insert(QStringLiteral("BitDepth"), stream.bitDepth);
  object.insert(QStringLiteral("PixelFormat"), stream.pixelFormat);
  object.insert(QStringLiteral("RefFrames"), stream.refFrames);
  object.insert(QStringLiteral("Language"), stream.language);
  object.insert(QStringLiteral("ChannelLayout"), stream.channelLayout);
  object.insert(QStringLiteral("Channels"), stream.channels);
  object.insert(QStringLiteral("SampleRate"), stream.sampleRate);
  object.insert(QStringLiteral("IsDefault"), stream.isDefault);
  object.insert(QStringLiteral("DisplayLanguage"), stream.displayLanguage);
  object.insert(QStringLiteral("IsForced"), stream.isForced);
  object.insert(QStringLiteral("IsHearingImpaired"),
                stream.isHearingImpaired);
  object.insert(QStringLiteral("Index"), stream.index);
  object.insert(QStringLiteral("IsExternal"), stream.isExternal);
  object.insert(QStringLiteral("DeliveryMethod"), stream.deliveryMethod);
  object.insert(QStringLiteral("IsExternalUrl"), stream.isExternalUrl);
  object.insert(QStringLiteral("IsTextSubtitleStream"),
                stream.isTextSubtitleStream);
  object.insert(QStringLiteral("SupportsExternalStream"),
                stream.supportsExternalStream);
  object.insert(QStringLiteral("Path"), stream.path);
  object.insert(QStringLiteral("Protocol"), stream.protocol);
  object.insert(QStringLiteral("ExtendedVideoType"),
                stream.extendedVideoType);
  object.insert(QStringLiteral("ExtendedVideoSubType"),
                stream.extendedVideoSubType);
  object.insert(QStringLiteral("AttachmentSize"),
                QString::number(stream.attachmentSize));
  return object;
}

QJsonObject sourceToJson(const MediaSourceInfo &source) {
  QJsonObject object;
  object.insert(QStringLiteral("Id"), source.id);
  object.insert(QStringLiteral("Name"), source.name);
  object.insert(QStringLiteral("Path"), source.path);
  object.insert(QStringLiteral("Container"), source.container);
  object.insert(QStringLiteral("Size"), QString::number(source.size));
  object.insert(QStringLiteral("RunTimeTicks"),
                QString::number(source.runTimeTicks));
  if (source.dateCreated.isValid()) {
    object.insert(QStringLiteral("DateCreated"),
                  source.dateCreated.toUTC().toString(Qt::ISODateWithMs));
  }
  if (source.dateModified.isValid()) {
    object.insert(QStringLiteral("DateModified"),
                  source.dateModified.toUTC().toString(Qt::ISODateWithMs));
  }

  QJsonArray streams;
  for (const MediaStreamInfo &stream : source.mediaStreams)
    streams.append(streamToJson(stream));
  object.insert(QStringLiteral("MediaStreams"), streams);
  return object;
}

QJsonObject itemToJson(const MediaItem &item) {
  QJsonObject object;
  object.insert(QStringLiteral("Id"), item.id);
  object.insert(QStringLiteral("Name"), item.name);
  object.insert(QStringLiteral("OriginalTitle"), item.originalTitle);
  object.insert(QStringLiteral("SortName"), item.sortName);
  object.insert(QStringLiteral("Type"), item.type);
  object.insert(QStringLiteral("MediaType"), item.mediaType);
  object.insert(QStringLiteral("CollectionType"), item.collectionType);
  object.insert(QStringLiteral("CollectionId"), item.collectionId);
  object.insert(QStringLiteral("CollectionName"), item.collectionName);
  object.insert(QStringLiteral("SeriesId"), item.seriesId);
  object.insert(QStringLiteral("SeriesName"), item.seriesName);
  object.insert(QStringLiteral("ProviderIds"),
                variantMapToJsonObject(item.providerIds));
  object.insert(QStringLiteral("ProductionYear"), item.productionYear);
  object.insert(QStringLiteral("Overview"), item.overview);
  object.insert(QStringLiteral("OfficialRating"), item.officialRating);
  object.insert(QStringLiteral("PremiereDate"), item.premiereDate);
  object.insert(QStringLiteral("DateCreated"),
                item.dateCreatedRaw.isEmpty() ? item.dateCreated
                                              : item.dateCreatedRaw);
  object.insert(QStringLiteral("CriticRating"), item.criticRating);
  object.insert(QStringLiteral("RunTimeTicks"),
                QString::number(item.runTimeTicks));
  object.insert(QStringLiteral("CommunityRating"), item.communityRating);
  object.insert(QStringLiteral("CanDownload"), item.canDownload);
  object.insert(QStringLiteral("Container"), item.container);
  object.insert(QStringLiteral("Path"), item.path);
  object.insert(QStringLiteral("Size"), QString::number(item.size));
  object.insert(QStringLiteral("Bitrate"), QString::number(item.bitrate));
  object.insert(QStringLiteral("Width"), item.width);
  object.insert(QStringLiteral("Height"), item.height);
  object.insert(QStringLiteral("PartCount"), item.partCount);
  object.insert(QStringLiteral("ChildCount"), item.childCount);
  object.insert(QStringLiteral("RecursiveItemCount"), item.recursiveItemCount);
  object.insert(QStringLiteral("ParentIndexNumber"), item.parentIndexNumber);
  object.insert(QStringLiteral("IndexNumber"), item.indexNumber);

  QJsonObject imageTags;
  imageTags.insert(QStringLiteral("Primary"), item.images.primaryTag);
  imageTags.insert(QStringLiteral("Thumb"), item.images.thumbTag);
  imageTags.insert(QStringLiteral("Backdrop"), item.images.backdropTag);
  imageTags.insert(QStringLiteral("Logo"), item.images.logoTag);
  object.insert(QStringLiteral("ImageTags"), imageTags);
  object.insert(QStringLiteral("PrimaryImageAspectRatio"),
                item.images.primaryImageAspectRatio);
  object.insert(QStringLiteral("PrimaryImageItemId"),
                item.images.primaryImageItemId);
  object.insert(QStringLiteral("SeriesPrimaryImageTag"),
                item.images.parentPrimaryTag);
  object.insert(QStringLiteral("ParentThumbImageTag"),
                item.images.parentThumbTag);
  QJsonArray parentBackdrops;
  if (!item.images.parentBackdropTag.isEmpty())
    parentBackdrops.append(item.images.parentBackdropTag);
  object.insert(QStringLiteral("ParentBackdropImageTags"), parentBackdrops);

  object.insert(QStringLiteral("UserData"), userDataToJson(item.userData));
  object.insert(QStringLiteral("Genres"), stringListToJsonArray(item.genres));

  QJsonArray genreItems;
  for (const QString &genre : item.genres) {
    QJsonObject genreObject;
    genreObject.insert(QStringLiteral("Name"), genre);
    genreItems.append(genreObject);
  }
  object.insert(QStringLiteral("GenreItems"), genreItems);

  object.insert(QStringLiteral("Taglines"),
                stringListToJsonArray(item.taglines));

  QJsonArray tagItems;
  for (const QString &tag : item.tags) {
    QJsonObject tagObject;
    tagObject.insert(QStringLiteral("Name"), tag);
    tagItems.append(tagObject);
  }
  object.insert(QStringLiteral("TagItems"), tagItems);

  QJsonArray trailers;
  for (const QString &trailer : item.remoteTrailers) {
    QJsonObject trailerObject;
    trailerObject.insert(QStringLiteral("Url"), trailer);
    trailers.append(trailerObject);
  }
  object.insert(QStringLiteral("RemoteTrailers"), trailers);

  QJsonArray people;
  for (const MediaPersonInfo &person : item.people)
    people.append(personToJson(person));
  object.insert(QStringLiteral("People"), people);

  QJsonArray studios;
  for (const MediaStudioInfo &studio : item.studios)
    studios.append(studioToJson(studio));
  object.insert(QStringLiteral("Studios"), studios);

  QJsonArray externalUrls;
  for (const MediaExternalUrlInfo &url : item.externalUrls)
    externalUrls.append(externalUrlToJson(url));
  object.insert(QStringLiteral("ExternalUrls"), externalUrls);

  QJsonArray mediaSources;
  for (const MediaSourceInfo &source : item.mediaSources)
    mediaSources.append(sourceToJson(source));
  object.insert(QStringLiteral("MediaSources"), mediaSources);

  return object;
}

MediaItem itemFromJson(const QJsonObject &object) {
  MediaItem item = MediaItem::fromJson(object);
  if (object.contains(QStringLiteral("PartCount")))
    item.partCount = object.value(QStringLiteral("PartCount")).toInt(1);
  if (object.contains(QStringLiteral("Genres")) && item.genres.isEmpty())
    item.genres =
        stringListFromJsonArray(object.value(QStringLiteral("Genres")).toArray());
  return item;
}

QJsonArray itemListToJson(const QList<MediaItem> &items) {
  QJsonArray array;
  for (const MediaItem &item : items)
    array.append(itemToJson(item));
  return array;
}

QList<MediaItem> itemListFromJson(const QJsonArray &array) {
  QList<MediaItem> items;
  items.reserve(array.size());
  for (const QJsonValue &value : array) {
    if (value.isObject())
      items.append(itemFromJson(value.toObject()));
  }
  return items;
}

QJsonObject sectionsToJson(const DetailCacheUtils::DetailCacheEntry &entry) {
  QJsonObject object;
  object.insert(QStringLiteral("seasonIndex"), entry.seasonIndex);
  object.insert(QStringLiteral("seasonId"), entry.seasonId);

  if (entry.hasPlayableItem) {
    object.insert(QStringLiteral("hasPlayableItem"), true);
    object.insert(QStringLiteral("playableItemFromServer"),
                  entry.playableItemFromServer);
    object.insert(QStringLiteral("playableItem"), itemToJson(entry.playableItem));
  }
  if (entry.hasSeasons) {
    object.insert(QStringLiteral("hasSeasons"), true);
    object.insert(QStringLiteral("seasons"), itemListToJson(entry.seasons));
  }
  if (entry.hasSeasonEpisodes) {
    object.insert(QStringLiteral("hasSeasonEpisodes"), true);
    object.insert(QStringLiteral("seasonEpisodes"),
                  itemListToJson(entry.seasonEpisodes));
  }
  if (entry.hasSimilarItems) {
    object.insert(QStringLiteral("hasSimilarItems"), true);
    object.insert(QStringLiteral("similarItems"),
                  itemListToJson(entry.similarItems));
  }
  if (entry.hasCollections) {
    object.insert(QStringLiteral("hasCollections"), true);
    object.insert(QStringLiteral("collections"),
                  itemListToJson(entry.collections));
  }
  if (entry.hasAdditionalParts) {
    object.insert(QStringLiteral("hasAdditionalParts"), true);
    object.insert(QStringLiteral("additionalParts"),
                  itemListToJson(entry.additionalParts));
  }

  return object;
}

void applySectionsFromJson(DetailCacheUtils::DetailCacheEntry &entry,
                           const QJsonObject &object) {
  entry.seasonIndex = object.value(QStringLiteral("seasonIndex")).toInt(-1);
  entry.seasonId = object.value(QStringLiteral("seasonId")).toString();

  entry.hasPlayableItem =
      object.value(QStringLiteral("hasPlayableItem")).toBool(false);
  entry.playableItemFromServer =
      object.value(QStringLiteral("playableItemFromServer")).toBool(false);
  if (entry.hasPlayableItem) {
    entry.playableItem =
        itemFromJson(object.value(QStringLiteral("playableItem")).toObject());
  }
  entry.hasSeasons = object.value(QStringLiteral("hasSeasons")).toBool(false);
  if (entry.hasSeasons) {
    entry.seasons =
        itemListFromJson(object.value(QStringLiteral("seasons")).toArray());
  }

  entry.hasSeasonEpisodes =
      object.value(QStringLiteral("hasSeasonEpisodes")).toBool(false);
  if (entry.hasSeasonEpisodes) {
    entry.seasonEpisodes = itemListFromJson(
        object.value(QStringLiteral("seasonEpisodes")).toArray());
  }

  entry.hasSimilarItems =
      object.value(QStringLiteral("hasSimilarItems")).toBool(false);
  if (entry.hasSimilarItems) {
    entry.similarItems = itemListFromJson(
        object.value(QStringLiteral("similarItems")).toArray());
  }

  entry.hasCollections =
      object.value(QStringLiteral("hasCollections")).toBool(false);
  if (entry.hasCollections) {
    entry.collections = itemListFromJson(
        object.value(QStringLiteral("collections")).toArray());
  }

  entry.hasAdditionalParts =
      object.value(QStringLiteral("hasAdditionalParts")).toBool(false);
  if (entry.hasAdditionalParts) {
    entry.additionalParts = itemListFromJson(
        object.value(QStringLiteral("additionalParts")).toArray());
  }
}

QByteArray normalizedItemBytes(const MediaItem &item) {
  return QJsonDocument(itemToJson(item)).toJson(QJsonDocument::Compact);
}

QByteArray normalizedSectionsBytes(
    const DetailCacheUtils::DetailCacheEntry &entry) {
  return QJsonDocument(sectionsToJson(entry)).toJson(QJsonDocument::Compact);
}

QString detailCacheRoot(QStandardPaths::StandardLocation location) {
  return QStandardPaths::writableLocation(location) +
         QStringLiteral("/detail-cache");
}

QString detailImageCacheRoot(QStandardPaths::StandardLocation location) {
  return QStandardPaths::writableLocation(location) +
         QStringLiteral("/detail-cache-images");
}

QString cacheFilePathForRoot(const QString &root, const QString &serverId,
                             const QString &userId, const QString &itemId) {
  const QString serverBucket = safeBucket(serverId, QStringLiteral("server"));
  const QString userBucket = safeBucket(userId, QStringLiteral("user"));
  const QString itemBucket = safeBucket(itemId, QStringLiteral("item"));
  return QDir(root).filePath(QStringLiteral("%1/%2/%3.json")
                                 .arg(serverBucket, userBucket, itemBucket));
}

QString imageCacheFilePathForRoot(const QString &root, const QString &serverId,
                                  const QString &userId,
                                  const QString &ownerItemId,
                                  const QString &role,
                                  const QString &imageItemId,
                                  const QString &imageType,
                                  const QString &imageTag, int maxWidth) {
  const QString serverBucket = safeBucket(serverId, QStringLiteral("server"));
  const QString userBucket = safeBucket(userId, QStringLiteral("user"));
  const QString rawKey =
      QStringLiteral("%1|%2|%3|%4|%5|%6")
          .arg(ownerItemId.trimmed(), role.trimmed(), imageItemId.trimmed(),
               imageType.trimmed(), imageTag.trimmed(),
               QString::number(maxWidth));
  const QString imageBucket = safeBucket(rawKey, QStringLiteral("image"));
  const QString extension =
      role.compare(QStringLiteral("backdrop"), Qt::CaseInsensitive) == 0
          ? QStringLiteral("jpg")
          : QStringLiteral("png");
  return QDir(root).filePath(QStringLiteral("%1/%2/%3.%4")
                                 .arg(serverBucket, userBucket, imageBucket,
                                      extension));
}

} 

namespace DetailCacheUtils {

QString cacheFilePath(const QString &serverId, const QString &userId,
                      const QString &itemId) {
  return cacheFilePathForRoot(detailCacheRoot(QStandardPaths::CacheLocation),
                              serverId, userId, itemId);
}

QString imageCacheFilePath(const QString &serverId, const QString &userId,
                           const QString &ownerItemId, const QString &role,
                           const QString &imageItemId,
                           const QString &imageType,
                           const QString &imageTag, int maxWidth) {
  return imageCacheFilePathForRoot(
      detailImageCacheRoot(QStandardPaths::CacheLocation), serverId, userId,
      ownerItemId, role, imageItemId, imageType, imageTag, maxWidth);
}

QString fingerprint(const MediaItem &item) {
  return QString::fromLatin1(
      QCryptographicHash::hash(normalizedItemBytes(item),
                               QCryptographicHash::Sha256)
          .toHex());
}

QString sectionsFingerprint(const DetailCacheEntry &entry) {
  return QString::fromLatin1(
      QCryptographicHash::hash(normalizedSectionsBytes(entry),
                               QCryptographicHash::Sha256)
          .toHex());
}

std::optional<DetailCacheEntry> load(const QString &serverId,
                                     const QString &userId,
                                     const QString &itemId) {
  QString filePath = cacheFilePath(serverId, userId, itemId);
  QFile file(filePath);
  if (!file.open(QIODevice::ReadOnly)) {
    const QString legacyFilePath =
        cacheFilePathForRoot(detailCacheRoot(QStandardPaths::AppDataLocation),
                             serverId, userId, itemId);
    if (legacyFilePath == filePath)
      return std::nullopt;
    file.setFileName(legacyFilePath);
    if (!file.open(QIODevice::ReadOnly))
      return std::nullopt;
    filePath = legacyFilePath;
  }

  QJsonParseError parseError{};
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    return std::nullopt;

  const QJsonObject root = doc.object();
  if (root.value(QStringLiteral("version")).toInt() != kCacheVersion)
    return std::nullopt;

  const QJsonObject itemObject = root.value(QStringLiteral("item")).toObject();
  MediaItem item = itemFromJson(itemObject);
  if (item.id.trimmed().isEmpty() || item.id != itemId)
    return std::nullopt;

  DetailCacheEntry entry;
  entry.item = item;
  entry.fingerprint = root.value(QStringLiteral("fingerprint")).toString();
  if (entry.fingerprint.isEmpty())
    entry.fingerprint = fingerprint(entry.item);
  applySectionsFromJson(entry, root.value(QStringLiteral("sections")).toObject());
  entry.sectionsFingerprint =
      root.value(QStringLiteral("sectionsFingerprint")).toString();
  if (entry.sectionsFingerprint.isEmpty())
    entry.sectionsFingerprint = sectionsFingerprint(entry);
  entry.savedAtMs =
      root.value(QStringLiteral("savedAtMs")).toVariant().toLongLong();
  entry.filePath = filePath;
  return entry;
}

bool save(const QString &serverId, const QString &userId,
          const MediaItem &item, QString *errorString) {
  DetailCacheEntry entry;
  entry.item = item;
  return save(serverId, userId, entry, errorString);
}

bool save(const QString &serverId, const QString &userId,
          const DetailCacheEntry &entry, QString *errorString) {
  const MediaItem &item = entry.item;
  if (item.id.trimmed().isEmpty()) {
    if (errorString)
      *errorString = QStringLiteral("empty item id");
    return false;
  }

  const QString filePath = cacheFilePath(serverId, userId, item.id);
  const QFileInfo fileInfo(filePath);
  if (!QDir().mkpath(fileInfo.absolutePath())) {
    if (errorString)
      *errorString = QStringLiteral("failed to create cache directory");
    return false;
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), kCacheVersion);
  root.insert(QStringLiteral("savedAtMs"),
              QString::number(QDateTime::currentMSecsSinceEpoch()));
  root.insert(QStringLiteral("fingerprint"), fingerprint(item));
  root.insert(QStringLiteral("sectionsFingerprint"),
              sectionsFingerprint(entry));
  root.insert(QStringLiteral("item"), itemToJson(item));
  root.insert(QStringLiteral("sections"), sectionsToJson(entry));

  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    if (errorString)
      *errorString = file.errorString();
    return false;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  if (!file.commit()) {
    if (errorString)
      *errorString = file.errorString();
    return false;
  }

  return true;
}

std::optional<QImage> loadImage(const QString &serverId, const QString &userId,
                                const QString &ownerItemId,
                                const QString &role,
                                const QString &imageItemId,
                                const QString &imageType,
                                const QString &imageTag, int maxWidth) {
  if (serverId.trimmed().isEmpty() || userId.trimmed().isEmpty() ||
      ownerItemId.trimmed().isEmpty() || imageTag.trimmed().isEmpty()) {
    return std::nullopt;
  }

  const QString filePath = imageCacheFilePath(
      serverId, userId, ownerItemId, role, imageItemId, imageType, imageTag,
      maxWidth);
  QImage image(filePath);
  if (image.isNull()) {
    const QString legacyFilePath = imageCacheFilePathForRoot(
        detailImageCacheRoot(QStandardPaths::AppDataLocation), serverId,
        userId, ownerItemId, role, imageItemId, imageType, imageTag, maxWidth);
    if (legacyFilePath != filePath)
      image.load(legacyFilePath);
  }
  if (image.isNull())
    return std::nullopt;
  return image;
}

bool saveImage(const QString &serverId, const QString &userId,
               const QString &ownerItemId, const QString &role,
               const QString &imageItemId, const QString &imageType,
               const QString &imageTag, int maxWidth, const QImage &image,
               QString *errorString) {
  if (image.isNull() || serverId.trimmed().isEmpty() ||
      userId.trimmed().isEmpty() || ownerItemId.trimmed().isEmpty() ||
      imageTag.trimmed().isEmpty()) {
    if (errorString)
      *errorString = QStringLiteral("invalid image cache input");
    return false;
  }

  const QString filePath = imageCacheFilePath(
      serverId, userId, ownerItemId, role, imageItemId, imageType, imageTag,
      maxWidth);
  const QFileInfo fileInfo(filePath);
  if (!QDir().mkpath(fileInfo.absolutePath())) {
    if (errorString)
      *errorString = QStringLiteral("failed to create image cache directory");
    return false;
  }

  QSaveFile file(filePath);
  if (!file.open(QIODevice::WriteOnly)) {
    if (errorString)
      *errorString = file.errorString();
    return false;
  }

  const bool useJpeg =
      role.compare(QStringLiteral("backdrop"), Qt::CaseInsensitive) == 0;
  if (!image.save(&file, useJpeg ? "JPG" : "PNG", useJpeg ? 88 : -1)) {
    if (errorString)
      *errorString = QStringLiteral("failed to encode cached image");
    return false;
  }

  if (!file.commit()) {
    if (errorString)
      *errorString = file.errorString();
    return false;
  }

  return true;
}

} 
