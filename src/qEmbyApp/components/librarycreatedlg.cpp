#include "librarycreatedlg.h"
#include "librarycreatedlg_p.h"
#include "collapsiblesection.h"
#include "../utils/layoututils.h"
#include "../utils/libraryoptionsserializer.h"
#include "../utils/librarytypeprofile.h"
#include "fetcherrowwidget.h"
#include "moderncombobox.h"
#include "modernmessagebox.h"
#include "moderntaginput.h"
#include "modernswitch.h"
#include "pathrowwidget.h"
#include "serverbrowserdlg.h"
#include <QHBoxLayout>
#include <QComboBox>
#include <QFrame>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QSpinBox>
#include <QVBoxLayout>
#include <QDebug>
#include <initializer_list>
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>

namespace LTU = LibraryTypeUtils;
namespace LOS = LibraryOptionsSerializer;

namespace {

QStringList deduplicateValues(const QStringList &values) {
  QStringList result;
  for (const QString &value : values) {
    const QString trimmed = value.trimmed();
    if (!trimmed.isEmpty() && !result.contains(trimmed)) {
      result.append(trimmed);
    }
  }
  return result;
}

QStringList jsonArrayToStringList(const QJsonValue &value) {
  if (!value.isArray()) {
    return {};
  }

  QStringList result;
  for (const auto &entry : value.toArray()) {
    const QString text = entry.toString().trimmed();
    if (!text.isEmpty() && !result.contains(text)) {
      result.append(text);
    }
  }
  return result;
}

QStringList firstStringListForKeys(
    const QJsonObject &object,
    std::initializer_list<const char *> keys) {
  for (const char *key : keys) {
    const QStringList values =
        jsonArrayToStringList(object.value(QLatin1String(key)));
    if (!values.isEmpty()) {
      return values;
    }
  }
  return {};
}

bool containsAnyKey(const QJsonObject &object,
                    std::initializer_list<const char *> keys) {
  for (const char *key : keys) {
    if (object.contains(QLatin1String(key))) {
      return true;
    }
  }
  return false;
}

QJsonObject firstMatchingTypeOption(
    const QJsonArray &typeOptions, const QStringList &preferredTypes,
    std::initializer_list<const char *> keys) {
  for (const QString &type : preferredTypes) {
    for (const auto &entry : typeOptions) {
      const QJsonObject candidate = entry.toObject();
      if (candidate.value("Type").toString() != type) {
        continue;
      }
      if (keys.size() == 0 || containsAnyKey(candidate, keys)) {
        return candidate;
      }
    }
  }
  return {};
}

QStringList topMetadataTypesForCollection(const QString &collectionType) {
  if (collectionType == "movies") {
    return {QStringLiteral("Movie")};
  }
  if (collectionType == "tvshows") {
    return {QStringLiteral("Series")};
  }
  if (collectionType == "music") {
    return {QStringLiteral("MusicArtist"), QStringLiteral("MusicAlbum")};
  }
  if (collectionType == "musicvideos") {
    return {QStringLiteral("MusicVideo"), QStringLiteral("Video")};
  }
  return {};
}

QStringList topImageTypesForCollection(const QString &collectionType) {
  if (collectionType == "movies") {
    return {QStringLiteral("Movie")};
  }
  if (collectionType == "tvshows") {
    return {QStringLiteral("Series")};
  }
  if (collectionType == "music") {
    return {QStringLiteral("MusicArtist"), QStringLiteral("MusicAlbum"),
            QStringLiteral("Audio")};
  }
  if (collectionType == "homevideos") {
    return {QStringLiteral("Video")};
  }
  if (collectionType == "musicvideos") {
    return {QStringLiteral("MusicVideo"), QStringLiteral("Video")};
  }
  return {};
}

QString typeOptionType(const QJsonObject &typeOption) {
  return typeOption.value(QStringLiteral("Type")).toString();
}

QJsonArray mergeTypeOptions(const QJsonArray &baseTypeOptions,
                            const QJsonArray &currentTypeOptions) {
  static const QStringList managedTypeKeys = {
      QStringLiteral("MetadataFetchers"),
      QStringLiteral("MetadataFetcherOrder"),
      QStringLiteral("DisabledMetadataFetchers"),
      QStringLiteral("ImageFetchers"),
      QStringLiteral("ImageFetcherOrder"),
      QStringLiteral("DisabledImageFetchers")};

  if (baseTypeOptions.isEmpty()) {
    return currentTypeOptions;
  }
  if (currentTypeOptions.isEmpty()) {
    return baseTypeOptions;
  }

  auto findTypeOption = [](const QJsonArray &typeOptions,
                           const QString &type) -> QJsonObject {
    for (const auto &entry : typeOptions) {
      const QJsonObject candidate = entry.toObject();
      if (typeOptionType(candidate) == type) {
        return candidate;
      }
    }
    return {};
  };

  QJsonArray mergedTypeOptions;
  QStringList mergedTypes;

  for (const auto &entry : currentTypeOptions) {
    const QJsonObject currentTypeOption = entry.toObject();
    const QString currentType = typeOptionType(currentTypeOption);
    if (currentType.isEmpty()) {
      mergedTypeOptions.append(currentTypeOption);
      continue;
    }

    QJsonObject mergedTypeOption = findTypeOption(baseTypeOptions, currentType);
    for (const QString &key : managedTypeKeys) {
      mergedTypeOption.remove(key);
    }
    for (auto it = currentTypeOption.begin(); it != currentTypeOption.end();
         ++it) {
      if (it.key() == QStringLiteral("ImageOptions") &&
          mergedTypeOption.contains(it.key())) {
        continue;
      }
      mergedTypeOption.insert(it.key(), it.value());
    }

    mergedTypeOptions.append(mergedTypeOption);
    mergedTypes.append(currentType);
  }

  for (const auto &entry : baseTypeOptions) {
    const QJsonObject baseTypeOption = entry.toObject();
    const QString baseType = typeOptionType(baseTypeOption);
    if (!baseType.isEmpty() && mergedTypes.contains(baseType)) {
      continue;
    }
    mergedTypeOptions.append(baseTypeOption);
  }

  return mergedTypeOptions;
}

QString scheduleMode(bool enabled, bool duringScan) {
  if (enabled) {
    return duringScan ? QStringLiteral("scanandtask")
                      : QStringLiteral("task");
  }
  return {};
}

QString multiVersionMode(bool byFiles, bool byMetadata) {
  if (byFiles && byMetadata) {
    return QStringLiteral("both");
  }
  if (byFiles) {
    return QStringLiteral("files");
  }
  if (byMetadata) {
    return QStringLiteral("metadata");
  }
  return QStringLiteral("none");
}

int bytesToMegabytes(const QJsonValue &value, int fallbackMegabytes) {
  const qint64 bytes = value.toVariant().toLongLong();
  if (bytes < 0) {
    return fallbackMegabytes;
  }

  return static_cast<int>(bytes / (1024 * 1024));
}

} 








QStringList LibraryCreateDialog::defaultMetadataFetchers(
    ServerProfile::ServerType serverType) {
  if (serverType == ServerProfile::Jellyfin) {
    return {"TheMovieDb", "The Open Movie Database", "MusicBrainz",
            "TheAudioDb"};
  }

  return {"TheMovieDb", "The Open Movie Database", "TheMovieDb Box Sets",
          "MusicBrainz", "TheAudioDb"};
}

QStringList LibraryCreateDialog::defaultImageFetchers(
    ServerProfile::ServerType serverType) {
  if (serverType == ServerProfile::Jellyfin) {
    return {"TheMovieDb", "The Open Movie Database",
            "Embedded Image Extractor", "Screen Grabber"};
  }

  return {"TheMovieDb", "TheTVDB", "FanArt", "Screen Grabber"};
}

QStringList LibraryCreateDialog::defaultSubtitleDownloaders(
    ServerProfile::ServerType ) {
  return {"Open Subtitles", "Podnapisi"};
}




LibraryCreateDialog::LibraryCreateDialog(QEmbyCore *core,
                                         const QStringList &metadataFetchers,
                                         const QStringList &imageFetchers,
                                         const QStringList &subtitleDownloaders,
                                         const QStringList &lyricsFetchers,
                                         const QList<LocalizationCulture> &cultures,
                                         const QList<LocalizationCountry> &countries,
                                         QWidget *parent)
    : ModernDialogBase(parent), m_core(core),
      m_availableCultures(cultures), m_availableCountries(countries) {
  setTitle(tr("Create Library"));
  setMinimumWidth(480);
  setMaximumHeight(700);
  const auto serverType = m_core->serverManager()->activeProfile().type;

  m_metadataFetcherNames =
      metadataFetchers.isEmpty() ? defaultMetadataFetchers(serverType)
                                 : metadataFetchers;
  m_imageFetcherNames =
      imageFetchers.isEmpty() ? defaultImageFetchers(serverType)
                              : imageFetchers;
  m_subtitleDownloaderNames = subtitleDownloaders.isEmpty()
                                  ? defaultSubtitleDownloaders(serverType)
                                  : subtitleDownloaders;
  m_lyricsFetcherNames = lyricsFetchers;

  setupUi();
  resize(520, 560);
}

void LibraryCreateDialog::setupUi() {
  auto *outerLayout = contentLayout();
  
  outerLayout->setContentsMargins(20, 10, 0, 20);

  auto *scrollArea = new QScrollArea(this);
  scrollArea->setObjectName("LibCreateScrollArea");
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  auto *scrollWidget = new QWidget();
  auto *layout = new QVBoxLayout(scrollWidget);
  layout->setContentsMargins(0, 0, 12, 0);
  layout->setSpacing(8);

  setupBasicSection(layout);
  layout->addSpacing(4);
  setupLibrarySettingsSection(layout);
  setupAdvancedSection(layout);
  setupImageSection(layout);
  setupSubtitleSection(layout);
  setupLyricsSection(layout);
  setupPlaybackSection(layout);

  layout->addStretch(1);
  scrollArea->setWidget(scrollWidget);
  outerLayout->addWidget(scrollArea, 1);
  outerLayout->addSpacing(12);

  
  auto *bottomRow = new QHBoxLayout();
  bottomRow->setContentsMargins(0, 0, 12, 0);
  bottomRow->addStretch(1);

  m_cancelBtn = new QPushButton(tr("Cancel"), this);
  m_cancelBtn->setObjectName("SettingsCardButton");
  m_cancelBtn->setCursor(Qt::PointingHandCursor);

  m_okBtn = new QPushButton(tr("Create"), this);
  m_okBtn->setObjectName("SettingsCardButton");
  m_okBtn->setCursor(Qt::PointingHandCursor);

  bottomRow->addWidget(m_cancelBtn);
  bottomRow->addSpacing(8);
  bottomRow->addWidget(m_okBtn);
  outerLayout->addLayout(bottomRow);

  connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
  connect(m_okBtn, &QPushButton::clicked, this, [this]() {
    if (m_nameEdit->text().trimmed().isEmpty()) {
      m_nameEdit->setFocus();
      return;
    }
    if (paths().isEmpty()) {
      bool confirmed = ModernMessageBox::question(
          this, tr("No Media Paths"),
          tr("No media paths have been added. The library will be created "
             "without any content folders.\n\nDo you want to continue?"),
          tr("Continue"), tr("Cancel"), ModernMessageBox::Primary,
          ModernMessageBox::Info);
      if (!confirmed)
        return;
    }
    accept();
  });

  connect(m_typeCombo, qOverload<int>(&QComboBox::currentIndexChanged), this,
          [this](int) {
            applyTypeDefaults();
            updateTypeSpecificSettings();
          });

  applyTypeDefaults();
  updateTypeSpecificSettings();
}

void LibraryCreateDialog::setConfirmButtonText(const QString &text) {
  if (m_okBtn) {
    m_okBtn->setText(text);
  }
}

void LibraryCreateDialog::setTypeEditable(bool editable) {
  if (!m_typeCombo) {
    return;
  }

  m_typeCombo->setEnabled(editable);
  m_typeCombo->setFocusPolicy(editable ? Qt::StrongFocus : Qt::NoFocus);
}

void LibraryCreateDialog::loadFromFolder(const VirtualFolder &folder) {
  qDebug() << "[LibraryCreateDialog] Loading folder into dialog"
           << "| name:" << folder.name
           << "| itemId:" << folder.itemId
           << "| type:" << folder.collectionType
           << "| pathCount:" << folder.locations.size();

  m_baseLibraryOptions = folder.libraryOptions;

  if (m_nameEdit) {
    m_nameEdit->setText(folder.name);
  }

  setComboCurrentData(m_typeCombo,
                      LTU::canonicalCollectionType(folder.collectionType));
  applyTypeDefaults();
  updateTypeSpecificSettings();
  setPaths(folder.locations);
  applyLibraryOptions(folder.libraryOptions);
  updateTypeSpecificSettings();
}

void LibraryCreateDialog::populatePreferredLanguageCombo(
    ModernComboBox *combo, bool useRegionalCodes) {
  if (!combo)
    return;

  constexpr int kMaxLangTextWidth = 180;
  combo->setMaxTextWidth(kMaxLangTextWidth);
  combo->clear();
  combo->addItem(tr("Server Default"), "");

  if (!m_availableCultures.isEmpty()) {
    QStringList addedCodes;
    for (const LocalizationCulture &culture : m_availableCultures) {
      const QString code = culture.canonicalCode();
      const QString label = culture.visibleName();
      if (code.isEmpty() || label.isEmpty() ||
          addedCodes.contains(code, Qt::CaseInsensitive)) {
        continue;
      }

      combo->addItem(label, code);
      
      combo->setItemData(combo->count() - 1, label, Qt::ToolTipRole);
      addedCodes.append(code);
    }
    return;
  }

  combo->addItem(tr("English"), useRegionalCodes ? "en-US" : "en");
  combo->addItem(tr("Chinese (Simplified)"),
                 useRegionalCodes ? "zh-CN" : "zh");
  combo->addItem(tr("Chinese (Traditional)"), "zh-TW");
  combo->addItem(tr("Chinese (Hong Kong)"), "zh-HK");
  combo->addItem(tr("Japanese"), useRegionalCodes ? "ja-JP" : "ja");
  combo->addItem(tr("Korean"), useRegionalCodes ? "ko-KR" : "ko");
  combo->addItem(tr("French"), useRegionalCodes ? "fr-FR" : "fr");
  combo->addItem(tr("German"), useRegionalCodes ? "de-DE" : "de");
  combo->addItem(tr("Spanish"), useRegionalCodes ? "es-ES" : "es");
  combo->addItem(tr("Russian"), useRegionalCodes ? "ru-RU" : "ru");
  combo->addItem(tr("Portuguese"), useRegionalCodes ? "pt-PT" : "pt");
  combo->addItem(tr("Italian"), useRegionalCodes ? "it-IT" : "it");
}

void LibraryCreateDialog::populateCountryCombo(ModernComboBox *combo) {
  if (!combo)
    return;

  combo->clear();
  combo->addItem(tr("Server Default"), "");

  if (!m_availableCountries.isEmpty()) {
    QStringList addedCodes;
    for (const LocalizationCountry &country : m_availableCountries) {
      const QString code = country.canonicalCode();
      const QString label = country.visibleName();
      if (code.isEmpty() || label.isEmpty() ||
          addedCodes.contains(code, Qt::CaseInsensitive)) {
        continue;
      }

      combo->addItem(label, code);
      addedCodes.append(code);
    }
    return;
  }

  combo->addItem(tr("United States"), "US");
  combo->addItem(tr("China"), "CN");
  combo->addItem(tr("Taiwan"), "TW");
  combo->addItem(tr("Hong Kong"), "HK");
  combo->addItem(tr("Japan"), "JP");
  combo->addItem(tr("South Korea"), "KR");
  combo->addItem(tr("United Kingdom"), "GB");
  combo->addItem(tr("France"), "FR");
  combo->addItem(tr("Germany"), "DE");
}

void LibraryCreateDialog::populateSubtitleLanguageInput(
    ModernTagInput *input, bool useRegionalCodes) {
  if (!input)
    return;

  input->clearPresets();

  if (!m_availableCultures.isEmpty()) {
    QStringList addedCodes;
    for (const LocalizationCulture &culture : m_availableCultures) {
      const QString code = culture.canonicalCode();
      const QString label = culture.visibleName();
      if (code.isEmpty() || label.isEmpty() ||
          addedCodes.contains(code, Qt::CaseInsensitive)) {
        continue;
      }

      input->addPreset(label, code);
      addedCodes.append(code);
    }
    return;
  }

  input->addPreset(tr("English"), useRegionalCodes ? "en-US" : "en");
  input->addPreset(tr("Chinese (Simplified)"),
                   useRegionalCodes ? "zh-CN" : "zh");
  input->addPreset(tr("Chinese (Traditional)"), "zh-TW");
  input->addPreset(tr("Chinese (Hong Kong)"), "zh-HK");
  input->addPreset(tr("Japanese"), useRegionalCodes ? "ja-JP" : "ja");
  input->addPreset(tr("Korean"), useRegionalCodes ? "ko-KR" : "ko");
  input->addPreset(tr("French"), useRegionalCodes ? "fr-FR" : "fr");
  input->addPreset(tr("German"), useRegionalCodes ? "de-DE" : "de");
  input->addPreset(tr("Spanish"), useRegionalCodes ? "es-ES" : "es");
  input->addPreset(tr("Russian"), useRegionalCodes ? "ru-RU" : "ru");
  input->addPreset(tr("Portuguese"), useRegionalCodes ? "pt-PT" : "pt");
  input->addPreset(tr("Italian"), useRegionalCodes ? "it-IT" : "it");
}

