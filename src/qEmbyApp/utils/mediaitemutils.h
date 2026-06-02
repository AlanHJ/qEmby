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
    static MediaItem withResumeContext(
        MediaItem displayItem,
        const MediaItem &resumeItem);
    static bool isCompletedWatchingItem(const MediaItem &item);
    static bool hasResumeProgress(const MediaItem &item);
    static bool canRemoveFromResume(const MediaItem &item);
    static QString resumeActionItemId(const MediaItem &item);
};

#endif 
