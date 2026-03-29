#include "librarycreatedlg_p.h"

#include "fetcherrowwidget.h"
#include "librarycreatedlg.h"
#include "moderncombobox.h"
#include "modernswitch.h"
#include <QSpinBox>
#include <QDebug>

namespace LTU = LibraryTypeUtils;
namespace LOS = LibraryOptionsSerializer;

LibraryCreateDialogOptionsBuilder::LibraryCreateDialogOptionsBuilder(
    const LibraryCreateDialog &dialog, const LTU::LibraryTypeProfile &profile,
    ServerProfile::ServerType serverType)
    : m_dialog(dialog), m_profile(profile), m_serverType(serverType) {}

LOS::LibraryOptionsState
LibraryCreateDialogOptionsBuilder::build(const QString &contentType) const {
  LOS::LibraryOptionsState state;
  state.contentType = contentType;

  collectBasicState(state);
  collectMetadataState(state);
  collectAdvancedState(state);
  collectImageState(state);
  collectSubtitleState(state);
  collectLyricsState(state);
  collectPlaybackState(state);
  collectFetcherState(state);

  return state;
}

void LibraryCreateDialogOptionsBuilder::assignChecked(
    std::optional<bool> &target, const ModernSwitch *toggle) {
  if (toggle) {
    target = toggle->isChecked();
  }
}

void LibraryCreateDialogOptionsBuilder::assignCurrentInt(
    std::optional<int> &target, const ModernComboBox *combo) {
  if (combo) {
    target = combo->currentData().toInt();
  }
}

void LibraryCreateDialogOptionsBuilder::collectBasicState(
    LOS::LibraryOptionsState &state) const {
  state.enableRealtimeMonitor =
      m_dialog.m_chkRealtimeMonitor &&
      m_dialog.m_chkRealtimeMonitor->isChecked();
  state.enableEmbeddedTitles =
      m_dialog.m_chkEmbeddedTitles &&
      m_dialog.m_chkEmbeddedTitles->isChecked();
  if (m_profile.showEnablePhotos) {
    assignChecked(state.enablePhotos, m_dialog.m_chkEnablePhotos);
  }
  if (m_profile.showImportPlaylists) {
    assignChecked(state.importPlaylists, m_dialog.m_chkImportPlaylists);
  }
  if (m_profile.showExcludeFromSearch) {
    assignChecked(state.excludeFromSearch, m_dialog.m_chkExcludeFromSearch);
  }
  if (m_profile.showMergeTopLevelFolders) {
    assignChecked(state.mergeTopLevelFolders, m_dialog.m_chkMergeFolders);
  }
}

void LibraryCreateDialogOptionsBuilder::collectMetadataState(
    LOS::LibraryOptionsState &state) const {
  assignChecked(state.saveMetadataAsNfo, m_dialog.m_chkSaveNfo);
  assignChecked(state.enableNfoReader, m_dialog.m_chkNfoReader);
  assignChecked(state.enableAdultMetadata, m_dialog.m_chkAllowAdultMetadata);
  assignChecked(state.importCollections, m_dialog.m_chkImportCollectionInfo);
  assignChecked(state.automaticallyAddToCollection,
                m_dialog.m_chkAutoCollection);
  assignCurrentInt(state.minCollectionItems, m_dialog.m_minCollSizeCombo);
  assignCurrentInt(state.automaticRefreshIntervalDays,
                   m_dialog.m_metaRefreshCombo);
  if (m_profile.showPlaceholderMetadataRefresh) {
    assignCurrentInt(state.placeholderMetadataRefreshIntervalDays,
                     m_dialog.m_placeholderRefreshCombo);
  }

  if (m_profile.showMusicFolderStructure &&
      m_dialog.m_musicFolderStructureCombo) {
    state.musicFolderStructure =
        m_dialog.m_musicFolderStructureCombo->currentData().toString();
  }

  const QString preferredMetadataLanguage =
      m_dialog.selectedMetadataLanguage();
  const QString metadataCountryCode =
      m_dialog.selectedMetadataCountryCode();
  const QString preferredImageLanguage =
      m_dialog.selectedImageLanguage();

  state.preferredMetadataLanguage = preferredMetadataLanguage;
  state.metadataCountryCode = metadataCountryCode;
  state.preferredImageLanguage = preferredImageLanguage;

  qDebug() << "[LibraryCreateDialogOptionsBuilder] metadata selection"
           << "| metadataLangText:"
           << (m_dialog.m_metaLangCombo ? m_dialog.m_metaLangCombo->currentText()
                                        : QString())
           << "| metadataLangData:" << preferredMetadataLanguage
           << "| metadataLangIndex:"
           << (m_dialog.m_metaLangCombo ? m_dialog.m_metaLangCombo->currentIndex()
                                        : -1)
           << "| countryText:"
           << (m_dialog.m_countryCombo ? m_dialog.m_countryCombo->currentText()
                                       : QString())
           << "| countryData:" << metadataCountryCode
           << "| countryIndex:"
           << (m_dialog.m_countryCombo ? m_dialog.m_countryCombo->currentIndex()
                                       : -1)
           << "| imageLangText:"
           << (m_dialog.m_imageLangCombo ? m_dialog.m_imageLangCombo->currentText()
                                         : QString())
           << "| imageLangData:" << preferredImageLanguage
           << "| imageLangIndex:"
           << (m_dialog.m_imageLangCombo ? m_dialog.m_imageLangCombo->currentIndex()
                                         : -1);
}