void LibraryCreateDialog::populateScheduleCombo(ModernComboBox *combo) const {
  if (!combo) {
    return;
  }

  combo->clear();
  combo->addItem(tr("Never"), "");
  combo->addItem(tr("As a scheduled task"), "task");
  combo->addItem(tr("On library scan and scheduled task"), "scanandtask");
}

void LibraryCreateDialog::populatePlaceholderRefreshCombo(
    ModernComboBox *combo) const {
  if (!combo) {
    return;
  }

  combo->clear();
  combo->addItem(tr("Never"), 0);
  combo->addItem(tr("Every 2 days"), 2);
  combo->addItem(tr("Every 3 days"), 3);
  combo->addItem(tr("Every 7 days"), 7);
  combo->addItem(tr("Every 14 days"), 14);
  combo->addItem(tr("Every 30 days"), 30);
  combo->addItem(tr("Every 60 days"), 60);
  combo->addItem(tr("Every 90 days"), 90);
}

void LibraryCreateDialog::populateMultiVersionCombo(ModernComboBox *combo) const {
  if (!combo) {
    return;
  }

  combo->clear();
  combo->addItem(tr("Both files and metadata"), "both");
  combo->addItem(tr("Files only"), "files");
  combo->addItem(tr("Metadata only"), "metadata");
  combo->addItem(tr("None"), "none");
}

void LibraryCreateDialog::populateThumbnailIntervalCombo(
    ModernComboBox *combo) const {
  if (!combo) {
    return;
  }

  combo->clear();
  combo->addItem(tr("10 seconds"), 10);
  combo->addItem(tr("Chapter markers"), -1);
}

void LibraryCreateDialog::populateChapterIntervalCombo(
    ModernComboBox *combo) const {
  if (!combo) {
    return;
  }

  combo->clear();
  for (const int minutes : {3, 4, 5, 10, 15, 20}) {
    combo->addItem(tr("%1 minutes").arg(minutes), minutes);
  }
}

QString LibraryCreateDialog::selectedComboValue(
    const ModernComboBox *combo) const {
  if (!combo)
    return {};

  QString currentData = combo->currentData().toString().trimmed();
  if (combo == m_metaLangCombo || combo == m_imageLangCombo) {
    currentData = normalizedCultureCode(currentData);
  } else if (combo == m_countryCombo) {
    currentData = normalizedCountryCode(currentData);
  }
  if (!currentData.isEmpty())
    return currentData;

  const QString currentText = combo->currentText().trimmed();
  if (currentText.isEmpty() || currentText == tr("Server Default"))
    return {};

  for (int i = 0; i < combo->count(); ++i) {
    if (combo->itemText(i).trimmed() == currentText) {
      return combo->itemData(i).toString().trimmed();
    }
  }

  return {};
}

QString LibraryCreateDialog::selectedMetadataLanguage() const {
  return selectedComboValue(m_metaLangCombo);
}

QString LibraryCreateDialog::selectedMetadataCountryCode() const {
  return selectedComboValue(m_countryCombo);
}

QString LibraryCreateDialog::selectedImageLanguage() const {
  return selectedComboValue(m_imageLangCombo);
}

QStringList LibraryCreateDialog::selectedSubtitleLanguages() const {
  if (!m_subtitleLangInput)
    return {};

  const QString serializedValues = m_subtitleLangInput->value().trimmed();
  if (serializedValues.isEmpty())
    return {};

  QStringList values;
  const QStringList rawValues =
      serializedValues.split(',', Qt::SkipEmptyParts);
  for (const QString &rawValue : rawValues) {
    const QString value = normalizedCultureCode(rawValue.trimmed());
    if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) {
      values.append(value);
    }
  }

  return values;
}

QStringList LibraryCreateDialog::selectedLyricsLanguages() const {
  if (!m_lyricsLangInput) {
    return {};
  }

  const QString serializedValues = m_lyricsLangInput->value().trimmed();
  if (serializedValues.isEmpty()) {
    return {};
  }

  QStringList values;
  const QStringList rawValues =
      serializedValues.split(',', Qt::SkipEmptyParts);
  for (const QString &rawValue : rawValues) {
    const QString value = normalizedCultureCode(rawValue.trimmed());
    if (!value.isEmpty() && !values.contains(value, Qt::CaseInsensitive)) {
      values.append(value);
    }
  }

  return values;
}

QString LibraryCreateDialog::normalizedCultureCode(const QString &value) const {
  const QString trimmed = value.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  for (const LocalizationCulture &culture : m_availableCultures) {
    const QString canonicalCode = culture.canonicalCode();
    const QStringList allCodes = culture.allCodes();
    for (const QString &candidate : allCodes) {
      if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
        return canonicalCode.isEmpty() ? trimmed : canonicalCode;
      }
    }
  }

  return trimmed;
}

QString LibraryCreateDialog::normalizedCountryCode(const QString &value) const {
  const QString trimmed = value.trimmed();
  if (trimmed.isEmpty()) {
    return {};
  }

  for (const LocalizationCountry &country : m_availableCountries) {
    const QString canonicalCode = country.canonicalCode();
    const QStringList allCodes = country.allCodes();
    for (const QString &candidate : allCodes) {
      if (candidate.compare(trimmed, Qt::CaseInsensitive) == 0) {
        return canonicalCode.isEmpty() ? trimmed : canonicalCode;
      }
    }
  }

  return trimmed;
}

QStringList LibraryCreateDialog::normalizedCultureCodes(
    const QStringList &values) const {
  QStringList normalizedValues;
  for (const QString &value : values) {
    const QString normalized = normalizedCultureCode(value);
    if (!normalized.isEmpty() &&
        !normalizedValues.contains(normalized, Qt::CaseInsensitive)) {
      normalizedValues.append(normalized);
    }
  }
  return normalizedValues;
}

void LibraryCreateDialog::setPaths(const QStringList &paths) {
  while (!m_pathRows.isEmpty()) {
    auto *row = m_pathRows.takeLast();
    m_pathRowsLayout->removeWidget(row);
    delete row;
  }

  const QStringList deduplicatedPaths = deduplicateValues(paths);
  if (deduplicatedPaths.isEmpty()) {
    addPathRow();
    return;
  }

  for (const QString &path : deduplicatedPaths) {
    addPathRow(path);
  }
  addPathRow();

  if (m_pathRowsLayout) {
    m_pathRowsLayout->invalidate();
  }
  if (layout()) {
    layout()->activate();
  }
  updateGeometry();
  update();
}

void LibraryCreateDialog::setComboCurrentData(ModernComboBox *combo,
                                              const QVariant &value) {
  if (!combo || !value.isValid()) {
    return;
  }

  int index = combo->findData(value);
  if (index < 0 && value.canConvert<QString>()) {
    QString normalizedValue = value.toString().trimmed();
    if (combo == m_metaLangCombo || combo == m_imageLangCombo) {
      normalizedValue = normalizedCultureCode(normalizedValue);
    } else if (combo == m_countryCombo) {
      normalizedValue = normalizedCountryCode(normalizedValue);
    }

    if (!normalizedValue.isEmpty()) {
      index = combo->findData(normalizedValue);
      if (index < 0) {
        for (int i = 0; i < combo->count(); ++i) {
          if (combo->itemData(i).toString().compare(normalizedValue,
                                                    Qt::CaseInsensitive) ==
              0) {
            index = i;
            break;
          }
        }
      }
    }
  }

  if (index >= 0) {
    combo->setCurrentIndex(index);
  }
}

void LibraryCreateDialog::applyFetcherSelection(
    QVBoxLayout *container, QList<FetcherRowWidget *> &rows,
    const QStringList &enabledNames, const QStringList &disabledNames,
    const QStringList &orderedNames) {
  if (!container || rows.isEmpty()) {
    return;
  }

  const QStringList enabledSet = deduplicateValues(enabledNames);
  const QStringList disabledSet = deduplicateValues(disabledNames);
  const QStringList orderedSet = deduplicateValues(orderedNames);

  QList<FetcherRowWidget *> reorderedRows;
  reorderedRows.reserve(rows.size());

  for (const QString &name : orderedSet) {
    for (auto *row : rows) {
      if (row && row->name() == name && !reorderedRows.contains(row)) {
        reorderedRows.append(row);
        break;
      }
    }
  }

  for (auto *row : rows) {
    if (row && !reorderedRows.contains(row)) {
      reorderedRows.append(row);
    }
  }

  rows = reorderedRows;
  for (int i = container->count() - 1; i >= 0; --i) {
    if (auto *item = container->itemAt(i); item && item->widget()) {
      container->removeWidget(item->widget());
    }
  }
  for (auto *row : rows) {
    container->addWidget(row);
  }

  const bool hasExplicitEnabled = !enabledSet.isEmpty();
  for (auto *row : rows) {
    if (!row) {
      continue;
    }

    bool isEnabled = true;
    if (hasExplicitEnabled) {
      isEnabled = enabledSet.contains(row->name());
    }
    if (disabledSet.contains(row->name())) {
      isEnabled = false;
    }
    row->setFetcherEnabled(isEnabled);
  }

  updateArrowStates(rows);
}

