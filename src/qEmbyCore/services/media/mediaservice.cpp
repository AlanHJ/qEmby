#include "mediaservice.h"
#include "../../api/apiclient.h"
#include "../../config/config_keys.h"
#include "../../config/configstore.h"
#include "../manager/servermanager.h"
#include <QBuffer>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
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
#include <QUrlQuery>
#include <QUuid>
#include <qcoronetwork.h>
#include <stdexcept>
#include <utility>

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

MediaQueryPage parseMediaQueryPage(const QJsonObject& response, int startIndex,
                                   int limit)
{
    MediaQueryPage page;
    page.items = parseJsonArray<MediaItem>(response.value("Items").toArray());
    page.startIndex = qMax(0, startIndex);
    page.limit = limit;
    page.totalRecordCount =
        response.value(QStringLiteral("TotalRecordCount"))
            .toInt(page.startIndex + page.items.size());
    return page;
}

constexpr int kRecommendCacheFormatVersion = 3;

QString appendMediaCardTooltipFields(QString fieldsCsv)
{
    QStringList fields =
        fieldsCsv.split(QLatin1Char(','), Qt::SkipEmptyParts);

    for (QString& field : fields) {
        field = field.trimmed();
    }

    const QStringList tooltipFields = {
        QStringLiteral("PremiereDate"),
        QStringLiteral("RunTimeTicks"),
        QStringLiteral("Overview")
    };

    for (const QString& field : tooltipFields) {
        if (!fields.contains(field, Qt::CaseInsensitive)) {
            fields.append(field);
        }
    }

    return fields.join(QLatin1Char(','));
}

QString findUserViewIdByCollectionType(const QList<MediaItem> &views,
                                       QString collectionType)
{
    collectionType = collectionType.trimmed();
    if (collectionType.isEmpty())
    {
        return {};
    }

    for (const MediaItem &view : views)
    {
        if (view.collectionType.compare(collectionType, Qt::CaseInsensitive) == 0
            && !view.id.isEmpty())
        {
            return view.id;
        }
    }

    return {};
}

int effectivePort(const QUrl& url)
{
    if (url.port() > 0) {
        return url.port();
    }

    if (url.scheme().compare(QStringLiteral("https"), Qt::CaseInsensitive) == 0) {
        return 443;
    }
    if (url.scheme().compare(QStringLiteral("http"), Qt::CaseInsensitive) == 0) {
        return 80;
    }
    return -1;
}

bool isSameOrigin(const QUrl& left, const QUrl& right)
{
    return left.scheme().compare(right.scheme(), Qt::CaseInsensitive) == 0 &&
           left.host().compare(right.host(), Qt::CaseInsensitive) == 0 &&
           effectivePort(left) == effectivePort(right);
}

NetworkRequestOptions buildImageRequestOptions(const ServerProfile& profile,
                                               const QUrl& requestUrl)
{
    NetworkRequestOptions options;
    if (!profile.isValid() || !requestUrl.isValid()) {
        return options;
    }

    const QUrl profileUrl(profile.url);
    if (profile.ignoreSslVerification &&
        isSameOrigin(requestUrl, profileUrl)) {
        options.ignoreSslErrors = true;
    }

    return options;
}

struct ResolvedImageRequest {
    QUrl url;

    bool isValid() const
    {
        return url.isValid() && !url.scheme().trimmed().isEmpty();
    }
};

QString sanitizeMimeType(QString mimeType)
{
    const int separatorIndex = mimeType.indexOf(QLatin1Char(';'));
    if (separatorIndex >= 0) {
        mimeType = mimeType.left(separatorIndex);
    }

    return mimeType.trimmed().toLower();
}

QString detectImageMimeType(const QByteArray& data)
{
    if (data.isEmpty()) {
        return {};
    }

    QBuffer buffer;
    buffer.setData(data);
    if (!buffer.open(QIODevice::ReadOnly)) {
        return {};
    }

    QImageReader reader(&buffer);
    const QString format = QString::fromLatin1(reader.format()).trimmed().toLower();
    if (format.isEmpty()) {
        return {};
    }

    return QStringLiteral("image/%1").arg(format);
}

