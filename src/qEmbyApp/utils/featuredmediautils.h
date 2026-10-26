#ifndef FEATUREDMEDIAUTILS_H
#define FEATUREDMEDIAUTILS_H

#include <models/media/mediaitem.h>
#include <QDate>

namespace FeaturedMediaUtils {

inline constexpr int MinimumSlides = 5;
inline constexpr int DefaultSlides = 8;
inline constexpr int MaximumSlides = 30;

struct DailySelection {
    QDate date;
    QStringList itemIds;
};


DailySelection loadSelection(const QString& serverId, const QString& userId);
bool saveSelection(const QString& serverId, const QString& userId,
                   const DailySelection& selection);
QStringList itemIds(const QList<MediaItem>& items);
QList<MediaItem> orderedItems(const QStringList& ids, const QList<MediaItem>& available);
QList<MediaItem> fillVacancies(const QStringList& ids, const QList<MediaItem>& available,
                             const QList<MediaItem>& candidates, int limit = DefaultSlides);



QList<MediaItem> selectItems(const QList<MediaItem>& candidates,
                           int limit = DefaultSlides,
                           const QList<MediaItem>& retained = {},
                           const QStringList& previousIds = {});

} 

#endif 