void LibraryCreateDialog::applyLibraryOptions(
    const QJsonObject &libraryOptions) {
  if (libraryOptions.isEmpty()) {
    qDebug() << "[LibraryCreateDialog] Skip applying library options: empty";
    return;
  }

  qDebug() << "[LibraryCreateDialog] Applying library options"
           << QString::fromUtf8(
                  QJsonDocument(libraryOptions).toJson(QJsonDocument::Compact));

  const bool isEmby =
      m_core->serverManager()->activeProfile().type == ServerProfile::Emby;

  if (m_chkRealtimeMonitor &&
      libraryOptions.contains("EnableRealtimeMonitor")) {
    m_chkRealtimeMonitor->setChecked(
        libraryOptions.value("EnableRealtimeMonitor").toBool());
  }
  if (m_chkEmbeddedTitles &&
      libraryOptions.contains("EnableEmbeddedTitles")) {
    m_chkEmbeddedTitles->setChecked(
        libraryOptions.value("EnableEmbeddedTitles").toBool());
  }
  if (m_chkEnablePhotos && libraryOptions.contains("EnablePhotos")) {
    m_chkEnablePhotos->setChecked(
        libraryOptions.value("EnablePhotos").toBool());
  }
  if (m_chkImportPlaylists && libraryOptions.contains("ImportPlaylists")) {
    m_chkImportPlaylists->setChecked(
        libraryOptions.value("ImportPlaylists").toBool());
  }
  if (m_chkExcludeFromSearch &&
      libraryOptions.contains("ExcludeFromSearch")) {
    m_chkExcludeFromSearch->setChecked(
        libraryOptions.value("ExcludeFromSearch").toBool());
  }
  if (m_chkMergeFolders && libraryOptions.contains("MergeTopLevelFolders")) {
    m_chkMergeFolders->setChecked(
        libraryOptions.value("MergeTopLevelFolders").toBool());
  }
  if (m_spinIgnoreSamplesMb && libraryOptions.contains("SampleIgnoreSize")) {
    m_spinIgnoreSamplesMb->setValue(
        bytesToMegabytes(libraryOptions.value("SampleIgnoreSize"), 300));
  }
  if (m_chkEnablePlexIgnore &&
      libraryOptions.contains("EnablePlexIgnore")) {
    m_chkEnablePlexIgnore->setChecked(
        libraryOptions.value("EnablePlexIgnore").toBool());
  }
  if (m_chkAutomaticallyGroupSeries &&
      libraryOptions.contains("EnableAutomaticSeriesGrouping")) {
    m_chkAutomaticallyGroupSeries->setChecked(
        libraryOptions.value("EnableAutomaticSeriesGrouping").toBool());
  }
  if (m_chkEnableMultiPart &&
      libraryOptions.contains("EnableMultiPartItems")) {
    m_chkEnableMultiPart->setChecked(
        libraryOptions.value("EnableMultiPartItems").toBool());
  }
  if (m_multiVersionCombo &&
      (libraryOptions.contains("EnableMultiVersionByFiles") ||
       libraryOptions.contains("EnableMultiVersionByMetadata"))) {
    setComboCurrentData(
        m_multiVersionCombo,
        multiVersionMode(
            libraryOptions.value("EnableMultiVersionByFiles").toBool(),
            libraryOptions.value("EnableMultiVersionByMetadata").toBool()));
  }

  const QStringList metadataSavers =
      firstStringListForKeys(libraryOptions, {"MetadataSavers"});
  if (m_chkSaveNfo) {
    if (isEmby) {
      
      if (!metadataSavers.isEmpty()) {
        m_chkSaveNfo->setChecked(
            metadataSavers.contains(QStringLiteral("Nfo")));
      } else if (libraryOptions.contains("MetadataSavers")) {
        m_chkSaveNfo->setChecked(false);
      }
    } else {
      
      if (libraryOptions.contains("SaveLocalMetadata")) {
        m_chkSaveNfo->setChecked(
            libraryOptions.value("SaveLocalMetadata").toBool());
      } else if (!metadataSavers.isEmpty()) {
        m_chkSaveNfo->setChecked(
            metadataSavers.contains(QStringLiteral("Nfo")));
      }
    }
  }

  const QStringList localMetadataReaders =
      firstStringListForKeys(libraryOptions, {"LocalMetadataReaderOrder"});
  const QStringList disabledLocalMetadataReaders =
      firstStringListForKeys(libraryOptions, {"DisabledLocalMetadataReaders"});
  if (m_chkNfoReader) {
    if (!localMetadataReaders.isEmpty()) {
      m_chkNfoReader->setChecked(
          localMetadataReaders.contains(QStringLiteral("Nfo")));
    } else if (!disabledLocalMetadataReaders.isEmpty()) {
      m_chkNfoReader->setChecked(
          !disabledLocalMetadataReaders.contains(QStringLiteral("Nfo")));
    }
  }

  if (m_chkAllowAdultMetadata &&
      libraryOptions.contains("EnableAdultMetadata")) {
    m_chkAllowAdultMetadata->setChecked(
        libraryOptions.value("EnableAdultMetadata").toBool());
  }
  if (m_chkImportCollectionInfo &&
      libraryOptions.contains("ImportCollections")) {
    m_chkImportCollectionInfo->setChecked(
        libraryOptions.value("ImportCollections").toBool());
  }
  if (m_chkAutoCollection &&
      libraryOptions.contains("AutomaticallyAddToCollection")) {
    m_chkAutoCollection->setChecked(
        libraryOptions.value("AutomaticallyAddToCollection").toBool());
  }
  if (m_minCollSizeCombo && libraryOptions.contains("MinCollectionItems")) {
    setComboCurrentData(m_minCollSizeCombo,
                        libraryOptions.value("MinCollectionItems").toInt());
  }
  if (m_metaRefreshCombo &&
      libraryOptions.contains("AutomaticRefreshIntervalDays")) {
    setComboCurrentData(
        m_metaRefreshCombo,
        libraryOptions.value("AutomaticRefreshIntervalDays").toInt());
  }
  if (m_placeholderRefreshCombo &&
      libraryOptions.contains("PlaceholderMetadataRefreshIntervalDays")) {
    setComboCurrentData(
        m_placeholderRefreshCombo,
        libraryOptions.value("PlaceholderMetadataRefreshIntervalDays").toInt());
  }
  if (m_metaLangCombo &&
      libraryOptions.contains("PreferredMetadataLanguage")) {
    setComboCurrentData(m_metaLangCombo,
                        libraryOptions.value("PreferredMetadataLanguage")
                            .toString());
  }
  if (m_countryCombo && libraryOptions.contains("MetadataCountryCode")) {
    setComboCurrentData(m_countryCombo,
                        libraryOptions.value("MetadataCountryCode").toString());
  }
  if (m_imageLangCombo &&
      libraryOptions.contains("PreferredImageLanguage")) {
    setComboCurrentData(m_imageLangCombo,
                        libraryOptions.value("PreferredImageLanguage")
                            .toString());
  }
  if (m_musicFolderStructureCombo &&
      libraryOptions.contains("MusicFolderStructure")) {
    setComboCurrentData(m_musicFolderStructureCombo,
                        libraryOptions.value("MusicFolderStructure").toString());
  }

  if (m_chkSaveArtwork) {
    if (isEmby && libraryOptions.contains("SaveLocalMetadata")) {
      m_chkSaveArtwork->setChecked(
          libraryOptions.value("SaveLocalMetadata").toBool());
    } else if (!isEmby &&
               libraryOptions.contains("SaveLocalThumbnailSets")) {
      m_chkSaveArtwork->setChecked(
          libraryOptions.value("SaveLocalThumbnailSets").toBool());
    }
  }
  if (m_chkCacheImages && libraryOptions.contains("CacheImages")) {
    m_chkCacheImages->setChecked(
        libraryOptions.value("CacheImages").toBool());
  }
  if (m_chkSaveMetadataHidden &&
      libraryOptions.contains("SaveMetadataHidden")) {
    m_chkSaveMetadataHidden->setChecked(
        libraryOptions.value("SaveMetadataHidden").toBool());
  }
  if (m_chkDownloadImagesInAdvance &&
      libraryOptions.contains("DownloadImagesInAdvance")) {
    m_chkDownloadImagesInAdvance->setChecked(
        libraryOptions.value("DownloadImagesInAdvance").toBool());
  }
  if (m_thumbnailScheduleCombo &&
      (libraryOptions.contains("EnableChapterImageExtraction") ||
       libraryOptions.contains("ExtractChapterImagesDuringLibraryScan"))) {
    setComboCurrentData(
        m_thumbnailScheduleCombo,
        scheduleMode(
            libraryOptions.value("EnableChapterImageExtraction").toBool(),
            libraryOptions.value("ExtractChapterImagesDuringLibraryScan")
                .toBool()));
  } else if (m_chkExtractChapterImages) {
    if (libraryOptions.contains("EnableChapterImageExtraction")) {
      m_chkExtractChapterImages->setChecked(
          libraryOptions.value("EnableChapterImageExtraction").toBool());
    } else if (libraryOptions.contains("ExtractChapterImagesDuringLibraryScan")) {
      m_chkExtractChapterImages->setChecked(
          libraryOptions.value("ExtractChapterImagesDuringLibraryScan")
              .toBool());
    }
  }
  if (m_thumbnailIntervalCombo &&
      libraryOptions.contains("ThumbnailImagesIntervalSeconds")) {
    setComboCurrentData(
        m_thumbnailIntervalCombo,
        libraryOptions.value("ThumbnailImagesIntervalSeconds").toInt());
  }
  if (m_chkSaveThumbnailSets &&
      libraryOptions.contains("SaveLocalThumbnailSets")) {
    m_chkSaveThumbnailSets->setChecked(
        libraryOptions.value("SaveLocalThumbnailSets").toBool());
  }
  if (m_chkGenerateChapters &&
      libraryOptions.contains("AutoGenerateChapters")) {
    m_chkGenerateChapters->setChecked(
        libraryOptions.value("AutoGenerateChapters").toBool());
  }
  if (m_chapterIntervalCombo &&
      libraryOptions.contains("AutoGenerateChapterIntervalMinutes")) {
    setComboCurrentData(
        m_chapterIntervalCombo,
        libraryOptions.value("AutoGenerateChapterIntervalMinutes").toInt());
  }
  if (m_introDetectionCombo &&
      (libraryOptions.contains("EnableMarkerDetection") ||
       libraryOptions.contains("EnableMarkerDetectionDuringLibraryScan"))) {
    setComboCurrentData(
        m_introDetectionCombo,
        scheduleMode(
            libraryOptions.value("EnableMarkerDetection").toBool(),
            libraryOptions.value("EnableMarkerDetectionDuringLibraryScan")
                .toBool()));
  }

  if (m_chkSaveSubtitles &&
      libraryOptions.contains("SaveSubtitlesWithMedia")) {
    m_chkSaveSubtitles->setChecked(
        libraryOptions.value("SaveSubtitlesWithMedia").toBool());
  }
  if (m_chkSkipIfAudioMatchesSub &&
      libraryOptions.contains("SkipSubtitlesIfAudioTrackMatches")) {
    m_chkSkipIfAudioMatchesSub->setChecked(
        libraryOptions.value("SkipSubtitlesIfAudioTrackMatches").toBool());
  }
  if (m_chkSkipIfEmbeddedSub &&
      libraryOptions.contains("SkipSubtitlesIfEmbeddedSubtitlesPresent")) {
    m_chkSkipIfEmbeddedSub->setChecked(
        libraryOptions.value("SkipSubtitlesIfEmbeddedSubtitlesPresent")
            .toBool());
  }
  if (m_subAgeLimitCombo &&
      libraryOptions.contains("SubtitleDownloadMaxAgeDays")) {
    setComboCurrentData(
        m_subAgeLimitCombo,
        libraryOptions.value("SubtitleDownloadMaxAgeDays").toInt());
  }
  if (m_chkRequirePerfectSubtitleMatch &&
      libraryOptions.contains("RequirePerfectSubtitleMatch")) {
    m_chkRequirePerfectSubtitleMatch->setChecked(
        libraryOptions.value("RequirePerfectSubtitleMatch").toBool());
  }
  if (m_chkForcedSubtitlesOnly &&
      libraryOptions.contains("ForcedSubtitlesOnly")) {
    m_chkForcedSubtitlesOnly->setChecked(
        libraryOptions.value("ForcedSubtitlesOnly").toBool());
  }
  if (m_subtitleLangInput &&
      libraryOptions.contains("SubtitleDownloadLanguages")) {
    m_subtitleLangInput->setValue(
        normalizedCultureCodes(
            jsonArrayToStringList(
                libraryOptions.value("SubtitleDownloadLanguages")))
            .join(','));
  }
  if (m_lyricsAgeLimitCombo &&
      libraryOptions.contains("LyricsDownloadMaxAgeDays")) {
    setComboCurrentData(
        m_lyricsAgeLimitCombo,
        libraryOptions.value("LyricsDownloadMaxAgeDays").toInt());
  }
  if (m_lyricsLangInput &&
      libraryOptions.contains("LyricsDownloadLanguages")) {
    m_lyricsLangInput->setValue(
        normalizedCultureCodes(
            jsonArrayToStringList(
                libraryOptions.value("LyricsDownloadLanguages")))
            .join(','));
  }
  if (m_chkSaveLyrics &&
      libraryOptions.contains("SaveLyricsWithMedia")) {
    m_chkSaveLyrics->setChecked(
        libraryOptions.value("SaveLyricsWithMedia").toBool());
  }
  applyFetcherSelection(
      m_lyricsFetcherLayout, m_lyricsFetcherRows,
      firstStringListForKeys(libraryOptions,
                             {"LyricsFetcherOrder", "LyricsFetchers",
                              "LyricFetcherOrder"}),
      firstStringListForKeys(libraryOptions, {"DisabledLyricsFetchers"}),
      firstStringListForKeys(libraryOptions,
                             {"LyricsFetcherOrder", "LyricsFetchers",
                              "LyricFetcherOrder"}));

  if (m_spinMinResumePercent && libraryOptions.contains("MinResumePct")) {
    m_spinMinResumePercent->setValue(
        libraryOptions.value("MinResumePct").toInt());
  }
  if (m_spinMaxResumePercent && libraryOptions.contains("MaxResumePct")) {
    m_spinMaxResumePercent->setValue(
        libraryOptions.value("MaxResumePct").toInt());
  }
  if (m_spinMinResumeDuration &&
      libraryOptions.contains("MinResumeDurationSeconds")) {
    m_spinMinResumeDuration->setValue(
        libraryOptions.value("MinResumeDurationSeconds").toInt());
  }

  const auto serverType = m_core->serverManager()->activeProfile().type;
  const auto profile =
      LTU::buildLibraryTypeProfile(serverType, normalizedCollectionType());
  const QJsonArray typeOptions = libraryOptions.value("TypeOptions").toArray();

  QStringList metadataEnabled;
  QStringList metadataDisabled;
  QStringList metadataOrder;
  if (serverType == ServerProfile::Emby) {
    metadataEnabled = firstStringListForKeys(
        libraryOptions,
        {"MetadataDownloaders", "MetadataFetchers", "MetadataFetcherOrder"});
    metadataOrder = firstStringListForKeys(
        libraryOptions,
        {"MetadataFetcherOrder", "MetadataDownloaders", "MetadataFetchers"});
    metadataDisabled =
        firstStringListForKeys(libraryOptions, {"DisabledMetadataFetchers"});
  }
  if (metadataEnabled.isEmpty() && metadataOrder.isEmpty() &&
      metadataDisabled.isEmpty()) {
    const QJsonObject metadataTypeOption = firstMatchingTypeOption(
        typeOptions, topMetadataTypesForCollection(profile.collectionType),
        {"MetadataFetcherOrder", "MetadataFetchers",
         "DisabledMetadataFetchers"});
    metadataEnabled = firstStringListForKeys(
        metadataTypeOption, {"MetadataFetchers", "MetadataFetcherOrder"});
    metadataOrder = firstStringListForKeys(
        metadataTypeOption, {"MetadataFetcherOrder", "MetadataFetchers"});
    metadataDisabled = firstStringListForKeys(
        metadataTypeOption, {"DisabledMetadataFetchers"});
  }
  applyFetcherSelection(m_metaFetcherLayout, m_metadataFetcherRows,
                        metadataEnabled, metadataDisabled, metadataOrder);

  const QJsonObject seasonMetadataTypeOption = firstMatchingTypeOption(
      typeOptions, {QStringLiteral("Season")},
      {"MetadataFetcherOrder", "MetadataFetchers",
       "DisabledMetadataFetchers"});
  applyFetcherSelection(
      m_seasonMetaFetcherLayout, m_seasonMetadataFetcherRows,
      firstStringListForKeys(seasonMetadataTypeOption,
                             {"MetadataFetchers", "MetadataFetcherOrder"}),
      firstStringListForKeys(seasonMetadataTypeOption,
                             {"DisabledMetadataFetchers"}),
      firstStringListForKeys(seasonMetadataTypeOption,
                             {"MetadataFetcherOrder", "MetadataFetchers"}));

  const QJsonObject episodeMetadataTypeOption = firstMatchingTypeOption(
      typeOptions, {QStringLiteral("Episode")},
      {"MetadataFetcherOrder", "MetadataFetchers",
       "DisabledMetadataFetchers"});
  applyFetcherSelection(
      m_episodeMetaFetcherLayout, m_episodeMetadataFetcherRows,
      firstStringListForKeys(episodeMetadataTypeOption,
                             {"MetadataFetchers", "MetadataFetcherOrder"}),
      firstStringListForKeys(episodeMetadataTypeOption,
                             {"DisabledMetadataFetchers"}),
      firstStringListForKeys(episodeMetadataTypeOption,
                             {"MetadataFetcherOrder", "MetadataFetchers"}));

  QStringList imageEnabled;
  QStringList imageDisabled;
  QStringList imageOrder;
  if (serverType == ServerProfile::Emby) {
    imageEnabled = firstStringListForKeys(
        libraryOptions, {"ImageFetchers", "ImageFetcherOrder"});
    imageOrder = firstStringListForKeys(
        libraryOptions, {"ImageFetcherOrder", "ImageFetchers"});
    imageDisabled =
        firstStringListForKeys(libraryOptions, {"DisabledImageFetchers"});
  }
  if (imageEnabled.isEmpty() && imageOrder.isEmpty() && imageDisabled.isEmpty()) {
    const QJsonObject imageTypeOption = firstMatchingTypeOption(
        typeOptions, topImageTypesForCollection(profile.collectionType),
        {"ImageFetcherOrder", "ImageFetchers", "DisabledImageFetchers"});
    imageEnabled = firstStringListForKeys(
        imageTypeOption, {"ImageFetchers", "ImageFetcherOrder"});
    imageOrder = firstStringListForKeys(
        imageTypeOption, {"ImageFetcherOrder", "ImageFetchers"});
    imageDisabled =
        firstStringListForKeys(imageTypeOption, {"DisabledImageFetchers"});
  }
  applyFetcherSelection(m_imgFetcherLayout, m_imageFetcherRows, imageEnabled,
                        imageDisabled, imageOrder);

  const QJsonObject seasonImageTypeOption = firstMatchingTypeOption(
      typeOptions, {QStringLiteral("Season")},
      {"ImageFetcherOrder", "ImageFetchers", "DisabledImageFetchers"});
  applyFetcherSelection(
      m_seasonImgFetcherLayout, m_seasonImageFetcherRows,
      firstStringListForKeys(seasonImageTypeOption,
                             {"ImageFetchers", "ImageFetcherOrder"}),
      firstStringListForKeys(seasonImageTypeOption,
                             {"DisabledImageFetchers"}),
      firstStringListForKeys(seasonImageTypeOption,
                             {"ImageFetcherOrder", "ImageFetchers"}));

  const QJsonObject episodeImageTypeOption = firstMatchingTypeOption(
      typeOptions, {QStringLiteral("Episode")},
      {"ImageFetcherOrder", "ImageFetchers", "DisabledImageFetchers"});
  applyFetcherSelection(
      m_episodeImgFetcherLayout, m_episodeImageFetcherRows,
      firstStringListForKeys(episodeImageTypeOption,
                             {"ImageFetchers", "ImageFetcherOrder"}),
      firstStringListForKeys(episodeImageTypeOption,
                             {"DisabledImageFetchers"}),
      firstStringListForKeys(episodeImageTypeOption,
                             {"ImageFetcherOrder", "ImageFetchers"}));

  applyFetcherSelection(
      m_subDownloaderLayout, m_subtitleDownloaderRows,
      firstStringListForKeys(libraryOptions,
                             {"SubtitleDownloaders", "SubtitleFetchers",
                              "SubtitleFetcherOrder"}),
      firstStringListForKeys(libraryOptions, {"DisabledSubtitleFetchers"}),
      firstStringListForKeys(libraryOptions,
                             {"SubtitleFetcherOrder", "SubtitleDownloaders",
                              "SubtitleFetchers"}));
}

QJsonObject LibraryCreateDialog::mergedLibraryOptions(
    const QJsonObject &currentOptions) const {
  if (m_baseLibraryOptions.isEmpty()) {
    return currentOptions;
  }

  const auto serverType = m_core->serverManager()->activeProfile().type;
  const QJsonArray mergedTypeOptions = mergeTypeOptions(
      m_baseLibraryOptions.value(QStringLiteral("TypeOptions")).toArray(),
      currentOptions.value(QStringLiteral("TypeOptions")).toArray());

  
  
  
  if (serverType == ServerProfile::Jellyfin) {
    QJsonObject mergedOptions = currentOptions;
    if (!mergedTypeOptions.isEmpty()) {
      mergedOptions.insert(QStringLiteral("TypeOptions"), mergedTypeOptions);
    } else {
      mergedOptions.remove(QStringLiteral("TypeOptions"));
    }
    return mergedOptions;
  }

  QJsonObject mergedOptions = m_baseLibraryOptions;
  clearManagedLibraryOptionKeys(mergedOptions);

  for (auto it = currentOptions.begin(); it != currentOptions.end(); ++it) {
    if (it.key() == QStringLiteral("TypeOptions")) {
      continue;
    }
    mergedOptions.insert(it.key(), it.value());
  }

  if (!mergedTypeOptions.isEmpty()) {
    mergedOptions.insert(QStringLiteral("TypeOptions"), mergedTypeOptions);
  } else {
    mergedOptions.remove(QStringLiteral("TypeOptions"));
  }

  return mergedOptions;
}

