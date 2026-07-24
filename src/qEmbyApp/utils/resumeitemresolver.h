#ifndef RESUMEITEMRESOLVER_H
#define RESUMEITEMRESOLVER_H

#include <QList>
#include <QString>

#include <models/media/mediaitem.h>
#include <qcorotask.h>

class MediaService;

class ResumeItemResolver
{
public:
    static QList<MediaItem> buildFallbackItems(
        QList<MediaItem> rawItems,
        const QString &context);
    static QCoro::Task<QList<MediaItem>> enrichSeriesCards(
        MediaService *mediaService,
        QList<MediaItem> displayItems,
        QString context);
    static QList<MediaItem> preserveExistingResolvedCards(
        QList<MediaItem> refreshedItems,
        QList<MediaItem> existingItems,
        const QString &context);
    static QCoro::Task<QList<MediaItem>> resolve(
        MediaService *mediaService,
        QList<MediaItem> rawItems,
        QString context);
};

#endif 