void LibraryCreateDialogOptionsBuilder::collectAdvancedState(
    LOS::LibraryOptionsState &state) const {
  if (m_profile.showEnablePlexIgnore) {
    assignChecked(state.enablePlexIgnore, m_dialog.m_chkEnablePlexIgnore);
  }
  if (m_profile.showAutomaticSeriesGrouping) {
    assignChecked(state.enableAutomaticSeriesGrouping,
                  m_dialog.m_chkAutomaticallyGroupSeries);
  }
  if (m_profile.showMultiPartItems) {
    assignChecked(state.enableMultiPartItems, m_dialog.m_chkEnableMultiPart);
  }

  if (m_profile.showSampleIgnoreSize && m_dialog.m_spinIgnoreSamplesMb) {
    state.sampleIgnoreSizeBytes =
        m_dialog.m_spinIgnoreSamplesMb->value() * 1024 * 1024;
  }

  if (m_profile.showMultiVersionGrouping && m_dialog.m_multiVersionCombo) {
    const QString mode = m_dialog.m_multiVersionCombo->currentData().toString();
    state.enableMultiVersionByFiles = (mode == "both" || mode == "files");
    state.enableMultiVersionByMetadata =
        (mode == "both" || mode == "metadata");
  }
}

void LibraryCreateDialogOptionsBuilder::collectImageState(
    LOS::LibraryOptionsState &state) const {
  if (m_serverType == ServerProfile::Emby) {
    assignChecked(state.saveLocalMetadata, m_dialog.m_chkSaveArtwork);
  } else {
    assignChecked(state.saveLocalThumbnailSets, m_dialog.m_chkSaveArtwork);
  }
  if (m_profile.showCacheImages) {
    assignChecked(state.cacheImages, m_dialog.m_chkCacheImages);
  }
  if (m_profile.showSaveMetadataHidden) {
    assignChecked(state.saveMetadataHidden, m_dialog.m_chkSaveMetadataHidden);
  }
  assignChecked(state.downloadImagesInAdvance,
                m_dialog.m_chkDownloadImagesInAdvance);
  if (m_profile.showSaveThumbnailSets) {
    assignChecked(state.saveLocalThumbnailSets, m_dialog.m_chkSaveThumbnailSets);
  }
  if (m_profile.showAutoGenerateChapters) {
    assignChecked(state.autoGenerateChapters, m_dialog.m_chkGenerateChapters);
  }

  if (m_profile.showThumbnailGeneration && m_dialog.m_thumbnailScheduleCombo) {
    const QString mode =
        m_dialog.m_thumbnailScheduleCombo->currentData().toString();
    state.enableChapterImageExtraction =
        !mode.isEmpty();
    state.extractChapterImagesDuringLibraryScan = (mode == "scanandtask");
  } else if (m_dialog.m_chkExtractChapterImages) {
    assignChecked(state.enableChapterImageExtraction,
                  m_dialog.m_chkExtractChapterImages);
  }

  if (m_profile.showThumbnailInterval) {
    assignCurrentInt(state.thumbnailImagesIntervalSeconds,
                     m_dialog.m_thumbnailIntervalCombo);
  }
  if (m_profile.showAutoGenerateChapterInterval) {
    assignCurrentInt(state.autoGenerateChapterIntervalMinutes,
                     m_dialog.m_chapterIntervalCombo);
  }

  if (m_profile.showIntroDetection && m_dialog.m_introDetectionCombo) {
    const QString mode =
        m_dialog.m_introDetectionCombo->currentData().toString();
    state.enableMarkerDetection = !mode.isEmpty();
    state.enableMarkerDetectionDuringLibraryScan = (mode == "scanandtask");
  }
}

