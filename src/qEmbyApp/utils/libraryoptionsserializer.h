#ifndef LIBRARYOPTIONSSERIALIZER_H
#define LIBRARYOPTIONSSERIALIZER_H

#include "librarytypeprofile.h"
#include <QJsonObject>
#include <models/profile/serverprofile.h>
#include <QStringList>
#include <optional>

namespace LibraryOptionsSerializer {

struct FetcherSelection {
    QStringList enabledNames;
    QStringList disabledNames;
};

struct LibraryOptionsState {
    QString contentType;
    bool enableRealtimeMonitor = false;
    bool enableEmbeddedTitles = false;
    std::optional<bool> enablePhotos;
    std::optional<bool> importPlaylists;
    std::optional<bool> excludeFromSearch;
    std::optional<bool> mergeTopLevelFolders;
    std::optional<int> sampleIgnoreSizeBytes;
    std::optional<bool> enablePlexIgnore;
    std::optional<bool> enableAutomaticSeriesGrouping;
    std::optional<bool> enableMultiPartItems;
    std::optional<bool> enableMultiVersionByFiles;
    std::optional<bool> enableMultiVersionByMetadata;
    std::optional<QString> musicFolderStructure;

    std::optional<bool> saveMetadataAsNfo;
    std::optional<bool> enableNfoReader;
    std::optional<bool> enableAdultMetadata;
    std::optional<bool> importCollections;
    std::optional<bool> automaticallyAddToCollection;
    std::optional<int> minCollectionItems;
    std::optional<int> automaticRefreshIntervalDays;
    std::optional<int> placeholderMetadataRefreshIntervalDays;
    std::optional<QString> preferredMetadataLanguage;
    std::optional<QString> metadataCountryCode;
    std::optional<QString> preferredImageLanguage;

    std::optional<bool> saveLocalMetadata;
    std::optional<bool> saveLocalThumbnailSets;
    std::optional<bool> cacheImages;
    std::optional<bool> saveMetadataHidden;
    std::optional<bool> downloadImagesInAdvance;
    std::optional<bool> extractChapterImagesDuringLibraryScan;
    std::optional<bool> enableChapterImageExtraction;
    std::optional<int> thumbnailImagesIntervalSeconds;
    std::optional<bool> autoGenerateChapters;
    std::optional<int> autoGenerateChapterIntervalMinutes;
    std::optional<bool> enableMarkerDetection;
    std::optional<bool> enableMarkerDetectionDuringLibraryScan;

    std::optional<bool> saveSubtitlesWithMedia;
    std::optional<bool> skipSubtitlesIfAudioTrackMatches;
    std::optional<bool> skipSubtitlesIfEmbeddedSubtitlesPresent;
    std::optional<int> subtitleDownloadMaxAgeDays;
    std::optional<bool> requirePerfectSubtitleMatch;
    std::optional<bool> forcedSubtitlesOnly;
    QStringList subtitleDownloadLanguages;

    std::optional<bool> saveLyricsWithMedia;
    std::optional<int> lyricsDownloadMaxAgeDays;
    QStringList lyricsDownloadLanguages;
    FetcherSelection lyricsFetchers;

    std::optional<int> minResumePct;
    std::optional<int> maxResumePct;
    std::optional<int> minResumeDurationSeconds;

    FetcherSelection metadataFetchers;
    FetcherSelection seasonMetadataFetchers;
    FetcherSelection episodeMetadataFetchers;
    FetcherSelection imageFetchers;
    FetcherSelection seasonImageFetchers;
    FetcherSelection episodeImageFetchers;
    FetcherSelection subtitleFetchers;
};

QJsonObject serializeLibraryOptions(
    const LibraryTypeUtils::LibraryTypeProfile& profile,
    const LibraryOptionsState& state,
    ServerProfile::ServerType serverType);

} 

#endif 
