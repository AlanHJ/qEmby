#include "resumeitemresolver.h"

#include "mediaitemutils.h"

#include <QDebug>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include <services/media/mediaservice.h>

#include <exception>
#include <utility>
#include <vector>

QList<MediaItem> ResumeItemResolver::buildFallbackItems(
    QList<MediaItem> rawItems,
    const QString &context)
{
    QList<MediaItem> displayItems;
    displayItems.reserve(rawItems.size());

    QSet<QString> seenSeriesIds;
    int duplicateEpisodeCount = 0;
    int fallbackWithoutImageCount = 0;

    for (const MediaItem &item : std::as_const(rawItems))
    {
        const QString seriesId = item.seriesId.trimmed();
        if (item.type == QStringLiteral("Episode") && !seriesId.isEmpty())
        {
            if (seenSeriesIds.contains(seriesId))
            {
                ++duplicateEpisodeCount;
                continue;
            }

            seenSeriesIds.insert(seriesId);

            
            
            
            
            MediaItem fallbackItem = item;
            fallbackItem.id = seriesId;
            fallbackItem.type = QStringLiteral("Series");
            if (!item.seriesName.trimmed().isEmpty())
            {
                fallbackItem.name = item.seriesName.trimmed();
            }
            fallbackItem.seriesId.clear();
            fallbackItem.seriesName.clear();
            fallbackItem.parentIndexNumber = -1;
            fallbackItem.indexNumber = -1;
            fallbackItem.userData = MediaUserDataInfo {};

            MediaImageInfo seriesImages;
            seriesImages.primaryTag = item.images.parentPrimaryTag;
            seriesImages.thumbTag = item.images.parentThumbTag;
            seriesImages.backdropTag = item.images.parentBackdropTag;
            seriesImages.primaryImageItemId = seriesId;
            if (seriesImages.primaryTag.isEmpty() &&
                seriesImages.thumbTag.isEmpty() &&
                seriesImages.backdropTag.isEmpty())
            {
                ++fallbackWithoutImageCount;
            }
            fallbackItem.images = std::move(seriesImages);
            fallbackItem.isResumeDisplayFallback = true;
            displayItems.append(MediaItemUtils::withResumeContext(
                std::move(fallbackItem), item));
            continue;
        }

        displayItems.append(MediaItemUtils::withResumeContext(item, item));
    }

    QString logContext = context.trimmed();
    if (logContext.isEmpty())
    {
        logContext = QStringLiteral("unknown");
    }

    qDebug() << "[ResumeItemResolver] built fallback items"
             << "| context=" << logContext
             << "| raw=" << rawItems.size()
             << "| duplicateEpisodes=" << duplicateEpisodeCount
             << "| fallbacksWithoutImage=" << fallbackWithoutImageCount
             << "| display=" << displayItems.size();

    return displayItems;
}

