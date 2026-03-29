#ifndef LIBRARYCREATEDLG_H
#define LIBRARYCREATEDLG_H

#include "moderndialogbase.h"
#include <models/admin/adminmodels.h>
#include <models/profile/serverprofile.h>
#include <QJsonObject>
#include <QStringList>
#include <QVariant>

class QLineEdit;
class QLabel;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QWidget;
class ModernComboBox;
class ModernSwitch;
class ModernTagInput;
class QEmbyCore;
class FetcherRowWidget;
class CollapsibleSection;
class PathRowWidget;




class LibraryCreateDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit LibraryCreateDialog(QEmbyCore* core,
                                  const QStringList& metadataFetchers = {},
                                  const QStringList& imageFetchers = {},
                                  const QStringList& subtitleDownloaders = {},
                                  const QStringList& lyricsFetchers = {},
                                  const QList<LocalizationCulture>& cultures = {},
                                  const QList<LocalizationCountry>& countries = {},
                                  QWidget *parent = nullptr);

    QString libraryName() const;
    QString collectionType() const;
    QStringList paths() const;
    QJsonObject libraryOptions() const;

protected:
    void setConfirmButtonText(const QString& text);
    void setTypeEditable(bool editable);
    void loadFromFolder(const VirtualFolder& folder);

