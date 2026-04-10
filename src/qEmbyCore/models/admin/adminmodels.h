#ifndef ADMINMODELS_H
#define ADMINMODELS_H

#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QMetaType>
#include "../../qEmbyCore_global.h"




struct QEMBYCORE_EXPORT SystemInfo {
    Q_GADGET

public:
    QString serverName;           
    QString version;              
    QString operatingSystem;      
    QString operatingSystemName;  
    QString architecture;         
    QString id;                   
    QString localAddress;         
    QString wanAddress;           
    bool hasUpdateAvailable = false;       
    bool supportsLibraryMonitor = false;   
    bool canSelfRestart = false;           
    bool canSelfUpdate = false;            

    static SystemInfo fromJson(const QJsonObject& obj) {
        SystemInfo info;
        info.serverName          = obj["ServerName"].toString();
        info.version             = obj["Version"].toString();
        info.operatingSystem     = obj["OperatingSystem"].toString();
        info.operatingSystemName = obj["OperatingSystemDisplayName"].toString();
        info.architecture        = obj["SystemArchitecture"].toString();
        info.id                  = obj["Id"].toString();
        info.localAddress        = obj["LocalAddress"].toString();
        info.wanAddress          = obj["WanAddress"].toString();
        info.hasUpdateAvailable      = obj["HasUpdateAvailable"].toBool();
        info.supportsLibraryMonitor  = obj["SupportsLibraryMonitor"].toBool();
        info.canSelfRestart          = obj["CanSelfRestart"].toBool();
        info.canSelfUpdate           = obj["CanSelfUpdate"].toBool();
        return info;
    }
};
Q_DECLARE_METATYPE(SystemInfo)




struct QEMBYCORE_EXPORT SessionInfo {
    Q_GADGET

public:
    QString id;              
    QString userId;          
    QString userName;        
    QString client;          
    QString deviceName;      
    QString deviceId;        
    QString applicationVersion; 
    QString remoteEndPoint;  
    QString lastActivityDate; 

    
    QString nowPlayingItemId;
    QString nowPlayingItemName;
    QString nowPlayingItemType;
    QString nowPlayingSeriesName;      
    int nowPlayingParentIndexNumber = -1; 
    int nowPlayingIndexNumber = -1;       
    QString nowPlayingImageTag;           
    QString nowPlayingThumbTag;           
    QString nowPlayingBackdropTag;        
    QString nowPlayingSeriesId;           
    QString nowPlayingSeriesPrimaryTag;   
    QString parentThumbItemId;            
    QString parentThumbImageTag;          
    QString parentBackdropItemId;         
    QString parentBackdropImageTag;       
    qint64 nowPlayingRunTimeTicks = 0;    

    
    bool isPaused = false;
    qint64 positionTicks = 0;          
    QString playMethod;                

    
    QString transcodingContainer;      
    QString transcodingVideoCodec;     
    QString transcodingAudioCodec;     
    int transcodingBitrate = 0;        
    double transcodingProgress = 0.0;  

    
    bool isNowPlaying() const { return !nowPlayingItemId.isEmpty(); }

