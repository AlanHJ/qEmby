#ifndef LIBRARYTYPEPROFILE_H
#define LIBRARYTYPEPROFILE_H

#include <QList>
#include <QString>
#include <models/profile/serverprofile.h>

namespace LibraryTypeUtils {

enum class FetcherKind { Metadata, Image, Subtitle, Lyrics };

struct JellyfinTypeOptionRule {
    QString itemType;
    bool includeMetadataFetchers = false;
    bool includeImageFetchers = false;
};

struct LibraryTypeProfile {
    QString collectionType;
    bool showEnablePhotos = false;
    bool showImportPlaylists = false;
    bool showExcludeFromSearch = false;
    bool showMergeTopLevelFolders = false;
    bool showMusicFolderStructure = false;
    bool showEmbeddedTitles = true;
    bool showMetadataLocale = false;
    bool showImageLanguage = false;
    bool showSaveNfo = false;
    bool showNfoReader = false;
    bool showAdultMetadata = false;
    bool showMovieCollectionSettings = false;
    bool showMetadataRefresh = false;
    bool showPlaceholderMetadataRefresh = false;
    bool showMetadataFetchers = false;
    bool showAdvancedSection = false;
    bool showSampleIgnoreSize = false;
    bool showEnablePlexIgnore = false;
    bool showAutomaticSeriesGrouping = false;
    bool showMultiPartItems = false;
    bool showMultiVersionGrouping = false;
    bool allowAdvancedMultiVersionModes = false;
    bool showImageSection = false;
    bool showCacheImages = false;
    bool showSaveMetadataHidden = false;
    bool showSaveArtwork = false;
    bool showDownloadImagesInAdvance = false;
    bool showThumbnailGeneration = false;
    bool showThumbnailInterval = false;
    bool showSaveThumbnailSets = false;
    bool showAutoGenerateChapters = false;
    bool showAutoGenerateChapterInterval = false;
    bool showIntroDetection = false;
    bool showChapterImages = false;
    bool showImageFetchers = false;
    bool showSubtitleSection = false;
    bool showRequirePerfectSubtitleMatch = false;
    bool showForcedSubtitlesOnly = false;
    bool showLyricsSection = false;
    bool showPlaybackSection = false;
    bool defaultEnableRealtimeMonitor = true;
    bool defaultSaveNfo = false;
    bool defaultEnableNfoReader = true;
    bool defaultAutomaticallyAddToCollection = true;
    bool defaultSaveArtwork = false;
    bool defaultSaveSubtitlesWithMedia = true;
    bool defaultSkipSubtitlesIfAudioTrackMatches = false;
    bool defaultSkipSubtitlesIfEmbeddedSubtitlesPresent = false;
    QList<JellyfinTypeOptionRule> jellyfinTypeOptions;
};

QString canonicalCollectionType(const QString& collectionType);
LibraryTypeProfile buildLibraryTypeProfile(ServerProfile::ServerType serverType,
                                           const QString& rawCollectionType);
bool isFetcherApplicable(const QString& name,
                         FetcherKind kind,
                         const LibraryTypeProfile& profile,
                         ServerProfile::ServerType serverType);

} 

#endif 