QString buildItemImagePath(QString itemId, QString imageType, int imageIndex)
{
    itemId = itemId.trimmed();
    imageType = imageType.trimmed();

    QString path = QStringLiteral("/Items/%1/Images/%2").arg(itemId, imageType);
    if (imageIndex >= 0) {
        path += QStringLiteral("/%1").arg(imageIndex);
    }

    return path;
}

QString buildImageCacheKey(QString itemId, QString imageType, int imageIndex)
{
    return QStringLiteral("%1|%2|%3")
        .arg(itemId.trimmed(), imageType.trimmed())
        .arg(imageIndex);
}

ResolvedImageRequest resolveImageRequest(const ServerManager* serverManager,
                                        QString imageUrl)
{
    ResolvedImageRequest result;

    imageUrl = imageUrl.trimmed();
    if (imageUrl.isEmpty()) {
        return result;
    }

    const ServerProfile profile =
        serverManager ? serverManager->activeProfile() : ServerProfile{};
    QUrl resolvedUrl(imageUrl);

    if (resolvedUrl.isRelative() && profile.isValid()) {
        QUrl baseUrl(profile.url);
        if (baseUrl.isValid() && baseUrl.path().isEmpty()) {
            baseUrl.setPath(QStringLiteral("/"));
        }
        resolvedUrl = baseUrl.resolved(QUrl(imageUrl));
    }

    if (!resolvedUrl.isValid() || resolvedUrl.scheme().trimmed().isEmpty()) {
        return result;
    }

    if (profile.isValid() && !profile.accessToken.isEmpty() &&
        isSameOrigin(resolvedUrl, QUrl(profile.url))) {
        QUrlQuery query(resolvedUrl);
        if (!query.hasQueryItem(QStringLiteral("api_key"))) {
            query.addQueryItem(QStringLiteral("api_key"), profile.accessToken);
            resolvedUrl.setQuery(query);
        }
    }

    result.url = resolvedUrl;
    return result;
}
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

QCoro::Task<QList<MediaItem>> MediaService::getUserViews(bool includeHidden)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    if (m_userViewsCache.isValid(profile.id, profile.userId, includeHidden))
    {
        qDebug() << "[MediaService] getUserViews cache hit"
                 << "| includeHidden=" << includeHidden
                 << "| count=" << m_userViewsCache.views.size();
        co_return m_userViewsCache.views;
    }

    
    QString path = QString("/Users/%1/Views").arg(profile.userId);
    if (includeHidden)
    {
        path += QStringLiteral("?IncludeHidden=true");
    }

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    const QList<MediaItem> views = parseJsonArray<MediaItem>(response["Items"].toArray());
    updateUserViewsCache(views, profile.id, profile.userId, includeHidden);

    qDebug() << "[MediaService] getUserViews fetched"
             << "| includeHidden=" << includeHidden
             << "| count=" << views.size()
             << "| playlistViewId="
             << findUserViewIdByCollectionType(views, QStringLiteral("playlists"));
    co_return views;
}

void MediaService::clearUserViewsCache()
{
    m_userViewsCache.clear();
}

QCoro::Task<QList<MediaItem>> MediaService::getLibraryItems(const QString &parentId, const QString &sortBy,
                                                            const QString &sortOrder, const QString &filters,
                                                            const QString &includeItemTypes, int startIndex, int limit,
                                                            bool recursive, bool includeChildCount)
{
    const MediaQueryPage page =
        co_await getLibraryItemsPage(parentId, sortBy, sortOrder, filters,
                                     includeItemTypes, startIndex, limit,
                                     recursive, includeChildCount);
    co_return page.items;
}

