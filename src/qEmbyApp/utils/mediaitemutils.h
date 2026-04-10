#ifndef MEDIAITEMUTILS_H
#define MEDIAITEMUTILS_H

#include <QString>

#include <models/media/mediaitem.h>

class MediaItemUtils
{
public:
    static QString effectiveSeriesTitle(
        const MediaItem &item,
        const QString &fallbackSeriesName = QString());
    static QString episodeCode(const MediaItem &item, bool zeroPad = true);
    static QString playbackTitle(
        const MediaItem &item,
        const QString &fallbackSeriesName = QString());
};

#endif 
