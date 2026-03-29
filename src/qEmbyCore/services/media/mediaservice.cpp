#include "mediaservice.h"
#include "../../api/apiclient.h"
#include "../../config/config_keys.h"
#include "../../config/configstore.h"
#include "../manager/servermanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QNetworkReply>
#include <QPointer>
#include <QSettings>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <qcoronetwork.h>
#include <stdexcept>

namespace
{
template <typename T> QList<T> parseJsonArray(const QJsonArray &array)
{
    QList<T> list;
    for (const auto &val : array)
    {
        list.append(T::fromJson(val.toObject()));
    }
    return list;
}

constexpr int kRecommendCacheFormatVersion = 2;
} 

MediaService::MediaService(ServerManager *serverManager, QObject *parent)
    : QObject(parent), m_serverManager(serverManager)
{
    m_imageManager = new QNetworkAccessManager(this);
    auto *diskCache = new QNetworkDiskCache(this);
    QString cachePath = QStandardPaths::writableLocation(QStandardPaths::CacheLocation) + "/qEmby_ImageCache";
    QDir().mkpath(cachePath);
    diskCache->setCacheDirectory(cachePath);
    diskCache->setMaximumCacheSize(500 * 1024 * 1024);
    m_imageManager->setCache(diskCache);
}

void MediaService::ensureValidProfile() const
{
    if (!m_serverManager || !m_serverManager->activeClient() || !m_serverManager->activeProfile().isValid())
    {
        throw std::runtime_error(tr("无效的服务器配置或用户未登录").toStdString());
    }
}

QCoro::Task<QList<MediaItem>> MediaService::getUserViews()
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    
    QJsonObject response =
        co_await m_serverManager->activeClient()->get(QString("/Users/%1/Views").arg(profile.userId));
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getLibraryItems(const QString &parentId, const QString &sortBy,
                                                            const QString &sortOrder, const QString &filters,
                                                            const QString &includeItemTypes, int startIndex, int limit,
                                                            bool recursive, bool includeChildCount)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    QStringList fields = {QStringLiteral("BasicSyncInfo"),  QStringLiteral("CanDelete"),
                          QStringLiteral("CanDownload"),    QStringLiteral("PrimaryImageAspectRatio"),
                          QStringLiteral("ProductionYear"), QStringLiteral("Status"),
                          QStringLiteral("EndDate"),        QStringLiteral("RecursiveItemCount"),
                          QStringLiteral("DateCreated")};
    if (includeChildCount)
    {
        fields.append(QStringLiteral("ChildCount"));
    }

    QString path = QString("/Users/%1/Items?ParentId=%2"
                           "&Fields=%3"
                           "&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(profile.userId, parentId, fields.join(','));

    if (recursive)
        path += "&Recursive=true";
    if (startIndex > 0)
        path += QString("&StartIndex=%1").arg(startIndex);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!includeItemTypes.isEmpty())
        path += QString("&IncludeItemTypes=%1").arg(includeItemTypes);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);
    if (!filters.isEmpty())
        path += QString("&Filters=%1").arg(filters);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::searchMedia(const QString &searchTerm, const QString &includeItemTypes,
                                                        const QString &sortBy, const QString &sortOrder, int limit)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    
    QString path = QString("/Users/%1/"
                           "Items?SearchTerm=%2&Recursive=true&Fields="
                           "ProductionYear,RecursiveItemCount")
                       .arg(profile.userId, QUrl::toPercentEncoding(searchTerm));

    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!includeItemTypes.isEmpty())
        path += QString("&IncludeItemTypes=%1").arg(includeItemTypes);

    if (!sortBy.isEmpty() && sortBy.toLower() != "relevance")
    {
        path += QString("&SortBy=%1").arg(sortBy);
        if (!sortOrder.isEmpty())
            path += QString("&SortOrder=%1").arg(sortOrder);
    }

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}