void LibraryCreateDialogOptionsBuilder::collectSubtitleState(
    LOS::LibraryOptionsState &state) const {
  assignChecked(state.saveSubtitlesWithMedia, m_dialog.m_chkSaveSubtitles);
  assignChecked(state.skipSubtitlesIfAudioTrackMatches,
                m_dialog.m_chkSkipIfAudioMatchesSub);
  assignChecked(state.skipSubtitlesIfEmbeddedSubtitlesPresent,
                m_dialog.m_chkSkipIfEmbeddedSub);
  assignCurrentInt(state.subtitleDownloadMaxAgeDays, m_dialog.m_subAgeLimitCombo);
  if (m_profile.showRequirePerfectSubtitleMatch) {
    assignChecked(state.requirePerfectSubtitleMatch,
                  m_dialog.m_chkRequirePerfectSubtitleMatch);
  }
  if (m_profile.showForcedSubtitlesOnly) {
    assignChecked(state.forcedSubtitlesOnly,
                  m_dialog.m_chkForcedSubtitlesOnly);
  }
  state.subtitleDownloadLanguages = m_dialog.selectedSubtitleLanguages();
}

void LibraryCreateDialogOptionsBuilder::collectLyricsState(
    LOS::LibraryOptionsState &state) const {
  if (!m_profile.showLyricsSection) {
    return;
  }

  assignChecked(state.saveLyricsWithMedia, m_dialog.m_chkSaveLyrics);
  assignCurrentInt(state.lyricsDownloadMaxAgeDays, m_dialog.m_lyricsAgeLimitCombo);
  state.lyricsDownloadLanguages = m_dialog.selectedLyricsLanguages();
}

void LibraryCreateDialogOptionsBuilder::collectPlaybackState(
    LOS::LibraryOptionsState &state) const {
  if (m_dialog.m_spinMinResumePercent) {
    state.minResumePct = m_dialog.m_spinMinResumePercent->value();
  }
  if (m_dialog.m_spinMaxResumePercent) {
    state.maxResumePct = m_dialog.m_spinMaxResumePercent->value();
  }
  if (m_dialog.m_spinMinResumeDuration) {
    state.minResumeDurationSeconds = m_dialog.m_spinMinResumeDuration->value();
  }
}

void LibraryCreateDialogOptionsBuilder::collectFetcherState(
    LOS::LibraryOptionsState &state) const {
  state.metadataFetchers = collectFetcherSelection(
      m_dialog.m_metadataFetcherRows, LTU::FetcherKind::Metadata);
  state.seasonMetadataFetchers = collectFetcherSelection(
      m_dialog.m_seasonMetadataFetcherRows, LTU::FetcherKind::Metadata);
  state.episodeMetadataFetchers = collectFetcherSelection(
      m_dialog.m_episodeMetadataFetcherRows, LTU::FetcherKind::Metadata);
  state.imageFetchers = collectFetcherSelection(m_dialog.m_imageFetcherRows,
                                                LTU::FetcherKind::Image);
  state.seasonImageFetchers = collectFetcherSelection(
      m_dialog.m_seasonImageFetcherRows, LTU::FetcherKind::Image);
  state.episodeImageFetchers = collectFetcherSelection(
      m_dialog.m_episodeImageFetcherRows, LTU::FetcherKind::Image);
  state.subtitleFetchers = collectFetcherSelection(
      m_dialog.m_subtitleDownloaderRows, LTU::FetcherKind::Subtitle);
  state.lyricsFetchers = collectFetcherSelection(
      m_dialog.m_lyricsFetcherRows, LTU::FetcherKind::Lyrics);
}

LOS::FetcherSelection LibraryCreateDialogOptionsBuilder::collectFetcherSelection(
    const QList<FetcherRowWidget *> &rows, LTU::FetcherKind kind) const {
  LOS::FetcherSelection selection;

  for (auto *row : rows) {
    if (!row ||
        !LTU::isFetcherApplicable(row->name(), kind, m_profile,
                                 m_serverType)) {
      continue;
    }

    if (row->isEnabled()) {
      selection.enabledNames.append(row->name());
    } else {
      selection.disabledNames.append(row->name());
    }
  }

  return selection;
}