    QString seasonEpisodeLabel() const {
        if (nowPlayingParentIndexNumber >= 0 && nowPlayingIndexNumber >= 0) {
            return QString("S%1:E%2")
                .arg(nowPlayingParentIndexNumber)
                .arg(nowPlayingIndexNumber);
        }
        return {};
    }

    
    static QString formatTicks(qint64 ticks) {
        qint64 totalSeconds = ticks / 10000000;
        int hours = totalSeconds / 3600;
        int minutes = (totalSeconds % 3600) / 60;
        int seconds = totalSeconds % 60;
        if (hours > 0)
            return QString("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
        return QString("%1:%2").arg(minutes).arg(seconds, 2, 10, QChar('0'));
    }

    double playbackProgress() const {
        if (nowPlayingRunTimeTicks <= 0) return 0.0;
        return static_cast<double>(positionTicks) / nowPlayingRunTimeTicks;
    }

    static SessionInfo fromJson(const QJsonObject& obj) {
        SessionInfo session;
        session.id                  = obj["Id"].toString();
        session.userId              = obj["UserId"].toString();
        session.userName            = obj["UserName"].toString();
        session.client              = obj["Client"].toString();
        session.deviceName          = obj["DeviceName"].toString();
        session.deviceId            = obj["DeviceId"].toString();
        session.applicationVersion  = obj["ApplicationVersion"].toString();
        session.remoteEndPoint      = obj["RemoteEndPoint"].toString();
        session.lastActivityDate    = obj["LastActivityDate"].toString();

        
        if (obj.contains("NowPlayingItem")) {
            QJsonObject np = obj["NowPlayingItem"].toObject();
            session.nowPlayingItemId   = np["Id"].toString();
            session.nowPlayingItemName = np["Name"].toString();
            session.nowPlayingItemType = np["Type"].toString();
            session.nowPlayingSeriesName = np["SeriesName"].toString();
            session.nowPlayingParentIndexNumber = np["ParentIndexNumber"].toInt(-1);
            session.nowPlayingIndexNumber = np["IndexNumber"].toInt(-1);
            session.nowPlayingRunTimeTicks = np["RunTimeTicks"].toVariant().toLongLong();

            
            QJsonObject imgTags = np["ImageTags"].toObject();
            session.nowPlayingImageTag = imgTags["Primary"].toString();
            session.nowPlayingThumbTag = imgTags["Thumb"].toString();

            
            QJsonArray bdTags = np["BackdropImageTags"].toArray();
            if (!bdTags.isEmpty()) {
                session.nowPlayingBackdropTag = bdTags.first().toString();
            }

            
            session.nowPlayingSeriesId = np["SeriesId"].toString();
            session.nowPlayingSeriesPrimaryTag = np["SeriesPrimaryImageTag"].toString();

            
            session.parentThumbItemId = np["ParentThumbItemId"].toString();
            session.parentThumbImageTag = np["ParentThumbImageTag"].toString();
            session.parentBackdropItemId = np["ParentBackdropItemId"].toString();
            QJsonArray parentBdTags = np["ParentBackdropImageTags"].toArray();
            if (!parentBdTags.isEmpty()) {
                session.parentBackdropImageTag = parentBdTags.first().toString();
            }
        }

        
        if (obj.contains("PlayState")) {
            QJsonObject ps = obj["PlayState"].toObject();
            session.isPaused = ps["IsPaused"].toBool();
            session.positionTicks = ps["PositionTicks"].toVariant().toLongLong();
            session.playMethod = ps["PlayMethod"].toString();
        }

        
        if (obj.contains("TranscodingInfo")) {
            QJsonObject ti = obj["TranscodingInfo"].toObject();
            session.transcodingContainer = ti["Container"].toString();
            session.transcodingVideoCodec = ti["VideoCodec"].toString();
            session.transcodingAudioCodec = ti["AudioCodec"].toString();
            session.transcodingBitrate = ti["Bitrate"].toInt();
            session.transcodingProgress = ti["CompletionPercentage"].toDouble();
        }

        return session;
    }
};
Q_DECLARE_METATYPE(SessionInfo)




struct QEMBYCORE_EXPORT UserInfo {
    Q_GADGET

public:
    QString id;
    QString name;
    QString serverIdRaw;          
    bool isAdministrator = false;
    bool isDisabled = false;
    bool hasPassword = false;
    QString lastLoginDate;        
    QString lastActivityDate;     
    QString primaryImageTag;      

    
    bool enableAllFolders = true;

    static UserInfo fromJson(const QJsonObject& obj) {
        UserInfo user;
        user.id               = obj["Id"].toString();
        user.name             = obj["Name"].toString();
        user.serverIdRaw      = obj["ServerId"].toString();
        user.hasPassword      = obj["HasPassword"].toBool();
        user.lastLoginDate    = obj["LastLoginDate"].toString();
        user.lastActivityDate = obj["LastActivityDate"].toString();
        user.primaryImageTag  = obj["PrimaryImageTag"].toString();

        
        if (obj.contains("Policy")) {
            QJsonObject policy = obj["Policy"].toObject();
            user.isAdministrator  = policy["IsAdministrator"].toBool();
            user.isDisabled       = policy["IsDisabled"].toBool();
            user.enableAllFolders = policy["EnableAllFolders"].toBool();
        }

        return user;
    }
};
Q_DECLARE_METATYPE(UserInfo)




struct QEMBYCORE_EXPORT AdminMediaSubFolderInfo {
    Q_GADGET

public:
    QString id;
    QString guid;
    QString path;
    bool isUserAccessConfigurable = true;