void LibraryCreateDialog::clearManagedLibraryOptionKeys(
    QJsonObject &options) const {
  static const QStringList managedKeys = {
      QStringLiteral("ContentType"),
      QStringLiteral("PathInfos"),
      QStringLiteral("EnableRealtimeMonitor"),
      QStringLiteral("EnableEmbeddedTitles"),
      QStringLiteral("EnablePhotos"),
      QStringLiteral("ImportPlaylists"),
      QStringLiteral("ExcludeFromSearch"),
      QStringLiteral("MergeTopLevelFolders"),
      QStringLiteral("SampleIgnoreSize"),
      QStringLiteral("EnablePlexIgnore"),
      QStringLiteral("EnableAutomaticSeriesGrouping"),
      QStringLiteral("EnableMultiPartItems"),
      QStringLiteral("EnableMultiVersionByFiles"),
      QStringLiteral("EnableMultiVersionByMetadata"),
      QStringLiteral("MusicFolderStructure"),
      QStringLiteral("SaveLocalMetadata"),
      QStringLiteral("MetadataSavers"),
      QStringLiteral("LocalMetadataReaderOrder"),
      QStringLiteral("DisabledLocalMetadataReaders"),
      QStringLiteral("EnableAdultMetadata"),
      QStringLiteral("ImportCollections"),
      QStringLiteral("AutomaticallyAddToCollection"),
      QStringLiteral("MinCollectionItems"),
      QStringLiteral("AutomaticRefreshIntervalDays"),
      QStringLiteral("PlaceholderMetadataRefreshIntervalDays"),
      QStringLiteral("PreferredMetadataLanguage"),
      QStringLiteral("MetadataCountryCode"),
      QStringLiteral("PreferredImageLanguage"),
      QStringLiteral("SaveLocalThumbnailSets"),
      QStringLiteral("CacheImages"),
      QStringLiteral("SaveMetadataHidden"),
      QStringLiteral("DownloadImagesInAdvance"),
      QStringLiteral("EnableChapterImageExtraction"),
      QStringLiteral("ExtractChapterImagesDuringLibraryScan"),
      QStringLiteral("ThumbnailImagesIntervalSeconds"),
      QStringLiteral("AutoGenerateChapters"),
      QStringLiteral("AutoGenerateChapterIntervalMinutes"),
      QStringLiteral("EnableMarkerDetection"),
      QStringLiteral("EnableMarkerDetectionDuringLibraryScan"),
      QStringLiteral("SaveSubtitlesWithMedia"),
      QStringLiteral("SkipSubtitlesIfAudioTrackMatches"),
      QStringLiteral("SkipSubtitlesIfEmbeddedSubtitlesPresent"),
      QStringLiteral("SubtitleDownloadMaxAgeDays"),
      QStringLiteral("RequirePerfectSubtitleMatch"),
      QStringLiteral("ForcedSubtitlesOnly"),
      QStringLiteral("SubtitleDownloadLanguages"),
      QStringLiteral("SubtitleDownloaders"),
      QStringLiteral("SubtitleFetchers"),
      QStringLiteral("SubtitleFetcherOrder"),
      QStringLiteral("DisabledSubtitleFetchers"),
      QStringLiteral("SaveLyricsWithMedia"),
      QStringLiteral("LyricsDownloadLanguages"),
      QStringLiteral("LyricsDownloadMaxAgeDays"),
      QStringLiteral("MinResumePct"),
      QStringLiteral("MaxResumePct"),
      QStringLiteral("MinResumeDurationSeconds"),
      QStringLiteral("MetadataDownloaders"),
      QStringLiteral("MetadataFetchers"),
      QStringLiteral("MetadataFetcherOrder"),
      QStringLiteral("DisabledMetadataFetchers"),
      QStringLiteral("ImageFetchers"),
      QStringLiteral("ImageFetcherOrder"),
      QStringLiteral("DisabledImageFetchers"),
      
      QStringLiteral("EnableArchiveMediaFiles"),
      QStringLiteral("EnablePhotos"),
      QStringLiteral("EnableInternetProviders"),
      QStringLiteral("ImportPlaylists"),
      QStringLiteral("EnableAutomaticSeriesGrouping"),
      QStringLiteral("EnableMarkerDetection"),
      QStringLiteral("EnableMarkerDetectionDuringLibraryScan"),
      QStringLiteral("IntroDetectionFingerprintLength"),
      QStringLiteral("EnableAudioResume"),
      QStringLiteral("ImportMissingEpisodes"),
      QStringLiteral("PlaceholderMetadataRefreshIntervalDays"),
      QStringLiteral("SeasonZeroDisplayName"),
      QStringLiteral("RequirePerfectSubtitleMatch"),
      QStringLiteral("ForcedSubtitlesOnly"),
      QStringLiteral("SaveMetadataHidden"),
      QStringLiteral("IgnoreHiddenFiles"),
      QStringLiteral("IgnoreFileExtensions"),
      QStringLiteral("CollapseSingleItemFolders"),
      QStringLiteral("SaveLyricsWithMedia"),
      QStringLiteral("LyricsDownloadMaxAgeDays"),
      QStringLiteral("LyricsFetcherOrder"),
      QStringLiteral("LyricsDownloadLanguages"),
      QStringLiteral("DisabledLyricsFetchers"),
      QStringLiteral("ThumbnailImagesIntervalSeconds"),
      QStringLiteral("SampleIgnoreSize"),
      
      QStringLiteral("Enabled"),
      QStringLiteral("EnableLUFSScan"),
      QStringLiteral("EnableEmbeddedExtrasTitles"),
      QStringLiteral("EnableEmbeddedEpisodeInfos"),
      QStringLiteral("AllowEmbeddedSubtitles"),
      QStringLiteral("PreferNonstandardArtistsTag"),
      QStringLiteral("UseCustomTagDelimiters"),
      QStringLiteral("CustomTagDelimiters"),
      QStringLiteral("DelimiterWhitelist"),
      QStringLiteral("DisabledLyricFetchers"),
      QStringLiteral("LyricFetcherOrder"),
      QStringLiteral("EnableArchiveMediaFiles"),
      QStringLiteral("ExtractTrickplayImagesDuringLibraryScan"),
      QStringLiteral("SaveTrickplayWithMedia"),
      QStringLiteral("EnableTrickplayImageExtraction"),
      QStringLiteral("SaveLyricsWithMedia"),
      QStringLiteral("DisabledMediaSegmentProviders"),
      QStringLiteral("MediaSegmentProvideOrder")};

  for (const QString &key : managedKeys) {
    options.remove(key);
  }
}




void LibraryCreateDialog::setupBasicSection(QVBoxLayout *layout) {
  
  auto *nameRow = new QHBoxLayout();
  nameRow->setContentsMargins(0, 0, 0, 0);
  nameRow->setSpacing(8);

  m_nameEdit = new QLineEdit(this);
  m_nameEdit->setObjectName("ManageLibInput");
  m_nameEdit->setPlaceholderText(tr("Enter library name..."));
  nameRow->addWidget(m_nameEdit, 1);

  m_typeCombo = LayoutUtils::createStyledCombo(this);
  m_typeCombo->addItem(tr("🎬 Movies"), "movies");
  m_typeCombo->addItem(tr("📺 TV Shows"), "tvshows");
  m_typeCombo->addItem(tr("🎵 Music"), "music");
  m_typeCombo->addItem(tr("🎞️ Home Videos"), "homevideos");
  m_typeCombo->addItem(tr("📚 Books"), "books");
  m_typeCombo->addItem(tr("🎤 Music Videos"), "musicvideos");
  m_typeCombo->addItem(tr("📷 Photos"), "photos");
  m_typeCombo->addItem(tr("📦 Mixed"), "");
  m_typeCombo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  nameRow->addWidget(m_typeCombo);

  layout->addLayout(nameRow);

  layout->addSpacing(8);

  layout->addWidget(LayoutUtils::createSectionLabel(tr("Media Paths:"), this));
  m_pathRowsLayout = new QVBoxLayout();
  m_pathRowsLayout->setContentsMargins(0, 0, 0, 0);
  m_pathRowsLayout->setSpacing(4);
  layout->addLayout(m_pathRowsLayout);

  
  addPathRow();
}




void LibraryCreateDialog::setupLibrarySettingsSection(QVBoxLayout *layout) {
  m_librarySettingsSection = new CollapsibleSection(tr("Library Settings"), this);
  auto *section = m_librarySettingsSection;
  auto *sec = section->contentLayout();
  const bool isEmby =
      m_core->serverManager()->activeProfile().type == ServerProfile::Emby;
  const bool useRegionalCodes = isEmby;

  
  auto *monitorRow = new QHBoxLayout();
  monitorRow->setContentsMargins(0, 0, 6, 0);
  auto *monitorLabel =
      LayoutUtils::createSectionLabel(tr("Enable real-time monitoring"), this);
  monitorLabel->setToolTip(
      tr("Automatically detect file changes in library folders"));
  m_chkRealtimeMonitor = new ModernSwitch(this);
  m_chkRealtimeMonitor->setObjectName("ManageLibSwitch");
  m_chkRealtimeMonitor->setChecked(true);
  m_chkRealtimeMonitor->setToolTip(
      tr("Automatically detect file changes in library folders"));
  monitorRow->addWidget(monitorLabel);
  monitorRow->addStretch(1);
  monitorRow->addWidget(m_chkRealtimeMonitor);
  sec->addLayout(monitorRow);

  
  m_embeddedTitlesRow = new QWidget(this);
  auto *embedRow = new QHBoxLayout(m_embeddedTitlesRow);
  embedRow->setContentsMargins(0, 0, 6, 0);
  auto *embedLabel = LayoutUtils::createSectionLabel(
      tr("Prefer embedded titles over filenames"), this);
  embedLabel->setToolTip(
      tr("Use titles stored in media file metadata instead of filenames"));
  m_chkEmbeddedTitles = new ModernSwitch(this);
  m_chkEmbeddedTitles->setObjectName("ManageLibSwitch");
  m_chkEmbeddedTitles->setChecked(false);
  m_chkEmbeddedTitles->setToolTip(
      tr("Use titles stored in media file metadata instead of filenames"));
  embedRow->addWidget(embedLabel);
  embedRow->addStretch(1);
  embedRow->addWidget(m_chkEmbeddedTitles);
  sec->addWidget(m_embeddedTitlesRow);

  if (isEmby) {
    m_enablePhotosRow = new QWidget(this);
    auto *enablePhotosRow = new QHBoxLayout(m_enablePhotosRow);
    enablePhotosRow->setContentsMargins(0, 0, 6, 0);
    auto *enablePhotosLabel = LayoutUtils::createSectionLabel(
        tr("Enable photos"), this);
    enablePhotosLabel->setToolTip(
        tr("Allow photo items to be included in this library"));
    m_chkEnablePhotos = new ModernSwitch(this);
    m_chkEnablePhotos->setObjectName("ManageLibSwitch");
    m_chkEnablePhotos->setChecked(true);
    m_chkEnablePhotos->setToolTip(
        tr("Allow photo items to be included in this library"));
    enablePhotosRow->addWidget(enablePhotosLabel);
    enablePhotosRow->addStretch(1);
    enablePhotosRow->addWidget(m_chkEnablePhotos);
    sec->addWidget(m_enablePhotosRow);

    m_importPlaylistsRow = new QWidget(this);
    auto *importPlaylistsRow = new QHBoxLayout(m_importPlaylistsRow);
    importPlaylistsRow->setContentsMargins(0, 0, 6, 0);
    auto *importPlaylistsLabel = LayoutUtils::createSectionLabel(
        tr("Import playlists"), this);
    importPlaylistsLabel->setToolTip(
        tr("Import playlist files that are found in the selected folders"));
    m_chkImportPlaylists = new ModernSwitch(this);
    m_chkImportPlaylists->setObjectName("ManageLibSwitch");
    m_chkImportPlaylists->setChecked(true);
    m_chkImportPlaylists->setToolTip(
        tr("Import playlist files that are found in the selected folders"));
    importPlaylistsRow->addWidget(importPlaylistsLabel);
    importPlaylistsRow->addStretch(1);
    importPlaylistsRow->addWidget(m_chkImportPlaylists);
    sec->addWidget(m_importPlaylistsRow);

    m_excludeFromSearchRow = new QWidget(this);
    auto *excludeRow = new QHBoxLayout(m_excludeFromSearchRow);
    excludeRow->setContentsMargins(0, 0, 6, 0);
    auto *excludeLabel = LayoutUtils::createSectionLabel(
        tr("Exclude from global search"), this);
    excludeLabel->setToolTip(
        tr("Hide items in this library from global search results"));
    m_chkExcludeFromSearch = new ModernSwitch(this);
    m_chkExcludeFromSearch->setObjectName("ManageLibSwitch");
    m_chkExcludeFromSearch->setChecked(false);
    m_chkExcludeFromSearch->setToolTip(
        tr("Hide items in this library from global search results"));
    excludeRow->addWidget(excludeLabel);
    excludeRow->addStretch(1);
    excludeRow->addWidget(m_chkExcludeFromSearch);
    sec->addWidget(m_excludeFromSearchRow);

    m_mergeFoldersRow = new QWidget(this);
    auto *mergeFoldersRow = new QHBoxLayout(m_mergeFoldersRow);
    mergeFoldersRow->setContentsMargins(0, 0, 6, 0);
    auto *mergeFoldersLabel = LayoutUtils::createSectionLabel(
        tr("Merge top folders in folder view"), this);
    mergeFoldersLabel->setToolTip(
        tr("Flatten the top-level folders when browsing in folder view"));
    m_chkMergeFolders = new ModernSwitch(this);
    m_chkMergeFolders->setObjectName("ManageLibSwitch");
    m_chkMergeFolders->setChecked(false);
    m_chkMergeFolders->setToolTip(
        tr("Flatten the top-level folders when browsing in folder view"));
    mergeFoldersRow->addWidget(mergeFoldersLabel);
    mergeFoldersRow->addStretch(1);
    mergeFoldersRow->addWidget(m_chkMergeFolders);
    sec->addWidget(m_mergeFoldersRow);
  }

  m_libraryTopSeparator = LayoutUtils::createSeparator(this);
  sec->addWidget(m_libraryTopSeparator);

  
  m_metaLangRow = new QWidget(this);
  auto *langRow = new QHBoxLayout(m_metaLangRow);
  langRow->setContentsMargins(0, 0, 0, 0);
  langRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Preferred metadata language:"), this));
  m_metaLangCombo = LayoutUtils::createStyledCombo(this);
  populatePreferredLanguageCombo(m_metaLangCombo, useRegionalCodes);
  langRow->addStretch(1);
  langRow->addWidget(m_metaLangCombo);
  sec->addWidget(m_metaLangRow);

  
  m_countryRow = new QWidget(this);
  auto *countryRow = new QHBoxLayout(m_countryRow);
  countryRow->setContentsMargins(0, 0, 0, 0);
  countryRow->addWidget(
      LayoutUtils::createSectionLabel(tr("Preferred country/region:"), this));
  m_countryCombo = LayoutUtils::createStyledCombo(this);
  populateCountryCombo(m_countryCombo);
  countryRow->addStretch(1);
  countryRow->addWidget(m_countryCombo);
  sec->addWidget(m_countryRow);

  
  if (isEmby) {
    m_imageLangRow = new QWidget(this);
    auto *imgLangRow = new QHBoxLayout(m_imageLangRow);
    imgLangRow->setContentsMargins(0, 0, 0, 0);
    imgLangRow->addWidget(
        LayoutUtils::createSectionLabel(tr("Preferred image language:"), this));
    m_imageLangCombo = LayoutUtils::createStyledCombo(this);
    populatePreferredLanguageCombo(m_imageLangCombo, true);
    imgLangRow->addStretch(1);
    imgLangRow->addWidget(m_imageLangCombo);
    sec->addWidget(m_imageLangRow);
  }

  if (isEmby) {
    m_musicFolderStructureRow = new QWidget(this);
    auto *musicStructureRow = new QHBoxLayout(m_musicFolderStructureRow);
    musicStructureRow->setContentsMargins(0, 0, 0, 0);
    musicStructureRow->addWidget(
        LayoutUtils::createSectionLabel(tr("Music folder structure:"), this));
    m_musicFolderStructureCombo = LayoutUtils::createStyledCombo(this);
    m_musicFolderStructureCombo->addItem(tr("Other or unstructured"), "");
    m_musicFolderStructureCombo->addItem(
        tr("Perfect artist/album/track"), "artist_album_track");
    m_musicFolderStructureCombo->addItem(
        tr("Perfect album/track"), "album_track");
    musicStructureRow->addStretch(1);
    musicStructureRow->addWidget(m_musicFolderStructureCombo);
    sec->addWidget(m_musicFolderStructureRow);
  }

  m_libraryMiddleSeparator = LayoutUtils::createSeparator(this);
  sec->addWidget(m_libraryMiddleSeparator);

  
  m_nfoRow = new QWidget(this);
  auto *nfoRow = new QHBoxLayout(m_nfoRow);
  nfoRow->setContentsMargins(0, 0, 6, 0);
  auto *nfoLabel =
      LayoutUtils::createSectionLabel(tr("Save metadata as Nfo"), this);
  nfoLabel->setToolTip(tr("Save metadata in NFO format alongside media files"));
  m_chkSaveNfo = new ModernSwitch(this);
  m_chkSaveNfo->setObjectName("ManageLibSwitch");
  m_chkSaveNfo->setChecked(false);
  m_chkSaveNfo->setToolTip(
      tr("Save metadata in NFO format alongside media files"));
  nfoRow->addWidget(nfoLabel);
  nfoRow->addStretch(1);
  nfoRow->addWidget(m_chkSaveNfo);
  sec->addWidget(m_nfoRow);

  
  {
    m_nfoReaderRow = new QWidget(this);
    auto *nfoReaderRow = new QHBoxLayout(m_nfoReaderRow);
    nfoReaderRow->setContentsMargins(0, 0, 6, 0);
    auto *nfoReaderLabel =
        LayoutUtils::createSectionLabel(tr("Nfo metadata reader"), this);
    nfoReaderLabel->setToolTip(
        tr("Sort preferred local metadata sources by priority. "
           "The first file found will be read."));
    m_chkNfoReader = new ModernSwitch(this);
    m_chkNfoReader->setObjectName("ManageLibSwitch");
    m_chkNfoReader->setChecked(false);
    m_chkNfoReader->setToolTip(
        tr("Sort preferred local metadata sources by priority. "
           "The first file found will be read."));
    nfoReaderRow->addWidget(nfoReaderLabel);
    nfoReaderRow->addStretch(1);
    nfoReaderRow->addWidget(m_chkNfoReader);
    sec->addWidget(m_nfoReaderRow);
  }

  if (isEmby) {
    
    m_adultMetadataRow = new QWidget(this);
    auto *adultRow = new QHBoxLayout(m_adultMetadataRow);
    adultRow->setContentsMargins(0, 0, 6, 0);
    auto *adultLabel =
        LayoutUtils::createSectionLabel(tr("Allow adult metadata"), this);
    adultLabel->setToolTip(
        tr("Allow matching adult titles when searching for metadata "
           "on the internet"));
    m_chkAllowAdultMetadata = new ModernSwitch(this);
    m_chkAllowAdultMetadata->setObjectName("ManageLibSwitch");
    m_chkAllowAdultMetadata->setChecked(false);
    m_chkAllowAdultMetadata->setToolTip(
        tr("Allow matching adult titles when searching for metadata "
           "on the internet"));
    adultRow->addWidget(adultLabel);
    adultRow->addStretch(1);
    adultRow->addWidget(m_chkAllowAdultMetadata);
    sec->addWidget(m_adultMetadataRow);
  }

  
  if (isEmby) {
    m_importCollectionRow = new QWidget(this);
    auto *importCollRow = new QHBoxLayout(m_importCollectionRow);
    importCollRow->setContentsMargins(0, 0, 6, 0);
    auto *importCollLabel = LayoutUtils::createSectionLabel(
        tr("Import collection info from metadata downloaders"), this);
    importCollLabel->setToolTip(
        tr("Automatically create collections based on metadata"));
    m_chkImportCollectionInfo = new ModernSwitch(this);
    m_chkImportCollectionInfo->setObjectName("ManageLibSwitch");
    m_chkImportCollectionInfo->setChecked(false);
    m_chkImportCollectionInfo->setToolTip(
        tr("Automatically create collections based on metadata"));
    importCollRow->addWidget(importCollLabel);
    importCollRow->addStretch(1);
    importCollRow->addWidget(m_chkImportCollectionInfo);
    sec->addWidget(m_importCollectionRow);
    connect(m_chkImportCollectionInfo, &ModernSwitch::toggled, this,
            [this](bool) { updateTypeSpecificSettings(); });
  }

  
  {
    m_autoCollectionRow = new QWidget(this);
    auto *autoCollRow = new QHBoxLayout(m_autoCollectionRow);
    autoCollRow->setContentsMargins(0, 0, 6, 0);
    auto *autoCollLabel = LayoutUtils::createSectionLabel(
        tr("Automatically group into collections"), this);
    autoCollLabel->setToolTip(tr("Automatically group movies into collections "
                                 "when enough related items exist"));
    m_chkAutoCollection = new ModernSwitch(this);
    m_chkAutoCollection->setObjectName("ManageLibSwitch");
    m_chkAutoCollection->setChecked(false);
    m_chkAutoCollection->setToolTip(
        tr("Automatically group movies into collections when enough related "
           "items exist"));
    autoCollRow->addWidget(autoCollLabel);
    autoCollRow->addStretch(1);
    autoCollRow->addWidget(m_chkAutoCollection);
    sec->addWidget(m_autoCollectionRow);
  }

  
  {
    m_minCollectionSizeRow = new QWidget(this);
    auto *minCollRow = new QHBoxLayout(m_minCollectionSizeRow);
    minCollRow->setContentsMargins(0, 0, 0, 0);
    minCollRow->addWidget(
        LayoutUtils::createSectionLabel(tr("Minimum collection size:"), this));
    m_minCollSizeCombo = LayoutUtils::createStyledCombo(this);
    m_minCollSizeCombo->setFixedWidth(70);
    m_minCollSizeCombo->addItem("1", 1);
    m_minCollSizeCombo->addItem("2", 2);
    m_minCollSizeCombo->addItem("3", 3);
    m_minCollSizeCombo->addItem("4", 4);
    m_minCollSizeCombo->setCurrentIndex(1); 
    m_minCollSizeCombo->setEnabled(isEmby ? true : false);
    minCollRow->addStretch(1);
    minCollRow->addWidget(m_minCollSizeCombo);
    sec->addWidget(m_minCollectionSizeRow);

    
    if (m_chkAutoCollection) {
      connect(m_chkAutoCollection, &ModernSwitch::toggled, this,
              [this](bool) { updateTypeSpecificSettings(); });
    }
  }

  
  {
    m_metaRefreshRow = new QWidget(this);
    auto *refreshRow = new QHBoxLayout(m_metaRefreshRow);
    refreshRow->setContentsMargins(0, 0, 0, 0);
    refreshRow->addWidget(LayoutUtils::createSectionLabel(
        tr("Automatic metadata refresh:"), this));
    m_metaRefreshCombo = LayoutUtils::createStyledCombo(this);
    m_metaRefreshCombo->setFixedWidth(120);
    m_metaRefreshCombo->addItem(tr("Never"), 0);
    m_metaRefreshCombo->addItem(tr("Every 30 days"), 30);
    m_metaRefreshCombo->addItem(tr("Every 60 days"), 60);
    m_metaRefreshCombo->addItem(tr("Every 90 days"), 90);
    m_metaRefreshCombo->setCurrentIndex(0); 
    refreshRow->addStretch(1);
    refreshRow->addWidget(m_metaRefreshCombo);
    sec->addWidget(m_metaRefreshRow);
  }

  if (isEmby) {
    m_placeholderRefreshRow = new QWidget(this);
    auto *placeholderRefreshRow = new QHBoxLayout(m_placeholderRefreshRow);
    placeholderRefreshRow->setContentsMargins(0, 0, 0, 0);
    placeholderRefreshRow->addWidget(LayoutUtils::createSectionLabel(
        tr("Placeholder metadata refresh:"), this));
    m_placeholderRefreshCombo = LayoutUtils::createStyledCombo(this);
    m_placeholderRefreshCombo->setFixedWidth(140);
    populatePlaceholderRefreshCombo(m_placeholderRefreshCombo);
    placeholderRefreshRow->addStretch(1);
    placeholderRefreshRow->addWidget(m_placeholderRefreshCombo);
    sec->addWidget(m_placeholderRefreshRow);
  }

  
  m_metadataFetchersPanel = new QWidget(this);
  auto *metadataFetchersLayout = new QVBoxLayout(m_metadataFetchersPanel);
  metadataFetchersLayout->setContentsMargins(0, 0, 0, 0);
  metadataFetchersLayout->setSpacing(4);
  m_metadataFetchersLabel =
      LayoutUtils::createSectionLabel(tr("Metadata fetchers:"),
                                      m_metadataFetchersPanel);
  metadataFetchersLayout->addWidget(m_metadataFetchersLabel);
  m_metaFetcherLayout = new QVBoxLayout();
  m_metaFetcherLayout->setContentsMargins(0, 0, 0, 0);
  m_metaFetcherLayout->setSpacing(2);
  addFetcherRows(m_metaFetcherLayout, m_metadataFetcherNames,
                 m_metadataFetcherRows);
  metadataFetchersLayout->addLayout(m_metaFetcherLayout);
  sec->addWidget(m_metadataFetchersPanel);

  m_seasonMetadataFetchersPanel = new QWidget(this);
  auto *seasonMetadataFetchersLayout =
      new QVBoxLayout(m_seasonMetadataFetchersPanel);
  seasonMetadataFetchersLayout->setContentsMargins(0, 0, 0, 0);
  seasonMetadataFetchersLayout->setSpacing(4);
  m_seasonMetadataFetchersLabel = LayoutUtils::createSectionLabel(
      tr("Season metadata fetchers:"), m_seasonMetadataFetchersPanel);
  seasonMetadataFetchersLayout->addWidget(m_seasonMetadataFetchersLabel);
  m_seasonMetaFetcherLayout = new QVBoxLayout();
  m_seasonMetaFetcherLayout->setContentsMargins(0, 0, 0, 0);
  m_seasonMetaFetcherLayout->setSpacing(2);
  addFetcherRows(m_seasonMetaFetcherLayout, m_metadataFetcherNames,
                 m_seasonMetadataFetcherRows);
  seasonMetadataFetchersLayout->addLayout(m_seasonMetaFetcherLayout);
  sec->addWidget(m_seasonMetadataFetchersPanel);

  m_episodeMetadataFetchersPanel = new QWidget(this);
  auto *episodeMetadataFetchersLayout =
      new QVBoxLayout(m_episodeMetadataFetchersPanel);
  episodeMetadataFetchersLayout->setContentsMargins(0, 0, 0, 0);
  episodeMetadataFetchersLayout->setSpacing(4);
  m_episodeMetadataFetchersLabel = LayoutUtils::createSectionLabel(
      tr("Episode metadata fetchers:"), m_episodeMetadataFetchersPanel);
  episodeMetadataFetchersLayout->addWidget(m_episodeMetadataFetchersLabel);
  m_episodeMetaFetcherLayout = new QVBoxLayout();
  m_episodeMetaFetcherLayout->setContentsMargins(0, 0, 0, 0);
  m_episodeMetaFetcherLayout->setSpacing(2);
  addFetcherRows(m_episodeMetaFetcherLayout, m_metadataFetcherNames,
                 m_episodeMetadataFetcherRows);
  episodeMetadataFetchersLayout->addLayout(m_episodeMetaFetcherLayout);
  sec->addWidget(m_episodeMetadataFetchersPanel);

  section->setExpanded(true); 
  layout->addWidget(section);
}

