#ifndef PLAYBACKINFO_H
#define PLAYBACKINFO_H

#include "../../qEmbyCore_global.h"
#include <QString>
#include <QList>
#include <QJsonObject>
#include <QMetaType>
#include <QObject>


struct QEMBYCORE_EXPORT MediaStreamInfo {
    Q_GADGET
    Q_PROPERTY(QString type MEMBER type)
    Q_PROPERTY(QString codec MEMBER codec)
    Q_PROPERTY(QString codecTag MEMBER codecTag)
    Q_PROPERTY(QString title MEMBER title)
    Q_PROPERTY(QString displayTitle MEMBER displayTitle)
    Q_PROPERTY(QString profile MEMBER profile)
    Q_PROPERTY(int level MEMBER level)
    Q_PROPERTY(int width MEMBER width)
    Q_PROPERTY(int height MEMBER height)
    Q_PROPERTY(QString aspectRatio MEMBER aspectRatio)
    Q_PROPERTY(bool isInterlaced MEMBER isInterlaced)
    Q_PROPERTY(float realFrameRate MEMBER realFrameRate)
    Q_PROPERTY(long long bitRate MEMBER bitRate)
    Q_PROPERTY(int bitDepth MEMBER bitDepth)
    Q_PROPERTY(QString pixelFormat MEMBER pixelFormat)
    Q_PROPERTY(int refFrames MEMBER refFrames)
    Q_PROPERTY(QString language MEMBER language)
    Q_PROPERTY(QString channelLayout MEMBER channelLayout)
    Q_PROPERTY(int channels MEMBER channels)
    Q_PROPERTY(int sampleRate MEMBER sampleRate)
    Q_PROPERTY(bool isDefault MEMBER isDefault)

    
    Q_PROPERTY(QString displayLanguage MEMBER displayLanguage)
    Q_PROPERTY(bool isForced MEMBER isForced)
    Q_PROPERTY(bool isHearingImpaired MEMBER isHearingImpaired)
    Q_PROPERTY(int index MEMBER index)
    Q_PROPERTY(bool isExternal MEMBER isExternal)
    Q_PROPERTY(QString deliveryMethod MEMBER deliveryMethod)
    Q_PROPERTY(QString deliveryUrl MEMBER deliveryUrl)
    Q_PROPERTY(bool isExternalUrl MEMBER isExternalUrl)
    Q_PROPERTY(bool isTextSubtitleStream MEMBER isTextSubtitleStream)
    Q_PROPERTY(bool supportsExternalStream MEMBER supportsExternalStream)
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(QString protocol MEMBER protocol)
    Q_PROPERTY(QString extendedVideoType MEMBER extendedVideoType)
    Q_PROPERTY(QString extendedVideoSubType MEMBER extendedVideoSubType)
    Q_PROPERTY(long long attachmentSize MEMBER attachmentSize)

public:
    QString type;           
    QString codec;          
    QString codecTag;       
    QString title;          
    QString displayTitle;   
    QString profile;        
    int level = 0;          
    int width = 0;
    int height = 0;
    QString aspectRatio;    
    bool isInterlaced = false;
    float realFrameRate = 0.0f;
    long long bitRate = 0;
    int bitDepth = 0;           
    QString pixelFormat;        
    int refFrames = 0;
    QString language;           
    QString channelLayout;      
    int channels = 0;
    int sampleRate = 0;         
    bool isDefault = false;

    
    QString displayLanguage;           
    bool isForced = false;             
    bool isHearingImpaired = false;    
    int index = -1;                    
    bool isExternal = false;           
    QString deliveryMethod;            
    QString deliveryUrl;               
    bool isExternalUrl = false;        
    bool isTextSubtitleStream = false; 
    bool supportsExternalStream = false;
    QString path;                      
    QString protocol;                  
    QString extendedVideoType;
    QString extendedVideoSubType;
    long long attachmentSize = 0;

    static MediaStreamInfo fromJson(const QJsonObject& json);
};
Q_DECLARE_METATYPE(MediaStreamInfo)


struct QEMBYCORE_EXPORT MediaSourceInfo {
    Q_GADGET
    Q_PROPERTY(QString id MEMBER id)
    Q_PROPERTY(QString name MEMBER name) 
    Q_PROPERTY(QString path MEMBER path)
    Q_PROPERTY(QString container MEMBER container)
    Q_PROPERTY(long long size MEMBER size)
    Q_PROPERTY(QString directStreamUrl MEMBER directStreamUrl)
    Q_PROPERTY(long long runTimeTicks MEMBER runTimeTicks)

public:
    QString id;
    QString name;           
    QString path;
    QString container;      
    long long size = 0;     
    QString directStreamUrl;
    long long runTimeTicks = 0;
    QList<MediaStreamInfo> mediaStreams;

    static MediaSourceInfo fromJson(const QJsonObject& json);
};
Q_DECLARE_METATYPE(MediaSourceInfo)


struct QEMBYCORE_EXPORT PlaybackInfo {
    Q_GADGET
    Q_PROPERTY(QString playSessionId MEMBER playSessionId)

public:
    QString playSessionId;
    QList<MediaSourceInfo> mediaSources;

    static PlaybackInfo fromJson(const QJsonObject& json);
};
Q_DECLARE_METATYPE(PlaybackInfo)

#endif 