QCoro::Task<MediaQueryPage> MediaService::getLibraryItemsPage(const QString &parentId, const QString &sortBy,
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
                          QStringLiteral("MediaType"),
                          QStringLiteral("DateCreated")};
    if (includeChildCount)
    {
        fields.append(QStringLiteral("ChildCount"));
    }

    const QString fieldQuery =
        appendMediaCardTooltipFields(fields.join(QLatin1Char(',')));

    QString path = QString("/Users/%1/Items?ParentId=%2"
                           "&Fields=%3"
                           "&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(profile.userId, parentId, fieldQuery);

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
    const MediaQueryPage page = parseMediaQueryPage(response, startIndex, limit);
    qDebug() << "[MediaService] getLibraryItemsPage"
             << "| parentId=" << parentId
             << "| startIndex=" << startIndex
             << "| limit=" << limit
             << "| recursive=" << recursive
             << "| includeTypes=" << includeItemTypes
             << "| filters=" << filters
             << "| returned=" << page.items.size()
             << "| total=" << page.totalRecordCount;
    co_return page;
}

QCoro::Task<QList<MediaItem>> MediaService::searchMedia(const QString &searchTerm, const QString &includeItemTypes,
                                                        const QString &sortBy, const QString &sortOrder, int limit)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("ProductionYear,RecursiveItemCount,CanDownload"));
    QString path = QString("/Users/%1/"
                           "Items?SearchTerm=%2&Recursive=true&Fields="
                           "%3")
                       .arg(profile.userId, QUrl::toPercentEncoding(searchTerm),
                            fieldQuery);

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
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,RecursiveItemCount,CanDownload"));
    QString path = QString("/Shows/%1/"
                           "Seasons?UserId=%2&Fields=%3&"
                           "EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(seriesId, m_serverManager->activeProfile().userId,
                            fieldQuery);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}




QCoro::Task<QList<MediaItem>> MediaService::getEpisodes(const QString &seriesId, const QString &seasonId,
                                                        const QString &sortBy, const QString &sortOrder)
{
    ensureValidProfile();
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,Overview,CanDownload"));
    QString path = QString("/Shows/%1/"
                           "Episodes?SeasonId=%2&UserId=%3&Fields=%4&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(seriesId, seasonId,
                            m_serverManager->activeProfile().userId, fieldQuery);

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
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,Overview,CanDownload"));
    QString path = QString("/Shows/"
                           "NextUp?UserId=%1&Fields=%2&"
                           "EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(m_serverManager->activeProfile().userId, fieldQuery);

    if (!seriesId.isEmpty())
    {
        path += QString("&SeriesId=%1").arg(seriesId);
    }

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QPixmap> MediaService::fetchImage(const QString &itemId,
                                              const QString &imageType,
                                              const QString &imageTag,
                                              int maxWidth, int imageIndex)
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

    const QString trimmedItemId = itemId.trimmed();
    const QString trimmedImageType = imageType.trimmed();
    const QString imagePath =
        buildItemImagePath(trimmedItemId, trimmedImageType, imageIndex);
    const QString imageCacheKey =
        buildImageCacheKey(trimmedItemId, trimmedImageType, imageIndex);
    const quint64 cacheVersion =
        m_invalidatedImageRequestVersions.value(imageCacheKey, 0);

    QString urlStr;
    if (effectiveMaxWidth > 0)
    {
        if (!imageTag.isEmpty())
        {
            urlStr = QString("%1%2?tag=%3&maxWidth=%4&api_key=%5")
                         .arg(profile.url, imagePath, imageTag)
                         .arg(effectiveMaxWidth)
                         .arg(profile.accessToken);
        }
        else
        {
            urlStr = QString("%1%2?maxWidth=%3&api_key=%4")
                         .arg(profile.url, imagePath)
                         .arg(effectiveMaxWidth)
                         .arg(profile.accessToken);
        }
    }
    else
    {
        
        if (!imageTag.isEmpty())
        {
            urlStr = QString("%1%2?tag=%3&api_key=%4")
                         .arg(profile.url, imagePath, imageTag,
                              profile.accessToken);
        }
        else
        {
            urlStr = QString("%1%2?api_key=%3")
                         .arg(profile.url, imagePath, profile.accessToken);
        }
    }

    if (cacheVersion > 0) {
        urlStr += QStringLiteral("&qemby_image_rev=%1").arg(cacheVersion);
        qDebug() << "[MediaService] fetchImage using invalidated cache version"
                 << "| itemId=" << trimmedItemId
                 << "| imageType=" << trimmedImageType
                 << "| imageIndex=" << imageIndex
                 << "| version=" << cacheVersion;
    }

    QNetworkRequest request((QUrl(urlStr)));
    const NetworkRequestOptions requestOptions =
        buildImageRequestOptions(profile, request.url());
    NetworkManager::applyRequestOptions(request, requestOptions);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         cacheVersion > 0 ? QNetworkRequest::PreferNetwork
                                          : QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    QNetworkReply *reply = m_imageManager->get(request);
    NetworkManager::attachReplyHandlers(reply, requestOptions,
                                        QStringLiteral("GET_IMAGE"));
    co_await reply;

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
        else
        {
            qWarning() << "[MediaService] fetchImage returned invalid image"
                       << "| itemId=" << trimmedItemId
                       << "| imageType=" << trimmedImageType
                       << "| imageIndex=" << imageIndex
                       << "| bytes=" << data.size();
        }
    }
    else
    {
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qWarning() << "[MediaService] fetchImage failed"
                   << "| itemId=" << trimmedItemId
                   << "| imageType=" << trimmedImageType
                   << "| imageIndex=" << imageIndex
                   << "| url=" << request.url().toString(QUrl::RemoveQuery)
                   << "| error="
                   << NetworkManager::buildReplyErrorMessage(reply, httpStatus);
    }

    reply->deleteLater();
    co_return result;
}

