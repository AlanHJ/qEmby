#ifndef PLAYLISTUTILS_H
#define PLAYLISTUTILS_H

#include <QString>

struct MediaItem;

namespace PlaylistUtils {

bool canAddItemToPlaylist(const MediaItem& item);
QString inferPlaylistMediaType(const MediaItem& item);
QString normalizePlaylistMediaType(QString mediaType);
bool isPlaylistCompatible(const QString& playlistMediaType,
                          const QString& targetPlaylistMediaType);

} 

#endif 
