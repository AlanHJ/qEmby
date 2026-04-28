#include "librarymetadataeditdialog.h"

#include <QCheckBox>
#include <QDateTime>
#include <QDoubleValidator>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QScrollArea>
#include <QTimeZone>
#include <QVBoxLayout>
#include <initializer_list>
#include <utility>

namespace {

constexpr qint64 TicksPerMinute = 600000000LL;

QStringList deduplicateValues(const QStringList& values)
{
    QStringList result;
    for (const QString& value : values) {
        const QString trimmed = value.trimmed();
        if (!trimmed.isEmpty() && !result.contains(trimmed)) {
            result.append(trimmed);
        }
    }
    return result;
}

QStringList splitDelimitedValues(QString text)
{
    text.replace(QChar('\r'), QChar('\n'));
    const QStringList parts =
        text.split(QRegularExpression(QStringLiteral("[,;\\n\\x{FF0C}\\x{FF1B}]")),
                   Qt::SkipEmptyParts);
    return deduplicateValues(parts);
}

QJsonArray stringArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : deduplicateValues(values)) {
        array.append(value);
    }
    return array;
}

QJsonArray namedObjectArray(const QStringList& values)
{
    QJsonArray array;
    for (const QString& value : deduplicateValues(values)) {
        QJsonObject object;
        object.insert(QStringLiteral("Name"), value);
        array.append(object);
    }
    return array;
}

QStringList valuesFromValue(const QJsonValue& value)
{
    QStringList values;
    auto appendValue = [&values](QString text) {
        text = text.trimmed();
        if (!text.isEmpty() && !values.contains(text)) {
            values.append(text);
        }
    };

    if (value.isArray()) {
        for (const QJsonValue& entry : value.toArray()) {
            if (entry.isObject()) {
                appendValue(
                    entry.toObject().value(QStringLiteral("Name")).toString());
            } else {
                appendValue(entry.toVariant().toString());
            }
        }
        return values;
    }

    if (value.isObject()) {
        appendValue(value.toObject().value(QStringLiteral("Name")).toString());
        return values;
    }

    appendValue(value.toVariant().toString());
    return values;
}

QString firstNonEmptyValue(const QJsonValue& value)
{
    const QStringList values = valuesFromValue(value);
    return values.isEmpty() ? QString() : values.first();
}

QString compactNumberText(double value, int precision)
{
    QString text = QString::number(value, 'f', precision);
    while (text.contains(QChar('.')) &&
           (text.endsWith(QChar('0')) || text.endsWith(QChar('.')))) {
        if (text.endsWith(QChar('.'))) {
            text.chop(1);
            break;
        }
        text.chop(1);
    }
    return text;
}

QString numberText(const QJsonValue& value)
{
    if (value.isDouble()) {
        return compactNumberText(value.toDouble(), 2);
    }

    return value.toVariant().toString().trimmed();
}

QString minutesTextFromTicks(const QJsonValue& value)
{
    const qint64 ticks = value.toVariant().toLongLong();
    if (ticks <= 0) {
        return {};
    }

    if (ticks % TicksPerMinute == 0) {
        return QString::number(ticks / TicksPerMinute);
    }

    return compactNumberText(
        static_cast<double>(ticks) / static_cast<double>(TicksPerMinute), 1);
}

QString dateOnlyText(QString rawValue)
{
    rawValue = rawValue.trimmed();
    const int separatorIndex = rawValue.indexOf(QChar('T'));
    if (separatorIndex > 0) {
        return rawValue.left(separatorIndex);
    }
    return rawValue;
}

QDateTime parseServerDateTime(QString rawValue)
{
    rawValue = rawValue.trimmed();
    if (rawValue.isEmpty()) {
        return {};
    }

    QDateTime parsed = QDateTime::fromString(rawValue, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(rawValue, Qt::ISODate);
    }
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(
            rawValue, QStringLiteral("yyyy-MM-ddTHH:mm:ss.zzzzzzzZ"));
        if (parsed.isValid()) {
            parsed.setTimeZone(QTimeZone::UTC);
        }
    }

    return parsed;
}

QDateTime parseEditableLocalDateTime(QString value)
{
    value = value.trimmed();
    if (value.isEmpty()) {
        return {};
    }

    static const QStringList formats = {
        QStringLiteral("yyyy-MM-dd HH:mm:ss"),
        QStringLiteral("yyyy-MM-dd HH:mm"),
        QStringLiteral("yyyy-MM-ddTHH:mm:ss"),
        QStringLiteral("yyyy-MM-ddTHH:mm")
    };

    for (const QString& format : formats) {
        const QDateTime parsed = QDateTime::fromString(value, format);
        if (parsed.isValid()) {
            return parsed;
        }
    }

    QDateTime parsed = QDateTime::fromString(value, Qt::ISODateWithMs);
    if (!parsed.isValid()) {
        parsed = QDateTime::fromString(value, Qt::ISODate);
    }
    return parsed;
}

QString localDateTimeDisplayText(QString rawValue)
{
    const QDateTime parsed = parseServerDateTime(rawValue);
    if (!parsed.isValid()) {
        return rawValue.trimmed();
    }

    return parsed.toLocalTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
}

QString normalizedDateCreated(QString dateText)
{
    dateText = dateText.trimmed();
    if (dateText.isEmpty()) {
        return {};
    }

    const QDateTime parsed = parseEditableLocalDateTime(dateText);
    if (!parsed.isValid()) {
        return dateText;
    }

    return parsed.toUTC().toString(Qt::ISODateWithMs);
}