void LibraryCreateDialog::setupAdvancedSection(QVBoxLayout *layout) {
  const bool isEmby =
      m_core->serverManager()->activeProfile().type == ServerProfile::Emby;
  if (!isEmby) {
    return;
  }

  m_advancedSection = new CollapsibleSection(tr("File Organization"), this);
  auto *section = m_advancedSection;
  auto *sec = section->contentLayout();

  m_ignoreSamplesRow = new QWidget(this);
  auto *ignoreSamplesRow = new QHBoxLayout(m_ignoreSamplesRow);
  ignoreSamplesRow->setContentsMargins(0, 0, 0, 0);
  ignoreSamplesRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Ignore sample files larger than:"), this));
  m_spinIgnoreSamplesMb = new QSpinBox(this);
  m_spinIgnoreSamplesMb->setObjectName("ManageLibSpinBox");
  m_spinIgnoreSamplesMb->setRange(0, 2047);
  m_spinIgnoreSamplesMb->setValue(300);
  m_spinIgnoreSamplesMb->setSuffix(tr(" MB"));
  m_spinIgnoreSamplesMb->setFixedWidth(100);
  ignoreSamplesRow->addStretch(1);
  ignoreSamplesRow->addWidget(m_spinIgnoreSamplesMb);
  sec->addWidget(m_ignoreSamplesRow);

  m_plexIgnoreRow = new QWidget(this);
  auto *plexIgnoreRow = new QHBoxLayout(m_plexIgnoreRow);
  plexIgnoreRow->setContentsMargins(0, 0, 6, 0);
  auto *plexIgnoreLabel = LayoutUtils::createSectionLabel(
      tr("Enable Plex .ignore compatibility"), this);
  plexIgnoreLabel->setToolTip(
      tr("Honor Plex .ignore files and folders while scanning"));
  m_chkEnablePlexIgnore = new ModernSwitch(this);
  m_chkEnablePlexIgnore->setObjectName("ManageLibSwitch");
  m_chkEnablePlexIgnore->setChecked(false);
  m_chkEnablePlexIgnore->setToolTip(
      tr("Honor Plex .ignore files and folders while scanning"));
  plexIgnoreRow->addWidget(plexIgnoreLabel);
  plexIgnoreRow->addStretch(1);
  plexIgnoreRow->addWidget(m_chkEnablePlexIgnore);
  sec->addWidget(m_plexIgnoreRow);

  m_autoGroupSeriesRow = new QWidget(this);
  auto *autoGroupSeriesRow = new QHBoxLayout(m_autoGroupSeriesRow);
  autoGroupSeriesRow->setContentsMargins(0, 0, 6, 0);
  auto *autoGroupSeriesLabel = LayoutUtils::createSectionLabel(
      tr("Automatically group series"), this);
  autoGroupSeriesLabel->setToolTip(
      tr("Try to group episodes and seasons into series automatically"));
  m_chkAutomaticallyGroupSeries = new ModernSwitch(this);
  m_chkAutomaticallyGroupSeries->setObjectName("ManageLibSwitch");
  m_chkAutomaticallyGroupSeries->setChecked(true);
  m_chkAutomaticallyGroupSeries->setToolTip(
      tr("Try to group episodes and seasons into series automatically"));
  autoGroupSeriesRow->addWidget(autoGroupSeriesLabel);
  autoGroupSeriesRow->addStretch(1);
  autoGroupSeriesRow->addWidget(m_chkAutomaticallyGroupSeries);
  sec->addWidget(m_autoGroupSeriesRow);

  m_multiPartRow = new QWidget(this);
  auto *multiPartRow = new QHBoxLayout(m_multiPartRow);
  multiPartRow->setContentsMargins(0, 0, 6, 0);
  auto *multiPartLabel = LayoutUtils::createSectionLabel(
      tr("Enable multi-part items"), this);
  multiPartLabel->setToolTip(
      tr("Recognize split video files as a single media item"));
  m_chkEnableMultiPart = new ModernSwitch(this);
  m_chkEnableMultiPart->setObjectName("ManageLibSwitch");
  m_chkEnableMultiPart->setChecked(true);
  m_chkEnableMultiPart->setToolTip(
      tr("Recognize split video files as a single media item"));
  multiPartRow->addWidget(multiPartLabel);
  multiPartRow->addStretch(1);
  multiPartRow->addWidget(m_chkEnableMultiPart);
  sec->addWidget(m_multiPartRow);

  m_multiVersionRow = new QWidget(this);
  auto *multiVersionRow = new QHBoxLayout(m_multiVersionRow);
  multiVersionRow->setContentsMargins(0, 0, 0, 0);
  multiVersionRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Multi-version item detection:"), this));
  m_multiVersionCombo = LayoutUtils::createStyledCombo(this);
  m_multiVersionCombo->setFixedWidth(180);
  populateMultiVersionCombo(m_multiVersionCombo);
  multiVersionRow->addStretch(1);
  multiVersionRow->addWidget(m_multiVersionCombo);
  sec->addWidget(m_multiVersionRow);

  section->setExpanded(false);
  layout->addWidget(section);
}