void MediaService::invalidateImageCache(QString itemId, QString imageType,
                                        int imageIndex)
{
    itemId = itemId.trimmed();
    imageType = imageType.trimmed();
    if (itemId.isEmpty() || imageType.isEmpty()) {
        qWarning() << "[MediaService] invalidateImageCache skipped"
                   << "| itemId=" << itemId
                   << "| imageType=" << imageType
                   << "| imageIndex=" << imageIndex;
        return;
    }

    const quint64 nextVersion = ++m_nextImageRequestVersion;
    m_invalidatedImageRequestVersions.insert(
        buildImageCacheKey(itemId, imageType, imageIndex), nextVersion);
    if (imageIndex >= 0) {
        m_invalidatedImageRequestVersions.insert(
            buildImageCacheKey(itemId, imageType, -1), nextVersion);
    }

    qDebug() << "[MediaService] invalidateImageCache"
             << "| itemId=" << itemId
             << "| imageType=" << imageType
             << "| imageIndex=" << imageIndex
             << "| version=" << nextVersion;
}

QCoro::Task<QPixmap> MediaService::fetchImageByUrl(QString imageUrl)
{
    try {
        const DownloadedImageData downloadedImage =
            co_await downloadImageByUrl(std::move(imageUrl));

        QImage image;
        image.loadFromData(downloadedImage.data);
        if (image.isNull()) {
            qWarning() << "[MediaService] fetchImageByUrl returned invalid image"
                       << "| bytes=" << downloadedImage.data.size();
            co_return QPixmap();
        }

        co_return QPixmap::fromImage(image);
    } catch (const std::exception& e) {
        qWarning() << "[MediaService] fetchImageByUrl failed"
                   << "| error=" << e.what();
        co_return QPixmap();
    }
}