QCoro::Task<QList<MediaItem>> MediaService::getSeasons(const QString &seriesId)
{
    ensureValidProfile();
    QString path = QString("/Shows/%1/"
                           "Seasons?UserId=%2&Fields=PrimaryImageAspectRatio,RecursiveItemCount&"
                           "EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(seriesId, m_serverManager->activeProfile().userId);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}




QCoro::Task<QList<MediaItem>> MediaService::getEpisodes(const QString &seriesId, const QString &seasonId,
                                                        const QString &sortBy, const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Shows/%1/"
                           "Episodes?SeasonId=%2&UserId=%3&Fields=PrimaryImageAspectRatio,"
                           "Overview&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(seriesId, seasonId, m_serverManager->activeProfile().userId);

    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}




QCoro::Task<QList<MediaItem>> MediaService::getNextUp(const QString &seriesId)
{
    ensureValidProfile();
    QString path = QString("/Shows/"
                           "NextUp?UserId=%1&Fields=PrimaryImageAspectRatio,Overview&"
                           "EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(m_serverManager->activeProfile().userId);

    if (!seriesId.isEmpty())
    {
        path += QString("&SeriesId=%1").arg(seriesId);
    }

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QPixmap> MediaService::fetchImage(const QString &itemId, const QString &imageType, const QString &imageTag,
                                              int maxWidth)
{
    ServerProfile profile = m_serverManager->activeProfile();
    if (!profile.isValid())
        co_return QPixmap();

    
    QString quality = ConfigStore::instance()->get<QString>(ConfigKeys::ImageQuality, "high");
    int effectiveMaxWidth = maxWidth;
    if (quality == "low")
        effectiveMaxWidth = maxWidth / 2;
    else if (quality == "medium")
        effectiveMaxWidth = maxWidth * 3 / 4;
    else if (quality == "original")
        effectiveMaxWidth = 0; 

    QString urlStr;
    if (effectiveMaxWidth > 0)
    {
        if (!imageTag.isEmpty())
        {
            urlStr = QString("%1/Items/%2/Images/%3?tag=%4&maxWidth=%5&api_key=%6")
                         .arg(profile.url, itemId, imageType, imageTag)
                         .arg(effectiveMaxWidth)
                         .arg(profile.accessToken);
        }
        else
        {
            urlStr = QString("%1/Items/%2/Images/%3?maxWidth=%4&api_key=%5")
                         .arg(profile.url, itemId, imageType)
                         .arg(effectiveMaxWidth)
                         .arg(profile.accessToken);
        }
    }
    else
    {
        
        if (!imageTag.isEmpty())
        {
            urlStr = QString("%1/Items/%2/Images/%3?tag=%4&api_key=%5")
                         .arg(profile.url, itemId, imageType, imageTag, profile.accessToken);
        }
        else
        {
            urlStr =
                QString("%1/Items/%2/Images/%3?api_key=%4").arg(profile.url, itemId, imageType, profile.accessToken);
        }
    }

    QNetworkRequest request((QUrl(urlStr)));
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    QNetworkReply *reply = co_await m_imageManager->get(request);

    QPixmap result;
    if (reply->error() == QNetworkReply::NoError)
    {
        QByteArray data = reply->readAll();
        QImage image;
        image.loadFromData(data);
        if (!image.isNull())
        {
            result = QPixmap::fromImage(image);
        }
    }

    reply->deleteLater();
    co_return result;
}

QCoro::Task<QList<MediaItem>> MediaService::getResumeItems(int limit, const QString &sortBy, const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/Items/"
                           "Resume?Recursive=true&Fields=ProductionYear,"
                           "RecursiveItemCount&MediaTypes=Video")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getLatestItems(int limit, const QString &sortBy, const QString &sortOrder)
{
    ensureValidProfile();
    
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Fields=ProductionYear,"
                           "RecursiveItemCount&IncludeItemTypes=Movie,Series")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getRecommendedMovies(int limit, const QString &sortBy,
                                                                 const QString &sortOrder)
{
    ensureValidProfile();

    const auto &profile = m_serverManager->activeProfile();
    const QString currentServerId = profile.id;
    const QString currentUserId = profile.userId;

    
    int cacheDurationHours = ConfigStore::instance()->get<QString>(ConfigKeys::DataCacheDuration, "24").toInt();
    if (cacheDurationHours <= 0)
        cacheDurationHours = 24;

    
    if (m_recommendCache.isValid(currentServerId, currentUserId, cacheDurationHours))
    {
        QList<MediaItem> cached = m_recommendCache.items;
        if (limit > 0 && cached.size() > limit)
        {
            co_return cached.mid(0, limit);
        }
        co_return cached;
    }

    
    if (m_recommendCache.loadFromDisk(currentServerId, currentUserId, cacheDurationHours))
    {
        QList<MediaItem> cached = m_recommendCache.items;
        if (limit > 0 && cached.size() > limit)
        {
            co_return cached.mid(0, limit);
        }
        co_return cached;
    }

    
    QString path = QString("/Users/%1/Items?IncludeItemTypes=Movie,Series"
                           "&Recursive=true"
                           "&Fields=ProductionYear,RecursiveItemCount,PrimaryImageAspectRatio"
                           "&EnableImageTypes=Primary,Backdrop,Thumb"
                           "&ImageTypeLimit=1")
                       .arg(currentUserId);
    path += "&Limit=1000&SortBy=Random&SortOrder=Ascending";

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    QList<MediaItem> allItems = parseJsonArray<MediaItem>(response["Items"].toArray());

    
    m_recommendCache.items = allItems;
    m_recommendCache.fetchTime = QDateTime::currentDateTime();
    m_recommendCache.serverId = currentServerId;
    m_recommendCache.userId = currentUserId;
    m_recommendCache.saveToDisk();

    if (limit > 0 && allItems.size() > limit)
    {
        co_return allItems.mid(0, limit);
    }
    co_return allItems;
}

QCoro::Task<QList<MediaItem>> MediaService::getFavoriteMovies(int limit, const QString &sortBy,
                                                              const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&"
                           "IncludeItemTypes=Movie&Fields=ProductionYear")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getFavoriteSeries(int limit, const QString &sortBy,
                                                              const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&IncludeItemTypes=Series&"
                           "Fields=ProductionYear,RecursiveItemCount")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getFavoriteCollections(int limit, const QString &sortBy,
                                                                   const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&IncludeItemTypes=BoxSet")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}


QCoro::Task<QList<MediaItem>> MediaService::getFavoritePlaylists(int limit, const QString &sortBy,
                                                                 const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&IncludeItemTypes=Playlist")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}


QCoro::Task<QList<MediaItem>> MediaService::getFavoriteFolders(int limit, const QString &sortBy,
                                                               const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&IncludeItemTypes=Folder")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getFavoritePeople(int limit, const QString &sortBy,
                                                              const QString &sortOrder)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&IncludeItemTypes=Person")
                       .arg(m_serverManager->activeProfile().userId);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);
    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}


QCoro::Task<MediaItem> MediaService::getItemDetail(const QString &itemId)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/Items/"
                           "%2?Fields=MediaStreams,MediaSources,People,Overview,Genres,"
                           "ProductionYear,OfficialRating,Tags,Studios,ExternalUrls")
                       .arg(m_serverManager->activeProfile().userId, itemId);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return MediaItem::fromJson(response);
}

QCoro::Task<bool> MediaService::toggleFavorite(const QString &itemId, bool isFavorite)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/FavoriteItems/%2").arg(m_serverManager->activeProfile().userId, itemId);

    if (isFavorite)
    {
        co_await m_serverManager->activeClient()->post(path, QJsonObject());
    }
    else
    {
        co_await m_serverManager->activeClient()->deleteResource(path);
    }
    co_return isFavorite;
}

QCoro::Task<void> MediaService::markAsPlayed(const QString &itemId)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/PlayedItems/%2").arg(m_serverManager->activeProfile().userId, itemId);
    co_await m_serverManager->activeClient()->post(path, QJsonObject());
}