    static AdminMediaSubFolderInfo fromJson(const QJsonObject& obj) {
        AdminMediaSubFolderInfo folder;
        folder.id = obj["Id"].toString();
        folder.guid = obj["Guid"].toString();
        folder.path = obj["Path"].toString();
        folder.isUserAccessConfigurable =
            obj["IsUserAccessConfigurable"].toBool(true);
        return folder;
    }

    QString effectiveId() const {
        return !guid.trimmed().isEmpty() ? guid.trimmed() : id.trimmed();
    }
};
Q_DECLARE_METATYPE(AdminMediaSubFolderInfo)




struct QEMBYCORE_EXPORT AdminMediaFolderInfo {
    Q_GADGET

public:
    QString id;
    QString name;

    static AdminMediaFolderInfo fromJson(const QJsonObject& obj) {
        AdminMediaFolderInfo folder;
        folder.id = obj["Guid"].toString();
        if (folder.id.isEmpty()) {
            folder.id = obj["Id"].toString();
        }
        folder.name = obj["Name"].toString();
        return folder;
    }
};
Q_DECLARE_METATYPE(AdminMediaFolderInfo)




struct QEMBYCORE_EXPORT SelectableMediaFolderInfo {
    Q_GADGET

public:
    QString id;
    QString name;
    bool isUserAccessConfigurable = true;
    QList<AdminMediaSubFolderInfo> subFolders;

    static SelectableMediaFolderInfo fromJson(const QJsonObject& obj) {
        SelectableMediaFolderInfo folder;
        folder.id = obj["Guid"].toString();
        if (folder.id.isEmpty()) {
            folder.id = obj["Id"].toString();
        }
        folder.name = obj["Name"].toString();
        folder.isUserAccessConfigurable =
            obj["IsUserAccessConfigurable"].toBool(true);
        const QJsonArray subFolderArray = obj["SubFolders"].toArray();
        for (const auto& value : subFolderArray) {
            folder.subFolders.append(
                AdminMediaSubFolderInfo::fromJson(value.toObject()));
        }
        return folder;
    }
};
Q_DECLARE_METATYPE(SelectableMediaFolderInfo)




struct QEMBYCORE_EXPORT AdminChannelInfo {
    Q_GADGET

public:
    QString id;
    QString name;

    static AdminChannelInfo fromJson(const QJsonObject& obj) {
        AdminChannelInfo channel;
        channel.id = obj["Guid"].toString();
        if (channel.id.isEmpty()) {
            channel.id = obj["Id"].toString();
        }
        channel.name = obj["Name"].toString();
        return channel;
    }
};
Q_DECLARE_METATYPE(AdminChannelInfo)




struct QEMBYCORE_EXPORT AdminDeviceInfo {
    Q_GADGET

public:
    QString id;
    QString name;
    QString appName;

    static AdminDeviceInfo fromJson(const QJsonObject& obj) {
        AdminDeviceInfo device;
        device.id = obj["ReportedDeviceId"].toString();
        if (device.id.isEmpty()) {
            device.id = obj["Id"].toString();
        }
        device.name = obj["Name"].toString();
        if (device.name.isEmpty()) {
            device.name = obj["DeviceName"].toString();
        }
        device.appName = obj["AppName"].toString();
        return device;
    }
};
Q_DECLARE_METATYPE(AdminDeviceInfo)




struct QEMBYCORE_EXPORT AdminAuthProviderInfo {
    Q_GADGET

public:
    QString id;
    QString name;

    static AdminAuthProviderInfo fromJson(const QJsonObject& obj) {
        AdminAuthProviderInfo provider;
        provider.id = obj["Id"].toString();
        provider.name = obj["Name"].toString();
        return provider;
    }
};
Q_DECLARE_METATYPE(AdminAuthProviderInfo)




struct QEMBYCORE_EXPORT AdminFeatureInfo {
    Q_GADGET

public:
    QString id;
    QString name;

    static AdminFeatureInfo fromJson(const QJsonObject& obj) {
        AdminFeatureInfo feature;
        feature.id = obj["Id"].toString();
        feature.name = obj["Name"].toString();
        return feature;
    }
};
Q_DECLARE_METATYPE(AdminFeatureInfo)




struct QEMBYCORE_EXPORT AdminParentalRatingInfo {
    Q_GADGET

public:
    QString name;
    int value = 0;