QString normalizedDateWithOriginalTime(const QJsonObject& itemData,
                                       const QString& key, QString dateText)
{
    dateText = dateText.trimmed();
    if (dateText.isEmpty()) {
        return {};
    }

    const QString rawValue = itemData.value(key).toString().trimmed();
    const int separatorIndex = rawValue.indexOf(QChar('T'));
    if (separatorIndex > 0) {
        return dateText + rawValue.mid(separatorIndex);
    }

    return dateText;
}

QWidget* createScrollablePage(QVBoxLayout*& innerLayout, QWidget* parent)
{
    auto* scrollArea = new QScrollArea(parent);
    scrollArea->setObjectName(QStringLiteral("LibCreateScrollArea"));
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->viewport()->setAutoFillBackground(false);

    auto* container = new QWidget(scrollArea);
    container->setAttribute(Qt::WA_StyledBackground, true);

    innerLayout = new QVBoxLayout(container);
    innerLayout->setContentsMargins(0, 0, 12, 0);
    innerLayout->setSpacing(8);
    innerLayout->setAlignment(Qt::AlignTop);

    scrollArea->setWidget(container);
    return scrollArea;
}

CollapsibleSection* createSectionCard(const QString& title, QWidget* parent,
                                      bool expanded = true)
{
    auto* section = new CollapsibleSection(title, parent);
    section->setExpanded(expanded);
    section->contentLayout()->setSpacing(10);
    return section;
}

bool isItemType(QString itemType, std::initializer_list<const char*> candidates)
{
    itemType = itemType.trimmed();
    for (const char* candidate : candidates) {
        if (itemType.compare(QLatin1String(candidate), Qt::CaseInsensitive) ==
            0) {
            return true;
        }
    }
    return false;
}

bool containsAnyKey(const QJsonObject& object,
                    std::initializer_list<const char*> keys)
{
    for (const char* key : keys) {
        if (object.contains(QLatin1String(key))) {
            return true;
        }
    }
    return false;
}

} 

