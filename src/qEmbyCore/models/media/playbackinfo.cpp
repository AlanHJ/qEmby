#include "playbackinfo.h"
#include <QJsonArray>
#include <QStringList>
#include <QVariant>

namespace {

QDateTime parseOptionalDateTime(const QJsonObject& json, const QStringList& keys)
{
    for (const QString& key : keys) {
        const QString rawValue = json.value(key).toString().trimmed();
        if (rawValue.isEmpty()) {
            continue;
        }

        QDateTime parsed = QDateTime::fromString(rawValue, Qt::ISODateWithMs);
        if (!parsed.isValid()) {
            parsed = QDateTime::fromString(rawValue, Qt::ISODate);
        }
        if (parsed.isValid()) {
            return parsed;
        }
    }

    return {};
}

} 

MediaStreamInfo MediaStreamInfo::fromJson(const QJsonObject& json) {
    MediaStreamInfo info;
    info.type = json["Type"].toString();
    info.codec = json["Codec"].toString();
    info.codecTag = json["CodecTag"].toString();
    info.title = json["Title"].toString();
    info.displayTitle = json["DisplayTitle"].toString();
    info.profile = json["Profile"].toString();
    info.level = json["Level"].toInt();
    info.width = json["Width"].toInt();
    info.height = json["Height"].toInt();
    info.aspectRatio = json["AspectRatio"].toString();
    info.isInterlaced = json["IsInterlaced"].toBool();
    info.realFrameRate = json["RealFrameRate"].toDouble();
    info.bitRate = json["BitRate"].toVariant().toLongLong();
    info.bitDepth = json["BitDepth"].toInt();
    info.pixelFormat = json["PixelFormat"].toString();
    info.refFrames = json["RefFrames"].toInt();
    info.language = json["Language"].toString();
    info.channelLayout = json["ChannelLayout"].toString();
    info.channels = json["Channels"].toInt();
    info.sampleRate = json["SampleRate"].toInt();
    info.isDefault = json["IsDefault"].toBool();

    
    info.displayLanguage = json["DisplayLanguage"].toString();
    info.isForced = json["IsForced"].toBool();
    info.isHearingImpaired = json["IsHearingImpaired"].toBool();
    
    
    info.index = json.contains("Index") ? json["Index"].toInt() : -1;
    
    info.isExternal = json["IsExternal"].toBool();
    info.deliveryMethod = json["DeliveryMethod"].toString();
    info.deliveryUrl = json["DeliveryUrl"].toString();
    info.isExternalUrl = json["IsExternalUrl"].toBool();
    info.isTextSubtitleStream = json["IsTextSubtitleStream"].toBool();
    info.supportsExternalStream = json["SupportsExternalStream"].toBool();
    info.path = json["Path"].toString();
    info.protocol = json["Protocol"].toString();
    info.extendedVideoType = json["ExtendedVideoType"].toString();
    info.extendedVideoSubType = json["ExtendedVideoSubType"].toString();
    info.attachmentSize = json["AttachmentSize"].toVariant().toLongLong();

    return info;
}

MediaSourceInfo MediaSourceInfo::fromJson(const QJsonObject& json) {
    MediaSourceInfo info;
    info.id = json["Id"].toString();
    info.name = json["Name"].toString(); 
    info.path = json["Path"].toString();
    info.container = json["Container"].toString();
    info.directStreamUrl = json["DirectStreamUrl"].toString();

    if (json.contains("Size")) {
        info.size = json["Size"].toVariant().toLongLong();
    }

    if (json.contains("RunTimeTicks")) {
        info.runTimeTicks = json["RunTimeTicks"].toVariant().toLongLong();
    }

    info.dateCreated = parseOptionalDateTime(
        json, {QStringLiteral("DateCreated"), QStringLiteral("CreatedAt")});
    info.dateModified = parseOptionalDateTime(
        json, {QStringLiteral("DateModified"), QStringLiteral("ModifiedAt"),
               QStringLiteral("LastModified"), QStringLiteral("UpdatedAt")});

    if (json.contains("MediaStreams")) {
        QJsonArray streamsArr = json["MediaStreams"].toArray();
        for (const auto& val : streamsArr) {
            info.mediaStreams.append(MediaStreamInfo::fromJson(val.toObject()));
        }
    }
    return info;
}

PlaybackInfo PlaybackInfo::fromJson(const QJsonObject& json) {
    PlaybackInfo info;
    info.playSessionId = json["PlaySessionId"].toString();

    if (json.contains("MediaSources")) {
        QJsonArray sourcesArr = json["MediaSources"].toArray();
        for (const auto& val : sourcesArr) {
            info.mediaSources.append(MediaSourceInfo::fromJson(val.toObject()));
        }
    }
    return info;
}
