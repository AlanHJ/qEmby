#include "librarytypeprofile.h"

namespace LibraryTypeUtils {
namespace {

bool isVideoLibraryType(const QString &collectionType) {
  return collectionType == "movies" || collectionType == "tvshows" ||
         collectionType == "homevideos" || collectionType == "musicvideos";
}

bool isPhotoLibraryType(const QString &collectionType) {
  return collectionType == "photos";
}

bool isMixedLibraryType(const QString &collectionType) {
  return collectionType.isEmpty();
}

bool isBooksLibraryType(const QString &collectionType) {
  return collectionType == "books";
}

bool isMusicMetadataProvider(const QString &name) {
  const QString lower = name.toLower();
  return lower.contains("musicbrainz") || lower.contains("audiodb");
}

bool isMovieCollectionMetadataProvider(const QString &name) {
  return name.toLower().contains("box sets");
}

bool isVideoMetadataProvider(const QString &name) {
  const QString lower = name.toLower();
  return lower.contains("moviedb") || lower.contains("open movie database") ||
         lower.contains("tvdb");
}

bool isScreenGrabberProvider(const QString &name) {
  return name.toLower().contains("screen grabber");
}

bool isEmbeddedImageExtractorProvider(const QString &name) {
  const QString lower = name.toLower();
  return lower.contains("embedded image extractor") ||
         lower.contains("embedded image");
}

bool isFanArtProvider(const QString &name) {
  return name.toLower().contains("fanart");
}

bool isMusicImageProvider(const QString &name) {
  return isFanArtProvider(name) || name.toLower().contains("audiodb");
}

bool isVideoImageProvider(const QString &name) {
  return isVideoMetadataProvider(name) || isFanArtProvider(name) ||
         isScreenGrabberProvider(name) ||
         isEmbeddedImageExtractorProvider(name);
}

} 

QString canonicalCollectionType(const QString &collectionType) {
  return collectionType == "audiobooks" ? QStringLiteral("books")
                                        : collectionType;
}

LibraryTypeProfile buildLibraryTypeProfile(ServerProfile::ServerType serverType,
                                           const QString &rawCollectionType) {
  const QString collectionType = canonicalCollectionType(rawCollectionType);
  const bool isEmby = serverType == ServerProfile::Emby;

  LibraryTypeProfile profile;
  profile.collectionType = collectionType;

  if (isEmby) {
    profile.showExcludeFromSearch = true;
    profile.showMergeTopLevelFolders = true;
    profile.showCacheImages = true;
    profile.showSaveMetadataHidden = true;
    profile.showImageSection = true;
    profile.showAdvancedSection = true;
  }

  if (collectionType == "movies") {
    profile.showImportPlaylists = false;
    profile.showMetadataLocale = true;
    profile.showImageLanguage = isEmby;
    profile.showSaveNfo = true;
    profile.showNfoReader = true;
    profile.showAdultMetadata = isEmby;
    profile.showMovieCollectionSettings = true;
    profile.showMetadataRefresh = true;
    profile.showPlaceholderMetadataRefresh = false;
    profile.showMetadataFetchers = true;
    profile.showImageSection = true;
    profile.showSaveArtwork = isEmby;
    profile.showDownloadImagesInAdvance = isEmby;
    profile.showChapterImages = true;
    profile.showImageFetchers = true;
    profile.showSubtitleSection = true;
    profile.showPlaybackSection = isEmby;
    profile.showSampleIgnoreSize = isEmby;
    profile.showEnablePlexIgnore = isEmby;
    profile.showMultiPartItems = isEmby;
    profile.showMultiVersionGrouping = isEmby;
    profile.allowAdvancedMultiVersionModes = isEmby;
    profile.showThumbnailGeneration = isEmby;
    profile.showThumbnailInterval = isEmby;
    profile.showSaveThumbnailSets = isEmby;
    profile.showAutoGenerateChapters = isEmby;
    profile.showAutoGenerateChapterInterval = isEmby;
    profile.showRequirePerfectSubtitleMatch = isEmby;
    profile.showForcedSubtitlesOnly = isEmby;
    profile.jellyfinTypeOptions.append({"Movie", true, true});
    return profile;
  }

  if (collectionType == "tvshows") {
    profile.showMetadataLocale = true;
    profile.showImageLanguage = isEmby;
    profile.showSaveNfo = true;
    profile.showNfoReader = true;
    profile.showAdultMetadata = isEmby;
    profile.showMetadataRefresh = true;
    profile.showPlaceholderMetadataRefresh = isEmby;
    profile.showMetadataFetchers = true;
    profile.showImageSection = true;
    profile.showSaveArtwork = isEmby;
    profile.showDownloadImagesInAdvance = isEmby;
    profile.showChapterImages = true;
    profile.showImageFetchers = true;
    profile.showSubtitleSection = true;
    profile.showPlaybackSection = isEmby;
    profile.showSampleIgnoreSize = isEmby;
    profile.showEnablePlexIgnore = isEmby;
    profile.showAutomaticSeriesGrouping = isEmby;
    profile.showThumbnailGeneration = isEmby;
    profile.showThumbnailInterval = isEmby;
    profile.showSaveThumbnailSets = isEmby;
    profile.showAutoGenerateChapters = isEmby;
    profile.showAutoGenerateChapterInterval = isEmby;
    profile.showIntroDetection = isEmby;
    profile.showRequirePerfectSubtitleMatch = isEmby;
    profile.showForcedSubtitlesOnly = isEmby;
    profile.jellyfinTypeOptions.append({"Series", true, true});
    profile.jellyfinTypeOptions.append({"Season", false, true});
    profile.jellyfinTypeOptions.append({"Episode", true, true});
    return profile;
  }

  if (collectionType == "music") {
    profile.showImportPlaylists = isEmby;
    profile.showMusicFolderStructure = isEmby;
    profile.showEmbeddedTitles = false;
    profile.showMetadataLocale = true;
    profile.showSaveNfo = true;
    profile.showNfoReader = true;
    profile.showMetadataRefresh = true;
    profile.showMetadataFetchers = true;
    profile.showImageSection = true;
    profile.showSaveArtwork = isEmby;
    profile.showCacheImages = isEmby;
    profile.showSaveMetadataHidden = isEmby;
    profile.showImageFetchers = true;
    profile.showLyricsSection = isEmby;
    profile.jellyfinTypeOptions.append({"MusicArtist", true, true});
    profile.jellyfinTypeOptions.append({"MusicAlbum", true, true});
    profile.jellyfinTypeOptions.append({"Audio", false, true});
    return profile;
  }

  if (collectionType == "homevideos") {
    profile.showEnablePhotos = isEmby;
    profile.showSaveNfo = true;
    profile.showNfoReader = true;
    profile.showImageSection = true;
    profile.showSaveArtwork = isEmby;
    profile.showChapterImages = true;
    profile.showImageFetchers = true;
    profile.showSubtitleSection = true;
    profile.showPlaybackSection = isEmby;
    profile.showSampleIgnoreSize = isEmby;
    profile.showEnablePlexIgnore = isEmby;
    profile.showMultiPartItems = isEmby;
    profile.showMultiVersionGrouping = isEmby;
    profile.showThumbnailGeneration = isEmby;
    profile.showThumbnailInterval = isEmby;
    profile.showSaveThumbnailSets = isEmby;
    profile.showAutoGenerateChapters = isEmby;
    profile.showAutoGenerateChapterInterval = isEmby;
    profile.showRequirePerfectSubtitleMatch = isEmby;
    profile.showForcedSubtitlesOnly = isEmby;
    profile.jellyfinTypeOptions.append({"Video", false, true});
    return profile;
  }

  if (collectionType == "musicvideos") {
    profile.showImportPlaylists = isEmby;
    profile.showSaveNfo = true;
    profile.showNfoReader = true;
    profile.showImageSection = true;
    profile.showSaveArtwork = isEmby;
    profile.showChapterImages = true;
    profile.showImageFetchers = true;
    profile.showSubtitleSection = true;
    profile.showPlaybackSection = isEmby;
    profile.showSampleIgnoreSize = isEmby;
    profile.showEnablePlexIgnore = isEmby;
    profile.showMultiPartItems = isEmby;
    profile.showMultiVersionGrouping = isEmby;
    profile.showThumbnailGeneration = isEmby;
    profile.showThumbnailInterval = isEmby;
    profile.showSaveThumbnailSets = isEmby;
    profile.showAutoGenerateChapters = isEmby;
    profile.showAutoGenerateChapterInterval = isEmby;
    profile.showRequirePerfectSubtitleMatch = isEmby;
    profile.showForcedSubtitlesOnly = isEmby;
    if (isEmby) {
      profile.showMetadataLocale = true;
      profile.showImageLanguage = true;
      profile.showAdultMetadata = true;
      profile.showMetadataRefresh = true;
      profile.showMetadataFetchers = true;
      profile.showDownloadImagesInAdvance = true;
    }
    profile.jellyfinTypeOptions.append({"MusicVideo", isEmby, true});
    if (!isEmby)
      profile.jellyfinTypeOptions.append({"Video", false, true});
    return profile;
  }

  if (isMixedLibraryType(collectionType)) {
    profile.showImportPlaylists = isEmby;
    profile.showMetadataLocale = isEmby;
    profile.showImageLanguage = isEmby;
    profile.showSaveNfo = true;
    profile.showNfoReader = true;
    profile.showMetadataRefresh = isEmby;
    profile.showPlaceholderMetadataRefresh = isEmby;
    profile.showMetadataFetchers = isEmby;
    profile.showImageSection = true;
    profile.showSaveArtwork = isEmby;
    profile.showDownloadImagesInAdvance = isEmby;
    profile.showChapterImages = true;
    profile.showImageFetchers = true;
    profile.showSubtitleSection = isEmby;
    profile.showPlaybackSection = isEmby;
    profile.showSampleIgnoreSize = isEmby;
    profile.showEnablePlexIgnore = isEmby;
    profile.showMultiPartItems = isEmby;
    profile.showMultiVersionGrouping = isEmby;
    profile.allowAdvancedMultiVersionModes = isEmby;
    profile.showThumbnailGeneration = isEmby;
    profile.showThumbnailInterval = isEmby;
    profile.showSaveThumbnailSets = isEmby;
    profile.showAutoGenerateChapters = isEmby;
    profile.showAutoGenerateChapterInterval = isEmby;
    profile.showIntroDetection = isEmby;
    profile.showRequirePerfectSubtitleMatch = isEmby;
    profile.showForcedSubtitlesOnly = isEmby;
    return profile;
  }

  if (isBooksLibraryType(collectionType) || isPhotoLibraryType(collectionType) ||
      isMixedLibraryType(collectionType)) {
    if (isBooksLibraryType(collectionType)) {
      profile.showEmbeddedTitles = false;
    }
    if (isEmby && (isBooksLibraryType(collectionType) ||
                   isPhotoLibraryType(collectionType))) {
      profile.showSaveNfo = true;
      profile.showNfoReader = true;
      profile.showImageSection = true;
      profile.showSaveArtwork = true;
      profile.showImageFetchers = true;
    }
    return profile;
  }

  return profile;
}

bool isFetcherApplicable(const QString &name, FetcherKind kind,
                         const LibraryTypeProfile &profile,
                         ServerProfile::ServerType serverType) {
  const QString collectionType = profile.collectionType;

  if (kind == FetcherKind::Subtitle) {
    return profile.showSubtitleSection && isVideoLibraryType(collectionType);
  }

  if (kind == FetcherKind::Lyrics) {
    return profile.showLyricsSection;
  }

  if (kind == FetcherKind::Metadata) {
    if (!profile.showMetadataFetchers)
      return false;

    if (collectionType == "movies")
      return isVideoMetadataProvider(name);
    if (collectionType == "tvshows")
      return isVideoMetadataProvider(name) &&
             !isMovieCollectionMetadataProvider(name);
    if (collectionType == "music")
      return isMusicMetadataProvider(name);
    if (collectionType == "musicvideos")
      return serverType == ServerProfile::Emby &&
             isVideoMetadataProvider(name) &&
             !isMovieCollectionMetadataProvider(name);
    return false;
  }

  if (!profile.showImageFetchers)
    return false;

  if (collectionType == "movies" || collectionType == "tvshows")
    return isVideoImageProvider(name);
  if (collectionType == "music")
    return isMusicImageProvider(name);
  if (collectionType == "homevideos")
    return isScreenGrabberProvider(name);
  if (collectionType == "musicvideos") {
    if (serverType == ServerProfile::Emby)
      return isVideoImageProvider(name);
    return isScreenGrabberProvider(name) || isFanArtProvider(name);
  }

  return false;
}

} 