QCoro::Task<void> MediaService::markAsUnplayed(const QString &itemId)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/PlayedItems/%2").arg(m_serverManager->activeProfile().userId, itemId);
    co_await m_serverManager->activeClient()->deleteResource(path);
}

QCoro::Task<void> MediaService::removeFromResume(const QString &itemId)
{
    ensureValidProfile();

    
    
    QString path =
        QString("/Users/%1/Items/%2/HideFromResume?Hide=true").arg(m_serverManager->activeProfile().userId, itemId);

    co_await m_serverManager->activeClient()->post(path, QJsonObject());
}

QCoro::Task<QList<MediaItem>> MediaService::getSimilarItems(const QString &itemId, int limit)
{
    ensureValidProfile();
    QString path = QString("/Items/%1/"
                           "Similar?UserId=%2&Limit=%3&Fields="
                           "PrimaryImageAspectRatio,ProductionYear")
                       .arg(itemId, m_serverManager->activeProfile().userId)
                       .arg(limit);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getItemCollections(const QString &itemId)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?IncludeItemTypes=Playlist,BoxSet&Recursive="
                           "true&ListItemIds=%2&Fields=PrimaryImageAspectRatio")
                       .arg(m_serverManager->activeProfile().userId, itemId);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getCollectionItems(const QString &collectionId)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/"
                           "Items?ParentId=%2&Fields=PrimaryImageAspectRatio,ProductionYear,"
                           "RecursiveItemCount&SortBy=SortName")
                       .arg(m_serverManager->activeProfile().userId, collectionId);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<PlaybackInfo> MediaService::getPlaybackInfo(const QString &itemId)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();
    QString path = QString("/Items/%1/PlaybackInfo?UserId=%2").arg(itemId, profile.userId);

    QJsonObject payload;
    payload["UserId"] = profile.userId;

    QJsonObject response = co_await m_serverManager->activeClient()->post(path, payload);
    co_return PlaybackInfo::fromJson(response);
}