void LibraryCreateDialog::setupImageSection(QVBoxLayout *layout) {
  const bool isEmby =
      m_core->serverManager()->activeProfile().type == ServerProfile::Emby;
  m_imageSection = new CollapsibleSection(tr("Images"), this);
  auto *section = m_imageSection;
  auto *sec = section->contentLayout();

  
  m_saveArtworkRow = new QWidget(this);
  auto *artworkRow = new QHBoxLayout(m_saveArtworkRow);
  artworkRow->setContentsMargins(0, 0, 6, 0);
  auto *artworkLabel = LayoutUtils::createSectionLabel(
      isEmby ? tr("Save local metadata")
             : tr("Save artwork into media folders"),
      this);
  artworkLabel->setToolTip(
      isEmby ? tr("Store downloaded metadata and images alongside media files")
             : tr("Store metadata images alongside media files"));
  m_chkSaveArtwork = new ModernSwitch(this);
  m_chkSaveArtwork->setObjectName("ManageLibSwitch");
  m_chkSaveArtwork->setChecked(false);
  m_chkSaveArtwork->setToolTip(
      isEmby ? tr("Store downloaded metadata and images alongside media files")
             : tr("Store metadata images alongside media files"));
  artworkRow->addWidget(artworkLabel);
  artworkRow->addStretch(1);
  artworkRow->addWidget(m_chkSaveArtwork);
  sec->addWidget(m_saveArtworkRow);

  if (isEmby) {
    m_cacheImagesRow = new QWidget(this);
    auto *cacheImagesRow = new QHBoxLayout(m_cacheImagesRow);
    cacheImagesRow->setContentsMargins(0, 0, 6, 0);
    auto *cacheImagesLabel = LayoutUtils::createSectionLabel(
        tr("Cache images in server data"), this);
    cacheImagesLabel->setToolTip(
        tr("Store generated and downloaded images in the server data folder"));
    m_chkCacheImages = new ModernSwitch(this);
    m_chkCacheImages->setObjectName("ManageLibSwitch");
    m_chkCacheImages->setChecked(false);
    m_chkCacheImages->setToolTip(
        tr("Store generated and downloaded images in the server data folder"));
    cacheImagesRow->addWidget(cacheImagesLabel);
    cacheImagesRow->addStretch(1);
    cacheImagesRow->addWidget(m_chkCacheImages);
    sec->addWidget(m_cacheImagesRow);

    m_saveMetadataHiddenRow = new QWidget(this);
    auto *saveMetadataHiddenRow = new QHBoxLayout(m_saveMetadataHiddenRow);
    saveMetadataHiddenRow->setContentsMargins(0, 0, 6, 0);
    auto *saveMetadataHiddenLabel = LayoutUtils::createSectionLabel(
        tr("Save metadata as hidden files"), this);
    saveMetadataHiddenLabel->setToolTip(
        tr("Mark locally saved metadata files as hidden on supported servers"));
    m_chkSaveMetadataHidden = new ModernSwitch(this);
    m_chkSaveMetadataHidden->setObjectName("ManageLibSwitch");
    m_chkSaveMetadataHidden->setChecked(false);
    m_chkSaveMetadataHidden->setToolTip(
        tr("Mark locally saved metadata files as hidden on supported servers"));
    saveMetadataHiddenRow->addWidget(saveMetadataHiddenLabel);
    saveMetadataHiddenRow->addStretch(1);
    saveMetadataHiddenRow->addWidget(m_chkSaveMetadataHidden);
    sec->addWidget(m_saveMetadataHiddenRow);
  }

  
  m_downloadImagesRow = new QWidget(this);
  auto *dlImgRow = new QHBoxLayout(m_downloadImagesRow);
  dlImgRow->setContentsMargins(0, 0, 6, 0);
  auto *dlImgLabel =
      LayoutUtils::createSectionLabel(tr("Download images in advance"), this);
  dlImgLabel->setToolTip(
      tr("Download all available images during library scan"));
  m_chkDownloadImagesInAdvance = new ModernSwitch(this);
  m_chkDownloadImagesInAdvance->setObjectName("ManageLibSwitch");
  m_chkDownloadImagesInAdvance->setChecked(false);
  m_chkDownloadImagesInAdvance->setToolTip(
      tr("Download all available images during library scan"));
  dlImgRow->addWidget(dlImgLabel);
  dlImgRow->addStretch(1);
  dlImgRow->addWidget(m_chkDownloadImagesInAdvance);
  sec->addWidget(m_downloadImagesRow);

  if (isEmby) {
    m_thumbnailScheduleRow = new QWidget(this);
    auto *thumbnailScheduleRow = new QHBoxLayout(m_thumbnailScheduleRow);
    thumbnailScheduleRow->setContentsMargins(0, 0, 0, 0);
    thumbnailScheduleRow->addWidget(LayoutUtils::createSectionLabel(
        tr("Generate video preview thumbnails:"), this));
    m_thumbnailScheduleCombo = LayoutUtils::createStyledCombo(this);
    m_thumbnailScheduleCombo->setFixedWidth(220);
    populateScheduleCombo(m_thumbnailScheduleCombo);
    thumbnailScheduleRow->addStretch(1);
    thumbnailScheduleRow->addWidget(m_thumbnailScheduleCombo);
    sec->addWidget(m_thumbnailScheduleRow);

    m_thumbnailIntervalRow = new QWidget(this);
    auto *thumbnailIntervalRow = new QHBoxLayout(m_thumbnailIntervalRow);
    thumbnailIntervalRow->setContentsMargins(0, 0, 0, 0);
    thumbnailIntervalRow->addWidget(LayoutUtils::createSectionLabel(
        tr("Thumbnail interval:"), this));
    m_thumbnailIntervalCombo = LayoutUtils::createStyledCombo(this);
    m_thumbnailIntervalCombo->setFixedWidth(160);
    populateThumbnailIntervalCombo(m_thumbnailIntervalCombo);
    thumbnailIntervalRow->addStretch(1);
    thumbnailIntervalRow->addWidget(m_thumbnailIntervalCombo);
    sec->addWidget(m_thumbnailIntervalRow);

    m_saveThumbnailSetsRow = new QWidget(this);
    auto *saveThumbnailSetsRow = new QHBoxLayout(m_saveThumbnailSetsRow);
    saveThumbnailSetsRow->setContentsMargins(0, 0, 6, 0);
    auto *saveThumbnailSetsLabel = LayoutUtils::createSectionLabel(
        tr("Save preview thumbnails into media folders"), this);
    saveThumbnailSetsLabel->setToolTip(
        tr("Store generated video preview thumbnail sets alongside media files"));
    m_chkSaveThumbnailSets = new ModernSwitch(this);
    m_chkSaveThumbnailSets->setObjectName("ManageLibSwitch");
    m_chkSaveThumbnailSets->setChecked(false);
    m_chkSaveThumbnailSets->setToolTip(
        tr("Store generated video preview thumbnail sets alongside media files"));
    saveThumbnailSetsRow->addWidget(saveThumbnailSetsLabel);
    saveThumbnailSetsRow->addStretch(1);
    saveThumbnailSetsRow->addWidget(m_chkSaveThumbnailSets);
    sec->addWidget(m_saveThumbnailSetsRow);

    m_generateChaptersRow = new QWidget(this);
    auto *generateChaptersRow = new QHBoxLayout(m_generateChaptersRow);
    generateChaptersRow->setContentsMargins(0, 0, 6, 0);
    auto *generateChaptersLabel = LayoutUtils::createSectionLabel(
        tr("Generate chapters for videos"), this);
    generateChaptersLabel->setToolTip(
        tr("Create chapter markers automatically for supported videos"));
    m_chkGenerateChapters = new ModernSwitch(this);
    m_chkGenerateChapters->setObjectName("ManageLibSwitch");
    m_chkGenerateChapters->setChecked(true);
    m_chkGenerateChapters->setToolTip(
        tr("Create chapter markers automatically for supported videos"));
    generateChaptersRow->addWidget(generateChaptersLabel);
    generateChaptersRow->addStretch(1);
    generateChaptersRow->addWidget(m_chkGenerateChapters);
    sec->addWidget(m_generateChaptersRow);

    m_chapterIntervalRow = new QWidget(this);
    auto *chapterIntervalRow = new QHBoxLayout(m_chapterIntervalRow);
    chapterIntervalRow->setContentsMargins(0, 0, 0, 0);
    chapterIntervalRow->addWidget(LayoutUtils::createSectionLabel(
        tr("Generated chapter interval:"), this));
    m_chapterIntervalCombo = LayoutUtils::createStyledCombo(this);
    m_chapterIntervalCombo->setFixedWidth(160);
    populateChapterIntervalCombo(m_chapterIntervalCombo);
    chapterIntervalRow->addStretch(1);
    chapterIntervalRow->addWidget(m_chapterIntervalCombo);
    sec->addWidget(m_chapterIntervalRow);

    m_introDetectionRow = new QWidget(this);
    auto *introDetectionRow = new QHBoxLayout(m_introDetectionRow);
    introDetectionRow->setContentsMargins(0, 0, 0, 0);
    introDetectionRow->addWidget(LayoutUtils::createSectionLabel(
        tr("Generate intro video markers:"), this));
    m_introDetectionCombo = LayoutUtils::createStyledCombo(this);
    m_introDetectionCombo->setFixedWidth(220);
    populateScheduleCombo(m_introDetectionCombo);
    introDetectionRow->addStretch(1);
    introDetectionRow->addWidget(m_introDetectionCombo);
    sec->addWidget(m_introDetectionRow);

    connect(m_chkGenerateChapters, &ModernSwitch::toggled, this,
            [this](bool) { updateTypeSpecificSettings(); });
    connect(m_thumbnailScheduleCombo, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateTypeSpecificSettings(); });
  } else {
    
    m_extractChapterImagesRow = new QWidget(this);
    auto *chapterRow = new QHBoxLayout(m_extractChapterImagesRow);
    chapterRow->setContentsMargins(0, 0, 6, 0);
    auto *chapterLabel =
        LayoutUtils::createSectionLabel(tr("Extract chapter images"), this);
    chapterLabel->setToolTip(
        tr("Generate thumbnail images for video chapters"));
    m_chkExtractChapterImages = new ModernSwitch(this);
    m_chkExtractChapterImages->setObjectName("ManageLibSwitch");
    m_chkExtractChapterImages->setChecked(false);
    m_chkExtractChapterImages->setToolTip(
        tr("Generate thumbnail images for video chapters"));
    chapterRow->addWidget(chapterLabel);
    chapterRow->addStretch(1);
    chapterRow->addWidget(m_chkExtractChapterImages);
    sec->addWidget(m_extractChapterImagesRow);
  }

  m_imageOptionsSeparator = LayoutUtils::createSeparator(this);
  sec->addWidget(m_imageOptionsSeparator);

  m_imageFetchersPanel = new QWidget(this);
  auto *imageFetchersLayout = new QVBoxLayout(m_imageFetchersPanel);
  imageFetchersLayout->setContentsMargins(0, 0, 0, 0);
  imageFetchersLayout->setSpacing(4);
  m_imageFetchersLabel =
      LayoutUtils::createSectionLabel(tr("Image fetchers:"), m_imageFetchersPanel);
  imageFetchersLayout->addWidget(m_imageFetchersLabel);
  m_imgFetcherLayout = new QVBoxLayout();
  m_imgFetcherLayout->setContentsMargins(0, 0, 0, 0);
  m_imgFetcherLayout->setSpacing(2);
  addFetcherRows(m_imgFetcherLayout, m_imageFetcherNames, m_imageFetcherRows);
  imageFetchersLayout->addLayout(m_imgFetcherLayout);
  sec->addWidget(m_imageFetchersPanel);

  m_seasonImageFetchersPanel = new QWidget(this);
  auto *seasonImageFetchersLayout =
      new QVBoxLayout(m_seasonImageFetchersPanel);
  seasonImageFetchersLayout->setContentsMargins(0, 0, 0, 0);
  seasonImageFetchersLayout->setSpacing(4);
  m_seasonImageFetchersLabel = LayoutUtils::createSectionLabel(
      tr("Season image fetchers:"), m_seasonImageFetchersPanel);
  seasonImageFetchersLayout->addWidget(m_seasonImageFetchersLabel);
  m_seasonImgFetcherLayout = new QVBoxLayout();
  m_seasonImgFetcherLayout->setContentsMargins(0, 0, 0, 0);
  m_seasonImgFetcherLayout->setSpacing(2);
  addFetcherRows(m_seasonImgFetcherLayout, m_imageFetcherNames,
                 m_seasonImageFetcherRows);
  seasonImageFetchersLayout->addLayout(m_seasonImgFetcherLayout);
  sec->addWidget(m_seasonImageFetchersPanel);

  m_episodeImageFetchersPanel = new QWidget(this);
  auto *episodeImageFetchersLayout =
      new QVBoxLayout(m_episodeImageFetchersPanel);
  episodeImageFetchersLayout->setContentsMargins(0, 0, 0, 0);
  episodeImageFetchersLayout->setSpacing(4);
  m_episodeImageFetchersLabel = LayoutUtils::createSectionLabel(
      tr("Episode image fetchers:"), m_episodeImageFetchersPanel);
  episodeImageFetchersLayout->addWidget(m_episodeImageFetchersLabel);
  m_episodeImgFetcherLayout = new QVBoxLayout();
  m_episodeImgFetcherLayout->setContentsMargins(0, 0, 0, 0);
  m_episodeImgFetcherLayout->setSpacing(2);
  addFetcherRows(m_episodeImgFetcherLayout, m_imageFetcherNames,
                 m_episodeImageFetcherRows);
  episodeImageFetchersLayout->addLayout(m_episodeImgFetcherLayout);
  sec->addWidget(m_episodeImageFetchersPanel);

  section->setExpanded(true); 
  layout->addWidget(section);
}




void LibraryCreateDialog::setupSubtitleSection(QVBoxLayout *layout) {
  const bool isEmby =
      m_core->serverManager()->activeProfile().type == ServerProfile::Emby;
  m_subtitleSection = new CollapsibleSection(tr("Subtitles"), this);
  auto *section = m_subtitleSection;
  auto *sec = section->contentLayout();

  m_subtitleDownloadersPanel = new QWidget(this);
  auto *subtitleDownloadersLayout =
      new QVBoxLayout(m_subtitleDownloadersPanel);
  subtitleDownloadersLayout->setContentsMargins(0, 0, 0, 0);
  subtitleDownloadersLayout->setSpacing(4);
  m_subtitleDownloadersLabel = LayoutUtils::createSectionLabel(
      tr("Subtitle downloaders:"), m_subtitleDownloadersPanel);
  subtitleDownloadersLayout->addWidget(m_subtitleDownloadersLabel);
  m_subDownloaderLayout = new QVBoxLayout();
  m_subDownloaderLayout->setContentsMargins(0, 0, 0, 0);
  m_subDownloaderLayout->setSpacing(2);
  addFetcherRows(m_subDownloaderLayout, m_subtitleDownloaderNames,
                 m_subtitleDownloaderRows);
  subtitleDownloadersLayout->addLayout(m_subDownloaderLayout);
  sec->addWidget(m_subtitleDownloadersPanel);

  m_subtitleDownloadersSeparator = LayoutUtils::createSeparator(this);
  sec->addWidget(m_subtitleDownloadersSeparator);

  auto *subLangRow = new QHBoxLayout();
  subLangRow->setContentsMargins(0, 0, 0, 0);
  subLangRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Preferred subtitle language:"), this));
  m_subtitleLangInput = new ModernTagInput(this);
  m_subtitleLangInput->setEditable(false);
  populateSubtitleLanguageInput(m_subtitleLangInput, isEmby);
  subLangRow->addSpacing(12);
  subLangRow->addWidget(m_subtitleLangInput, 1);
  sec->addLayout(subLangRow);

  sec->addWidget(LayoutUtils::createSeparator(this));

  
  auto *saveSubRow = new QHBoxLayout();
  saveSubRow->setContentsMargins(0, 0, 6, 0);
  auto *saveSubLabel = LayoutUtils::createSectionLabel(
      tr("Save subtitles into media folders"), this);
  saveSubLabel->setToolTip(
      tr("Download subtitles and save them alongside media files"));
  m_chkSaveSubtitles = new ModernSwitch(this);
  m_chkSaveSubtitles->setObjectName("ManageLibSwitch");
  m_chkSaveSubtitles->setChecked(true);
  m_chkSaveSubtitles->setToolTip(
      tr("Download subtitles and save them alongside media files"));
  saveSubRow->addWidget(saveSubLabel);
  saveSubRow->addStretch(1);
  saveSubRow->addWidget(m_chkSaveSubtitles);
  sec->addLayout(saveSubRow);

  
  auto *skipAudioRow = new QHBoxLayout();
  skipAudioRow->setContentsMargins(0, 0, 6, 0);
  auto *skipAudioLabel = LayoutUtils::createSectionLabel(
      tr("Skip if audio track matches download language"), this);
  skipAudioLabel->setToolTip(
      tr("Uncheck to ensure all videos have subtitles regardless of "
         "audio language"));
  m_chkSkipIfAudioMatchesSub = new ModernSwitch(this);
  m_chkSkipIfAudioMatchesSub->setObjectName("ManageLibSwitch");
  m_chkSkipIfAudioMatchesSub->setChecked(true);
  m_chkSkipIfAudioMatchesSub->setToolTip(
      tr("Uncheck to ensure all videos have subtitles regardless of "
         "audio language"));
  skipAudioRow->addWidget(skipAudioLabel);
  skipAudioRow->addStretch(1);
  skipAudioRow->addWidget(m_chkSkipIfAudioMatchesSub);
  sec->addLayout(skipAudioRow);

  
  auto *skipEmbeddedRow = new QHBoxLayout();
  skipEmbeddedRow->setContentsMargins(0, 0, 6, 0);
  auto *skipEmbeddedLabel = LayoutUtils::createSectionLabel(
      tr("Skip if video has embedded subtitles"), this);
  skipEmbeddedLabel->setToolTip(
      tr("Keeping text versions of subtitles will improve delivery "
         "efficiency and reduce likelihood of video transcoding"));
  m_chkSkipIfEmbeddedSub = new ModernSwitch(this);
  m_chkSkipIfEmbeddedSub->setObjectName("ManageLibSwitch");
  m_chkSkipIfEmbeddedSub->setChecked(true);
  m_chkSkipIfEmbeddedSub->setToolTip(
      tr("Keeping text versions of subtitles will improve delivery "
         "efficiency and reduce likelihood of video transcoding"));
  skipEmbeddedRow->addWidget(skipEmbeddedLabel);
  skipEmbeddedRow->addStretch(1);
  skipEmbeddedRow->addWidget(m_chkSkipIfEmbeddedSub);
  sec->addLayout(skipEmbeddedRow);

  if (isEmby) {
    auto *requireMatchRow = new QHBoxLayout();
    requireMatchRow->setContentsMargins(0, 0, 6, 0);
    auto *requireMatchLabel = LayoutUtils::createSectionLabel(
        tr("Require hash match for subtitle downloads"), this);
    requireMatchLabel->setToolTip(
        tr("Only download subtitles when the provider reports a matching file hash"));
    m_chkRequirePerfectSubtitleMatch = new ModernSwitch(this);
    m_chkRequirePerfectSubtitleMatch->setObjectName("ManageLibSwitch");
    m_chkRequirePerfectSubtitleMatch->setChecked(true);
    m_chkRequirePerfectSubtitleMatch->setToolTip(
        tr("Only download subtitles when the provider reports a matching file hash"));
    requireMatchRow->addWidget(requireMatchLabel);
    requireMatchRow->addStretch(1);
    requireMatchRow->addWidget(m_chkRequirePerfectSubtitleMatch);
    sec->addLayout(requireMatchRow);

    auto *forcedOnlyRow = new QHBoxLayout();
    forcedOnlyRow->setContentsMargins(0, 0, 6, 0);
    auto *forcedOnlyLabel = LayoutUtils::createSectionLabel(
        tr("Search only for forced subtitles"), this);
    forcedOnlyLabel->setToolTip(
        tr("Restrict automatic subtitle downloads to forced subtitle tracks"));
    m_chkForcedSubtitlesOnly = new ModernSwitch(this);
    m_chkForcedSubtitlesOnly->setObjectName("ManageLibSwitch");
    m_chkForcedSubtitlesOnly->setChecked(false);
    m_chkForcedSubtitlesOnly->setToolTip(
        tr("Restrict automatic subtitle downloads to forced subtitle tracks"));
    forcedOnlyRow->addWidget(forcedOnlyLabel);
    forcedOnlyRow->addStretch(1);
    forcedOnlyRow->addWidget(m_chkForcedSubtitlesOnly);
    sec->addLayout(forcedOnlyRow);
  }

  
  auto *subAgeRow = new QHBoxLayout();
  subAgeRow->setContentsMargins(0, 0, 0, 0);
  subAgeRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Stop downloading subtitles older than:"), this));
  m_subAgeLimitCombo = LayoutUtils::createStyledCombo(this);
  m_subAgeLimitCombo->addItem(tr("No Limit"), 0);
  m_subAgeLimitCombo->addItem(tr("14 days"), 14);
  m_subAgeLimitCombo->addItem(tr("30 days"), 30);
  m_subAgeLimitCombo->addItem(tr("60 days"), 60);
  m_subAgeLimitCombo->addItem(tr("90 days"), 90);
  m_subAgeLimitCombo->addItem(tr("120 days"), 120);
  m_subAgeLimitCombo->addItem(tr("180 days"), 180);
  m_subAgeLimitCombo->setCurrentIndex(0); 
  subAgeRow->addStretch(1);
  subAgeRow->addWidget(m_subAgeLimitCombo);
  sec->addLayout(subAgeRow);

  section->setExpanded(true); 
  layout->addWidget(section);
}

