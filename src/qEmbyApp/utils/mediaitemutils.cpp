#include "mediaitemutils.h"

#include <QChar>
#include <QCoreApplication>

namespace
{

bool hasResumeProgressData(const MediaUserDataInfo &userData)
{
    return userData.playbackPositionTicks > 0 || (userData.playedPercentage > 0.0 && userData.playedPercentage < 100.0);
}

void copyResumeContext(MediaItem &target, const MediaItem &source)
{
    target.hasResumeContext = true;
    target.resumeItemId = source.id.trimmed();
    target.resumeUserData = source.userData;
}

} 

QString MediaItemUtils::effectiveSeriesTitle(const MediaItem &item, const QString &fallbackSeriesName)
{
    const QString seriesTitle = item.seriesName.trimmed();
    if (!seriesTitle.isEmpty())
    {
        return seriesTitle;
    }

    return fallbackSeriesName.trimmed();
}

QString MediaItemUtils::episodeCode(const MediaItem &item, bool zeroPad)
{
    if (item.parentIndexNumber < 0 || item.indexNumber < 0)
    {
        return QString();
    }

    if (zeroPad)
    {
        return QStringLiteral("S%1E%2")
            .arg(item.parentIndexNumber, 2, 10, QChar('0'))
            .arg(item.indexNumber, 2, 10, QChar('0'));
    }

    return QStringLiteral("S%1E%2").arg(item.parentIndexNumber).arg(item.indexNumber);
}

QString MediaItemUtils::playbackTitle(const MediaItem &item, const QString &fallbackSeriesName)
{
    const QString itemTitle = item.name.trimmed();
    const bool isEpisode =
        item.type == QStringLiteral("Episode") || (!item.seriesId.trimmed().isEmpty() && item.indexNumber >= 0);

    if (!isEpisode)
    {
        return itemTitle.isEmpty() ? item.name : itemTitle;
    }

    const QString seriesTitle = effectiveSeriesTitle(item, fallbackSeriesName);
    const QString code = episodeCode(item);

    if (!seriesTitle.isEmpty() && !code.isEmpty() && !itemTitle.isEmpty())
    {
        return QCoreApplication::translate("MediaItemUtils", "%1 - %2 - %3").arg(seriesTitle, code, itemTitle);
    }

    if (!seriesTitle.isEmpty() && !itemTitle.isEmpty())
    {
        return QCoreApplication::translate("MediaItemUtils", "%1 - %2").arg(seriesTitle, itemTitle);
    }

    if (!code.isEmpty() && !itemTitle.isEmpty())
    {
        return QCoreApplication::translate("MediaItemUtils", "%1 - %2").arg(code, itemTitle);
    }

    return itemTitle.isEmpty() ? item.name : itemTitle;
}

MediaItem MediaItemUtils::withResumeContext(MediaItem displayItem, const MediaItem &resumeItem)
{
    copyResumeContext(displayItem, resumeItem);
    return displayItem;
}

bool MediaItemUtils::isCompletedWatchingItem(const MediaItem &item)
{
    const QString itemId = item.id.trimmed();
    return !itemId.isEmpty() && item.userData.played &&
           (item.type == QStringLiteral("Movie") ||
            item.type == QStringLiteral("Series"));
}

bool MediaItemUtils::hasResumeProgress(const MediaItem &item)
{
    const MediaUserDataInfo &userData = item.hasResumeContext ? item.resumeUserData : item.userData;
    return hasResumeProgressData(userData);
}

bool MediaItemUtils::canRemoveFromResume(const MediaItem &item)
{
    if (item.hasResumeContext)
    {
        return !item.resumeItemId.trimmed().isEmpty() && hasResumeProgressData(item.resumeUserData);
    }

    return hasResumeProgressData(item.userData);
}

QString MediaItemUtils::resumeActionItemId(const MediaItem &item)
{
    if (item.hasResumeContext && !item.resumeItemId.trimmed().isEmpty())
    {
        return item.resumeItemId.trimmed();
    }

    return item.id.trimmed();
}