LibraryMetadataEditDialog::LibraryMetadataEditDialog(QJsonObject itemData,
                                                     QWidget* parent)
    : ModernDialogBase(parent), m_itemData(std::move(itemData))
{
    resize(820, 760);
    setMinimumWidth(760);
    setMaximumHeight(820);
    setTitle(tr("Edit Metadata"));

    auto* outerLayout = contentLayout();
    outerLayout->setContentsMargins(20, 10, 0, 20);

    QVBoxLayout* pageLayout = nullptr;
    outerLayout->addWidget(createScrollablePage(pageLayout, this), 1);
    outerLayout->addSpacing(12);

    auto* basicSection = createSectionCard(tr("Basic Information"), this, true);
    pageLayout->addWidget(basicSection);
    auto* basicForm = createSectionForm(basicSection);

    int basicRow = 0;
    const QString pathText = displayPath();
    if (!pathText.isEmpty()) {
        m_pathEdit = createLineEdit({}, true);
        m_pathEdit->setToolTip(pathText);
        addFormRow(basicForm, basicRow++, tr("Path"), m_pathEdit);
    }

    m_nameEdit = createLineEdit();
    addFormRow(basicForm, basicRow++, tr("Name"), m_nameEdit);

    m_originalTitleEdit = createLineEdit();
    addFormRow(basicForm, basicRow++, tr("Original Title"),
               m_originalTitleEdit);

    m_sortNameEdit = createLineEdit();
    addFormRow(basicForm, basicRow++, tr("Sort Name"), m_sortNameEdit);

    m_taglineEdit = createLineEdit();
    addFormRow(basicForm, basicRow++, tr("Tagline"), m_taglineEdit);

    m_overviewEdit = new QPlainTextEdit(this);
    m_overviewEdit->setObjectName(QStringLiteral("LibraryMetadataOverviewEdit"));
    m_overviewEdit->setMinimumHeight(170);
    addFormRow(basicForm, basicRow++, tr("Overview"), m_overviewEdit,
               Qt::AlignLeft | Qt::AlignTop);

    auto* classificationSection =
        createSectionCard(tr("Classification"), this, true);
    pageLayout->addWidget(classificationSection);
    auto* classificationForm = createSectionForm(classificationSection);

    m_genresEdit = createLineEdit(tr("Comma separated values"));
    addFormRow(classificationForm, 0, tr("Genres"), m_genresEdit);

    m_tagsEdit = createLineEdit(tr("Comma separated values"));
    addFormRow(classificationForm, 1, tr("Tags"), m_tagsEdit);

    m_studiosEdit = createLineEdit(tr("Comma separated values"));
    addFormRow(classificationForm, 2, tr("Studios"), m_studiosEdit);

    auto* datesSection =
        createSectionCard(tr("Ratings & Release"), this, true);
    pageLayout->addWidget(datesSection);
    auto* datesForm = createSectionForm(datesSection);

    int datesRow = 0;

    m_dateAddedEdit = createLineEdit(tr("yyyy-MM-dd HH:mm:ss"));
    addFormRow(datesForm, datesRow++, tr("Date Added"), m_dateAddedEdit);

    m_productionYearEdit = createLineEdit();
    m_productionYearEdit->setValidator(new QIntValidator(0, 9999, this));
    addFormRow(datesForm, datesRow++, tr("Production Year"),
               m_productionYearEdit);

    m_premiereDateEdit = createLineEdit(tr("yyyy-MM-dd"));
    addFormRow(datesForm, datesRow++, tr("Premiere Date"), m_premiereDateEdit);

    if (shouldShowSeriesMetadata()) {
        m_endDateEdit = createLineEdit(tr("yyyy-MM-dd"));
        addFormRow(datesForm, datesRow++, tr("End Date"), m_endDateEdit);

        m_statusEdit = createLineEdit(tr("Continuing / Ended"));
        addFormRow(datesForm, datesRow++, tr("Status"), m_statusEdit);
    }

    if (shouldShowRuntimeMetadata()) {
        m_runtimeMinutesEdit = createLineEdit(tr("Minutes"));
        auto* runtimeValidator = new QDoubleValidator(0.0, 100000.0, 1, this);
        runtimeValidator->setNotation(QDoubleValidator::StandardNotation);
        m_runtimeMinutesEdit->setValidator(runtimeValidator);
        addFormRow(datesForm, datesRow++, tr("Runtime (Minutes)"),
                   m_runtimeMinutesEdit);
    }

    m_officialRatingEdit = createLineEdit();
    addFormRow(datesForm, datesRow++, tr("Official Rating"),
               m_officialRatingEdit);

    m_customRatingEdit = createLineEdit();
    addFormRow(datesForm, datesRow++, tr("Custom Rating"), m_customRatingEdit);

    m_communityRatingEdit = createLineEdit(tr("0-10"));
    auto* communityValidator = new QDoubleValidator(0.0, 10.0, 2, this);
    communityValidator->setNotation(QDoubleValidator::StandardNotation);
    m_communityRatingEdit->setValidator(communityValidator);
    addFormRow(datesForm, datesRow++, tr("Community Rating"),
               m_communityRatingEdit);

    m_criticRatingEdit = createLineEdit(tr("0-100"));
    auto* criticValidator = new QDoubleValidator(0.0, 100.0, 1, this);
    criticValidator->setNotation(QDoubleValidator::StandardNotation);
    m_criticRatingEdit->setValidator(criticValidator);
    addFormRow(datesForm, datesRow++, tr("Critic Rating"),
               m_criticRatingEdit);

    auto* externalIdsSection =
        createSectionCard(tr("External IDs"), this, false);
    pageLayout->addWidget(externalIdsSection);
    auto* externalIdsForm = createSectionForm(externalIdsSection);

    m_imdbIdEdit = createLineEdit();
    addFormRow(externalIdsForm, 0, tr("IMDb Id"), m_imdbIdEdit);

    m_movieDbIdEdit = createLineEdit();
    addFormRow(externalIdsForm, 1, tr("MovieDb Id"), m_movieDbIdEdit);

    m_tvdbIdEdit = createLineEdit();
    addFormRow(externalIdsForm, 2, tr("TheTVDB Id"), m_tvdbIdEdit);

    if (shouldShowMusicMetadata()) {
        auto* musicSection = createSectionCard(tr("Music Metadata"), this,
                                               false);
        pageLayout->addWidget(musicSection);
        auto* musicForm = createSectionForm(musicSection);

        int musicRow = 0;

        m_albumEdit = createLineEdit();
        addFormRow(musicForm, musicRow++, tr("Album"), m_albumEdit);

        m_albumArtistEdit = createLineEdit();
        addFormRow(musicForm, musicRow++, tr("Album Artist"),
                   m_albumArtistEdit);

        m_artistsEdit = createLineEdit(tr("Comma separated values"));
        addFormRow(musicForm, musicRow++, tr("Artists"), m_artistsEdit);

        m_composersEdit = createLineEdit(tr("Comma separated values"));
        addFormRow(musicForm, musicRow++, tr("Composers"), m_composersEdit);
    }

    const bool hasAdditionalSection = shouldShowChannelMetadata() ||
                                      shouldShowPersonMetadata() ||
                                      shouldShowIndexMetadata();
    if (hasAdditionalSection) {
        auto* additionalSection =
            createSectionCard(tr("Additional Details"), this, false);
        pageLayout->addWidget(additionalSection);
        auto* additionalForm = createSectionForm(additionalSection);

        int additionalRow = 0;

        if (shouldShowChannelMetadata()) {
            m_channelNumberEdit = createLineEdit();
            addFormRow(additionalForm, additionalRow++, tr("Channel Number"),
                       m_channelNumberEdit);
        }

        if (shouldShowPersonMetadata()) {
            m_placeOfBirthEdit = createLineEdit();
            addFormRow(additionalForm, additionalRow++, tr("Place of Birth"),
                       m_placeOfBirthEdit);
        }

        if (shouldShowIndexMetadata()) {
            m_parentIndexNumberEdit = createLineEdit();
            m_parentIndexNumberEdit->setValidator(
                new QIntValidator(0, 99999, this));
            addFormRow(additionalForm, additionalRow++,
                       tr("Parent Index Number"), m_parentIndexNumberEdit);

            m_indexNumberEdit = createLineEdit();
            m_indexNumberEdit->setValidator(
                new QIntValidator(0, 99999, this));
            addFormRow(additionalForm, additionalRow++, tr("Index Number"),
                       m_indexNumberEdit);
        }
    }

    auto* settingsSection =
        createSectionCard(tr("Metadata Settings"), this, false);
    pageLayout->addWidget(settingsSection);
    auto* settingsForm = createSectionForm(settingsSection);

    m_preferredMetadataLanguageEdit = createLineEdit(tr("en-US"));
    addFormRow(settingsForm, 0, tr("Preferred Metadata Language"),
               m_preferredMetadataLanguageEdit);

    m_metadataCountryCodeEdit = createLineEdit(tr("US"));
    addFormRow(settingsForm, 1, tr("Metadata Country Code"),
               m_metadataCountryCodeEdit);

    m_lockDataCheck = new QCheckBox(tr("Lock metadata"), this);
    m_lockDataCheck->setObjectName(QStringLiteral("LibraryMetadataLockCheck"));
    settingsSection->contentLayout()->addWidget(m_lockDataCheck);

    auto* lockedFieldsSection = createSectionCard(tr("Field Locks"), this,
                                                  false);
    pageLayout->addWidget(lockedFieldsSection);
    auto* lockedFieldsLayout = lockedFieldsSection->contentLayout();

    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Name"), tr("Name"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("OriginalTitle"),
                         tr("Original Title"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("SortName"),
                         tr("Sort Name"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Tagline"),
                         tr("Tagline"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Overview"),
                         tr("Overview"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Genres"),
                         tr("Genres"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Tags"),
                         tr("Tags"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Studios"),
                         tr("Studios"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("PremiereDate"),
                         tr("Premiere Date"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("ProductionYear"),
                         tr("Production Year"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("OfficialRating"),
                         tr("Official Rating"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("CustomRating"),
                         tr("Custom Rating"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("CommunityRating"),
                         tr("Community Rating"));
    addLockedFieldOption(lockedFieldsLayout, QStringLiteral("CriticRating"),
                         tr("Critic Rating"));

    if (m_statusEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Status"),
                             tr("Status"));
    }
    if (m_endDateEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("EndDate"),
                             tr("End Date"));
    }
    if (m_runtimeMinutesEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("RunTimeTicks"),
                             tr("Runtime (Minutes)"));
    }
    if (m_albumEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Album"),
                             tr("Album"));
    }
    if (m_albumArtistEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("AlbumArtist"),
                             tr("Album Artist"));
    }
    if (m_artistsEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Artists"),
                             tr("Artists"));
    }
    if (m_composersEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("Composers"),
                             tr("Composers"));
    }
    if (m_channelNumberEdit) {
        addLockedFieldOption(lockedFieldsLayout,
                             QStringLiteral("ChannelNumber"),
                             tr("Channel Number"));
    }
    if (m_placeOfBirthEdit) {
        addLockedFieldOption(lockedFieldsLayout,
                             QStringLiteral("PlaceOfBirth"),
                             tr("Place of Birth"));
    }
    if (m_parentIndexNumberEdit) {
        addLockedFieldOption(lockedFieldsLayout,
                             QStringLiteral("ParentIndexNumber"),
                             tr("Parent Index Number"));
    }
    if (m_indexNumberEdit) {
        addLockedFieldOption(lockedFieldsLayout, QStringLiteral("IndexNumber"),
                             tr("Index Number"));
    }

    pageLayout->addStretch(1);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 0, 12, 0);
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName(QStringLiteral("dialog-btn-cancel"));
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_saveButton = new QPushButton(tr("Save"), this);
    m_saveButton->setObjectName(QStringLiteral("dialog-btn-primary"));
    m_saveButton->setCursor(Qt::PointingHandCursor);
    connect(m_saveButton, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(m_saveButton);

    outerLayout->addLayout(buttonLayout);

    connect(m_nameEdit, &QLineEdit::textChanged, this,
            [this]() { updateSaveButtonState(); });
    connect(m_nameEdit, &QLineEdit::returnPressed, this, [this]() {
        if (m_saveButton && m_saveButton->isEnabled()) {
            accept();
        }
    });

    populateForm();
    updateSaveButtonState();
}

QLineEdit* LibraryMetadataEditDialog::createLineEdit(QString placeholderText,
                                                     bool readOnly) const
{
    auto* edit = new QLineEdit(const_cast<LibraryMetadataEditDialog*>(this));
    edit->setObjectName(QStringLiteral("LibraryMetadataLineEdit"));
    edit->setReadOnly(readOnly);
    edit->setClearButtonEnabled(!readOnly);
    if (!placeholderText.trimmed().isEmpty()) {
        edit->setPlaceholderText(placeholderText.trimmed());
    }
    return edit;
}

QGridLayout* LibraryMetadataEditDialog::createSectionForm(
    CollapsibleSection* section) const
{
    auto* formWidget = new QWidget(section);
    auto* formLayout = new QGridLayout(formWidget);
    formLayout->setContentsMargins(0, 0, 0, 0);
    formLayout->setHorizontalSpacing(14);
    formLayout->setVerticalSpacing(10);
    formLayout->setColumnStretch(1, 1);
    section->contentLayout()->addWidget(formWidget);
    return formLayout;
}

QLabel* LibraryMetadataEditDialog::addFormRow(
    QGridLayout* formLayout, int row, QString labelText, QWidget* field,
    Qt::Alignment labelAlignment) const
{
    auto* label = new QLabel(std::move(labelText),
                             const_cast<LibraryMetadataEditDialog*>(this));
    label->setObjectName(QStringLiteral("LibraryMetadataFieldLabel"));
    label->setAlignment(labelAlignment);
    formLayout->addWidget(label, row, 0);
    formLayout->addWidget(field, row, 1);
    return label;
}

void LibraryMetadataEditDialog::addLockedFieldOption(QVBoxLayout* layout,
                                                     QString key,
                                                     QString labelText)
{
    auto* checkBox = new QCheckBox(labelText, this);
    checkBox->setObjectName(QStringLiteral("LibraryMetadataLockCheck"));
    layout->addWidget(checkBox);

    LockedFieldOption option;
    option.key = std::move(key);
    option.label = std::move(labelText);
    option.checkBox = checkBox;
    m_lockedFieldOptions.append(option);
}

QJsonObject LibraryMetadataEditDialog::updatedItemData() const
{
    QJsonObject itemData = m_itemData;

    itemData.insert(QStringLiteral("Name"),
                    m_nameEdit ? m_nameEdit->text().trimmed() : QString());
    itemData.insert(QStringLiteral("OriginalTitle"),
                    m_originalTitleEdit ? m_originalTitleEdit->text().trimmed()
                                        : QString());
    itemData.insert(QStringLiteral("SortName"),
                    m_sortNameEdit ? m_sortNameEdit->text().trimmed()
                                   : QString());
    if (itemData.contains(QStringLiteral("ForcedSortName"))) {
        itemData.insert(QStringLiteral("ForcedSortName"),
                        m_sortNameEdit ? m_sortNameEdit->text().trimmed()
                                       : QString());
    }

    itemData.insert(QStringLiteral("Overview"),
                    m_overviewEdit ? m_overviewEdit->toPlainText().trimmed()
                                   : QString());

    const QString tagline = currentTagline();
    itemData.insert(QStringLiteral("Taglines"),
                    tagline.isEmpty() ? QJsonArray()
                                      : QJsonArray {tagline});
    if (itemData.contains(QStringLiteral("Tagline")) || !tagline.isEmpty()) {
        itemData.insert(QStringLiteral("Tagline"), tagline);
    }

    const QStringList genres = multiValueList(m_genresEdit);
    itemData.insert(QStringLiteral("Genres"), stringArray(genres));
    itemData.insert(QStringLiteral("GenreItems"), namedObjectArray(genres));

    const QStringList tags = multiValueList(m_tagsEdit);
    itemData.insert(QStringLiteral("Tags"), stringArray(tags));
    itemData.insert(QStringLiteral("TagItems"), namedObjectArray(tags));

    const QStringList studios = multiValueList(m_studiosEdit);
    itemData.insert(QStringLiteral("Studios"), namedObjectArray(studios));

    const QString dateAdded =
        m_dateAddedEdit ? m_dateAddedEdit->text().trimmed() : QString();
    if (dateAdded.isEmpty()) {
        itemData.remove(QStringLiteral("DateCreated"));
    } else {
        itemData.insert(QStringLiteral("DateCreated"),
                        normalizedDateCreated(dateAdded));
    }

    const QString productionYearText =
        m_productionYearEdit ? m_productionYearEdit->text().trimmed()
                             : QString();
    if (productionYearText.isEmpty()) {
        itemData.remove(QStringLiteral("ProductionYear"));
    } else {
        itemData.insert(QStringLiteral("ProductionYear"),
                        productionYearText.toInt());
    }

    const QString premiereDate = normalizedPremiereDate(
        m_premiereDateEdit ? m_premiereDateEdit->text() : QString());
    if (premiereDate.isEmpty()) {
        itemData.remove(QStringLiteral("PremiereDate"));
    } else {
        itemData.insert(QStringLiteral("PremiereDate"), premiereDate);
    }

    if (m_endDateEdit) {
        const QString endDate = normalizedDateWithOriginalTime(
            m_itemData, QStringLiteral("EndDate"), m_endDateEdit->text());
        if (endDate.isEmpty()) {
            itemData.remove(QStringLiteral("EndDate"));
        } else {
            itemData.insert(QStringLiteral("EndDate"), endDate);
        }
    }

    if (m_statusEdit) {
        const QString status = m_statusEdit->text().trimmed();
        if (status.isEmpty()) {
            itemData.remove(QStringLiteral("Status"));
        } else {
            itemData.insert(QStringLiteral("Status"), status);
        }
    }

    if (m_runtimeMinutesEdit) {
        const QString runtimeText = m_runtimeMinutesEdit->text().trimmed();
        if (runtimeText.isEmpty()) {
            itemData.remove(QStringLiteral("RunTimeTicks"));
        } else {
            const double runtimeMinutes = runtimeText.toDouble();
            itemData.insert(QStringLiteral("RunTimeTicks"),
                            static_cast<qint64>(runtimeMinutes *
                                                static_cast<double>(
                                                    TicksPerMinute)));
        }
    }

    itemData.insert(QStringLiteral("OfficialRating"),
                    m_officialRatingEdit
                        ? m_officialRatingEdit->text().trimmed()
                        : QString());

    if (m_customRatingEdit) {
        const QString customRating = m_customRatingEdit->text().trimmed();
        if (customRating.isEmpty()) {
            itemData.remove(QStringLiteral("CustomRating"));
        } else {
            itemData.insert(QStringLiteral("CustomRating"), customRating);
        }
    }

    const QString communityRatingText =
        m_communityRatingEdit ? m_communityRatingEdit->text().trimmed()
                              : QString();
    if (communityRatingText.isEmpty()) {
        itemData.remove(QStringLiteral("CommunityRating"));
    } else {
        itemData.insert(QStringLiteral("CommunityRating"),
                        communityRatingText.toDouble());
    }

    const QString criticRatingText =
        m_criticRatingEdit ? m_criticRatingEdit->text().trimmed() : QString();
    if (criticRatingText.isEmpty()) {
        itemData.remove(QStringLiteral("CriticRating"));
    } else {
        itemData.insert(QStringLiteral("CriticRating"),
                        criticRatingText.toDouble());
    }

    QJsonObject providerIds =
        itemData.value(QStringLiteral("ProviderIds")).toObject();
    setProviderIdValue(providerIds, {QStringLiteral("Imdb")},
                       m_imdbIdEdit ? m_imdbIdEdit->text() : QString());
    setProviderIdValue(providerIds,
                       {QStringLiteral("Tmdb"), QStringLiteral("TheMovieDb")},
                       m_movieDbIdEdit ? m_movieDbIdEdit->text() : QString());
    setProviderIdValue(providerIds,
                       {QStringLiteral("Tvdb"), QStringLiteral("TheTVDB")},
                       m_tvdbIdEdit ? m_tvdbIdEdit->text() : QString());
    itemData.insert(QStringLiteral("ProviderIds"), providerIds);

    if (m_albumEdit) {
        const QString album = m_albumEdit->text().trimmed();
        if (album.isEmpty()) {
            itemData.remove(QStringLiteral("Album"));
        } else {
            itemData.insert(QStringLiteral("Album"), album);
        }
    }

    if (m_albumArtistEdit) {
        const QString albumArtistName = m_albumArtistEdit->text().trimmed();
        if (albumArtistName.isEmpty()) {
            itemData.remove(QStringLiteral("AlbumArtist"));
        } else {
            QJsonObject albumArtist;
            albumArtist.insert(QStringLiteral("Name"), albumArtistName);
            itemData.insert(QStringLiteral("AlbumArtist"), albumArtist);
        }
    }

    if (m_artistsEdit) {
        itemData.insert(QStringLiteral("Artists"),
                        namedObjectArray(multiValueList(m_artistsEdit)));
    }

    if (m_composersEdit) {
        itemData.insert(QStringLiteral("Composers"),
                        stringArray(multiValueList(m_composersEdit)));
    }

    if (m_channelNumberEdit) {
        const QString channelNumber = m_channelNumberEdit->text().trimmed();
        if (channelNumber.isEmpty()) {
            itemData.remove(QStringLiteral("ChannelNumber"));
        } else {
            itemData.insert(QStringLiteral("ChannelNumber"), channelNumber);
        }
    }

    if (m_placeOfBirthEdit) {
        const QString placeOfBirth = m_placeOfBirthEdit->text().trimmed();
        if (placeOfBirth.isEmpty()) {
            itemData.remove(QStringLiteral("PlaceOfBirth"));
        } else {
            itemData.insert(QStringLiteral("PlaceOfBirth"), placeOfBirth);
        }
    }

    if (m_parentIndexNumberEdit) {
        const QString parentIndexNumber =
            m_parentIndexNumberEdit->text().trimmed();
        if (parentIndexNumber.isEmpty()) {
            itemData.remove(QStringLiteral("ParentIndexNumber"));
        } else {
            itemData.insert(QStringLiteral("ParentIndexNumber"),
                            parentIndexNumber.toInt());
        }
    }

    if (m_indexNumberEdit) {
        const QString indexNumber = m_indexNumberEdit->text().trimmed();
        if (indexNumber.isEmpty()) {
            itemData.remove(QStringLiteral("IndexNumber"));
        } else {
            itemData.insert(QStringLiteral("IndexNumber"), indexNumber.toInt());
        }
    }

    if (m_preferredMetadataLanguageEdit) {
        const QString preferredLanguage =
            m_preferredMetadataLanguageEdit->text().trimmed();
        if (preferredLanguage.isEmpty()) {
            itemData.remove(QStringLiteral("PreferredMetadataLanguage"));
        } else {
            itemData.insert(QStringLiteral("PreferredMetadataLanguage"),
                            preferredLanguage);
        }
    }

    if (m_metadataCountryCodeEdit) {
        const QString countryCode =
            m_metadataCountryCodeEdit->text().trimmed();
        if (countryCode.isEmpty()) {
            itemData.remove(QStringLiteral("MetadataCountryCode"));
        } else {
            itemData.insert(QStringLiteral("MetadataCountryCode"),
                            countryCode);
        }
    }

    itemData.insert(QStringLiteral("LockedFields"),
                    stringArray(lockedFieldKeys()));
    itemData.insert(QStringLiteral("LockData"),
                    m_lockDataCheck && m_lockDataCheck->isChecked());

    return itemData;
}

QString LibraryMetadataEditDialog::providerIdValue(
    const QStringList& keys) const
{
    const QJsonObject providerIds =
        m_itemData.value(QStringLiteral("ProviderIds")).toObject();
    for (const QString& key : keys) {
        const QString value = providerIds.value(key).toString().trimmed();
        if (!value.isEmpty()) {
            return value;
        }
    }

    return {};
}

void LibraryMetadataEditDialog::setProviderIdValue(QJsonObject& providerIds,
                                                   const QStringList& keys,
                                                   QString value) const
{
    value = value.trimmed();
    for (const QString& key : keys) {
        if (key.trimmed().isEmpty()) {
            continue;
        }

        if (value.isEmpty()) {
            providerIds.remove(key);
        } else {
            providerIds.insert(key, value);
        }
    }
}

QStringList LibraryMetadataEditDialog::multiValueList(
    const QLineEdit* edit) const
{
    if (!edit) {
        return {};
    }

    return splitDelimitedValues(edit->text());
}

QStringList LibraryMetadataEditDialog::valuesFromJson(
    const QJsonValue& value) const
{
    return valuesFromValue(value);
}

QString LibraryMetadataEditDialog::joinedValues(const QJsonValue& value) const
{
    return valuesFromJson(value).join(QStringLiteral(", "));
}

QString LibraryMetadataEditDialog::currentTagline() const
{
    return m_taglineEdit ? m_taglineEdit->text().trimmed() : QString();
}

QString LibraryMetadataEditDialog::normalizedPremiereDate(
    QString dateText) const
{
    return normalizedDateWithOriginalTime(
        m_itemData, QStringLiteral("PremiereDate"), std::move(dateText));
}

QString LibraryMetadataEditDialog::displayPath() const
{
    const QString directPath =
        m_itemData.value(QStringLiteral("Path")).toString().trimmed();
    if (!directPath.isEmpty()) {
        return directPath;
    }

    const QJsonArray mediaSources =
        m_itemData.value(QStringLiteral("MediaSources")).toArray();
    for (const QJsonValue& sourceValue : mediaSources) {
        const QString path =
            sourceValue.toObject().value(QStringLiteral("Path")).toString().trimmed();
        if (!path.isEmpty()) {
            return path;
        }
    }

    return {};
}

QStringList LibraryMetadataEditDialog::lockedFieldKeys() const
{
    QStringList keys;
    for (const LockedFieldOption& option : m_lockedFieldOptions) {
        if (option.checkBox && option.checkBox->isChecked() &&
            !option.key.trimmed().isEmpty() && !keys.contains(option.key)) {
            keys.append(option.key);
        }
    }
    return keys;
}

bool LibraryMetadataEditDialog::shouldShowSeriesMetadata() const
{
    const QString itemType =
        m_itemData.value(QStringLiteral("Type")).toString().trimmed();
    return isItemType(itemType, {"Series", "Season", "Episode"}) ||
           containsAnyKey(m_itemData, {"Status", "EndDate"});
}

bool LibraryMetadataEditDialog::shouldShowMusicMetadata() const
{
    const QString itemType =
        m_itemData.value(QStringLiteral("Type")).toString().trimmed();
    return itemType.startsWith(QStringLiteral("Music"),
                               Qt::CaseInsensitive) ||
           isItemType(itemType, {"Audio", "MusicVideo"}) ||
           containsAnyKey(m_itemData,
                          {"Album", "AlbumArtist", "Artists", "Composers"});
}

bool LibraryMetadataEditDialog::shouldShowPersonMetadata() const
{
    return isItemType(m_itemData.value(QStringLiteral("Type")).toString(),
                      {"Person"}) ||
           m_itemData.contains(QStringLiteral("PlaceOfBirth"));
}

bool LibraryMetadataEditDialog::shouldShowChannelMetadata() const
{
    return isItemType(m_itemData.value(QStringLiteral("Type")).toString(),
                      {"Channel"}) ||
           m_itemData.contains(QStringLiteral("ChannelNumber"));
}

bool LibraryMetadataEditDialog::shouldShowIndexMetadata() const
{
    const QString itemType =
        m_itemData.value(QStringLiteral("Type")).toString().trimmed();
    return containsAnyKey(m_itemData, {"ParentIndexNumber", "IndexNumber"}) ||
           isItemType(itemType, {"Season", "Episode", "Audio"});
}

bool LibraryMetadataEditDialog::shouldShowRuntimeMetadata() const
{
    const QString itemType =
        m_itemData.value(QStringLiteral("Type")).toString().trimmed();
    return m_itemData.contains(QStringLiteral("RunTimeTicks")) ||
           isItemType(itemType, {"Movie", "Video", "Episode", "Series",
                                 "Season", "Audio", "Trailer",
                                 "MusicVideo"});
}

void LibraryMetadataEditDialog::populateForm()
{
    if (m_pathEdit) {
        m_pathEdit->setText(displayPath());
        m_pathEdit->setCursorPosition(0);
    }

    if (m_nameEdit) {
        m_nameEdit->setText(m_itemData.value(QStringLiteral("Name")).toString());
        m_nameEdit->setFocus();
        m_nameEdit->selectAll();
    }

    if (m_originalTitleEdit) {
        m_originalTitleEdit->setText(
            m_itemData.value(QStringLiteral("OriginalTitle")).toString());
    }

    if (m_sortNameEdit) {
        m_sortNameEdit->setText(
            m_itemData.value(QStringLiteral("SortName")).toString());
    }

    if (m_taglineEdit) {
        const QString tagline =
            !m_itemData.value(QStringLiteral("Tagline")).toString().trimmed().isEmpty()
                ? m_itemData.value(QStringLiteral("Tagline")).toString().trimmed()
                : firstNonEmptyValue(m_itemData.value(QStringLiteral("Taglines")));
        m_taglineEdit->setText(tagline);
    }

    if (m_overviewEdit) {
        m_overviewEdit->setPlainText(
            m_itemData.value(QStringLiteral("Overview")).toString());
    }

    if (m_dateAddedEdit) {
        m_dateAddedEdit->setText(localDateTimeDisplayText(
            m_itemData.value(QStringLiteral("DateCreated")).toString()));
    }

    if (m_genresEdit) {
        const QString genreItemsText =
            joinedValues(m_itemData.value(QStringLiteral("GenreItems")));
        m_genresEdit->setText(
            genreItemsText.isEmpty()
                ? joinedValues(m_itemData.value(QStringLiteral("Genres")))
                : genreItemsText);
    }

    if (m_tagsEdit) {
        const QString tagItemsText =
            joinedValues(m_itemData.value(QStringLiteral("TagItems")));
        m_tagsEdit->setText(
            tagItemsText.isEmpty()
                ? joinedValues(m_itemData.value(QStringLiteral("Tags")))
                : tagItemsText);
    }

    if (m_studiosEdit) {
        m_studiosEdit->setText(
            joinedValues(m_itemData.value(QStringLiteral("Studios"))));
    }

    if (m_productionYearEdit &&
        m_itemData.contains(QStringLiteral("ProductionYear"))) {
        const int productionYear =
            m_itemData.value(QStringLiteral("ProductionYear")).toInt();
        if (productionYear > 0) {
            m_productionYearEdit->setText(QString::number(productionYear));
        }
    }

    if (m_premiereDateEdit) {
        m_premiereDateEdit->setText(
            dateOnlyText(m_itemData.value(QStringLiteral("PremiereDate"))
                             .toString()));
    }

    if (m_endDateEdit) {
        m_endDateEdit->setText(
            dateOnlyText(m_itemData.value(QStringLiteral("EndDate")).toString()));
    }

    if (m_statusEdit) {
        m_statusEdit->setText(
            m_itemData.value(QStringLiteral("Status")).toString());
    }

    if (m_runtimeMinutesEdit) {
        m_runtimeMinutesEdit->setText(
            minutesTextFromTicks(m_itemData.value(QStringLiteral("RunTimeTicks"))));
    }

    if (m_officialRatingEdit) {
        m_officialRatingEdit->setText(
            m_itemData.value(QStringLiteral("OfficialRating")).toString());
    }

    if (m_customRatingEdit) {
        m_customRatingEdit->setText(
            m_itemData.value(QStringLiteral("CustomRating")).toString());
    }

    if (m_communityRatingEdit &&
        m_itemData.contains(QStringLiteral("CommunityRating"))) {
        m_communityRatingEdit->setText(
            numberText(m_itemData.value(QStringLiteral("CommunityRating"))));
    }

    if (m_criticRatingEdit &&
        m_itemData.contains(QStringLiteral("CriticRating"))) {
        m_criticRatingEdit->setText(
            numberText(m_itemData.value(QStringLiteral("CriticRating"))));
    }

    if (m_imdbIdEdit) {
        m_imdbIdEdit->setText(providerIdValue({QStringLiteral("Imdb")}));
    }

    if (m_movieDbIdEdit) {
        m_movieDbIdEdit->setText(
            providerIdValue({QStringLiteral("Tmdb"),
                             QStringLiteral("TheMovieDb")}));
    }

    if (m_tvdbIdEdit) {
        m_tvdbIdEdit->setText(
            providerIdValue({QStringLiteral("Tvdb"),
                             QStringLiteral("TheTVDB")}));
    }

    if (m_albumEdit) {
        m_albumEdit->setText(m_itemData.value(QStringLiteral("Album")).toString());
    }

    if (m_albumArtistEdit) {
        const QJsonValue albumArtistValue =
            m_itemData.value(QStringLiteral("AlbumArtist"));
        if (albumArtistValue.isObject()) {
            m_albumArtistEdit->setText(
                albumArtistValue.toObject()
                    .value(QStringLiteral("Name"))
                    .toString());
        } else {
            m_albumArtistEdit->setText(albumArtistValue.toString());
        }
    }

    if (m_artistsEdit) {
        m_artistsEdit->setText(
            joinedValues(m_itemData.value(QStringLiteral("Artists"))));
    }

    if (m_composersEdit) {
        m_composersEdit->setText(
            joinedValues(m_itemData.value(QStringLiteral("Composers"))));
    }

    if (m_channelNumberEdit) {
        m_channelNumberEdit->setText(
            m_itemData.value(QStringLiteral("ChannelNumber")).toString());
    }

    if (m_placeOfBirthEdit) {
        m_placeOfBirthEdit->setText(
            m_itemData.value(QStringLiteral("PlaceOfBirth")).toString());
    }

    if (m_parentIndexNumberEdit &&
        m_itemData.contains(QStringLiteral("ParentIndexNumber"))) {
        m_parentIndexNumberEdit->setText(
            QString::number(
                m_itemData.value(QStringLiteral("ParentIndexNumber")).toInt()));
    }

    if (m_indexNumberEdit &&
        m_itemData.contains(QStringLiteral("IndexNumber"))) {
        m_indexNumberEdit->setText(
            QString::number(
                m_itemData.value(QStringLiteral("IndexNumber")).toInt()));
    }

    if (m_preferredMetadataLanguageEdit) {
        m_preferredMetadataLanguageEdit->setText(
            m_itemData.value(QStringLiteral("PreferredMetadataLanguage"))
                .toString());
    }

    if (m_metadataCountryCodeEdit) {
        m_metadataCountryCodeEdit->setText(
            m_itemData.value(QStringLiteral("MetadataCountryCode"))
                .toString());
    }

    if (m_lockDataCheck) {
        m_lockDataCheck->setChecked(
            m_itemData.value(QStringLiteral("LockData")).toBool(false));
    }

    populateLockedFieldChecks();
}

void LibraryMetadataEditDialog::populateLockedFieldChecks()
{
    const QStringList lockedFields =
        valuesFromJson(m_itemData.value(QStringLiteral("LockedFields")));
    for (LockedFieldOption& option : m_lockedFieldOptions) {
        if (option.checkBox) {
            option.checkBox->setChecked(lockedFields.contains(option.key));
        }
    }
}

void LibraryMetadataEditDialog::updateSaveButtonState()
{
    if (!m_saveButton || !m_nameEdit) {
        return;
    }

    m_saveButton->setEnabled(!m_nameEdit->text().trimmed().isEmpty());
}