QCoro::Task<DownloadedImageData> MediaService::downloadImageByUrl(QString imageUrl)
{
    const ResolvedImageRequest resolvedRequest =
        resolveImageRequest(m_serverManager, imageUrl);
    if (!resolvedRequest.isValid()) {
        qWarning() << "[MediaService] downloadImageByUrl skipped invalid url"
                   << "| imageUrl=" << imageUrl;
        throw std::runtime_error(tr("Invalid image URL").toStdString());
    }

    QNetworkRequest request(resolvedRequest.url);
    const ServerProfile profile =
        m_serverManager ? m_serverManager->activeProfile() : ServerProfile{};
    const NetworkRequestOptions requestOptions =
        buildImageRequestOptions(profile, resolvedRequest.url);
    NetworkManager::applyRequestOptions(request, requestOptions);
    request.setAttribute(QNetworkRequest::CacheLoadControlAttribute,
                         QNetworkRequest::PreferCache);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);

    qDebug() << "[MediaService] downloadImageByUrl"
             << "| url=" << resolvedRequest.url.toString(QUrl::RemoveQuery);

    QNetworkReply* reply = m_imageManager->get(request);
    NetworkManager::attachReplyHandlers(reply, requestOptions,
                                        QStringLiteral("GET_IMAGE_URL"));
    co_await reply;

    if (reply->error() != QNetworkReply::NoError) {
        const int httpStatus =
            reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const QString errorString =
            NetworkManager::buildReplyErrorMessage(reply, httpStatus).trimmed();
        qWarning() << "[MediaService] downloadImageByUrl failed"
                   << "| url=" << resolvedRequest.url.toString(QUrl::RemoveQuery)
                   << "| error=" << errorString;
        reply->deleteLater();
        throw std::runtime_error(errorString.toStdString());
    }

    DownloadedImageData result;
    result.data = reply->readAll();
    result.mimeType =
        sanitizeMimeType(reply->header(QNetworkRequest::ContentTypeHeader).toString());
    reply->deleteLater();

    if (!result.mimeType.startsWith(QStringLiteral("image/"))) {
        result.mimeType = detectImageMimeType(result.data);
    }

    if (result.data.isEmpty() ||
        !result.mimeType.startsWith(QStringLiteral("image/"))) {
        qWarning() << "[MediaService] downloadImageByUrl returned invalid image"
                   << "| url=" << resolvedRequest.url.toString(QUrl::RemoveQuery)
                   << "| bytes=" << result.data.size()
                   << "| mimeType=" << result.mimeType;
        throw std::runtime_error(
            tr("Downloaded data is not a valid image").toStdString());
    }

    co_return result;
}

QCoro::Task<QList<MediaItem>> MediaService::getResumeItems(int limit, const QString &sortBy, const QString &sortOrder)
{
    ensureValidProfile();
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("ProductionYear,RecursiveItemCount,CanDownload"));
    QString path = QString("/Users/%1/Items/"
                           "Resume?Recursive=true&Fields=%2&MediaTypes=Video")
                       .arg(m_serverManager->activeProfile().userId, fieldQuery);
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
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("ProductionYear,RecursiveItemCount,CanDownload"));
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Fields=%2&IncludeItemTypes=Movie,Series")
                       .arg(m_serverManager->activeProfile().userId, fieldQuery);
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

    
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("ProductionYear,RecursiveItemCount,PrimaryImageAspectRatio,CanDownload"));
    QString path = QString("/Users/%1/Items?IncludeItemTypes=Movie,Series"
                           "&Recursive=true"
                           "&Fields=%2"
                           "&EnableImageTypes=Primary,Backdrop,Thumb"
                           "&ImageTypeLimit=1")
                       .arg(currentUserId, fieldQuery);
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
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("ProductionYear,CanDownload"));
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&"
                           "IncludeItemTypes=Movie&Fields=%2")
                       .arg(m_serverManager->activeProfile().userId, fieldQuery);
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
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("ProductionYear,RecursiveItemCount,CanDownload"));
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&Filters=IsFavorite&IncludeItemTypes=Series&"
                           "Fields=%2")
                       .arg(m_serverManager->activeProfile().userId, fieldQuery);
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
    const ServerProfile profile = m_serverManager->activeProfile();
    QString path = QString("/Users/%1/Items/"
                           "%2?Fields=MediaStreams,MediaSources,People,Overview,Genres,"
                           "ProductionYear,OfficialRating,Tags,Studios,ExternalUrls,CanDownload")
                       .arg(profile.userId, itemId);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    MediaItem item = MediaItem::fromJson(response);
    updateUserViewsCache(item, profile.id, profile.userId);
    co_return item;
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
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,ProductionYear,CanDownload"));
    QString path = QString("/Items/%1/"
                           "Similar?UserId=%2&Limit=%3&Fields=%4")
                       .arg(itemId, m_serverManager->activeProfile().userId)
                       .arg(limit)
                       .arg(fieldQuery);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getItemCollections(const QString &itemId)
{
    ensureValidProfile();
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,CanDownload"));
    QString path = QString("/Users/%1/"
                           "Items?IncludeItemTypes=Playlist,BoxSet&Recursive="
                           "true&ListItemIds=%2&Fields=%3")
                       .arg(m_serverManager->activeProfile().userId, itemId,
                            fieldQuery);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    co_return parseJsonArray<MediaItem>(response["Items"].toArray());
}