private:
    friend class LibraryCreateDialogOptionsBuilder;

    void setupUi();
    void setupBasicSection(QVBoxLayout* layout);
    void setupLibrarySettingsSection(QVBoxLayout* layout);
    void setupAdvancedSection(QVBoxLayout* layout);
    void setupImageSection(QVBoxLayout* layout);
    void setupSubtitleSection(QVBoxLayout* layout);
    void setupLyricsSection(QVBoxLayout* layout);
    void setupPlaybackSection(QVBoxLayout* layout);
    void applyTypeDefaults();
    void populatePreferredLanguageCombo(ModernComboBox* combo,
                                        bool useRegionalCodes);
    void populateCountryCombo(ModernComboBox* combo);
    void populateSubtitleLanguageInput(ModernTagInput* input,
                                       bool useRegionalCodes);
    void populateScheduleCombo(ModernComboBox* combo) const;
    void populatePlaceholderRefreshCombo(ModernComboBox* combo) const;
    void populateMultiVersionCombo(ModernComboBox* combo) const;
    void populateThumbnailIntervalCombo(ModernComboBox* combo) const;
    void populateChapterIntervalCombo(ModernComboBox* combo) const;
    QString selectedComboValue(const ModernComboBox* combo) const;
    QString selectedMetadataLanguage() const;
    QString selectedMetadataCountryCode() const;
    QString selectedImageLanguage() const;
    QStringList selectedSubtitleLanguages() const;
    QStringList selectedLyricsLanguages() const;
    QString normalizedCultureCode(const QString& value) const;
    QString normalizedCountryCode(const QString& value) const;
    QStringList normalizedCultureCodes(const QStringList& values) const;
    QString normalizedCollectionType() const;
    void updateTypeSpecificSettings();
    int adjacentApplicableFetcherIndex(const QList<FetcherRowWidget*>& rows,
                                       int currentIndex,
                                       int direction) const;
    void setPaths(const QStringList& paths);
    void setComboCurrentData(ModernComboBox* combo, const QVariant& value);
    void applyLibraryOptions(const QJsonObject& libraryOptions);
    void applyFetcherSelection(QVBoxLayout* container,
                               QList<FetcherRowWidget*>& rows,
                               const QStringList& enabledNames,
                               const QStringList& disabledNames,
                               const QStringList& orderedNames);
    QJsonObject mergedLibraryOptions(const QJsonObject& currentOptions) const;
    void clearManagedLibraryOptionKeys(QJsonObject& options) const;

    
    void addFetcherRows(QVBoxLayout* container,
                        const QStringList& names,
                        QList<FetcherRowWidget*>& rows);
    void swapFetcherRow(QVBoxLayout* container,
                        QList<FetcherRowWidget*>& rows,
                        int from, int to);
    void updateArrowStates(const QList<FetcherRowWidget*>& rows);

    
    PathRowWidget* addPathRow(const QString& path = QString());
    void removePathRow(PathRowWidget* row);
    void onBrowsePath(PathRowWidget* row);

    static QStringList defaultMetadataFetchers(ServerProfile::ServerType serverType);
    static QStringList defaultImageFetchers(ServerProfile::ServerType serverType);
    static QStringList defaultSubtitleDownloaders(ServerProfile::ServerType serverType);

    QEmbyCore* m_core = nullptr;

    QStringList m_metadataFetcherNames;
    QStringList m_imageFetcherNames;
    QStringList m_subtitleDownloaderNames;
    QStringList m_lyricsFetcherNames;
    QList<LocalizationCulture> m_availableCultures;
    QList<LocalizationCountry> m_availableCountries;
    QJsonObject m_baseLibraryOptions;

    
    QLineEdit* m_nameEdit = nullptr;
    ModernComboBox* m_typeCombo = nullptr;

    
    QVBoxLayout* m_pathRowsLayout = nullptr;
    QList<PathRowWidget*> m_pathRows;

    
    CollapsibleSection* m_librarySettingsSection = nullptr;
    CollapsibleSection* m_advancedSection = nullptr;
    ModernSwitch* m_chkRealtimeMonitor = nullptr;
    ModernSwitch* m_chkEmbeddedTitles = nullptr;
    ModernSwitch* m_chkEnablePhotos = nullptr;
    ModernSwitch* m_chkImportPlaylists = nullptr;
    ModernSwitch* m_chkExcludeFromSearch = nullptr;
    ModernSwitch* m_chkMergeFolders = nullptr;
    ModernSwitch* m_chkSaveNfo = nullptr;
    ModernSwitch* m_chkNfoReader = nullptr;
    ModernSwitch* m_chkAllowAdultMetadata = nullptr;    
    ModernSwitch* m_chkImportCollectionInfo = nullptr;  
    ModernSwitch* m_chkAutoCollection = nullptr;
    ModernSwitch* m_chkAutomaticallyGroupSeries = nullptr;
    ModernSwitch* m_chkEnablePlexIgnore = nullptr;
    ModernSwitch* m_chkEnableMultiPart = nullptr;
    ModernComboBox* m_minCollSizeCombo = nullptr;
    ModernComboBox* m_placeholderRefreshCombo = nullptr;
    ModernComboBox* m_musicFolderStructureCombo = nullptr;
    ModernComboBox* m_multiVersionCombo = nullptr;
    QSpinBox* m_spinIgnoreSamplesMb = nullptr;
    QWidget* m_enablePhotosRow = nullptr;
    QWidget* m_importPlaylistsRow = nullptr;
    QWidget* m_excludeFromSearchRow = nullptr;
    QWidget* m_mergeFoldersRow = nullptr;
    QWidget* m_embeddedTitlesRow = nullptr;
    QWidget* m_metaLangRow = nullptr;
    QWidget* m_countryRow = nullptr;
    QWidget* m_imageLangRow = nullptr;
    QWidget* m_musicFolderStructureRow = nullptr;
    QWidget* m_nfoRow = nullptr;
    QWidget* m_nfoReaderRow = nullptr;
    QWidget* m_adultMetadataRow = nullptr;
    QWidget* m_importCollectionRow = nullptr;           
    QWidget* m_autoCollectionRow = nullptr;             
    QWidget* m_minCollectionSizeRow = nullptr;          
    QWidget* m_metaRefreshRow = nullptr;
    QWidget* m_placeholderRefreshRow = nullptr;
    QWidget* m_ignoreSamplesRow = nullptr;
    QWidget* m_plexIgnoreRow = nullptr;
    QWidget* m_autoGroupSeriesRow = nullptr;
    QWidget* m_multiPartRow = nullptr;
    QWidget* m_multiVersionRow = nullptr;
    QWidget* m_metadataFetchersPanel = nullptr;
    QWidget* m_seasonMetadataFetchersPanel = nullptr;
    QWidget* m_episodeMetadataFetchersPanel = nullptr;
    QWidget* m_libraryTopSeparator = nullptr;
    QWidget* m_libraryMiddleSeparator = nullptr;

    
    ModernComboBox* m_metaLangCombo = nullptr;
    ModernComboBox* m_countryCombo = nullptr;
    ModernComboBox* m_imageLangCombo = nullptr;  
    ModernComboBox* m_metaRefreshCombo = nullptr;
    QLabel* m_metadataFetchersLabel = nullptr;
    QLabel* m_seasonMetadataFetchersLabel = nullptr;
    QLabel* m_episodeMetadataFetchersLabel = nullptr;
    QVBoxLayout* m_metaFetcherLayout = nullptr;
    QList<FetcherRowWidget*> m_metadataFetcherRows;
    QVBoxLayout* m_seasonMetaFetcherLayout = nullptr;
    QList<FetcherRowWidget*> m_seasonMetadataFetcherRows;
    QVBoxLayout* m_episodeMetaFetcherLayout = nullptr;
    QList<FetcherRowWidget*> m_episodeMetadataFetcherRows;

    
    CollapsibleSection* m_imageSection = nullptr;
    ModernSwitch* m_chkSaveArtwork = nullptr;
    ModernSwitch* m_chkCacheImages = nullptr;
    ModernSwitch* m_chkSaveMetadataHidden = nullptr;
    ModernSwitch* m_chkDownloadImagesInAdvance = nullptr;
    ModernSwitch* m_chkExtractChapterImages = nullptr;
    ModernSwitch* m_chkSaveThumbnailSets = nullptr;
    ModernSwitch* m_chkGenerateChapters = nullptr;
    ModernComboBox* m_thumbnailScheduleCombo = nullptr;
    ModernComboBox* m_thumbnailIntervalCombo = nullptr;
    ModernComboBox* m_chapterIntervalCombo = nullptr;
    ModernComboBox* m_introDetectionCombo = nullptr;
    QWidget* m_saveArtworkRow = nullptr;
    QWidget* m_cacheImagesRow = nullptr;
    QWidget* m_saveMetadataHiddenRow = nullptr;
    QWidget* m_downloadImagesRow = nullptr;
    QWidget* m_extractChapterImagesRow = nullptr;
    QWidget* m_thumbnailScheduleRow = nullptr;
    QWidget* m_thumbnailIntervalRow = nullptr;
    QWidget* m_saveThumbnailSetsRow = nullptr;
    QWidget* m_generateChaptersRow = nullptr;
    QWidget* m_chapterIntervalRow = nullptr;
    QWidget* m_introDetectionRow = nullptr;
    QWidget* m_imageFetchersPanel = nullptr;
    QWidget* m_seasonImageFetchersPanel = nullptr;
    QWidget* m_episodeImageFetchersPanel = nullptr;
    QWidget* m_imageOptionsSeparator = nullptr;
    QLabel* m_imageFetchersLabel = nullptr;
    QLabel* m_seasonImageFetchersLabel = nullptr;
    QLabel* m_episodeImageFetchersLabel = nullptr;
    QVBoxLayout* m_imgFetcherLayout = nullptr;
    QList<FetcherRowWidget*> m_imageFetcherRows;
    QVBoxLayout* m_seasonImgFetcherLayout = nullptr;
    QList<FetcherRowWidget*> m_seasonImageFetcherRows;
    QVBoxLayout* m_episodeImgFetcherLayout = nullptr;
    QList<FetcherRowWidget*> m_episodeImageFetcherRows;

    
    CollapsibleSection* m_subtitleSection = nullptr;
    ModernSwitch* m_chkSaveSubtitles = nullptr;
    ModernSwitch* m_chkSkipIfAudioMatchesSub = nullptr;
    ModernSwitch* m_chkSkipIfEmbeddedSub = nullptr;
    ModernSwitch* m_chkRequirePerfectSubtitleMatch = nullptr;
    ModernSwitch* m_chkForcedSubtitlesOnly = nullptr;
    ModernComboBox* m_subAgeLimitCombo = nullptr;
    ModernTagInput* m_subtitleLangInput = nullptr;
    QWidget* m_subtitleDownloadersPanel = nullptr;
    QWidget* m_subtitleDownloadersSeparator = nullptr;
    QLabel* m_subtitleDownloadersLabel = nullptr;
    QVBoxLayout* m_subDownloaderLayout = nullptr;
    QList<FetcherRowWidget*> m_subtitleDownloaderRows;

    
    CollapsibleSection* m_lyricsSection = nullptr;
    QWidget* m_lyricsFetchersPanel = nullptr;
    QWidget* m_lyricsFetchersSeparator = nullptr;
    QLabel* m_lyricsFetchersLabel = nullptr;
    QVBoxLayout* m_lyricsFetcherLayout = nullptr;
    QList<FetcherRowWidget*> m_lyricsFetcherRows;
    ModernTagInput* m_lyricsLangInput = nullptr;
    ModernComboBox* m_lyricsAgeLimitCombo = nullptr;
    ModernSwitch* m_chkSaveLyrics = nullptr;

    
    CollapsibleSection* m_playbackSection = nullptr;
    QSpinBox* m_spinMinResumePercent = nullptr;
    QSpinBox* m_spinMaxResumePercent = nullptr;
    QSpinBox* m_spinMinResumeDuration = nullptr;

    
    QPushButton* m_okBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;
};

#endif 
