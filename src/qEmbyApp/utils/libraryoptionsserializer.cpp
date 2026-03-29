#include "libraryoptionsserializer.h"

#include <QJsonArray>

namespace LTU = LibraryTypeUtils;

namespace LibraryOptionsSerializer {
namespace {

QJsonArray toJsonArray(const QStringList &values) {
  QJsonArray array;
  for (const QString &value : values) {
    array.append(value);
  }
  return array;
}

void setBoolIfPresent(QJsonObject &target, const char *key,
                      const std::optional<bool> &value) {
  if (value.has_value()) {
    target[key] = *value;
  }
}

void setIntIfPresent(QJsonObject &target, const char *key,
                     const std::optional<int> &value) {
  if (value.has_value()) {
    target[key] = *value;
  }
}

void setStringIfNotEmpty(QJsonObject &target, const char *key,
                         const std::optional<QString> &value) {
  if (value.has_value() && !value->isEmpty()) {
    target[key] = *value;
  }
}

void setStringOrEmpty(QJsonObject &target, const char *key,
                      const std::optional<QString> &value) {
  target[key] = value.value_or(QString());
}

void setStringOrNull(QJsonObject &target, const char *key,
                     const std::optional<QString> &value) {
  if (!value.has_value()) {
    return;
  }

  if (value->isEmpty()) {
    target[key] = QJsonValue::Null;
  } else {
    target[key] = *value;
  }
}

void setArrayIfNotEmpty(QJsonObject &target, const char *key,
                        const QStringList &values) {
  if (!values.isEmpty()) {
    target[key] = toJsonArray(values);
  }
}

const FetcherSelection &metadataSelectionForItemType(
    const QString &itemType, const LibraryOptionsState &state) {
  if (itemType == "Season") {
    return state.seasonMetadataFetchers;
  }
  if (itemType == "Episode") {
    return state.episodeMetadataFetchers;
  }
  return state.metadataFetchers;
}

const FetcherSelection &imageSelectionForItemType(
    const QString &itemType, const LibraryOptionsState &state) {
  if (itemType == "Season") {
    return state.seasonImageFetchers;
  }
  if (itemType == "Episode") {
    return state.episodeImageFetchers;
  }
  return state.imageFetchers;
}

QJsonArray buildTypeOptions(
    const QList<LTU::JellyfinTypeOptionRule> &rules,
    const LibraryOptionsState &state,
    bool isEmby) {
  QJsonArray typeOptions;

  for (const auto &rule : rules) {
    QJsonObject typeOption;
    typeOption["Type"] = rule.itemType;
    const FetcherSelection &metadataFetchers =
        metadataSelectionForItemType(rule.itemType, state);
    const FetcherSelection &imageFetchers =
        imageSelectionForItemType(rule.itemType, state);

    if (rule.includeMetadataFetchers &&
        (!metadataFetchers.enabledNames.isEmpty() ||
         !metadataFetchers.disabledNames.isEmpty())) {
      typeOption["MetadataFetchers"] =
          toJsonArray(metadataFetchers.enabledNames);
      typeOption["MetadataFetcherOrder"] =
          toJsonArray(metadataFetchers.enabledNames);
      if (isEmby && !metadataFetchers.disabledNames.isEmpty()) {
        typeOption["DisabledMetadataFetchers"] =
            toJsonArray(metadataFetchers.disabledNames);
      }
    }
    if (rule.includeImageFetchers &&
        (!imageFetchers.enabledNames.isEmpty() ||
         !imageFetchers.disabledNames.isEmpty())) {
      typeOption["ImageFetchers"] = toJsonArray(imageFetchers.enabledNames);
      typeOption["ImageFetcherOrder"] = toJsonArray(imageFetchers.enabledNames);
      if (isEmby && !imageFetchers.disabledNames.isEmpty()) {
        typeOption["DisabledImageFetchers"] =
            toJsonArray(imageFetchers.disabledNames);
      }
    }

    typeOption["ImageOptions"] = QJsonArray();

    if (typeOption.size() > 1) {
      typeOptions.append(typeOption);
    }
  }

  return typeOptions;
}

} 

QJsonObject serializeLibraryOptions(const LTU::LibraryTypeProfile &profile,
                                    const LibraryOptionsState &state,
                                    ServerProfile::ServerType serverType) {
  const bool isEmby = serverType == ServerProfile::Emby;
  QJsonObject options;

  if (!isEmby) {
    options["Enabled"] = true;
    options["EnablePhotos"] = true;
    options["EnableInternetProviders"] = true;
    options["EnableLUFSScan"] = true;
    options["EnableAutomaticSeriesGrouping"] = false;
    options["EnableEmbeddedExtrasTitles"] = false;
    options["EnableEmbeddedEpisodeInfos"] = false;
    options["AllowEmbeddedSubtitles"] = QStringLiteral("AllowAll");
    options["RequirePerfectSubtitleMatch"] = true;
    options["PreferNonstandardArtistsTag"] = false;
    options["UseCustomTagDelimiters"] = false;
    options["MetadataSavers"] = QJsonArray();
    options["CustomTagDelimiters"] = toJsonArray(
        QStringList{QStringLiteral("/"), QStringLiteral("|"),
                    QStringLiteral(";"), QStringLiteral("\\")});
    options["DelimiterWhitelist"] = QJsonArray();
    options["DisabledLyricFetchers"] = QJsonArray();
    options["LyricFetcherOrder"] = QJsonArray();
    options["EnableArchiveMediaFiles"] = false;
    options["ExtractTrickplayImagesDuringLibraryScan"] = false;
    options["SaveTrickplayWithMedia"] = false;
    options["EnableTrickplayImageExtraction"] = false;
    options["ExtractChapterImagesDuringLibraryScan"] = false;
    options["SaveLyricsWithMedia"] = false;
    options["SeasonZeroDisplayName"] = QStringLiteral("Specials");
    options["DisabledMediaSegmentProviders"] = QJsonArray();
    options["MediaSegmentProvideOrder"] = QJsonArray();
  }

  
  if (isEmby) {
    options["EnableArchiveMediaFiles"] = false;
    options["EnablePhotos"] = true;
    options["EnableInternetProviders"] = true;
    options["ImportPlaylists"] = true;
    options["EnableAutomaticSeriesGrouping"] = true;
    options["EnableMarkerDetection"] = false;
    options["EnableMarkerDetectionDuringLibraryScan"] = false;
    options["IntroDetectionFingerprintLength"] = 10;
    options["EnableAudioResume"] = false;
    options["ImportMissingEpisodes"] = false;
    options["AutoGenerateChapters"] = true;
    options["AutoGenerateChapterIntervalMinutes"] = 5;
    options["CacheImages"] = false;
    options["ExcludeFromSearch"] = false;
    options["EnablePlexIgnore"] = false;
    options["PlaceholderMetadataRefreshIntervalDays"] = 0;
    options["SeasonZeroDisplayName"] = QStringLiteral("Specials");
    options["RequirePerfectSubtitleMatch"] = true;
    options["ForcedSubtitlesOnly"] = false;
    options["SaveMetadataHidden"] = false;
    options["IgnoreHiddenFiles"] = false;
    options["IgnoreFileExtensions"] = QJsonArray();
    options["CollapseSingleItemFolders"] = false;
    options["MergeTopLevelFolders"] = false;
    options["EnableMultiPartItems"] = true;
    options["SaveLyricsWithMedia"] = true;
    options["LyricsDownloadMaxAgeDays"] = 180;
    options["LyricsFetcherOrder"] = QJsonArray();
    options["LyricsDownloadLanguages"] = QJsonArray();
    options["DisabledLyricsFetchers"] = QJsonArray();
    options["ThumbnailImagesIntervalSeconds"] = -1;
    options["SampleIgnoreSize"] = 314572800;
    options["SaveLocalThumbnailSets"] = false;
    options["MinResumePct"] = 2;
    options["MaxResumePct"] = 90;
    options["MinResumeDurationSeconds"] = 120;
  }

  if (isEmby && !state.contentType.isEmpty()) {
    options["ContentType"] = state.contentType;
  }
  options["EnableRealtimeMonitor"] = state.enableRealtimeMonitor;
  options["EnableEmbeddedTitles"] = state.enableEmbeddedTitles;
  setBoolIfPresent(options, "EnablePhotos", state.enablePhotos);
  setBoolIfPresent(options, "ImportPlaylists", state.importPlaylists);
  setBoolIfPresent(options, "ExcludeFromSearch", state.excludeFromSearch);
  setBoolIfPresent(options, "MergeTopLevelFolders",
                   state.mergeTopLevelFolders);
  setIntIfPresent(options, "SampleIgnoreSize", state.sampleIgnoreSizeBytes);
  setBoolIfPresent(options, "EnablePlexIgnore", state.enablePlexIgnore);
  setBoolIfPresent(options, "EnableAutomaticSeriesGrouping",
                   state.enableAutomaticSeriesGrouping);
  setBoolIfPresent(options, "EnableMultiPartItems", state.enableMultiPartItems);
  setBoolIfPresent(options, "EnableMultiVersionByFiles",
                   state.enableMultiVersionByFiles);
  setBoolIfPresent(options, "EnableMultiVersionByMetadata",
                   state.enableMultiVersionByMetadata);
  if (profile.showMusicFolderStructure) {
    setStringOrNull(options, "MusicFolderStructure",
                    state.musicFolderStructure);
  }

  if (profile.showSaveNfo) {
    if (isEmby) {
      options["MetadataSavers"] =
          state.saveMetadataAsNfo.value_or(false)
              ? toJsonArray(QStringList{QStringLiteral("Nfo")})
              : QJsonArray();
    } else {
      
      setBoolIfPresent(options, "SaveLocalMetadata",
                       state.saveMetadataAsNfo);
      options["MetadataSavers"] =
          state.saveMetadataAsNfo.value_or(false)
              ? toJsonArray(QStringList{QStringLiteral("Nfo")})
              : QJsonArray();
    }
  }

  if (profile.showNfoReader && state.enableNfoReader.has_value()) {
    if (*state.enableNfoReader) {
      options["LocalMetadataReaderOrder"] =
          toJsonArray(QStringList{QStringLiteral("Nfo")});
      options["DisabledLocalMetadataReaders"] = QJsonArray();
    } else {
      options["LocalMetadataReaderOrder"] = QJsonArray();
      options["DisabledLocalMetadataReaders"] =
          toJsonArray(QStringList{QStringLiteral("Nfo")});
    }
  }

  if (profile.showAdultMetadata) {
    setBoolIfPresent(options, "EnableAdultMetadata", state.enableAdultMetadata);
  }

  if (profile.showMovieCollectionSettings) {
    if (isEmby) {
      setBoolIfPresent(options, "ImportCollections", state.importCollections);
      setIntIfPresent(options, "MinCollectionItems", state.minCollectionItems);
    } else {
      setBoolIfPresent(options, "AutomaticallyAddToCollection",
                       state.automaticallyAddToCollection);
    }
  }

  if (profile.showMetadataRefresh) {
    setIntIfPresent(options, "AutomaticRefreshIntervalDays",
                    state.automaticRefreshIntervalDays);
  }
  if (profile.showPlaceholderMetadataRefresh) {
    setIntIfPresent(options, "PlaceholderMetadataRefreshIntervalDays",
                    state.placeholderMetadataRefreshIntervalDays);
  }

  if (profile.showMetadataLocale) {
    if (isEmby) {
      setStringOrEmpty(options, "PreferredMetadataLanguage",
                       state.preferredMetadataLanguage);
      setStringOrEmpty(options, "MetadataCountryCode",
                       state.metadataCountryCode);
    } else {
      setStringIfNotEmpty(options, "PreferredMetadataLanguage",
                          state.preferredMetadataLanguage);
      setStringIfNotEmpty(options, "MetadataCountryCode",
                          state.metadataCountryCode);
    }
  }

  if (profile.showImageLanguage) {
    if (isEmby) {
      setStringOrEmpty(options, "PreferredImageLanguage",
                       state.preferredImageLanguage);
    } else {
      setStringIfNotEmpty(options, "PreferredImageLanguage",
                          state.preferredImageLanguage);
    }
  }

  if (profile.showImageSection) {
    setBoolIfPresent(options, "SaveLocalMetadata", state.saveLocalMetadata);
    setBoolIfPresent(options, "CacheImages", state.cacheImages);
    setBoolIfPresent(options, "SaveMetadataHidden", state.saveMetadataHidden);
    if (isEmby) {
      setBoolIfPresent(options, "DownloadImagesInAdvance",
                       state.downloadImagesInAdvance);
    } else {
      setBoolIfPresent(options, "SaveLocalThumbnailSets",
                       state.saveLocalThumbnailSets);
    }
    setBoolIfPresent(options, "SaveLocalThumbnailSets",
                     state.saveLocalThumbnailSets);
    setBoolIfPresent(options, "EnableChapterImageExtraction",
                     state.enableChapterImageExtraction);
    setBoolIfPresent(options, "ExtractChapterImagesDuringLibraryScan",
                     state.extractChapterImagesDuringLibraryScan);
    setIntIfPresent(options, "ThumbnailImagesIntervalSeconds",
                    state.thumbnailImagesIntervalSeconds);
    setBoolIfPresent(options, "AutoGenerateChapters",
                     state.autoGenerateChapters);
    setIntIfPresent(options, "AutoGenerateChapterIntervalMinutes",
                    state.autoGenerateChapterIntervalMinutes);
    setBoolIfPresent(options, "EnableMarkerDetection",
                     state.enableMarkerDetection);
    setBoolIfPresent(options, "EnableMarkerDetectionDuringLibraryScan",
                     state.enableMarkerDetectionDuringLibraryScan);
    if (!isEmby) {
      const bool extractChapterImages =
          state.enableChapterImageExtraction.value_or(false);
      options["ExtractChapterImagesDuringLibraryScan"] =
          extractChapterImages;
    }
  }

  if (profile.showSubtitleSection) {
    setBoolIfPresent(options, "SaveSubtitlesWithMedia",
                     state.saveSubtitlesWithMedia);
    setBoolIfPresent(options, "SkipSubtitlesIfAudioTrackMatches",
                     state.skipSubtitlesIfAudioTrackMatches);
    setBoolIfPresent(options, "SkipSubtitlesIfEmbeddedSubtitlesPresent",
                     state.skipSubtitlesIfEmbeddedSubtitlesPresent);
    if (isEmby) {
      setIntIfPresent(options, "SubtitleDownloadMaxAgeDays",
                      state.subtitleDownloadMaxAgeDays);
    }
    setBoolIfPresent(options, "RequirePerfectSubtitleMatch",
                     state.requirePerfectSubtitleMatch);
    setBoolIfPresent(options, "ForcedSubtitlesOnly",
                     state.forcedSubtitlesOnly);
    options["SubtitleDownloadLanguages"] =
        toJsonArray(state.subtitleDownloadLanguages);
    options["SubtitleFetcherOrder"] =
        toJsonArray(state.subtitleFetchers.enabledNames);
    options["DisabledSubtitleFetchers"] =
        toJsonArray(state.subtitleFetchers.disabledNames);
  }

  if (profile.showLyricsSection) {
    setBoolIfPresent(options, "SaveLyricsWithMedia",
                     state.saveLyricsWithMedia);
    setIntIfPresent(options, "LyricsDownloadMaxAgeDays",
                    state.lyricsDownloadMaxAgeDays);
    options["LyricsDownloadLanguages"] =
        toJsonArray(state.lyricsDownloadLanguages);
    options["LyricsFetcherOrder"] =
        toJsonArray(state.lyricsFetchers.enabledNames);
    options["DisabledLyricsFetchers"] =
        toJsonArray(state.lyricsFetchers.disabledNames);
  }

  if (profile.showPlaybackSection) {
    setIntIfPresent(options, "MinResumePct", state.minResumePct);
    setIntIfPresent(options, "MaxResumePct", state.maxResumePct);
    setIntIfPresent(options, "MinResumeDurationSeconds",
                    state.minResumeDurationSeconds);
  }

  
  if (isEmby) {
    options["MetadataDownloaders"] =
        toJsonArray(state.metadataFetchers.enabledNames);
    options["MetadataFetcherOrder"] =
        toJsonArray(state.metadataFetchers.enabledNames);
    options["DisabledMetadataFetchers"] =
        toJsonArray(state.metadataFetchers.disabledNames);
    options["ImageFetchers"] =
        toJsonArray(state.imageFetchers.enabledNames);
    options["ImageFetcherOrder"] =
        toJsonArray(state.imageFetchers.enabledNames);
    options["DisabledImageFetchers"] =
        toJsonArray(state.imageFetchers.disabledNames);
  }

  
  const QJsonArray typeOptions = buildTypeOptions(
      profile.jellyfinTypeOptions, state,
      isEmby);
  if (!typeOptions.isEmpty()) {
    options["TypeOptions"] = typeOptions;
  }

  return options;
}

} 
