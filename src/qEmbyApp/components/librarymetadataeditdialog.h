#ifndef LIBRARYMETADATAEDITDIALOG_H
#define LIBRARYMETADATAEDITDIALOG_H

#include "collapsiblesection.h"
#include "moderndialogbase.h"
#include <QJsonObject>
#include <QJsonValue>
#include <QList>
#include <QStringList>

class QCheckBox;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QVBoxLayout;

class LibraryMetadataEditDialog : public ModernDialogBase
{
    Q_OBJECT
public:
    explicit LibraryMetadataEditDialog(QJsonObject itemData,
                                       QWidget* parent = nullptr);

    QJsonObject updatedItemData() const;

private:
    struct LockedFieldOption {
        QString key;
        QString label;
        QCheckBox* checkBox = nullptr;
    };

    QLineEdit* createLineEdit(QString placeholderText = QString(),
                              bool readOnly = false) const;
    QGridLayout* createSectionForm(CollapsibleSection* section) const;
    QLabel* addFormRow(QGridLayout* formLayout, int row, QString labelText,
                       QWidget* field, Qt::Alignment labelAlignment =
                           Qt::AlignLeft | Qt::AlignVCenter) const;
    void addLockedFieldOption(QVBoxLayout* layout, QString key,
                              QString labelText);
    QString providerIdValue(const QStringList& keys) const;
    void setProviderIdValue(QJsonObject& providerIds, const QStringList& keys,
                            QString value) const;
    QStringList multiValueList(const QLineEdit* edit) const;
    QStringList valuesFromJson(const QJsonValue& value) const;
    QString joinedValues(const QJsonValue& value) const;
    QString currentTagline() const;
    QString normalizedPremiereDate(QString dateText) const;
    QString displayPath() const;
    QStringList lockedFieldKeys() const;
    bool shouldShowSeriesMetadata() const;
    bool shouldShowMusicMetadata() const;
    bool shouldShowPersonMetadata() const;
    bool shouldShowChannelMetadata() const;
    bool shouldShowIndexMetadata() const;
    bool shouldShowRuntimeMetadata() const;
    void populateForm();
    void populateLockedFieldChecks();
    void updateSaveButtonState();

    QJsonObject m_itemData;

    QLineEdit* m_pathEdit = nullptr;
    QLineEdit* m_nameEdit = nullptr;
    QLineEdit* m_originalTitleEdit = nullptr;
    QLineEdit* m_sortNameEdit = nullptr;
    QLineEdit* m_taglineEdit = nullptr;
    QLineEdit* m_dateAddedEdit = nullptr;
    QLineEdit* m_genresEdit = nullptr;
    QLineEdit* m_tagsEdit = nullptr;
    QLineEdit* m_studiosEdit = nullptr;
    QLineEdit* m_productionYearEdit = nullptr;
    QLineEdit* m_premiereDateEdit = nullptr;
    QLineEdit* m_endDateEdit = nullptr;
    QLineEdit* m_statusEdit = nullptr;
    QLineEdit* m_runtimeMinutesEdit = nullptr;
    QLineEdit* m_officialRatingEdit = nullptr;
    QLineEdit* m_customRatingEdit = nullptr;
    QLineEdit* m_communityRatingEdit = nullptr;
    QLineEdit* m_criticRatingEdit = nullptr;
    QLineEdit* m_imdbIdEdit = nullptr;
    QLineEdit* m_movieDbIdEdit = nullptr;
    QLineEdit* m_tvdbIdEdit = nullptr;
    QLineEdit* m_albumEdit = nullptr;
    QLineEdit* m_albumArtistEdit = nullptr;
    QLineEdit* m_artistsEdit = nullptr;
    QLineEdit* m_composersEdit = nullptr;
    QLineEdit* m_channelNumberEdit = nullptr;
    QLineEdit* m_placeOfBirthEdit = nullptr;
    QLineEdit* m_parentIndexNumberEdit = nullptr;
    QLineEdit* m_indexNumberEdit = nullptr;
    QLineEdit* m_preferredMetadataLanguageEdit = nullptr;
    QLineEdit* m_metadataCountryCodeEdit = nullptr;
    QPlainTextEdit* m_overviewEdit = nullptr;
    QCheckBox* m_lockDataCheck = nullptr;
    QList<LockedFieldOption> m_lockedFieldOptions;
    QPushButton* m_saveButton = nullptr;
};

#endif 
