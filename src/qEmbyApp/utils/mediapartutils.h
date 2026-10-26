#ifndef MEDIAPARTUTILS_H
#define MEDIAPARTUTILS_H

#include <QCoreApplication>
#include <models/media/mediaitem.h>

class MediaPartUtils
{
    Q_DECLARE_TR_FUNCTIONS(MediaPartUtils)
public:
    
    
    static QList<MediaItem> sequence(const MediaItem &primary,
                                     const QList<MediaItem> &additionalParts);
    static int indexOf(const QList<MediaItem> &parts, const QString &mediaId);
    static QString displayTitle(const QList<MediaItem> &parts, int index);
    static long long resumeTicks(const MediaItem &part);
};

#endif 
