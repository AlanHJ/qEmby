#ifndef MEDIAITEM_H
#define MEDIAITEM_H

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QStringList>
#include <QList>
#include <QVariant>
#include <QMetaType>
#include <QObject>
#include "../../qEmbyCore_global.h"


#include "playbackinfo.h"


struct QEMBYCORE_EXPORT MediaPersonInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString role MEMBER role)
    Q_PROPERTY(QString type MEMBER type)
    Q_PROPERTY(QString primaryImageTag MEMBER primaryImageTag)

public:
    QString id;
    QString name;
    QString role;
    QString type;
    QString primaryImageTag;

    static MediaPersonInfo fromJson(const QJsonObject& obj);
};
Q_DECLARE_METATYPE(MediaPersonInfo)


struct QEMBYCORE_EXPORT MediaStudioInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)

public:
    QString id;
    QString name;

    static MediaStudioInfo fromJson(const QJsonObject& obj);
};
Q_DECLARE_METATYPE(MediaStudioInfo)


struct QEMBYCORE_EXPORT MediaExternalUrlInfo {
    Q_GADGET
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString url MEMBER url)

public:
    QString name;
    QString url;

    static MediaExternalUrlInfo fromJson(const QJsonObject& obj);
};
Q_DECLARE_METATYPE(MediaExternalUrlInfo)


struct QEMBYCORE_EXPORT MediaImageInfo {
    Q_GADGET
    Q_PROPERTY(QString primaryTag MEMBER primaryTag)
    Q_PROPERTY(QString thumbTag MEMBER thumbTag)
    Q_PROPERTY(QString backdropTag MEMBER backdropTag)
    Q_PROPERTY(QString logoTag MEMBER logoTag)
    Q_PROPERTY(double primaryImageAspectRatio MEMBER primaryImageAspectRatio)
    Q_PROPERTY(QString primaryImageItemId MEMBER primaryImageItemId) 

public:
    QString primaryTag;
    QString thumbTag;
    QString backdropTag;
    QString logoTag;
    double primaryImageAspectRatio = 0.0;
    QString primaryImageItemId; 

    
    QString parentPrimaryTag;
    QString parentBackdropTag;
    QString parentThumbTag;
    QString parentImageItemId;  

    
    
    QPair<QString, QString> bestPoster() const;

    
    
    QPair<QString, QString> bestThumb() const;

    
    
    QPair<QString, QString> bestBackdrop() const;

    
    bool isParentTag(const QString& tag) const;

    static MediaImageInfo fromJson(const QJsonObject& obj);
};
Q_DECLARE_METATYPE(MediaImageInfo)


struct QEMBYCORE_EXPORT MediaUserDataInfo {
    Q_GADGET
    Q_PROPERTY(bool isFavorite MEMBER isFavorite)
    Q_PROPERTY(double playedPercentage MEMBER playedPercentage)
    Q_PROPERTY(long long playbackPositionTicks MEMBER playbackPositionTicks)
    Q_PROPERTY(bool played MEMBER played)
    Q_PROPERTY(int playCount MEMBER playCount)
    Q_PROPERTY(QString lastPlayedDate MEMBER lastPlayedDate)

public:
    bool isFavorite = false;
    double playedPercentage = 0.0;
    long long playbackPositionTicks = 0;
    bool played = false;
    int playCount = 0;             
    QString lastPlayedDate;        

    static MediaUserDataInfo fromJson(const QJsonObject& obj);
};
Q_DECLARE_METATYPE(MediaUserDataInfo)


struct QEMBYCORE_EXPORT MediaItem {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name)
    Q_PROPERTY(QString originalTitle MEMBER originalTitle)
    Q_PROPERTY(QString sortName MEMBER sortName)
    Q_PROPERTY(QString type MEMBER type)
    Q_PROPERTY(QString mediaType MEMBER mediaType)
    Q_PROPERTY(QString collectionType MEMBER collectionType)
    Q_PROPERTY(QString collectionId MEMBER collectionId)     
    Q_PROPERTY(QString collectionName MEMBER collectionName) 
    Q_PROPERTY(QString playlistId MEMBER playlistId)
    Q_PROPERTY(QString playlistItemId MEMBER playlistItemId)
    Q_PROPERTY(QString seriesId MEMBER seriesId)             
    Q_PROPERTY(QString seriesName MEMBER seriesName)         
    Q_PROPERTY(QVariantMap providerIds MEMBER providerIds)
    Q_PROPERTY(int productionYear MEMBER productionYear)
    Q_PROPERTY(QString overview MEMBER overview)
    Q_PROPERTY(QString officialRating MEMBER officialRating)
    Q_PROPERTY(QString premiereDate MEMBER premiereDate)
    Q_PROPERTY(QString dateCreated MEMBER dateCreated)
    Q_PROPERTY(QString dateCreatedRaw MEMBER dateCreatedRaw)
    Q_PROPERTY(int criticRating MEMBER criticRating)
    Q_PROPERTY(bool canDownload MEMBER canDownload)

    
    Q_PROPERTY(QString container MEMBER container)
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(long long size MEMBER size)
    Q_PROPERTY(long long bitrate MEMBER bitrate)
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(int height MEMBER height)
    Q_PROPERTY(int partCount MEMBER partCount) 
    Q_PROPERTY(int childCount MEMBER childCount)
    Q_PROPERTY(int recursiveItemCount MEMBER recursiveItemCount) 
    Q_PROPERTY(int parentIndexNumber MEMBER parentIndexNumber)   
    Q_PROPERTY(int indexNumber MEMBER indexNumber)               

public:
    
    QString id;
    QString name;
    QString originalTitle; 
    QString sortName;      
    QString type;
    QString mediaType;     
    QString collectionType;
    QString collectionId;
    QString collectionName;
    QString playlistId;      
    QString playlistItemId;  
    QString seriesId;      
    QString seriesName;    
    QVariantMap providerIds; 

    
    int productionYear = 0;
    QString overview;
    QString officialRating;
    QString premiereDate;
    QString dateCreated;   
    QString dateCreatedRaw; 
    long long runTimeTicks = 0;
    double communityRating = 0.0;
    int criticRating = 0;  
    bool canDownload = false;

    
    QString container;     
    QString path;          
    long long size = 0;    
    long long bitrate = 0; 
    int width = 0;         
    int height = 0;        
    int partCount = 1;     
    int childCount = 0;    
    int recursiveItemCount = 0; 
    int parentIndexNumber = -1; 
    int indexNumber = -1;       

    
    QStringList genres;
    QStringList tags;            
    QStringList taglines;        
    QStringList remoteTrailers;  

    
    MediaImageInfo images;
    MediaUserDataInfo userData;
    QList<MediaPersonInfo> people;
    QList<MediaStudioInfo> studios; 
    QList<MediaExternalUrlInfo> externalUrls; 
    QList<MediaSourceInfo> mediaSources;

    
    bool isFavorite() const { return userData.isFavorite; }
    QString primaryImageTag() const { return images.primaryTag; }
    QString backdropImageTag() const { return images.backdropTag; }

    
    
    QString getPrimaryImageId() const {
        return images.primaryImageItemId.isEmpty() ? id : images.primaryImageItemId;
    }

    static MediaItem fromJson(const QJsonObject& obj);
};
Q_DECLARE_METATYPE(MediaItem)

#endif 