void LibraryCreateDialog::setupLyricsSection(QVBoxLayout *layout) {
  const bool isEmby =
      m_core->serverManager()->activeProfile().type == ServerProfile::Emby;
  if (!isEmby) {
    return;
  }

  m_lyricsSection = new CollapsibleSection(tr("Lyrics"), this);
  auto *section = m_lyricsSection;
  auto *sec = section->contentLayout();

  m_lyricsFetchersPanel = new QWidget(this);
  auto *lyricsFetchersLayout = new QVBoxLayout(m_lyricsFetchersPanel);
  lyricsFetchersLayout->setContentsMargins(0, 0, 0, 0);
  lyricsFetchersLayout->setSpacing(4);
  m_lyricsFetchersLabel = LayoutUtils::createSectionLabel(
      tr("Lyrics fetchers:"), m_lyricsFetchersPanel);
  lyricsFetchersLayout->addWidget(m_lyricsFetchersLabel);
  m_lyricsFetcherLayout = new QVBoxLayout();
  m_lyricsFetcherLayout->setContentsMargins(0, 0, 0, 0);
  m_lyricsFetcherLayout->setSpacing(2);
  addFetcherRows(m_lyricsFetcherLayout, m_lyricsFetcherNames,
                 m_lyricsFetcherRows);
  lyricsFetchersLayout->addLayout(m_lyricsFetcherLayout);
  sec->addWidget(m_lyricsFetchersPanel);

  if (!m_lyricsFetcherNames.isEmpty()) {
    m_lyricsFetchersSeparator = LayoutUtils::createSeparator(this);
    sec->addWidget(m_lyricsFetchersSeparator);
  }

  auto *lyricsLangRow = new QHBoxLayout();
  lyricsLangRow->setContentsMargins(0, 0, 0, 0);
  lyricsLangRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Preferred lyrics language:"), this));
  m_lyricsLangInput = new ModernTagInput(this);
  m_lyricsLangInput->setEditable(false);
  populateSubtitleLanguageInput(m_lyricsLangInput, true);
  lyricsLangRow->addSpacing(12);
  lyricsLangRow->addWidget(m_lyricsLangInput, 1);
  sec->addLayout(lyricsLangRow);

  sec->addWidget(LayoutUtils::createSeparator(this));

  auto *lyricsAgeRow = new QHBoxLayout();
  lyricsAgeRow->setContentsMargins(0, 0, 0, 0);
  lyricsAgeRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Stop downloading lyrics older than:"), this));
  m_lyricsAgeLimitCombo = LayoutUtils::createStyledCombo(this);
  m_lyricsAgeLimitCombo->setFixedWidth(130);
  m_lyricsAgeLimitCombo->addItem(tr("No Limit"), 0);
  m_lyricsAgeLimitCombo->addItem(tr("14 days"), 14);
  m_lyricsAgeLimitCombo->addItem(tr("30 days"), 30);
  m_lyricsAgeLimitCombo->addItem(tr("60 days"), 60);
  m_lyricsAgeLimitCombo->addItem(tr("90 days"), 90);
  m_lyricsAgeLimitCombo->addItem(tr("120 days"), 120);
  m_lyricsAgeLimitCombo->addItem(tr("180 days"), 180);
  m_lyricsAgeLimitCombo->setCurrentIndex(6);
  lyricsAgeRow->addStretch(1);
  lyricsAgeRow->addWidget(m_lyricsAgeLimitCombo);
  sec->addLayout(lyricsAgeRow);

  auto *saveLyricsRow = new QHBoxLayout();
  saveLyricsRow->setContentsMargins(0, 0, 6, 0);
  auto *saveLyricsLabel = LayoutUtils::createSectionLabel(
      tr("Save lyrics into media folders"), this);
  saveLyricsLabel->setToolTip(
      tr("Store downloaded lyrics files alongside media files"));
  m_chkSaveLyrics = new ModernSwitch(this);
  m_chkSaveLyrics->setObjectName("ManageLibSwitch");
  m_chkSaveLyrics->setChecked(true);
  m_chkSaveLyrics->setToolTip(
      tr("Store downloaded lyrics files alongside media files"));
  saveLyricsRow->addWidget(saveLyricsLabel);
  saveLyricsRow->addStretch(1);
  saveLyricsRow->addWidget(m_chkSaveLyrics);
  sec->addLayout(saveLyricsRow);

  section->setExpanded(false);
  layout->addWidget(section);
}




void LibraryCreateDialog::setupPlaybackSection(QVBoxLayout *layout) {
  bool isEmby =
      m_core->serverManager()->activeProfile().type == ServerProfile::Emby;
  if (!isEmby)
    return;

  m_playbackSection = new CollapsibleSection(tr("Playback"), this);
  auto *section = m_playbackSection;
  auto *sec = section->contentLayout();

  
  auto *minPctRow = new QHBoxLayout();
  minPctRow->setContentsMargins(0, 0, 0, 0);
  minPctRow->addWidget(
      LayoutUtils::createSectionLabel(tr("Minimum resume percentage:"), this));
  m_spinMinResumePercent = new QSpinBox(this);
  m_spinMinResumePercent->setObjectName("ManageLibSpinBox");
  m_spinMinResumePercent->setRange(0, 100);
  m_spinMinResumePercent->setValue(2);
  m_spinMinResumePercent->setSuffix("%");
  m_spinMinResumePercent->setFixedWidth(80);
  m_spinMinResumePercent->setToolTip(
      tr("If stopped before this point, the item will be marked as unplayed"));
  minPctRow->addStretch(1);
  minPctRow->addWidget(m_spinMinResumePercent);
  sec->addLayout(minPctRow);

  
  auto *maxPctRow = new QHBoxLayout();
  maxPctRow->setContentsMargins(0, 0, 0, 0);
  maxPctRow->addWidget(
      LayoutUtils::createSectionLabel(tr("Maximum resume percentage:"), this));
  m_spinMaxResumePercent = new QSpinBox(this);
  m_spinMaxResumePercent->setObjectName("ManageLibSpinBox");
  m_spinMaxResumePercent->setRange(1, 100);
  m_spinMaxResumePercent->setValue(90);
  m_spinMaxResumePercent->setSuffix("%");
  m_spinMaxResumePercent->setFixedWidth(80);
  m_spinMaxResumePercent->setToolTip(
      tr("If stopped after this point, the item will be marked as played"));
  maxPctRow->addStretch(1);
  maxPctRow->addWidget(m_spinMaxResumePercent);
  sec->addLayout(maxPctRow);

  
  auto *minDurRow = new QHBoxLayout();
  minDurRow->setContentsMargins(0, 0, 0, 0);
  minDurRow->addWidget(LayoutUtils::createSectionLabel(
      tr("Minimum resume duration (seconds):"), this));
  m_spinMinResumeDuration = new QSpinBox(this);
  m_spinMinResumeDuration->setObjectName("ManageLibSpinBox");
  m_spinMinResumeDuration->setRange(0, 600);
  m_spinMinResumeDuration->setValue(120);
  m_spinMinResumeDuration->setSuffix(tr(" s"));
  m_spinMinResumeDuration->setFixedWidth(80);
  m_spinMinResumeDuration->setToolTip(
      tr("Items shorter than this duration cannot be resumed"));
  minDurRow->addStretch(1);
  minDurRow->addWidget(m_spinMinResumeDuration);
  sec->addLayout(minDurRow);

  section->setExpanded(true);
  layout->addWidget(section);
}

QString LibraryCreateDialog::normalizedCollectionType() const {
  if (!m_typeCombo) {
    qWarning() << "[LibraryCreateDialog] normalizedCollectionType fallback:"
               << "type combo is null";
    return {};
  }

  return LTU::canonicalCollectionType(m_typeCombo->currentData().toString());
}

void LibraryCreateDialog::applyTypeDefaults() {
  const auto serverType = m_core->serverManager()->activeProfile().type;
  const auto profile =
      LTU::buildLibraryTypeProfile(serverType, normalizedCollectionType());
  const bool isEmby = serverType == ServerProfile::Emby;

  if (m_chkRealtimeMonitor)
    m_chkRealtimeMonitor->setChecked(profile.defaultEnableRealtimeMonitor);
  if (m_chkEnablePhotos)
    m_chkEnablePhotos->setChecked(true);
  if (m_chkImportPlaylists)
    m_chkImportPlaylists->setChecked(true);
  if (m_chkExcludeFromSearch)
    m_chkExcludeFromSearch->setChecked(false);
  if (m_chkMergeFolders)
    m_chkMergeFolders->setChecked(false);
  if (m_chkSaveNfo)
    m_chkSaveNfo->setChecked(profile.defaultSaveNfo);
  if (m_chkNfoReader)
    m_chkNfoReader->setChecked(profile.defaultEnableNfoReader);
  if (m_chkAutoCollection)
    m_chkAutoCollection->setChecked(
        profile.defaultAutomaticallyAddToCollection);
  if (m_chkSaveArtwork)
    m_chkSaveArtwork->setChecked(profile.defaultSaveArtwork);
  if (m_chkCacheImages)
    m_chkCacheImages->setChecked(false);
  if (m_chkSaveMetadataHidden)
    m_chkSaveMetadataHidden->setChecked(false);
  if (m_chkSaveThumbnailSets)
    m_chkSaveThumbnailSets->setChecked(false);
  if (m_chkGenerateChapters)
    m_chkGenerateChapters->setChecked(true);
  if (m_chkEnablePlexIgnore)
    m_chkEnablePlexIgnore->setChecked(false);
  if (m_chkAutomaticallyGroupSeries)
    m_chkAutomaticallyGroupSeries->setChecked(true);
  if (m_chkEnableMultiPart)
    m_chkEnableMultiPart->setChecked(true);
  if (m_chkSaveSubtitles)
    m_chkSaveSubtitles->setChecked(profile.defaultSaveSubtitlesWithMedia);
  if (m_chkSkipIfAudioMatchesSub) {
    m_chkSkipIfAudioMatchesSub->setChecked(
        profile.defaultSkipSubtitlesIfAudioTrackMatches);
  }
  if (m_chkSkipIfEmbeddedSub) {
    m_chkSkipIfEmbeddedSub->setChecked(
        profile.defaultSkipSubtitlesIfEmbeddedSubtitlesPresent);
  }
  if (m_chkRequirePerfectSubtitleMatch) {
    m_chkRequirePerfectSubtitleMatch->setChecked(true);
  }
  if (m_chkForcedSubtitlesOnly) {
    m_chkForcedSubtitlesOnly->setChecked(false);
  }
  if (m_chkSaveLyrics) {
    m_chkSaveLyrics->setChecked(true);
  }
  if (m_metaRefreshCombo) {
    setComboCurrentData(m_metaRefreshCombo, 0);
  }
  if (m_placeholderRefreshCombo) {
    setComboCurrentData(m_placeholderRefreshCombo, 0);
  }
  if (m_minCollSizeCombo) {
    setComboCurrentData(m_minCollSizeCombo, 2);
  }
  if (m_thumbnailScheduleCombo) {
    setComboCurrentData(m_thumbnailScheduleCombo, "");
  }
  if (m_thumbnailIntervalCombo) {
    setComboCurrentData(m_thumbnailIntervalCombo, 10);
  }
  if (m_chapterIntervalCombo) {
    setComboCurrentData(m_chapterIntervalCombo, 5);
  }
  if (m_introDetectionCombo) {
    setComboCurrentData(m_introDetectionCombo, "");
  }
  if (m_subAgeLimitCombo) {
    setComboCurrentData(m_subAgeLimitCombo, 0);
  }
  if (m_lyricsAgeLimitCombo) {
    setComboCurrentData(m_lyricsAgeLimitCombo, 180);
  }
  if (m_multiVersionCombo) {
    QString defaultMultiVersionMode = QStringLiteral("files");
    if (profile.collectionType == "movies" || profile.collectionType.isEmpty()) {
      defaultMultiVersionMode = QStringLiteral("both");
    } else if (profile.collectionType == "homevideos") {
      defaultMultiVersionMode = QStringLiteral("none");
    }
    setComboCurrentData(m_multiVersionCombo, defaultMultiVersionMode);
  }
  if (m_musicFolderStructureCombo) {
    setComboCurrentData(m_musicFolderStructureCombo, "");
  }
  if (m_spinIgnoreSamplesMb) {
    m_spinIgnoreSamplesMb->setValue(300);
  }
  if (m_spinMinResumePercent) {
    m_spinMinResumePercent->setValue(isEmby ? 2 : 3);
  }
  if (m_spinMaxResumePercent) {
    m_spinMaxResumePercent->setValue(90);
  }
  if (m_spinMinResumeDuration) {
    m_spinMinResumeDuration->setValue(120);
  }
  if (m_lyricsLangInput) {
    m_lyricsLangInput->setValue(QString());
  }
}

int LibraryCreateDialog::adjacentApplicableFetcherIndex(
    const QList<FetcherRowWidget *> &rows, int currentIndex,
    int direction) const {
  if (direction == 0)
    return -1;

  LTU::FetcherKind kind = LTU::FetcherKind::Metadata;
  if (&rows == &m_imageFetcherRows) {
    kind = LTU::FetcherKind::Image;
  } else if (&rows == &m_seasonImageFetcherRows) {
    kind = LTU::FetcherKind::Image;
  } else if (&rows == &m_episodeImageFetcherRows) {
    kind = LTU::FetcherKind::Image;
  } else if (&rows == &m_subtitleDownloaderRows) {
    kind = LTU::FetcherKind::Subtitle;
  } else if (&rows == &m_lyricsFetcherRows) {
    kind = LTU::FetcherKind::Lyrics;
  }

  const auto serverType = m_core->serverManager()->activeProfile().type;
  const auto profile =
      LTU::buildLibraryTypeProfile(serverType, normalizedCollectionType());

  for (int idx = currentIndex + direction; idx >= 0 && idx < rows.size();
       idx += direction) {
    auto *candidate = rows[idx];
    if (candidate &&
        LTU::isFetcherApplicable(candidate->name(), kind, profile,
                                 serverType)) {
      return idx;
    }
  }

  return -1;
}