    static AdminParentalRatingInfo fromJson(const QJsonObject& obj) {
        AdminParentalRatingInfo rating;
        rating.name = obj["Name"].toString();
        rating.value = obj["Value"].toInt();
        return rating;
    }
};
Q_DECLARE_METATYPE(AdminParentalRatingInfo)




struct QEMBYCORE_EXPORT LocalizationCulture {
    Q_GADGET

public:
    QString name;
    QString displayName;
    QString twoLetterISOLanguageName;
    QString threeLetterISOLanguageName;
    QStringList threeLetterISOLanguageNames;
    QStringList twoLetterISOLanguageNames;

    static LocalizationCulture fromJson(const QJsonObject& obj) {
        LocalizationCulture culture;
        culture.name = obj["Name"].toString();
        culture.displayName = obj["DisplayName"].toString();
        culture.twoLetterISOLanguageName =
            obj["TwoLetterISOLanguageName"].toString();
        culture.threeLetterISOLanguageName =
            obj["ThreeLetterISOLanguageName"].toString();

        const QJsonArray threeLetterNames =
            obj["ThreeLetterISOLanguageNames"].toArray();
        for (const auto& value : threeLetterNames) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty() &&
                !culture.threeLetterISOLanguageNames.contains(text)) {
                culture.threeLetterISOLanguageNames.append(text);
            }
        }

        const QJsonArray twoLetterNames =
            obj["TwoLetterISOLanguageNames"].toArray();
        for (const auto& value : twoLetterNames) {
            const QString text = value.toString().trimmed();
            if (!text.isEmpty() &&
                !culture.twoLetterISOLanguageNames.contains(text)) {
                culture.twoLetterISOLanguageNames.append(text);
            }
        }

        return culture;
    }

    QString canonicalCode() const {
        if (!twoLetterISOLanguageName.trimmed().isEmpty()) {
            return twoLetterISOLanguageName.trimmed();
        }
        if (!twoLetterISOLanguageNames.isEmpty()) {
            return twoLetterISOLanguageNames.first().trimmed();
        }
        if (!threeLetterISOLanguageName.trimmed().isEmpty()) {
            return threeLetterISOLanguageName.trimmed();
        }
        if (!threeLetterISOLanguageNames.isEmpty()) {
            return threeLetterISOLanguageNames.first().trimmed();
        }
        return {};
    }

    QString visibleName() const {
        if (!displayName.trimmed().isEmpty()) {
            return displayName.trimmed();
        }
        return name.trimmed();
    }

    QStringList allCodes() const {
        QStringList codes;
        auto appendUnique = [&codes](const QString& value) {
            const QString trimmed = value.trimmed();
            if (!trimmed.isEmpty() && !codes.contains(trimmed, Qt::CaseInsensitive)) {
                codes.append(trimmed);
            }
        };

        appendUnique(twoLetterISOLanguageName);
        appendUnique(threeLetterISOLanguageName);
        for (const QString& value : twoLetterISOLanguageNames) {
            appendUnique(value);
        }
        for (const QString& value : threeLetterISOLanguageNames) {
            appendUnique(value);
        }
        return codes;
    }
};
Q_DECLARE_METATYPE(LocalizationCulture)




struct QEMBYCORE_EXPORT LocalizationCountry {
    Q_GADGET

public:
    QString name;
    QString displayName;
    QString englishName;
    QString twoLetterISORegionName;
    QString threeLetterISORegionName;

    static LocalizationCountry fromJson(const QJsonObject& obj) {
        LocalizationCountry country;
        country.name = obj["Name"].toString();
        country.displayName = obj["DisplayName"].toString();
        country.englishName = obj["EnglishName"].toString();
        country.twoLetterISORegionName =
            obj["TwoLetterISORegionName"].toString();
        country.threeLetterISORegionName =
            obj["ThreeLetterISORegionName"].toString();
        return country;
    }

