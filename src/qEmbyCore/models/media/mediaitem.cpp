#include "mediaitem.h"
#include <QDateTime> 

MediaPersonInfo MediaPersonInfo::fromJson(const QJsonObject& obj) {
    MediaPersonInfo person;
    person.id = obj["Id"].toString();
    person.name = obj["Name"].toString();
    person.role = obj["Role"].toString();
    person.type = obj["Type"].toString();
    if (obj.contains("PrimaryImageTag")) {
        person.primaryImageTag = obj["PrimaryImageTag"].toString();
    }
    return person;
}

MediaStudioInfo MediaStudioInfo::fromJson(const QJsonObject& obj) {
    MediaStudioInfo studio;
    studio.name = obj["Name"].toString();
    
    studio.id = obj["Id"].toVariant().toString();
    return studio;
}


MediaExternalUrlInfo MediaExternalUrlInfo::fromJson(const QJsonObject& obj) {
    MediaExternalUrlInfo urlInfo;
    urlInfo.name = obj["Name"].toString();
    urlInfo.url = obj["Url"].toString();
    return urlInfo;
}



QPair<QString, QString> MediaImageInfo::bestPoster() const {
    if (!primaryTag.isEmpty()) return {primaryTag, QStringLiteral("Primary")};
    if (!thumbTag.isEmpty())   return {thumbTag,   QStringLiteral("Thumb")};
    if (!backdropTag.isEmpty()) return {backdropTag, QStringLiteral("Backdrop")};
    
    if (!parentPrimaryTag.isEmpty()) return {parentPrimaryTag, QStringLiteral("Primary")};
    if (!parentBackdropTag.isEmpty()) return {parentBackdropTag, QStringLiteral("Backdrop")};
    return {{}, {}};
}

QPair<QString, QString> MediaImageInfo::bestThumb() const {
    if (!thumbTag.isEmpty())    return {thumbTag,    QStringLiteral("Thumb")};
    if (!backdropTag.isEmpty()) return {backdropTag, QStringLiteral("Backdrop")};
    if (!primaryTag.isEmpty())  return {primaryTag,  QStringLiteral("Primary")};
    
    if (!parentBackdropTag.isEmpty()) return {parentBackdropTag, QStringLiteral("Backdrop")};
    if (!parentThumbTag.isEmpty())    return {parentThumbTag,    QStringLiteral("Thumb")};
    if (!parentPrimaryTag.isEmpty())  return {parentPrimaryTag,  QStringLiteral("Primary")};
    return {{}, {}};
}


QPair<QString, QString> MediaImageInfo::bestBackdrop() const {
    if (!backdropTag.isEmpty()) return {backdropTag, QStringLiteral("Backdrop")};
    if (!thumbTag.isEmpty())    return {thumbTag,    QStringLiteral("Thumb")};
    if (!primaryTag.isEmpty())  return {primaryTag,  QStringLiteral("Primary")};
    
    if (!parentBackdropTag.isEmpty()) return {parentBackdropTag, QStringLiteral("Backdrop")};
    if (!parentThumbTag.isEmpty())    return {parentThumbTag,    QStringLiteral("Thumb")};
    return {{}, {}};
}


bool MediaImageInfo::isParentTag(const QString& tag) const {
    if (tag.isEmpty()) return false;
    return (tag == parentPrimaryTag || tag == parentBackdropTag || tag == parentThumbTag);
}

MediaImageInfo MediaImageInfo::fromJson(const QJsonObject& obj) {
    MediaImageInfo info;
    info.primaryImageAspectRatio = obj["PrimaryImageAspectRatio"].toDouble();

    
    if (obj.contains("ImageTags")) {
        QJsonObject tagsObj = obj["ImageTags"].toObject();
        if (tagsObj.contains("Primary")) info.primaryTag = tagsObj["Primary"].toString();
        if (tagsObj.contains("Thumb")) info.thumbTag = tagsObj["Thumb"].toString();
        if (tagsObj.contains("Logo")) info.logoTag = tagsObj["Logo"].toString();
        if (tagsObj.contains("Backdrop")) info.backdropTag = tagsObj["Backdrop"].toString();
    }

    
    if (obj.contains("PrimaryImageItemId")) {
        info.primaryImageItemId = obj["PrimaryImageItemId"].toString();
    }

    if (info.primaryTag.isEmpty() && obj.contains("PrimaryImageTag")) {
        
        info.primaryTag = obj["PrimaryImageTag"].toString();
    }

    
    if (obj.contains("BackdropImageTags")) {
        QJsonArray backdropTags = obj["BackdropImageTags"].toArray();
        if (!backdropTags.isEmpty()) {
            info.backdropTag = backdropTags[0].toString();
        }
    }

    
    if (obj.contains("SeriesPrimaryImageTag")) {
        info.parentPrimaryTag = obj["SeriesPrimaryImageTag"].toString();
    }
    if (obj.contains("ParentThumbImageTag")) {
        info.parentThumbTag = obj["ParentThumbImageTag"].toString();
    }
    if (obj.contains("ParentBackdropImageTags")) {
        QJsonArray parentBackdrops = obj["ParentBackdropImageTags"].toArray();
        if (!parentBackdrops.isEmpty()) {
            info.parentBackdropTag = parentBackdrops[0].toString();
        }
    }
    
    if (obj.contains("SeriesId")) {
        info.parentImageItemId = obj["SeriesId"].toString();
    }

    return info;
}

