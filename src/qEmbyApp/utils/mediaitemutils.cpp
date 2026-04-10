#include "mediaitemutils.h"

#include <QChar>
#include <QCoreApplication>

QString MediaItemUtils::effectiveSeriesTitle(
    const MediaItem &item, const QString &fallbackSeriesName)
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

    return QStringLiteral("S%1E%2")
        .arg(item.parentIndexNumber)
        .arg(item.indexNumber);
}

QString MediaItemUtils::playbackTitle(
    const MediaItem &item, const QString &fallbackSeriesName)
{
    const QString itemTitle = item.name.trimmed();
    const bool isEpisode =
        item.type == QStringLiteral("Episode") ||
        (!item.seriesId.trimmed().isEmpty() && item.indexNumber >= 0);

    if (!isEpisode)
    {
        return itemTitle.isEmpty() ? item.name : itemTitle;
    }

    const QString seriesTitle = effectiveSeriesTitle(item, fallbackSeriesName);
    const QString code = episodeCode(item);

    if (!seriesTitle.isEmpty() && !code.isEmpty() && !itemTitle.isEmpty())
    {
        return QCoreApplication::translate("MediaItemUtils", "%1 - %2 - %3")
            .arg(seriesTitle, code, itemTitle);
    }

    if (!seriesTitle.isEmpty() && !itemTitle.isEmpty())
    {
        return QCoreApplication::translate("MediaItemUtils", "%1 - %2")
            .arg(seriesTitle, itemTitle);
    }

    if (!code.isEmpty() && !itemTitle.isEmpty())
    {
        return QCoreApplication::translate("MediaItemUtils", "%1 - %2")
            .arg(code, itemTitle);
    }

    return itemTitle.isEmpty() ? item.name : itemTitle;
}