void LibraryCreateDialog::updateTypeSpecificSettings() {
  const auto serverType = m_core->serverManager()->activeProfile().type;
  const auto profile =
      LTU::buildLibraryTypeProfile(serverType, normalizedCollectionType());
  auto isVisible = [](const QWidget *widget) {
    return widget && widget->isVisible();
  };

  if (m_enablePhotosRow)
    m_enablePhotosRow->setVisible(profile.showEnablePhotos);
  if (m_importPlaylistsRow)
    m_importPlaylistsRow->setVisible(profile.showImportPlaylists);
  if (m_excludeFromSearchRow)
    m_excludeFromSearchRow->setVisible(profile.showExcludeFromSearch);
  if (m_mergeFoldersRow)
    m_mergeFoldersRow->setVisible(profile.showMergeTopLevelFolders);
  if (m_embeddedTitlesRow)
    m_embeddedTitlesRow->setVisible(profile.showEmbeddedTitles);
  if (m_metaLangRow)
    m_metaLangRow->setVisible(profile.showMetadataLocale);
  if (m_countryRow)
    m_countryRow->setVisible(profile.showMetadataLocale);
  if (m_imageLangRow)
    m_imageLangRow->setVisible(profile.showImageLanguage);
  if (m_musicFolderStructureRow)
    m_musicFolderStructureRow->setVisible(profile.showMusicFolderStructure);
  if (m_nfoRow)
    m_nfoRow->setVisible(profile.showSaveNfo);
  if (m_nfoReaderRow)
    m_nfoReaderRow->setVisible(profile.showNfoReader);
  if (m_adultMetadataRow)
    m_adultMetadataRow->setVisible(profile.showAdultMetadata);
  if (m_importCollectionRow)
    m_importCollectionRow->setVisible(profile.showMovieCollectionSettings);
  if (m_autoCollectionRow)
    m_autoCollectionRow->setVisible(
        profile.showMovieCollectionSettings &&
        serverType != ServerProfile::Emby);
  if (m_minCollectionSizeRow)
    m_minCollectionSizeRow->setVisible(
        profile.showMovieCollectionSettings &&
        (serverType != ServerProfile::Emby ||
         (m_chkImportCollectionInfo &&
          m_chkImportCollectionInfo->isChecked())));
  if (m_metaRefreshRow)
    m_metaRefreshRow->setVisible(profile.showMetadataRefresh);
  if (m_placeholderRefreshRow)
    m_placeholderRefreshRow->setVisible(profile.showPlaceholderMetadataRefresh);

  auto updateFetcherVisibility = [&](const QList<FetcherRowWidget *> &rows,
                                     LTU::FetcherKind kind) {
    bool hasVisibleRows = false;
    for (auto *row : rows) {
      const bool visible =
          row &&
          LTU::isFetcherApplicable(row->name(), kind, profile, serverType);
      if (row)
        row->setVisible(visible);
      hasVisibleRows = hasVisibleRows || visible;
    }
    return hasVisibleRows;
  };

  const bool isTvShowLibrary = profile.collectionType == "tvshows";
  if (m_metadataFetchersLabel) {
    m_metadataFetchersLabel->setText(
        isTvShowLibrary ? tr("Series metadata fetchers:")
                        : tr("Metadata fetchers:"));
  }
  const bool hasMetadataFetchers =
      updateFetcherVisibility(m_metadataFetcherRows,
                              LTU::FetcherKind::Metadata);
  const bool hasSeasonMetadataFetchers =
      updateFetcherVisibility(m_seasonMetadataFetcherRows,
                              LTU::FetcherKind::Metadata);
  const bool hasEpisodeMetadataFetchers =
      updateFetcherVisibility(m_episodeMetadataFetcherRows,
                              LTU::FetcherKind::Metadata);
  const bool hasAnyMetadataProviders =
      hasMetadataFetchers || hasSeasonMetadataFetchers ||
      hasEpisodeMetadataFetchers;
  if (m_metaLangRow)
    m_metaLangRow->setVisible(profile.showMetadataLocale &&
                              hasAnyMetadataProviders);
  if (m_countryRow)
    m_countryRow->setVisible(profile.showMetadataLocale &&
                             hasAnyMetadataProviders);
  if (m_imageLangRow)
    m_imageLangRow->setVisible(profile.showImageLanguage &&
                               hasAnyMetadataProviders);
  if (m_placeholderRefreshRow)
    m_placeholderRefreshRow->setVisible(
        profile.showPlaceholderMetadataRefresh && hasAnyMetadataProviders);
  const bool showMetadataFetchersPanel =
      profile.showMetadataFetchers && hasMetadataFetchers;
  const bool showSeasonMetadataFetchersPanel =
      profile.showMetadataFetchers && isTvShowLibrary &&
      hasSeasonMetadataFetchers;
  const bool showEpisodeMetadataFetchersPanel =
      profile.showMetadataFetchers && isTvShowLibrary &&
      hasEpisodeMetadataFetchers;
  if (m_metadataFetchersPanel)
    m_metadataFetchersPanel->setVisible(showMetadataFetchersPanel);
  if (m_metadataFetchersLabel)
    m_metadataFetchersLabel->setVisible(showMetadataFetchersPanel);
  if (m_seasonMetadataFetchersPanel)
    m_seasonMetadataFetchersPanel->setVisible(showSeasonMetadataFetchersPanel);
  if (m_seasonMetadataFetchersLabel)
    m_seasonMetadataFetchersLabel->setVisible(showSeasonMetadataFetchersPanel);
  if (m_episodeMetadataFetchersPanel)
    m_episodeMetadataFetchersPanel->setVisible(
        showEpisodeMetadataFetchersPanel);
  if (m_episodeMetadataFetchersLabel)
    m_episodeMetadataFetchersLabel->setVisible(
        showEpisodeMetadataFetchersPanel);

  const bool hasLibraryLocaleGroup =
      isVisible(m_metaLangRow) || isVisible(m_countryRow) ||
      isVisible(m_imageLangRow) || isVisible(m_musicFolderStructureRow);
  const bool hasLibraryMetadataGroup =
      isVisible(m_enablePhotosRow) || isVisible(m_importPlaylistsRow) ||
      isVisible(m_excludeFromSearchRow) || isVisible(m_mergeFoldersRow) ||
      isVisible(m_nfoRow) || isVisible(m_nfoReaderRow) ||
      isVisible(m_adultMetadataRow) || isVisible(m_importCollectionRow) ||
      isVisible(m_autoCollectionRow) || isVisible(m_minCollectionSizeRow) ||
      isVisible(m_metaRefreshRow) || isVisible(m_placeholderRefreshRow) ||
      isVisible(m_metadataFetchersPanel) ||
      isVisible(m_seasonMetadataFetchersPanel) ||
      isVisible(m_episodeMetadataFetchersPanel);
  if (m_libraryTopSeparator)
    m_libraryTopSeparator->setVisible(hasLibraryLocaleGroup ||
                                      hasLibraryMetadataGroup);
  if (m_libraryMiddleSeparator)
    m_libraryMiddleSeparator->setVisible(hasLibraryLocaleGroup &&
                                         hasLibraryMetadataGroup);

  if (m_ignoreSamplesRow)
    m_ignoreSamplesRow->setVisible(profile.showSampleIgnoreSize);
  if (m_plexIgnoreRow)
    m_plexIgnoreRow->setVisible(profile.showEnablePlexIgnore);
  if (m_autoGroupSeriesRow)
    m_autoGroupSeriesRow->setVisible(profile.showAutomaticSeriesGrouping);
  if (m_multiPartRow)
    m_multiPartRow->setVisible(profile.showMultiPartItems);
  if (m_multiVersionRow)
    m_multiVersionRow->setVisible(profile.showMultiVersionGrouping);
  if (m_multiVersionCombo) {
    const bool allowAdvancedModes = profile.allowAdvancedMultiVersionModes;
    const QString currentMode =
        m_multiVersionCombo->currentData().toString();
    if (!allowAdvancedModes &&
        (currentMode == "both" || currentMode == "metadata")) {
      setComboCurrentData(m_multiVersionCombo, "files");
    }
  }
  if (m_advancedSection) {
    const bool showAdvancedSection =
        profile.showAdvancedSection &&
        (isVisible(m_ignoreSamplesRow) || isVisible(m_plexIgnoreRow) ||
         isVisible(m_autoGroupSeriesRow) || isVisible(m_multiPartRow) ||
         isVisible(m_multiVersionRow));
    m_advancedSection->setVisible(showAdvancedSection);
  }

  if (m_saveArtworkRow)
    m_saveArtworkRow->setVisible(profile.showSaveArtwork);
  if (m_cacheImagesRow)
    m_cacheImagesRow->setVisible(profile.showCacheImages);
  if (m_saveMetadataHiddenRow)
    m_saveMetadataHiddenRow->setVisible(profile.showSaveMetadataHidden);
  if (m_downloadImagesRow)
    m_downloadImagesRow->setVisible(profile.showDownloadImagesInAdvance);
  if (m_extractChapterImagesRow)
    m_extractChapterImagesRow->setVisible(profile.showChapterImages);
  if (m_thumbnailScheduleRow)
    m_thumbnailScheduleRow->setVisible(profile.showThumbnailGeneration);
  if (m_thumbnailIntervalRow)
    m_thumbnailIntervalRow->setVisible(
        profile.showThumbnailInterval &&
        m_thumbnailScheduleCombo &&
        !m_thumbnailScheduleCombo->currentData().toString().isEmpty());
  if (m_saveThumbnailSetsRow)
    m_saveThumbnailSetsRow->setVisible(
        profile.showSaveThumbnailSets && m_thumbnailScheduleCombo &&
        !m_thumbnailScheduleCombo->currentData().toString().isEmpty());
  if (m_generateChaptersRow)
    m_generateChaptersRow->setVisible(profile.showAutoGenerateChapters);
  if (m_chapterIntervalRow)
    m_chapterIntervalRow->setVisible(
        profile.showAutoGenerateChapterInterval &&
        m_chkGenerateChapters &&
        m_chkGenerateChapters->isChecked());
  if (m_introDetectionRow)
    m_introDetectionRow->setVisible(profile.showIntroDetection);

  if (m_imageFetchersLabel) {
    m_imageFetchersLabel->setText(
        isTvShowLibrary ? tr("Series image fetchers:")
                        : tr("Image fetchers:"));
  }
  const bool hasImageFetchers =
      updateFetcherVisibility(m_imageFetcherRows, LTU::FetcherKind::Image);
  const bool hasSeasonImageFetchers =
      updateFetcherVisibility(m_seasonImageFetcherRows, LTU::FetcherKind::Image);
  const bool hasEpisodeImageFetchers = updateFetcherVisibility(
      m_episodeImageFetcherRows, LTU::FetcherKind::Image);
  const bool showImageFetchersPanel =
      profile.showImageFetchers && hasImageFetchers;
  const bool showSeasonImageFetchersPanel =
      profile.showImageFetchers && isTvShowLibrary && hasSeasonImageFetchers;
  const bool showEpisodeImageFetchersPanel =
      profile.showImageFetchers && isTvShowLibrary && hasEpisodeImageFetchers;
  if (m_imageFetchersPanel)
    m_imageFetchersPanel->setVisible(showImageFetchersPanel);
  if (m_imageFetchersLabel)
    m_imageFetchersLabel->setVisible(showImageFetchersPanel);
  if (m_seasonImageFetchersPanel)
    m_seasonImageFetchersPanel->setVisible(showSeasonImageFetchersPanel);
  if (m_seasonImageFetchersLabel)
    m_seasonImageFetchersLabel->setVisible(showSeasonImageFetchersPanel);
  if (m_episodeImageFetchersPanel)
    m_episodeImageFetchersPanel->setVisible(showEpisodeImageFetchersPanel);
  if (m_episodeImageFetchersLabel)
    m_episodeImageFetchersLabel->setVisible(showEpisodeImageFetchersPanel);
  const bool hasImageOptions = isVisible(m_saveArtworkRow) ||
                               isVisible(m_cacheImagesRow) ||
                               isVisible(m_saveMetadataHiddenRow) ||
                               isVisible(m_downloadImagesRow) ||
                               isVisible(m_extractChapterImagesRow) ||
                               isVisible(m_thumbnailScheduleRow) ||
                               isVisible(m_thumbnailIntervalRow) ||
                               isVisible(m_saveThumbnailSetsRow) ||
                               isVisible(m_generateChaptersRow) ||
                               isVisible(m_chapterIntervalRow) ||
                               isVisible(m_introDetectionRow);
  const bool hasImageFetcherPanels =
      isVisible(m_imageFetchersPanel) || isVisible(m_seasonImageFetchersPanel) ||
      isVisible(m_episodeImageFetchersPanel);
  if (m_imageOptionsSeparator)
    m_imageOptionsSeparator->setVisible(hasImageOptions &&
                                        hasImageFetcherPanels);

  if (m_subtitleSection)
    m_subtitleSection->setVisible(profile.showSubtitleSection);
  const bool hasSubtitleDownloaders =
      updateFetcherVisibility(m_subtitleDownloaderRows,
                              LTU::FetcherKind::Subtitle);
  const bool showSubtitleDownloadersPanel =
      profile.showSubtitleSection && hasSubtitleDownloaders;
  if (m_subtitleDownloadersPanel)
    m_subtitleDownloadersPanel->setVisible(showSubtitleDownloadersPanel);
  if (m_subtitleDownloadersLabel)
    m_subtitleDownloadersLabel->setVisible(showSubtitleDownloadersPanel);
  if (m_subtitleDownloadersSeparator)
    m_subtitleDownloadersSeparator->setVisible(showSubtitleDownloadersPanel);
  const bool hasLyricsFetchers =
      updateFetcherVisibility(m_lyricsFetcherRows, LTU::FetcherKind::Lyrics);
  const bool showLyricsFetchersPanel =
      profile.showLyricsSection && hasLyricsFetchers;
  if (m_lyricsFetchersPanel)
    m_lyricsFetchersPanel->setVisible(showLyricsFetchersPanel);
  if (m_lyricsFetchersLabel)
    m_lyricsFetchersLabel->setVisible(showLyricsFetchersPanel);
  if (m_lyricsFetchersSeparator)
    m_lyricsFetchersSeparator->setVisible(showLyricsFetchersPanel);
  if (m_lyricsSection)
    m_lyricsSection->setVisible(profile.showLyricsSection);

  if (m_imageSection) {
    const bool showImageSection =
        profile.showImageSection &&
        (profile.showSaveArtwork || profile.showCacheImages ||
         profile.showSaveMetadataHidden ||
         profile.showDownloadImagesInAdvance || profile.showChapterImages ||
         profile.showThumbnailGeneration || profile.showThumbnailInterval ||
         profile.showSaveThumbnailSets || profile.showAutoGenerateChapters ||
         profile.showAutoGenerateChapterInterval || profile.showIntroDetection ||
         (profile.showImageFetchers &&
          (hasImageFetchers || hasSeasonImageFetchers ||
           hasEpisodeImageFetchers)));
    m_imageSection->setVisible(showImageSection);
  }

  if (m_playbackSection)
    m_playbackSection->setVisible(profile.showPlaybackSection);

  if (m_minCollSizeCombo) {
    const bool enableMinCollectionSize =
        profile.showMovieCollectionSettings &&
        ((serverType == ServerProfile::Emby &&
          m_chkImportCollectionInfo &&
          m_chkImportCollectionInfo->isChecked()) ||
         (serverType != ServerProfile::Emby && m_chkAutoCollection &&
          m_chkAutoCollection->isChecked()));
    m_minCollSizeCombo->setEnabled(enableMinCollectionSize);
  }

  updateArrowStates(m_metadataFetcherRows);
  updateArrowStates(m_seasonMetadataFetcherRows);
  updateArrowStates(m_episodeMetadataFetcherRows);
  updateArrowStates(m_imageFetcherRows);
  updateArrowStates(m_seasonImageFetcherRows);
  updateArrowStates(m_episodeImageFetcherRows);
  updateArrowStates(m_subtitleDownloaderRows);
  updateArrowStates(m_lyricsFetcherRows);
}




PathRowWidget *LibraryCreateDialog::addPathRow(const QString &path) {
  if (!m_pathRowsLayout) {
    qWarning() << "[LibraryCreateDialog] addPathRow skipped:"
               << "path rows layout is null";
    return nullptr;
  }

  auto *row = new PathRowWidget(this);
  row->setPath(path);

  connect(row, &PathRowWidget::browseClicked, this,
          [this, row]() { onBrowsePath(row); });
  connect(row, &PathRowWidget::removeClicked, this,
          [this, row]() { removePathRow(row); });

  m_pathRows.append(row);
  m_pathRowsLayout->addWidget(row);
  return row;
}

void LibraryCreateDialog::removePathRow(PathRowWidget *row) {
  if (m_pathRows.size() <= 1) {
    
    row->setPath("");
    return;
  }
  m_pathRows.removeOne(row);
  m_pathRowsLayout->removeWidget(row);
  row->deleteLater();
}

void LibraryCreateDialog::onBrowsePath(PathRowWidget *row) {
  ServerBrowserDialog dlg(m_core, this);
  if (dlg.exec() == QDialog::Accepted) {
    QString selected = dlg.selectedPath();
    if (!selected.isEmpty()) {
      row->setPath(selected);

      
      if (row == m_pathRows.last()) {
        addPathRow();
      }
    }
  }
}

QString LibraryCreateDialog::libraryName() const {
  return m_nameEdit ? m_nameEdit->text().trimmed() : QString();
}

QString LibraryCreateDialog::collectionType() const {
  return normalizedCollectionType();
}

QStringList LibraryCreateDialog::paths() const {
  QStringList result;
  for (auto *row : m_pathRows) {
    if (!row->isEmpty()) {
      result.append(row->path());
    }
  }
  return result;
}

QJsonObject LibraryCreateDialog::libraryOptions() const {
  const auto serverType = m_core->serverManager()->activeProfile().type;
  const QString type = collectionType();
  const auto profile = LTU::buildLibraryTypeProfile(serverType, type);
  const LibraryCreateDialogOptionsBuilder builder(*this, profile, serverType);
  const QJsonObject currentOptions =
      LOS::serializeLibraryOptions(profile, builder.build(type), serverType);
  const QJsonObject mergedOptions = mergedLibraryOptions(currentOptions);

  qDebug() << "[LibraryCreateDialog] libraryOptions generated"
           << "| current:"
           << QString::fromUtf8(
                  QJsonDocument(currentOptions).toJson(QJsonDocument::Compact))
           << "| merged:"
           << QString::fromUtf8(
                  QJsonDocument(mergedOptions).toJson(QJsonDocument::Compact));

  return mergedOptions;
}




void LibraryCreateDialog::addFetcherRows(QVBoxLayout *container,
                                         const QStringList &names,
                                         QList<FetcherRowWidget *> &rows) {
  for (const QString &name : names) {
    auto *row = new FetcherRowWidget(name, this);
    container->addWidget(row);
    rows.append(row);

    connect(row, &FetcherRowWidget::moveUpClicked, this,
            [this, container, &rows, row]() {
              int idx = rows.indexOf(row);
              int target = adjacentApplicableFetcherIndex(rows, idx, -1);
              if (idx > 0 && target >= 0)
                swapFetcherRow(container, rows, idx, target);
            });
    connect(row, &FetcherRowWidget::moveDownClicked, this,
            [this, container, &rows, row]() {
              int idx = rows.indexOf(row);
              int target = adjacentApplicableFetcherIndex(rows, idx, 1);
              if (idx >= 0 && target >= 0)
                swapFetcherRow(container, rows, idx, target);
            });
  }
  updateArrowStates(rows);
}

void LibraryCreateDialog::swapFetcherRow(QVBoxLayout *container,
                                         QList<FetcherRowWidget *> &rows,
                                         int from, int to) {
  if (from == to)
    return;

  
  rows.swapItemsAt(from, to);

  
  for (int i = 0; i < rows.size(); ++i) {
    container->removeWidget(rows[i]);
  }
  for (int i = 0; i < rows.size(); ++i) {
    container->addWidget(rows[i]);
  }

  updateArrowStates(rows);
}

void LibraryCreateDialog::updateArrowStates(
    const QList<FetcherRowWidget *> &rows) {
  for (int i = 0; i < rows.size(); ++i) {
    auto *row = rows[i];
    if (!row)
      continue;

    const bool rowVisible = !row->isHidden();
    row->setUpEnabled(rowVisible &&
                      adjacentApplicableFetcherIndex(rows, i, -1) >= 0);
    row->setDownEnabled(rowVisible &&
                        adjacentApplicableFetcherIndex(rows, i, 1) >= 0);
  }
}