QCoro::Task<QList<MediaItem>> MediaService::getAdditionalParts(const QString &itemId)
{
    ensureValidProfile();
    QString path = QString("/Videos/%1/AdditionalParts?UserId=%2").arg(itemId, m_serverManager->activeProfile().userId);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    QList<MediaItem> parts;
    if (response.contains("Items"))
    {
        parts = parseJsonArray<MediaItem>(response["Items"].toArray());
    }
    else if (response.contains("data") && response["data"].isArray())
    {
        parts = parseJsonArray<MediaItem>(response["data"].toArray());
    }
    co_return parts;
}

QCoro::Task<QList<MediaItem>> MediaService::getItemsByPerson(const QString &personId, const QString &sortBy,
                                                             const QString &sortOrder)
{
    ensureValidProfile();
    
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&PersonIds=%2&IncludeItemTypes="
                           "Movie,Series,Episode&Fields=PrimaryImageAspectRatio,"
                           "ProductionYear,RecursiveItemCount")
                       .arg(m_serverManager->activeProfile().userId, personId);

    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getItemsByFilter(const QString &genreFilter, const QString &tagFilter,
                                                             const QString &studioFilter, const QString &sortBy,
                                                             const QString &sortOrder, int limit)
{
    ensureValidProfile();
    QString path = QString("/Users/%1/Items?Recursive=true"
                           "&IncludeItemTypes=Movie,Series"
                           "&Fields=PrimaryImageAspectRatio,ProductionYear,RecursiveItemCount"
                           "&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(m_serverManager->activeProfile().userId);

    if (!genreFilter.isEmpty())
        path += QString("&Genres=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(genreFilter)));
    if (!tagFilter.isEmpty())
        path += QString("&Tags=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(tagFilter)));
    if (!studioFilter.isEmpty())
        path += QString("&Studios=%1").arg(QString::fromUtf8(QUrl::toPercentEncoding(studioFilter)));

    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);

    qDebug() << "[MediaService] getItemsByFilter"
             << "genre=" << genreFilter << "tag=" << tagFilter << "studio=" << studioFilter << "sortBy=" << sortBy
             << "sortOrder=" << sortOrder;

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QString MediaService::getStreamUrl(const QString &itemId, const QString &mediaSourceId) const
{
    ServerProfile profile = m_serverManager->activeProfile();
    if (!profile.isValid() || itemId.isEmpty() || mediaSourceId.isEmpty())
    {
        return QString();
    }
    return QString("%1/Videos/%2/stream?static=true&mediaSourceId=%3&api_key=%4")
        .arg(profile.url, itemId, mediaSourceId, profile.accessToken);
}

QString MediaService::getStreamUrl(const QString &itemId, const MediaSourceInfo &sourceInfo) const
{
    ServerProfile profile = m_serverManager->activeProfile();
    if (!profile.isValid() || itemId.isEmpty() || sourceInfo.id.isEmpty())
    {
        return QString();
    }

    bool defaultStrmDirect = (profile.type == ServerProfile::Jellyfin);
    QSettings settings("qEmby", "Player");
    bool enableStrmDirect = settings.value("EnableStrmDirectPlay", defaultStrmDirect).toBool();

    if (enableStrmDirect)
    {
        if (sourceInfo.path.startsWith("http://", Qt::CaseInsensitive) ||
            sourceInfo.path.startsWith("https://", Qt::CaseInsensitive))
        {
            return sourceInfo.path;
        }

        if (!sourceInfo.directStreamUrl.isEmpty())
        {
            if (sourceInfo.directStreamUrl.startsWith("http://", Qt::CaseInsensitive) ||
                sourceInfo.directStreamUrl.startsWith("https://", Qt::CaseInsensitive))
            {
                return sourceInfo.directStreamUrl;
            }
            else
            {
                return profile.url + sourceInfo.directStreamUrl;
            }
        }
    }

    return getStreamUrl(itemId, sourceInfo.id);
}

QCoro::Task<QString> MediaService::reportPlaybackStart(QString itemId, QString mediaSourceId, long long positionTicks)
{
    ensureValidProfile();
    QPointer<ServerManager> serverManager(m_serverManager);
    if (!serverManager)
    {
        qWarning() << "[API Warning] Playback Start skipped: ServerManager is no "
                      "longer available."
                   << "ItemId:" << itemId << "MediaSourceId:" << mediaSourceId;
        co_return QString();
    }
    ServerProfile profile = serverManager->activeProfile();
    QString playSessionId;

    if (profile.type == ServerProfile::Jellyfin)
    {
        
        try
        {
            PlaybackInfo pbInfo = co_await getPlaybackInfo(itemId);
            if (!serverManager)
            {
                qWarning() << "[API Warning] Playback Start aborted after "
                              "PlaybackInfo: ServerManager was destroyed."
                           << "ItemId:" << itemId;
                co_return QString();
            }
            playSessionId = pbInfo.playSessionId;
            qDebug() << "[API] Jellyfin PlaybackInfo retrieved. ServerSessionId:" << playSessionId;
        }
        catch (const std::exception &e)
        {
            qWarning() << "[API] Jellyfin getPlaybackInfo failed, will use local UUID:" << e.what();
        }
        if (playSessionId.isEmpty())
        {
            playSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
            qWarning() << "[API] Jellyfin PlaybackInfo returned empty PlaySessionId, "
                          "using local UUID as fallback:"
                       << playSessionId;
        }
    }
    else
    {
        
        playSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    QJsonObject payload;
    payload["ItemId"] = itemId;
    payload["MediaSourceId"] = mediaSourceId;
    payload["PositionTicks"] = static_cast<qint64>(positionTicks);
    payload["PlayMethod"] = "DirectPlay";
    payload["IsPaused"] = false;
    payload["IsMuted"] = false;
    payload["CanSeek"] = true;
    payload["PlaySessionId"] = playSessionId;

    QJsonArray mediaTypes;
    mediaTypes.append("Video");
    payload["QueueableMediaTypes"] = mediaTypes;

    ApiClient *client = serverManager->activeClient();
    if (!client)
    {
        qWarning() << "[API Warning] Playback Start skipped: active client is no "
                      "longer available."
                   << "ItemId:" << itemId << "MediaSourceId:" << mediaSourceId;
        co_return QString();
    }

    try
    {
        co_await client->post("/Sessions/Playing", payload);
        qDebug() << "[API] Playback Started successfully."
                 << "ServerType:" << (profile.type == ServerProfile::Jellyfin ? "Jellyfin" : "Emby")
                 << "SessionId:" << playSessionId << "ItemId:" << itemId << "MediaSourceId:" << mediaSourceId;
    }
    catch (const std::exception &e)
    {
        qDebug() << "[API Error] Playback Start failed:" << e.what();
    }
    co_return playSessionId;
}

QCoro::Task<void> MediaService::reportPlaybackProgress(QString itemId, QString mediaSourceId, long long positionTicks,
                                                       bool isPaused, QString playSessionId)
{
    if (playSessionId.isEmpty())
        co_return;
    ensureValidProfile();

    ApiClient *client = m_serverManager->activeClient();
    if (!client)
    {
        qWarning() << "[API Warning] Playback Progress skipped: active client is "
                      "no longer available."
                   << "ItemId:" << itemId << "MediaSourceId:" << mediaSourceId << "SessionId:" << playSessionId;
        co_return;
    }

    QJsonObject payload;
    payload["ItemId"] = itemId;
    payload["MediaSourceId"] = mediaSourceId;
    payload["PositionTicks"] = static_cast<qint64>(positionTicks);
    payload["IsPaused"] = isPaused;
    payload["IsMuted"] = false;
    payload["PlayMethod"] = "DirectPlay";
    payload["CanSeek"] = true;
    payload["PlaySessionId"] = playSessionId;

    QJsonArray mediaTypes;
    mediaTypes.append("Video");
    payload["QueueableMediaTypes"] = mediaTypes;
    payload["EventName"] = isPaused ? "pause" : "timeupdate";

    try
    {
        co_await client->post("/Sessions/Playing/Progress", payload);
    }
    catch (const std::exception &e)
    {
        qDebug() << "[API Error] Playback Progress failed:" << e.what();
    }
}

QCoro::Task<void> MediaService::reportPlaybackStopped(QString itemId, QString mediaSourceId, long long positionTicks,
                                                      QString playSessionId)
{
    if (playSessionId.isEmpty())
        co_return;
    ensureValidProfile();

    ApiClient *client = m_serverManager->activeClient();
    if (!client)
    {
        qWarning() << "[API Warning] Playback Stopped skipped: active client is no "
                      "longer available."
                   << "ItemId:" << itemId << "MediaSourceId:" << mediaSourceId << "SessionId:" << playSessionId;
        co_return;
    }

    QJsonObject payload;
    payload["ItemId"] = itemId;
    payload["MediaSourceId"] = mediaSourceId;
    payload["PositionTicks"] = static_cast<qint64>(positionTicks);
    payload["PlayMethod"] = "DirectPlay";
    payload["PlaySessionId"] = playSessionId;

    try
    {
        co_await client->post("/Sessions/Playing/Stopped", payload);
        qDebug() << "[API] Playback Stopped successfully. SessionId:" << playSessionId;
    }
    catch (const std::exception &e)
    {
        qDebug() << "[API Error] Playback Stop failed:" << e.what();
    }
}

void MediaService::clearRecommendCache()
{
    m_recommendCache.clear();
    Q_EMIT recommendCacheCleared();
}




QString RecommendCache::cacheFilePath(const QString &serverId)
{
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
           QStringLiteral("/qEmby_RecommendCache_%1.json").arg(serverId);
}

void RecommendCache::clear()
{
    QString cachedServerId = serverId; 
    items.clear();
    fetchTime = QDateTime();
    userId.clear();
    serverId.clear();
    if (!cachedServerId.isEmpty())
    {
        QFile::remove(cacheFilePath(cachedServerId));
    }
}

void RecommendCache::saveToDisk() const
{
    QJsonArray itemsArray;
    for (const auto &item : items)
    {
        
        QJsonObject obj;
        obj["Id"] = item.id;
        obj["Name"] = item.name;
        obj["Type"] = item.type;
        obj["ProductionYear"] = item.productionYear;
        obj["RecursiveItemCount"] = item.recursiveItemCount;
        
        QJsonObject imgTags;
        if (!item.images.primaryTag.isEmpty())
            imgTags["Primary"] = item.images.primaryTag;
        if (!item.images.thumbTag.isEmpty())
            imgTags["Thumb"] = item.images.thumbTag;
        if (!item.images.backdropTag.isEmpty())
            imgTags["Backdrop"] = item.images.backdropTag;
        if (!imgTags.isEmpty())
            obj["ImageTags"] = imgTags;
        if (item.images.primaryImageAspectRatio > 0)
            obj["PrimaryImageAspectRatio"] = item.images.primaryImageAspectRatio;
        itemsArray.append(obj);
    }

    QJsonObject root;
    root["formatVersion"] = kRecommendCacheFormatVersion;
    root["serverId"] = serverId;
    root["userId"] = userId;
    root["fetchTime"] = fetchTime.toString(Qt::ISODate);
    root["items"] = itemsArray;

    QString filePath = cacheFilePath(serverId);
    QDir().mkpath(QFileInfo(filePath).absolutePath());
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly))
    {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        qDebug() << "[RecommendCache] Saved" << items.size() << "items to disk for server" << serverId;
    }
}

bool RecommendCache::loadFromDisk(const QString &currentServerId, const QString &currentUserId, int cacheDurationHours)
{
    QFile file(cacheFilePath(currentServerId));
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    const int formatVersion = root["formatVersion"].toInt();
    if (formatVersion < kRecommendCacheFormatVersion)
    {
        qDebug() << "[RecommendCache] Disk cache format is outdated, ignoring "
                    "cache for server"
                 << currentServerId << "| cachedVersion:" << formatVersion
                 << "| requiredVersion:" << kRecommendCacheFormatVersion;
        return false;
    }

    QString cachedServerId = root["serverId"].toString();
    QString cachedUserId = root["userId"].toString();
    QDateTime cachedTime = QDateTime::fromString(root["fetchTime"].toString(), Qt::ISODate);

    
    if (cachedServerId != currentServerId)
        return false;
    if (cachedUserId != currentUserId)
        return false;
    if (cachedTime.secsTo(QDateTime::currentDateTime()) >= cacheDurationHours * 3600)
        return false;

    QJsonArray itemsArray = root["items"].toArray();
    QList<MediaItem> loadedItems;
    loadedItems.reserve(itemsArray.size());
    for (const auto &val : itemsArray)
    {
        loadedItems.append(MediaItem::fromJson(val.toObject()));
    }

    
    items = loadedItems;
    fetchTime = cachedTime;
    serverId = cachedServerId;
    userId = cachedUserId;
    qDebug() << "[RecommendCache] Loaded" << items.size() << "items from disk for server" << serverId;
    return true;
}