    QString canonicalCode() const {
        if (!twoLetterISORegionName.trimmed().isEmpty()) {
            return twoLetterISORegionName.trimmed();
        }
        if (!name.trimmed().isEmpty()) {
            return name.trimmed();
        }
        if (!threeLetterISORegionName.trimmed().isEmpty()) {
            return threeLetterISORegionName.trimmed();
        }
        return {};
    }

    QString visibleName() const {
        if (!displayName.trimmed().isEmpty()) {
            return displayName.trimmed();
        }
        if (!englishName.trimmed().isEmpty()) {
            return englishName.trimmed();
        }
        return name.trimmed();
    }

    QStringList allCodes() const {
        QStringList codes;
        auto appendUnique = [&codes](const QString& value) {
            const QString trimmed = value.trimmed();
            if (!trimmed.isEmpty() && !codes.contains(trimmed, Qt::CaseInsensitive)) {
                codes.append(trimmed);
            }
        };

        appendUnique(name);
        appendUnique(twoLetterISORegionName);
        appendUnique(threeLetterISORegionName);
        return codes;
    }
};
Q_DECLARE_METATYPE(LocalizationCountry)




struct QEMBYCORE_EXPORT VirtualFolder {
    Q_GADGET

public:
    QString id;                   
    QString name;                 
    QString itemId;               
    QString guid;                 
    QString collectionType;       
    QStringList locations;        
    QString primaryImageItemId;   
    QJsonObject libraryOptions;   

    static VirtualFolder fromJson(const QJsonObject& obj) {
        VirtualFolder folder;
        folder.id                 = obj["Id"].toString();
        folder.name               = obj["Name"].toString();
        folder.itemId             = obj["ItemId"].toString();
        folder.guid               = obj["Guid"].toString();
        folder.collectionType     = obj["CollectionType"].toString();
        folder.primaryImageItemId = obj["PrimaryImageItemId"].toString();
        folder.libraryOptions     = obj["LibraryOptions"].toObject();

        if (folder.id.isEmpty()) {
            folder.id = folder.itemId;
        }
        if (folder.itemId.isEmpty()) {
            folder.itemId = folder.id;
        }

        
        if (obj.contains("Locations")) {
            QJsonArray locs = obj["Locations"].toArray();
            for (const auto& loc : locs) {
                folder.locations.append(loc.toString());
            }
        }

        return folder;
    }

    QString virtualFolderId() const {
        return id.isEmpty() ? itemId : id;
    }

    
    QString collectionTypeDisplayName() const {
        if (collectionType == "movies")       return QObject::tr("Movies");
        if (collectionType == "tvshows")      return QObject::tr("TV Shows");
        if (collectionType == "music")        return QObject::tr("Music");
        if (collectionType == "homevideos")   return QObject::tr("Home Videos");
        if (collectionType == "audiobooks" || collectionType == "books")
                                              return QObject::tr("Books");
        if (collectionType == "musicvideos")  return QObject::tr("Music Videos");
        if (collectionType == "boxsets")      return QObject::tr("Collections");
        if (collectionType == "playlists")    return QObject::tr("Playlists");
        if (collectionType.isEmpty())         return QObject::tr("Mixed");
        return collectionType; 
    }
};
Q_DECLARE_METATYPE(VirtualFolder)




struct QEMBYCORE_EXPORT ItemImageInfo {
    Q_GADGET

public:
    QString imageType;
    int imageIndex = -1;
    QString path;
    QString fileName;
    int width = 0;
    int height = 0;
    qint64 size = 0;

    static ItemImageInfo fromJson(const QJsonObject& obj) {
        ItemImageInfo info;
        info.imageType = obj.value(QStringLiteral("ImageType")).toString();
        if (info.imageType.isEmpty()) {
            info.imageType = obj.value(QStringLiteral("Type")).toString();
        }
        info.imageIndex = obj.value(QStringLiteral("ImageIndex")).toInt(-1);
        info.path = obj.value(QStringLiteral("Path")).toString();
        info.fileName = obj.value(QStringLiteral("Filename")).toString();
        info.width = obj.value(QStringLiteral("Width")).toInt();
        info.height = obj.value(QStringLiteral("Height")).toInt();
        info.size = obj.value(QStringLiteral("Size")).toVariant().toLongLong();
        return info;
    }
};
Q_DECLARE_METATYPE(ItemImageInfo)