MediaUserDataInfo MediaUserDataInfo::fromJson(const QJsonObject& obj) {
    MediaUserDataInfo ud;
    ud.isFavorite = obj["IsFavorite"].toBool();
    ud.playedPercentage = obj["PlayedPercentage"].toDouble();
    ud.playbackPositionTicks = obj["PlaybackPositionTicks"].toVariant().toLongLong();
    ud.played = obj["Played"].toBool();
    ud.playCount = obj["PlayCount"].toInt();
    ud.lastPlayedDate = obj["LastPlayedDate"].toString();
    return ud;
}

MediaItem MediaItem::fromJson(const QJsonObject& obj) {
    MediaItem item;

    
    item.id = obj["Id"].toString();
    item.name = obj["Name"].toString();
    item.originalTitle = obj["OriginalTitle"].toString();
    item.sortName = obj["SortName"].toString();
    item.type = obj["Type"].toString();
    item.mediaType = obj["MediaType"].toString();
    item.collectionType = obj["CollectionType"].toString();
    
    
    item.collectionId = obj["CollectionId"].toString();
    item.collectionName = obj["CollectionName"].toString();
    item.playlistItemId = obj["PlaylistItemId"].toString();

    
    item.seriesId = obj["SeriesId"].toString();
    item.seriesName = obj["SeriesName"].toString();
    item.parentIndexNumber = obj.contains("ParentIndexNumber") ? obj["ParentIndexNumber"].toInt(-1) : -1;
    item.indexNumber = obj.contains("IndexNumber") ? obj["IndexNumber"].toInt(-1) : -1;
    if (obj.contains("ProviderIds")) {
        const QJsonObject providerIdsObj = obj["ProviderIds"].toObject();
        for (auto it = providerIdsObj.constBegin(); it != providerIdsObj.constEnd(); ++it) {
            item.providerIds.insert(it.key(), it.value().toVariant());
        }
    }

    
    item.productionYear = obj["ProductionYear"].toInt();
    item.overview = obj["Overview"].toString();
    item.officialRating = obj["OfficialRating"].toString();
    item.criticRating = obj["CriticRating"].toInt();
    item.runTimeTicks = obj["RunTimeTicks"].toVariant().toLongLong();
    item.communityRating = obj["CommunityRating"].toDouble();
    item.canDownload = obj["CanDownload"].toBool(false);

    
    if (obj.contains("PremiereDate")) {
        item.premiereDate = obj["PremiereDate"].toString();
        int tIdx = item.premiereDate.indexOf('T');
        if (tIdx > 0) item.premiereDate = item.premiereDate.left(tIdx);
    }
    
    
    if (obj.contains("DateCreated")) {
        QString rawDate = obj["DateCreated"].toString();
        item.dateCreatedRaw = rawDate;
        QDateTime dt = QDateTime::fromString(rawDate, Qt::ISODate);
        if (dt.isValid()) {
            item.dateCreated = dt.toLocalTime().toString("yyyy-MM-dd HH:mm");
        } else {
            
            item.dateCreated = rawDate;
            int tIdx = item.dateCreated.indexOf('T');
            if (tIdx > 0 && item.dateCreated.length() >= tIdx + 6) {
                item.dateCreated = item.dateCreated.left(tIdx + 6).replace('T', ' ');
            }
        }
    }

    
    item.container = obj["Container"].toString();
    item.path = obj["Path"].toString();
    item.size = obj["Size"].toVariant().toLongLong();
    item.bitrate = obj["Bitrate"].toVariant().toLongLong();
    item.width = obj["Width"].toInt();
    item.height = obj["Height"].toInt();
    item.childCount = obj["ChildCount"].toInt();
    item.recursiveItemCount = obj["RecursiveItemCount"].toInt(); 

    
    if (obj.contains("UserData")) {
        item.userData = MediaUserDataInfo::fromJson(obj["UserData"].toObject());
    }

    
    
    item.images = MediaImageInfo::fromJson(obj);

    
    
    
    if (obj.contains("GenreItems")) {
        QJsonArray arr = obj["GenreItems"].toArray();
        for (const auto& val : arr) {
            QString name = val.toObject()["Name"].toString();
            if (!name.isEmpty()) item.genres.append(name);
        }
    }
    if (item.genres.isEmpty() && obj.contains("Genres")) {
        QJsonArray arr = obj["Genres"].toArray();
        for (const auto& val : arr) item.genres.append(val.toString());
    }

    if (obj.contains("Taglines")) {
        QJsonArray arr = obj["Taglines"].toArray();
        for (const auto& val : arr) item.taglines.append(val.toString());
    }

    if (obj.contains("TagItems")) {
        QJsonArray arr = obj["TagItems"].toArray();
        for (const auto& val : arr) item.tags.append(val.toObject()["Name"].toString());
    }

    if (obj.contains("RemoteTrailers")) {
        QJsonArray arr = obj["RemoteTrailers"].toArray();
        for (const auto& val : arr) item.remoteTrailers.append(val.toObject()["Url"].toString());
    }

    
    if (obj.contains("People")) {
        QJsonArray arr = obj["People"].toArray();
        for (const auto& val : arr) {
            item.people.append(MediaPersonInfo::fromJson(val.toObject()));
        }
    }

    if (obj.contains("Studios")) {
        QJsonArray arr = obj["Studios"].toArray();
        for (const auto& val : arr) {
            item.studios.append(MediaStudioInfo::fromJson(val.toObject()));
        }
    }

    
    if (obj.contains("ExternalUrls")) {
        QJsonArray arr = obj["ExternalUrls"].toArray();
        for (const auto& val : arr) {
            item.externalUrls.append(MediaExternalUrlInfo::fromJson(val.toObject()));
        }
    }

    if (obj.contains("MediaSources")) {
        QJsonArray arr = obj["MediaSources"].toArray();
        for (const auto& val : arr) {
            item.mediaSources.append(MediaSourceInfo::fromJson(val.toObject()));
        }
    }

    return item;
}