QCoro::Task<QList<MediaItem>> MediaService::getCollectionItems(const QString &collectionId)
{
    ensureValidProfile();
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,ProductionYear,RecursiveItemCount,CanDownload"));
    QString path = QString("/Users/%1/"
                           "Items?ParentId=%2&Fields=%3&SortBy=SortName")
                       .arg(m_serverManager->activeProfile().userId, collectionId,
                            fieldQuery);

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
    const MediaQueryPage page =
        co_await getItemsByPersonPage(personId, sortBy, sortOrder);
    co_return page.items;
}

QCoro::Task<MediaQueryPage> MediaService::getItemsByPersonPage(const QString &personId, const QString &sortBy,
                                                               const QString &sortOrder, int startIndex, int limit)
{
    ensureValidProfile();
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,ProductionYear,RecursiveItemCount,CanDownload"));
    QString path = QString("/Users/%1/"
                           "Items?Recursive=true&PersonIds=%2&IncludeItemTypes="
                           "Movie,Series,Episode&Fields=%3")
                       .arg(m_serverManager->activeProfile().userId, personId,
                            fieldQuery);

    if (!sortBy.isEmpty())
        path += QString("&SortBy=%1").arg(sortBy);
    if (!sortOrder.isEmpty())
        path += QString("&SortOrder=%1").arg(sortOrder);
    if (startIndex > 0)
        path += QString("&StartIndex=%1").arg(startIndex);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    const MediaQueryPage page = parseMediaQueryPage(response, startIndex, limit);
    qDebug() << "[MediaService] getItemsByPersonPage"
             << "| personId=" << personId
             << "| startIndex=" << startIndex
             << "| limit=" << limit
             << "| returned=" << page.items.size()
             << "| total=" << page.totalRecordCount;
    co_return page;
}

QCoro::Task<QList<MediaItem>> MediaService::getItemsByFilter(const QString &genreFilter, const QString &tagFilter,
                                                             const QString &studioFilter, const QString &sortBy,
                                                             const QString &sortOrder, int limit)
{
    const MediaQueryPage page =
        co_await getItemsByFilterPage(genreFilter, tagFilter, studioFilter,
                                      sortBy, sortOrder, 0, limit);
    co_return page.items;
}

QCoro::Task<MediaQueryPage> MediaService::getItemsByFilterPage(const QString &genreFilter, const QString &tagFilter,
                                                               const QString &studioFilter, const QString &sortBy,
                                                               const QString &sortOrder, int startIndex, int limit)
{
    ensureValidProfile();
    const QString fieldQuery = appendMediaCardTooltipFields(
        QStringLiteral("PrimaryImageAspectRatio,ProductionYear,RecursiveItemCount,CanDownload"));
    QString path = QString("/Users/%1/Items?Recursive=true"
                           "&IncludeItemTypes=Movie,Series"
                           "&Fields=%2"
                           "&EnableImageTypes=Primary,Backdrop,Thumb&ImageTypeLimit=1")
                       .arg(m_serverManager->activeProfile().userId, fieldQuery);

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
    if (startIndex > 0)
        path += QString("&StartIndex=%1").arg(startIndex);
    if (limit > 0)
        path += QString("&Limit=%1").arg(limit);

    qDebug() << "[MediaService] getItemsByFilterPage"
             << "| genre=" << genreFilter
             << "| tag=" << tagFilter
             << "| studio=" << studioFilter
             << "| sortBy=" << sortBy
             << "| sortOrder=" << sortOrder
             << "| startIndex=" << startIndex
             << "| limit=" << limit;

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);
    const MediaQueryPage page = parseMediaQueryPage(response, startIndex, limit);
    qDebug() << "[MediaService] getItemsByFilterPage result"
             << "| returned=" << page.items.size()
             << "| total=" << page.totalRecordCount;
    co_return page;
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