struct QEMBYCORE_EXPORT RemoteSearchResult {
    Q_GADGET

public:
    QString name;
    int productionYear = 0;
    int indexNumber = -1;
    int indexNumberEnd = -1;
    int parentIndexNumber = -1;
    QString premiereDate;
    QString imageUrl;
    QString searchProviderName;
    QString overview;
    QString albumArtistName;
    QStringList artistNames;
    QJsonObject providerIds;
    QJsonObject rawData;

    QString providerIdSummary() const {
        QStringList pairs;
        const QStringList keys = providerIds.keys();
        for (const QString& key : keys) {
            const QString value = providerIds.value(key).toString().trimmed();
            if (!key.trimmed().isEmpty() && !value.isEmpty()) {
                pairs.append(QStringLiteral("%1=%2").arg(key.trimmed(), value));
            }
        }
        return pairs.join(QStringLiteral(", "));
    }

    QString secondaryText() const {
        QStringList parts;

        if (productionYear > 0) {
            parts.append(QString::number(productionYear));
        }

        if (parentIndexNumber >= 0 && indexNumber >= 0) {
            parts.append(QStringLiteral("S%1E%2")
                             .arg(parentIndexNumber, 2, 10, QChar('0'))
                             .arg(indexNumber, 2, 10, QChar('0')));
        } else if (indexNumber >= 0) {
            parts.append(QStringLiteral("E%1").arg(indexNumber));
        }

        if (!searchProviderName.trimmed().isEmpty()) {
            parts.append(searchProviderName.trimmed());
        }

        return parts.join(QStringLiteral(" | "));
    }

    QJsonObject toJson() const {
        if (!rawData.isEmpty()) {
            return rawData;
        }

        QJsonObject obj;
        obj.insert(QStringLiteral("Name"), name);
        obj.insert(QStringLiteral("ProductionYear"), productionYear);
        obj.insert(QStringLiteral("IndexNumber"), indexNumber);
        obj.insert(QStringLiteral("IndexNumberEnd"), indexNumberEnd);
        obj.insert(QStringLiteral("ParentIndexNumber"), parentIndexNumber);
        obj.insert(QStringLiteral("PremiereDate"), premiereDate);
        obj.insert(QStringLiteral("ImageUrl"), imageUrl);
        obj.insert(QStringLiteral("SearchProviderName"), searchProviderName);
        obj.insert(QStringLiteral("Overview"), overview);
        obj.insert(QStringLiteral("ProviderIds"), providerIds);
        if (!albumArtistName.trimmed().isEmpty()) {
            QJsonObject albumArtist;
            albumArtist.insert(QStringLiteral("Name"), albumArtistName.trimmed());
            obj.insert(QStringLiteral("AlbumArtist"), albumArtist);
        }
        if (!artistNames.isEmpty()) {
            QJsonArray artists;
            for (const QString& artistName : artistNames) {
                if (artistName.trimmed().isEmpty()) {
                    continue;
                }

                QJsonObject artist;
                artist.insert(QStringLiteral("Name"), artistName.trimmed());
                artists.append(artist);
            }
            obj.insert(QStringLiteral("Artists"), artists);
        }
        return obj;
    }

    static RemoteSearchResult fromJson(const QJsonObject& obj) {
        RemoteSearchResult result;
        result.name = obj.value(QStringLiteral("Name")).toString();
        result.productionYear =
            obj.value(QStringLiteral("ProductionYear")).toInt();
        result.indexNumber = obj.value(QStringLiteral("IndexNumber")).toInt(-1);
        result.indexNumberEnd =
            obj.value(QStringLiteral("IndexNumberEnd")).toInt(-1);
        result.parentIndexNumber =
            obj.value(QStringLiteral("ParentIndexNumber")).toInt(-1);
        result.premiereDate = obj.value(QStringLiteral("PremiereDate")).toString();
        result.imageUrl = obj.value(QStringLiteral("ImageUrl")).toString();
        result.searchProviderName =
            obj.value(QStringLiteral("SearchProviderName")).toString();
        result.overview = obj.value(QStringLiteral("Overview")).toString();
        result.providerIds = obj.value(QStringLiteral("ProviderIds")).toObject();
        result.rawData = obj;

        const QJsonValue albumArtistValue = obj.value(QStringLiteral("AlbumArtist"));
        if (albumArtistValue.isObject()) {
            result.albumArtistName =
                albumArtistValue.toObject().value(QStringLiteral("Name")).toString();
        } else if (albumArtistValue.isString()) {
            result.albumArtistName = albumArtistValue.toString();
        }

        const QJsonArray artists = obj.value(QStringLiteral("Artists")).toArray();
        for (const QJsonValue& artistValue : artists) {
            if (artistValue.isObject()) {
                const QString artistName =
                    artistValue.toObject().value(QStringLiteral("Name")).toString().trimmed();
                if (!artistName.isEmpty()) {
                    result.artistNames.append(artistName);
                }
            } else if (artistValue.isString()) {
                const QString artistName = artistValue.toString().trimmed();
                if (!artistName.isEmpty()) {
                    result.artistNames.append(artistName);
                }
            }
        }

        return result;
    }
};
Q_DECLARE_METATYPE(RemoteSearchResult)




