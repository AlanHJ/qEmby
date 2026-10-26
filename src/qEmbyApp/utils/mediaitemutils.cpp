#include "mediaitemutils.h"

#include <QChar>
#include <QCoreApplication>
#include <QDate>

namespace
{

bool hasResumeProgressData(const MediaUserDataInfo &userData)
{
    return userData.playbackPositionTicks > 0 || (userData.playedPercentage > 0.0 && userData.playedPercentage < 100.0);
}

double calculatePlaybackProgressPercentage(const MediaUserDataInfo &userData,
                                           long long runTimeTicks)
{
    if (userData.playedPercentage > 0.0 && userData.playedPercentage < 100.0)
    {
        return userData.playedPercentage;
    }

    if (userData.playbackPositionTicks > 0 && runTimeTicks > userData.playbackPositionTicks)
    {
        return 100.0 * static_cast<double>(userData.playbackPositionTicks) /
               static_cast<double>(runTimeTicks);
    }

    return 0.0;
}

void copyResumeContext(MediaItem &target, const MediaItem &source)
{
    target.hasResumeContext = true;
    target.resumeItemId = source.id.trimmed();
    target.resumeUserData = source.userData;
    const double progressPercentage =
        calculatePlaybackProgressPercentage(source.userData, source.runTimeTicks);
    if (progressPercentage > 0.0)
    {
        target.resumeUserData.playedPercentage = progressPercentage;
    }
}

} 

QString MediaItemUtils::releaseDateText(const MediaItem& item)
{
    QString dateText = item.premiereDate.trimmed();
    if (!dateText.isEmpty()) {
        const int timeSeparator = dateText.indexOf(QLatin1Char('T'));
        if (timeSeparator > 0) dateText = dateText.left(timeSeparator);
        const QString dateOnly = dateText.left(qMin(dateText.length(), qsizetype(10)));
        const QDate date = QDate::fromString(dateOnly, Qt::ISODate);
        return date.isValid() ? date.toString(QStringLiteral("yyyy-MM-dd")) : dateOnly;
    }
    return item.productionYear > 0 ? QString::number(item.productionYear) : QString();
}

QStringList MediaItemUtils::displayTags(const MediaItem& item)
{
    QStringList tags;
    for (const QStringList* values : {&item.genres, &item.tags}) {
        for (const QString& value : *values) {
            const QString tag = value.trimmed();
            if (!tag.isEmpty() && !tags.contains(tag, Qt::CaseInsensitive))
                tags.append(tag);
        }
    }
    return tags;
}

MediaItem MediaItemUtils::withMissingDisplayMetadata(MediaItem item, const MediaItem& detail)
{
    if (item.id != detail.id || (!detail.type.isEmpty() && detail.type != item.type))
        return item;
    
    if (item.overview.trimmed().isEmpty()) item.overview = detail.overview;
    if (item.premiereDate.trimmed().isEmpty()) item.premiereDate = detail.premiereDate;
    if (item.productionYear <= 0) item.productionYear = detail.productionYear;
    if (item.communityRating <= 0) item.communityRating = detail.communityRating;
    if (item.officialRating.trimmed().isEmpty()) item.officialRating = detail.officialRating;
    if (item.genres.isEmpty()) item.genres = detail.genres;
    if (item.tags.isEmpty()) item.tags = detail.tags;
    if (item.runTimeTicks <= 0) item.runTimeTicks = detail.runTimeTicks;
    return item;
}

QStringList MediaItemUtils::personBiographicalDetails(const MediaItem& item)
{
    QStringList details;
    if (item.type != QStringLiteral("Person")) return details;
    if (!item.premiereDate.trimmed().isEmpty())
        details.append(tr("Born: %1").arg(item.premiereDate));
    if (!item.productionLocations.isEmpty())
        details.append(tr("Birthplace: %1").arg(item.productionLocations.join(QStringLiteral(", "))));
    if (!item.endDate.trimmed().isEmpty())
        details.append(tr("Died: %1").arg(item.endDate));
    return details;
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

double MediaItemUtils::playbackProgressPercentage(const MediaItem &item)
{
    const MediaUserDataInfo &userData = item.hasResumeContext ? item.resumeUserData : item.userData;
    return calculatePlaybackProgressPercentage(userData, item.runTimeTicks);
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