QCoro::Task<QList<MediaItem>> ResumeItemResolver::enrichSeriesCards(
    MediaService *mediaService,
    QList<MediaItem> displayItems,
    QString context)
{
    QList<int> seriesDisplayIndexes;
    QList<MediaItem> seriesResumeItems;
    QStringList seriesIds;

    for (int i = 0; i < displayItems.size(); ++i)
    {
        const MediaItem &item = displayItems.at(i);
        QString seriesId;
        if (item.isResumeDisplayFallback &&
            item.type == QStringLiteral("Series"))
        {
            seriesId = item.id.trimmed();
        }
        else if (item.type == QStringLiteral("Episode"))
        {
            seriesId = item.seriesId.trimmed();
        }

        if (seriesId.isEmpty())
        {
            continue;
        }

        seriesDisplayIndexes.append(i);
        seriesIds.append(seriesId);

        MediaItem resumeItem = item;
        if (item.hasResumeContext && !item.resumeItemId.trimmed().isEmpty())
        {
            resumeItem.id = item.resumeItemId.trimmed();
            resumeItem.userData = item.resumeUserData;
        }
        seriesResumeItems.append(std::move(resumeItem));
    }

    context = context.trimmed();
    if (context.isEmpty())
    {
        context = QStringLiteral("unknown");
    }

    qDebug() << "[ResumeItemResolver] enriching series cards"
             << "| context=" << context
             << "| display=" << displayItems.size()
             << "| seriesCandidates=" << seriesResumeItems.size();

    QPointer<MediaService> serviceGuard(mediaService);
    int resolvedSeriesCount = 0;
    int fallbackSeriesCount = 0;

    std::vector<QCoro::Task<MediaItem>> detailTasks;
    detailTasks.reserve(seriesResumeItems.size());
    if (!serviceGuard)
    {
        fallbackSeriesCount = seriesResumeItems.size();
        qWarning() << "[ResumeItemResolver] media service is unavailable; "
                      "keeping projected series fallbacks"
                   << "| context=" << context
                   << "| fallbackSeries=" << fallbackSeriesCount;
        co_return displayItems;
    }

    for (const QString &seriesId : std::as_const(seriesIds))
    {
        detailTasks.push_back(serviceGuard->getItemDetail(seriesId));
    }

    for (int i = 0; i < static_cast<int>(detailTasks.size()); ++i)
    {
        const MediaItem resumeItem = seriesResumeItems.at(i);
        const QString seriesId = seriesIds.at(i);
        const int displayIndex = seriesDisplayIndexes.at(i);

        if (!serviceGuard)
        {
            fallbackSeriesCount += seriesResumeItems.size() - i;
            qWarning() << "[ResumeItemResolver] media service was destroyed; "
                          "keeping remaining projected series fallbacks"
                       << "| context=" << context
                       << "| remaining=" << seriesResumeItems.size() - i;
            break;
        }

        try
        {
            MediaItem seriesItem = co_await std::move(detailTasks[i]);
            if (!serviceGuard)
            {
                fallbackSeriesCount += seriesResumeItems.size() - i;
                qWarning() << "[ResumeItemResolver] media service was destroyed "
                              "during series resolution; keeping projected "
                              "series fallbacks"
                           << "| context=" << context
                           << "| remaining=" << seriesResumeItems.size() - i;
                break;
            }

            if (seriesItem.id.trimmed().isEmpty())
            {
                ++fallbackSeriesCount;
                qWarning() << "[ResumeItemResolver] series detail returned an "
                              "empty id; keeping projected series fallback"
                           << "| context=" << context
                           << "| seriesId=" << seriesId
                           << "| resumeItemId=" << resumeItem.id;
                continue;
            }

            displayItems[displayIndex] =
                MediaItemUtils::withResumeContext(std::move(seriesItem),
                                                  resumeItem);
            ++resolvedSeriesCount;
        }
        catch (const std::exception &e)
        {
            ++fallbackSeriesCount;
            qWarning() << "[ResumeItemResolver] failed to resolve series; "
                          "keeping projected series fallback"
                       << "| context=" << context
                       << "| seriesId=" << seriesId
                       << "| resumeItemId=" << resumeItem.id
                       << "| error=" << e.what();
        }
        catch (...)
        {
            ++fallbackSeriesCount;
            qWarning() << "[ResumeItemResolver] failed to resolve series with "
                          "an unknown error; keeping projected series fallback"
                       << "| context=" << context
                       << "| seriesId=" << seriesId
                       << "| resumeItemId=" << resumeItem.id;
        }
    }

    qDebug() << "[ResumeItemResolver] resolution complete"
             << "| context=" << context
             << "| display=" << displayItems.size()
             << "| resolvedSeries=" << resolvedSeriesCount
             << "| fallbackSeries=" << fallbackSeriesCount;

    co_return displayItems;
}

QList<MediaItem> ResumeItemResolver::preserveExistingResolvedCards(
    QList<MediaItem> refreshedItems,
    QList<MediaItem> existingItems,
    const QString &context)
{
    int preservedCount = 0;

    for (MediaItem &refreshedItem : refreshedItems)
    {
        if (!refreshedItem.isResumeDisplayFallback)
        {
            continue;
        }

        for (const MediaItem &existingItem : std::as_const(existingItems))
        {
            if (existingItem.id.trimmed() != refreshedItem.id.trimmed() ||
                existingItem.isResumeDisplayFallback)
            {
                continue;
            }

            MediaItem resumeItem;
            resumeItem.id = refreshedItem.resumeItemId.trimmed();
            resumeItem.userData = refreshedItem.resumeUserData;
            refreshedItem = MediaItemUtils::withResumeContext(existingItem,
                                                               resumeItem);
            ++preservedCount;
            break;
        }
    }

    QString logContext = context.trimmed();
    if (logContext.isEmpty())
    {
        logContext = QStringLiteral("unknown");
    }

    qDebug() << "[ResumeItemResolver] preserved existing resolved cards"
             << "| context=" << logContext
             << "| existing=" << existingItems.size()
             << "| refreshed=" << refreshedItems.size()
             << "| preserved=" << preservedCount;

    return refreshedItems;
}

QCoro::Task<QList<MediaItem>> ResumeItemResolver::resolve(
    MediaService *mediaService,
    QList<MediaItem> rawItems,
    QString context)
{
    QList<MediaItem> displayItems =
        buildFallbackItems(std::move(rawItems), context);
    co_return co_await enrichSeriesCards(
        mediaService, std::move(displayItems), std::move(context));
}