void MediaService::removeRecommendCacheItem(const QString& itemId)
{
    const QString trimmedItemId = itemId.trimmed();
    if (trimmedItemId.isEmpty() || !m_serverManager ||
        !m_serverManager->activeProfile().isValid()) {
        return;
    }

    const ServerProfile profile = m_serverManager->activeProfile();
    int cacheDurationHours =
        ConfigStore::instance()
            ->get<QString>(ConfigKeys::DataCacheDuration, "24")
            .toInt();
    if (cacheDurationHours <= 0) {
        cacheDurationHours = 24;
    }

    bool hasUsableCache =
        m_recommendCache.isValid(profile.id, profile.userId, cacheDurationHours);
    if (!hasUsableCache) {
        hasUsableCache =
            m_recommendCache.loadFromDisk(profile.id, profile.userId,
                                          cacheDurationHours);
    }

    if (!hasUsableCache) {
        qDebug() << "[MediaService] Skip recommend cache item removal"
                 << "| reason=no-usable-cache"
                 << "| itemId=" << trimmedItemId;
        return;
    }

    const int previousCount = m_recommendCache.items.size();
    for (int i = m_recommendCache.items.size() - 1; i >= 0; --i) {
        if (m_recommendCache.items.at(i).id == trimmedItemId) {
            m_recommendCache.items.removeAt(i);
        }
    }

    if (m_recommendCache.items.size() == previousCount) {
        qDebug() << "[MediaService] Skip recommend cache item removal"
                 << "| reason=item-not-found"
                 << "| itemId=" << trimmedItemId
                 << "| cachedCount=" << previousCount;
        return;
    }

    if (m_recommendCache.items.isEmpty()) {
        m_recommendCache.clear();
    } else {
        m_recommendCache.saveToDisk();
    }

    qDebug() << "[MediaService] Removed item from recommend cache"
             << "| itemId=" << trimmedItemId
             << "| previousCount=" << previousCount
             << "| currentCount=" << m_recommendCache.items.size();
}

void MediaService::updateUserViewsCache(QList<MediaItem> views, QString serverId,
                                        QString userId, bool includesHidden)
{
    m_userViewsCache.views = std::move(views);
    m_userViewsCache.serverId = std::move(serverId);
    m_userViewsCache.userId = std::move(userId);
    m_userViewsCache.includesHidden = includesHidden;
}

void MediaService::updateUserViewsCache(MediaItem view, QString serverId,
                                        QString userId)
{
    const QString viewId = view.id.trimmed();
    const QString viewType = view.type.trimmed();
    const QString collectionType = view.collectionType.trimmed();

    if (viewId.isEmpty() || collectionType.isEmpty())
    {
        return;
    }

    if (!m_userViewsCache.isValid(serverId, userId, false)
        || m_userViewsCache.views.isEmpty())
    {
        qDebug() << "[MediaService] Skipping partial user view cache update"
                 << "| reason=base-cache-missing"
                 << "| itemId=" << viewId
                 << "| itemType=" << viewType
                 << "| collectionType=" << collectionType;
        return;
    }

    for (MediaItem &cachedView : m_userViewsCache.views)
    {
        if (cachedView.id == viewId)
        {
            cachedView = std::move(view);
            qDebug() << "[MediaService] Patched cached user view"
                     << "| itemId=" << viewId
                     << "| itemType=" << viewType
                     << "| collectionType=" << collectionType
                     << "| cachedCount=" << m_userViewsCache.views.size();
            return;
        }
    }

    qDebug() << "[MediaService] Skipping partial user view cache update"
             << "| reason=view-not-found"
             << "| itemId=" << viewId
             << "| itemType=" << viewType
             << "| collectionType=" << collectionType
             << "| cachedCount=" << m_userViewsCache.views.size();
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