struct QEMBYCORE_EXPORT ScheduledTaskInfo {
    Q_GADGET

public:
    QString id;
    QString name;
    QString description;
    QString category;             
    QString state;                
    double currentProgressPercentage = 0.0;
    QJsonArray triggers;          
    bool isHidden = false;        
    QString key;                  

    
    QString lastExecutionResult;  
    QString lastStartTime;        
    QString lastEndTime;          

    bool isRunning() const { return state == "Running"; }
    bool hasTriggers() const { return !triggers.isEmpty(); }

    static ScheduledTaskInfo fromJson(const QJsonObject& obj) {
        ScheduledTaskInfo task;
        task.id          = obj["Id"].toString();
        task.name        = obj["Name"].toString();
        task.description = obj["Description"].toString();
        task.category    = obj["Category"].toString();
        task.state       = obj["State"].toString();
        task.currentProgressPercentage = obj["CurrentProgressPercentage"].toDouble();
        task.triggers    = obj["Triggers"].toArray();
        task.isHidden    = obj["IsHidden"].toBool(false);
        task.key         = obj["Key"].toString();

        
        if (obj.contains("LastExecutionResult")) {
            QJsonObject result = obj["LastExecutionResult"].toObject();
            task.lastExecutionResult = result["Status"].toString();
            task.lastStartTime       = result["StartTimeUtc"].toString();
            task.lastEndTime         = result["EndTimeUtc"].toString();
        }

        return task;
    }
};
Q_DECLARE_METATYPE(ScheduledTaskInfo)




struct QEMBYCORE_EXPORT LogFileInfo {
    Q_GADGET

public:
    QString name;           
    QString dateCreated;    
    QString dateModified;   
    long long size = 0;     

    
    QString readableSize() const {
        if (size < 1024) return QString("%1 B").arg(size);
        if (size < 1024 * 1024) return QString("%1 KB").arg(size / 1024.0, 0, 'f', 1);
        return QString("%1 MB").arg(size / (1024.0 * 1024.0), 0, 'f', 1);
    }

    static LogFileInfo fromJson(const QJsonObject& obj) {
        LogFileInfo log;
        log.name         = obj["Name"].toString();
        log.dateCreated  = obj["DateCreated"].toString();
        log.dateModified = obj["DateModified"].toString();
        log.size         = obj["Size"].toVariant().toLongLong();
        return log;
    }
};
Q_DECLARE_METATYPE(LogFileInfo)




struct QEMBYCORE_EXPORT ActivityLogEntry {
    Q_GADGET

public:
    qint64 id = 0;
    QString name;           
    QString type;           
    QString overview;       
    QString shortOverview;  
    QString date;           
    QString userId;         
    QString severity;       
    QString itemId;         

    static ActivityLogEntry fromJson(const QJsonObject& obj) {
        ActivityLogEntry entry;
        entry.id            = obj["Id"].toVariant().toLongLong();
        entry.name          = obj["Name"].toString();
        entry.type          = obj["Type"].toString();
        entry.overview      = obj["Overview"].toString();
        entry.shortOverview = obj["ShortOverview"].toString();
        entry.date          = obj["Date"].toString();
        entry.userId        = obj["UserId"].toString();
        entry.severity      = obj["Severity"].toString();
        entry.itemId        = obj["ItemId"].toString();
        return entry;
    }
};
Q_DECLARE_METATYPE(ActivityLogEntry)

#endif 
