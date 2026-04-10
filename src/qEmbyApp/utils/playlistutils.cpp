#include "playlistutils.h"

#include <models/media/mediaitem.h>

namespace {

bool equalsInsensitive(const QString& left, const QString& right)
{
    return left.compare(right, Qt::CaseInsensitive) == 0;
}

bool matchesAnyType(const QString& value, const QStringList& candidates)
{
    for (const QString& candidate : candidates) {
        if (equalsInsensitive(value, candidate)) {
            return true;
        }
    }
    return false;
}

} 

namespace PlaylistUtils {

QString normalizePlaylistMediaType(QString mediaType)
{
    mediaType = mediaType.trimmed();

    if (equalsInsensitive(mediaType, QStringLiteral("Audio"))) {
        return QStringLiteral("Audio");
    }

    if (equalsInsensitive(mediaType, QStringLiteral("Video"))) {
        return QStringLiteral("Video");
    }

    return mediaType;
}

QString inferPlaylistMediaType(const MediaItem& item)
{
    const QString normalizedMediaType = normalizePlaylistMediaType(item.mediaType);
    if (equalsInsensitive(normalizedMediaType, QStringLiteral("Audio")) ||
        equalsInsensitive(normalizedMediaType, QStringLiteral("Video"))) {
        return normalizedMediaType;
    }

    const QString itemType = item.type.trimmed();
    if (matchesAnyType(itemType, {QStringLiteral("Audio"),
                                  QStringLiteral("Song"),
                                  QStringLiteral("MusicAlbum"),
                                  QStringLiteral("AudioBook")})) {
        return QStringLiteral("Audio");
    }

    if (matchesAnyType(itemType, {QStringLiteral("Movie"),
                                  QStringLiteral("Episode"),
                                  QStringLiteral("Series"),
                                  QStringLiteral("Season"),
                                  QStringLiteral("Video"),
                                  QStringLiteral("MusicVideo"),
                                  QStringLiteral("Trailer")})) {
        return QStringLiteral("Video");
    }

    return {};
}

bool canAddItemToPlaylist(const MediaItem& item)
{
    if (item.id.trimmed().isEmpty()) {
        return false;
    }

    const QString itemType = item.type.trimmed();
    if (matchesAnyType(itemType, {QStringLiteral("Playlist"),
                                  QStringLiteral("BoxSet"),
                                  QStringLiteral("Folder"),
                                  QStringLiteral("Person"),
                                  QStringLiteral("Genre"),
                                  QStringLiteral("Studio"),
                                  QStringLiteral("Tag")})) {
        return false;
    }

    return !inferPlaylistMediaType(item).isEmpty();
}

bool isPlaylistCompatible(const QString& playlistMediaType,
                          const QString& targetPlaylistMediaType)
{
    const QString normalizedTarget =
        normalizePlaylistMediaType(targetPlaylistMediaType);
    if (normalizedTarget.isEmpty()) {
        return true;
    }

    const QString normalizedPlaylist =
        normalizePlaylistMediaType(playlistMediaType);
    if (normalizedPlaylist.isEmpty()) {
        return true;
    }

    return equalsInsensitive(normalizedPlaylist, normalizedTarget);
}

} 
