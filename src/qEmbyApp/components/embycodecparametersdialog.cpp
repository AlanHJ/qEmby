#include "embycodecparametersdialog.h"

#include "../utils/layoututils.h"
#include "moderncombobox.h"
#include "moderntoast.h"
#include "modernswitch.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>

#include <QDebug>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>
#include <functional>
#include <utility>

namespace {

QString normalizePathElement(QString element) {
    return element.replace(QStringLiteral("colitem"), QString(), Qt::CaseInsensitive);
}

QStringList splitPath(const QString& path) {
    QStringList parts;
    for (const QString& rawPart : path.split(QLatin1Char('.'), Qt::SkipEmptyParts)) {
        const QString part = normalizePathElement(rawPart.trimmed());
        if (!part.isEmpty()) {
            parts.append(part);
        }
    }
    return parts;
}

bool containsJsonPath(const QJsonObject& object, const QString& path) {
    QJsonValue current(object);
    const QStringList parts = splitPath(path);
    if (parts.isEmpty()) {
        return false;
    }

    for (const QString& part : parts) {
        if (!current.isObject()) {
            return false;
        }

        const QJsonObject currentObject = current.toObject();
        if (!currentObject.contains(part)) {
            return false;
        }
        current = currentObject.value(part);
    }

    return true;
}

QJsonValue jsonValueAtPath(const QJsonObject& object, const QString& path) {
    QJsonValue current(object);
    const QStringList parts = splitPath(path);
    if (parts.isEmpty()) {
        return {};
    }

    for (const QString& part : parts) {
        if (!current.isObject()) {
            return {};
        }
        current = current.toObject().value(part);
    }

    return current;
}

void setJsonValueAtPath(QJsonObject& object, const QString& path, const QJsonValue& value) {
    const QStringList parts = splitPath(path);
    if (parts.isEmpty()) {
        return;
    }

    std::function<void(QJsonObject&, int)> applyValue = [&](QJsonObject& currentObject,
                                                            int depth) {
        const QString& key = parts.at(depth);
        if (depth == parts.size() - 1) {
            currentObject.insert(key, value);
            return;
        }

        QJsonObject childObject = currentObject.value(key).toObject();
        applyValue(childObject, depth + 1);
        currentObject.insert(key, childObject);
    };

    applyValue(object, 0);
}

QStringList optionFilterValues(const QJsonObject& object, const QString& sourceId) {
    QStringList values;
    if (sourceId.trimmed().isEmpty()) {
        return values;
    }

    const QJsonValue sourceValue = jsonValueAtPath(object, sourceId);
    if (!sourceValue.isArray()) {
        return values;
    }

    for (const QJsonValue& entry : sourceValue.toArray()) {
        QString text;
        if (entry.isString()) {
            text = entry.toString().trimmed();
        } else if (entry.isObject()) {
            text = entry.toObject().value(QStringLiteral("Value")).toString().trimmed();
        }

        if (!text.isEmpty() && !values.contains(text)) {
            values.append(text);
        }
    }

    return values;
}

QWidget* createSectionCard(const QString& title,
                           QVBoxLayout*& bodyLayout,
                           QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("TranscodingSectionCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 16);
    layout->setSpacing(12);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("TranscodingSectionTitle");
    layout->addWidget(titleLabel);

    bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(8);
    layout->addLayout(bodyLayout);

    return card;
}

QWidget* createFieldBlock(const QString& title,
                          const QString& description,
                          QWidget* control,
                          QWidget* parent,
                          bool controlOwnRow = false) {
    auto* block = new QFrame(parent);
    block->setObjectName("TranscodingFieldBlock");
    block->setAttribute(Qt::WA_StyledBackground, true);

    const bool hasDescription = !description.trimmed().isEmpty();
    if (controlOwnRow) {
        auto* layout = new QVBoxLayout(block);
        layout->setContentsMargins(14, 11, 14, 11);
        layout->setSpacing(8);

        auto* titleLabel = new QLabel(title, block);
        titleLabel->setObjectName("TranscodingFieldTitle");
        titleLabel->setWordWrap(true);
        layout->addWidget(titleLabel);

        if (hasDescription) {
            auto* descriptionLabel = new QLabel(description, block);
            descriptionLabel->setObjectName("TranscodingFieldDescription");
            descriptionLabel->setWordWrap(true);
            layout->addWidget(descriptionLabel);
        }

        if (control) {
            control->setParent(block);
            layout->addWidget(control);
        }

        return block;
    }

    auto* layout = new QHBoxLayout(block);
    layout->setContentsMargins(14, 11, 14, 11);
    layout->setSpacing(16);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(hasDescription ? 4 : 0);

    auto* titleLabel = new QLabel(title, block);
    titleLabel->setObjectName("TranscodingFieldTitle");
    titleLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);

    if (hasDescription) {
        auto* descriptionLabel = new QLabel(description, block);
        descriptionLabel->setObjectName("TranscodingFieldDescription");
        descriptionLabel->setWordWrap(true);
        textLayout->addWidget(descriptionLabel);
    }

    layout->addLayout(textLayout, 1);

    if (control) {
        control->setParent(block);
        layout->addWidget(control, 0, hasDescription ? Qt::AlignTop : Qt::AlignVCenter);
    }

    return block;
}

QWidget* createSwitchBlock(const QString& title,
                           const QString& description,
                           ModernSwitch* control,
                           QWidget* parent) {
    auto* block = new QFrame(parent);
    block->setObjectName("TranscodingFieldBlock");
    block->setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(block);
    layout->setContentsMargins(14, 11, 14, 11);
    layout->setSpacing(12);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);

    auto* titleLabel = new QLabel(title, block);
    titleLabel->setObjectName("TranscodingFieldTitle");
    titleLabel->setWordWrap(true);
    textLayout->addWidget(titleLabel);

    if (!description.trimmed().isEmpty()) {
        auto* descriptionLabel = new QLabel(description, block);
        descriptionLabel->setObjectName("TranscodingFieldDescription");
        descriptionLabel->setWordWrap(true);
        textLayout->addWidget(descriptionLabel);
    }

    layout->addLayout(textLayout, 1);
    if (control) {
        control->setParent(block);
        layout->addWidget(control, 0, Qt::AlignVCenter);
    }

    return block;
}

} 

EmbyCodecParametersDialog::EmbyCodecParametersDialog(QEmbyCore* core,
                                                     QJsonObject codecInfo,
                                                     QWidget* parent)
    : ModernDialogBase(parent), m_core(core), m_codecInfo(std::move(codecInfo)) {
    m_codecId = m_codecInfo.value(QStringLiteral("Id")).toString().trimmed();

    const QString title = codecTitle();
    setTitle(title.isEmpty() ? tr("Codec Settings") : title);
    setMinimumWidth(660);
    setMaximumHeight(760);
    resize(760, 640);

    setupUi();
    refreshUiState();

    m_pendingTask = loadParameters();
}

void EmbyCodecParametersDialog::setupUi() {
    contentLayout()->setContentsMargins(20, 10, 0, 18);
    contentLayout()->setSpacing(10);

    m_summaryLabel = new QLabel(this);
    m_summaryLabel->setObjectName("TranscodingSectionSubtitle");
    m_summaryLabel->setWordWrap(true);
    const QString summary = codecSummary();
    m_summaryLabel->setText(summary);
    m_summaryLabel->setVisible(!summary.isEmpty());
    contentLayout()->addWidget(m_summaryLabel);

    m_feedbackLabel = new QLabel(tr("Loading codec parameters..."), this);
    m_feedbackLabel->setObjectName("ManageEmptyLabel");
    m_feedbackLabel->setAlignment(Qt::AlignCenter);
    m_feedbackLabel->setWordWrap(true);
    contentLayout()->addWidget(m_feedbackLabel);

    m_formPage = new QWidget(this);
    m_formLayout = new QVBoxLayout(m_formPage);
    m_formLayout->setContentsMargins(0, 0, 12, 4);
    m_formLayout->setSpacing(10);

    m_basicCard = createSectionCard(tr("Basic Parameters"), m_basicBody, m_formPage);
    m_formLayout->addWidget(m_basicCard);

    m_advancedCard =
        createSectionCard(tr("Advanced Parameters"), m_advancedBody, m_formPage);
    m_formLayout->addWidget(m_advancedCard);
    m_formLayout->addStretch(1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("SettingsScrollArea");
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->setWidget(m_formPage);
    contentLayout()->addWidget(m_scrollArea, 1);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setContentsMargins(0, 4, 12, 0);
    buttonLayout->setSpacing(10);
    buttonLayout->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setObjectName("dialog-btn-cancel");
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    buttonLayout->addWidget(m_btnCancel);

    m_btnReset = new QPushButton(tr("Reset to Defaults"), this);
    m_btnReset->setObjectName("SettingsCardButton");
    m_btnReset->setCursor(Qt::PointingHandCursor);
    buttonLayout->addWidget(m_btnReset);

    m_btnSave = new QPushButton(tr("Save"), this);
    m_btnSave->setObjectName("dialog-btn-primary");
    m_btnSave->setCursor(Qt::PointingHandCursor);
    buttonLayout->addWidget(m_btnSave);

    contentLayout()->addLayout(buttonLayout);

    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnReset, &QPushButton::clicked, this,
            [this]() { resetToDefaults(); });
    connect(m_btnSave, &QPushButton::clicked, this,
            [this]() { m_pendingTask = saveParameters(); });
}

void EmbyCodecParametersDialog::clearFieldBindings() {
    if (m_basicBody) {
        LayoutUtils::clearLayout(m_basicBody);
    }
    if (m_advancedBody) {
        LayoutUtils::clearLayout(m_advancedBody);
    }
    m_fields.clear();
}

void EmbyCodecParametersDialog::refreshUiState() {
    const QString summary = codecSummary();
    if (m_summaryLabel) {
        m_summaryLabel->setText(summary);
        m_summaryLabel->setVisible(!summary.isEmpty());
    }

    const bool hasFields = !m_fields.isEmpty();
    QString feedback = m_feedbackText;
    if (m_isLoading) {
        feedback = tr("Loading codec parameters...");
    } else if (feedback.isEmpty() && !hasFields) {
        feedback = tr("No editable parameters available for this codec.");
    }

    const bool showForm = !m_isLoading && m_feedbackText.isEmpty() && hasFields;
    if (m_feedbackLabel) {
        m_feedbackLabel->setText(feedback);
        m_feedbackLabel->setVisible(!showForm);
    }
    if (m_scrollArea) {
        m_scrollArea->setVisible(showForm);
    }
    if (m_basicCard && m_basicBody) {
        m_basicCard->setVisible(showForm && m_basicBody->count() > 0);
    }
    if (m_advancedCard && m_advancedBody) {
        m_advancedCard->setVisible(showForm && m_advancedBody->count() > 0);
    }

    if (m_btnCancel) {
        m_btnCancel->setEnabled(!m_isSaving);
    }
    if (m_btnReset) {
        m_btnReset->setEnabled(showForm && !m_isSaving);
    }
    if (m_btnSave) {
        m_btnSave->setEnabled(showForm && !m_isSaving);
        m_btnSave->setText(m_isSaving ? tr("Saving...") : tr("Save"));
    }
}

QString EmbyCodecParametersDialog::codecTitle() const {
    const bool isHardwareCodec =
        m_codecInfo.value(QStringLiteral("IsHardwareCodec")).toBool(false);
    const QString mediaTypeName =
        m_codecInfo.value(QStringLiteral("MediaTypeName")).toString().trimmed();
    const QString name = m_codecInfo.value(QStringLiteral("Name")).toString().trimmed();

    if (!isHardwareCodec && !mediaTypeName.isEmpty()) {
        return mediaTypeName;
    }
    if (!name.isEmpty()) {
        return name;
    }
    return mediaTypeName.isEmpty() ? m_codecId : mediaTypeName;
}

QString EmbyCodecParametersDialog::codecSummary() const {
    QStringList parts;
    const QString title = codecTitle();
    const QString name = m_codecInfo.value(QStringLiteral("Name")).toString().trimmed();
    const QString description =
        m_codecInfo.value(QStringLiteral("Description")).toString().trimmed();

    if (!name.isEmpty() && name != title) {
        parts.append(name);
    }
    if (!description.isEmpty() && !parts.contains(description)) {
        parts.append(description);
    }

    return parts.join(QStringLiteral("\n"));
}

void EmbyCodecParametersDialog::appendEditorItems(const QJsonObject& item,
                                                  QList<QJsonObject>& items) const {
    if (item.isEmpty()) {
        return;
    }

    const QString editorType = item.value(QStringLiteral("EditorType")).toString().trimmed();
    if (editorType.compare(QStringLiteral("Group"), Qt::CaseInsensitive) == 0) {
        for (const QJsonValue& value : item.value(QStringLiteral("EditorItems")).toArray()) {
            appendEditorItems(value.toObject(), items);
        }
        return;
    }

    items.append(item);
}

void EmbyCodecParametersDialog::setWidgetValue(const FieldBinding& field,
                                               const QJsonValue& value) {
    if (field.lineEdit) {
        if (value.isUndefined() || value.isNull()) {
            field.lineEdit->clear();
            return;
        }

        if (value.isString()) {
            field.lineEdit->setText(value.toString());
            return;
        }

        if (value.isDouble()) {
            if (field.editorType == QStringLiteral("Numeric") && field.decimals > 0) {
                field.lineEdit->setText(
                    QString::number(value.toDouble(), 'f', field.decimals));
            } else {
                field.lineEdit->setText(QString::number(value.toDouble(), 'g', 15));
            }
            return;
        }

        field.lineEdit->setText(value.toVariant().toString());
        return;
    }

    if (field.comboBox) {
        if (!value.isUndefined() && !value.isNull()) {
            const QVariant targetValue = value.toVariant();
            int index = field.comboBox->findData(targetValue);
            if (index < 0 && !targetValue.toString().isEmpty()) {
                field.comboBox->addItem(targetValue.toString(), targetValue);
                index = field.comboBox->count() - 1;
            }
            field.comboBox->setCurrentIndex(index);
            return;
        }

        const int emptyIndex = field.comboBox->findData(QString());
        field.comboBox->setCurrentIndex(emptyIndex >= 0 ? emptyIndex : -1);
        return;
    }

    if (field.switchControl) {
        field.switchControl->setChecked(value.toBool(false));
    }
}

void EmbyCodecParametersDialog::rebuildForm() {
    clearFieldBindings();

    const QJsonObject editorRoot = m_editorContainer.value(QStringLiteral("EditorRoot")).toObject();
    QList<QJsonObject> editorItems;
    appendEditorItems(editorRoot, editorItems);

    qDebug() << "[EmbyCodecParametersDialog] Rebuilding codec parameter form"
             << "| codecId=" << m_codecId
             << "| itemCount=" << editorItems.size();

    for (const QJsonObject& item : std::as_const(editorItems)) {
        const QString editorType =
            item.value(QStringLiteral("EditorType")).toString().trimmed();
        if (editorType != QStringLiteral("Text") &&
            editorType != QStringLiteral("Numeric") &&
            editorType != QStringLiteral("Boolean") &&
            editorType != QStringLiteral("SelectSingle")) {
            qDebug() << "[EmbyCodecParametersDialog] Skipping unsupported editor type"
                     << "| codecId=" << m_codecId
                     << "| editorType=" << editorType
                     << "| id=" << item.value(QStringLiteral("Id")).toString();
            continue;
        }

        FieldBinding field;
        field.id = item.value(QStringLiteral("Id")).toString().trimmed();
        field.editorType = editorType;
        field.displayName =
            item.value(QStringLiteral("DisplayName")).toString(field.id).trimmed();
        field.description = item.value(QStringLiteral("Description")).toString().trimmed();
        field.staticItemsSourceId =
            item.value(QStringLiteral("StaticItemsSourceId")).toString().trimmed();
        field.allowEmpty = item.value(QStringLiteral("AllowEmpty")).toBool(true);
        field.readOnly = item.value(QStringLiteral("IsReadOnly")).toBool(false);
        field.advanced = item.value(QStringLiteral("IsAdvanced")).toBool(false);
        field.decimals = item.value(QStringLiteral("DecimalPlaces")).toInt(0);
        field.hasMinimum = item.contains(QStringLiteral("MinValue"));
        field.hasMaximum = item.contains(QStringLiteral("MaxValue"));
        field.minimumValue = item.value(QStringLiteral("MinValue")).toInt(0);
        field.maximumValue = item.value(QStringLiteral("MaxValue")).toInt(0);

        if (field.id.isEmpty()) {
            continue;
        }

        QVBoxLayout* targetBody = field.advanced ? m_advancedBody : m_basicBody;
        QWidget* sectionParent = targetBody ? targetBody->parentWidget() : this;
        if (!targetBody || !sectionParent) {
            continue;
        }

        if (editorType == QStringLiteral("Boolean")) {
            auto* control = new ModernSwitch(sectionParent);
            control->setCursor(Qt::PointingHandCursor);
            control->setEnabled(!field.readOnly);
            field.switchControl = control;
            field.container = createSwitchBlock(field.displayName,
                                                field.description,
                                                control,
                                                sectionParent);
        } else if (editorType == QStringLiteral("SelectSingle")) {
            auto* control = LayoutUtils::createStyledCombo(sectionParent);
            control->setMinimumWidth(260);
            control->setMaxTextWidth(260);
            control->setEnabled(!field.readOnly);

            const bool hasOptionFilter =
                !field.staticItemsSourceId.isEmpty() &&
                jsonValueAtPath(m_parameterObject, field.staticItemsSourceId).isArray();
            const QStringList allowedValues =
                optionFilterValues(m_parameterObject, field.staticItemsSourceId);
            for (const QJsonValue& optionValue :
                 item.value(QStringLiteral("SelectOptions")).toArray()) {
                const QJsonObject option = optionValue.toObject();
                const QString value = option.value(QStringLiteral("Value")).toString();
                if (hasOptionFilter && !value.isEmpty() &&
                    !allowedValues.contains(value)) {
                    continue;
                }

                QString label = option.value(QStringLiteral("Name")).toString();
                if (label.isEmpty()) {
                    label = value;
                }
                control->addItem(label, option.value(QStringLiteral("Value")).toVariant());
            }

            field.comboBox = control;
            field.container =
                createFieldBlock(field.displayName, field.description, control, sectionParent);
        } else {
            auto* control = new QLineEdit(sectionParent);
            control->setObjectName("ManageLibInput");
            control->setClearButtonEnabled(field.allowEmpty);
            const bool isTextField = editorType == QStringLiteral("Text");
            control->setMinimumWidth(isTextField ? 360 : 220);
            control->setSizePolicy(isTextField ? QSizePolicy::MinimumExpanding
                                               : QSizePolicy::Preferred,
                                   QSizePolicy::Fixed);
            control->setReadOnly(field.readOnly);
            control->setProperty("readOnly", field.readOnly);

            if (editorType == QStringLiteral("Numeric")) {
                if (field.decimals > 0) {
                    auto* validator = new QDoubleValidator(control);
                    validator->setNotation(QDoubleValidator::StandardNotation);
                    validator->setDecimals(field.decimals);
                    if (field.hasMinimum && field.hasMaximum) {
                        validator->setRange(field.minimumValue, field.maximumValue,
                                            field.decimals);
                    }
                    control->setValidator(validator);
                } else if (field.hasMinimum && field.hasMaximum) {
                    control->setValidator(
                        new QIntValidator(field.minimumValue, field.maximumValue, control));
                }
            }

            field.lineEdit = control;
            field.container = createFieldBlock(field.displayName,
                                               field.description,
                                               control,
                                               sectionParent,
                                               isTextField);
        }

        if (field.container) {
            targetBody->addWidget(field.container);
        }

        const QJsonValue initialValue = containsJsonPath(m_parameterObject, field.id)
                                            ? jsonValueAtPath(m_parameterObject, field.id)
                                            : containsJsonPath(m_defaultObject, field.id)
                                                  ? jsonValueAtPath(m_defaultObject, field.id)
                                                  : QJsonValue();
        setWidgetValue(field, initialValue);
        m_fields.append(field);
    }
}

QJsonObject EmbyCodecParametersDialog::collectParameters() const {
    QJsonObject parameters = m_parameterObject;

    for (const FieldBinding& field : m_fields) {
        if (field.lineEdit) {
            QString text = field.lineEdit->text();
            if (field.editorType == QStringLiteral("Numeric")) {
                text = text.trimmed();
            }
            setJsonValueAtPath(parameters, field.id, text);
            continue;
        }

        if (field.comboBox) {
            const QVariant value =
                field.comboBox->currentIndex() >= 0 ? field.comboBox->currentData()
                                                    : QVariant(QString());
            setJsonValueAtPath(parameters, field.id, QJsonValue::fromVariant(value));
            continue;
        }

        if (field.switchControl) {
            setJsonValueAtPath(parameters, field.id, field.switchControl->isChecked());
        }
    }

    return parameters;
}

void EmbyCodecParametersDialog::resetToDefaults() {
    if (m_isLoading || m_isSaving || m_fields.isEmpty()) {
        return;
    }

    qDebug() << "[EmbyCodecParametersDialog] Resetting codec parameters to defaults"
             << "| codecId=" << m_codecId
             << "| fieldCount=" << m_fields.size();

    for (const FieldBinding& field : std::as_const(m_fields)) {
        const QJsonValue defaultValue =
            containsJsonPath(m_defaultObject, field.id)
                ? jsonValueAtPath(m_defaultObject, field.id)
                : QJsonValue(QJsonValue::Null);
        setJsonValueAtPath(m_parameterObject, field.id, defaultValue);
        setWidgetValue(field, defaultValue);
    }
}

QCoro::Task<void> EmbyCodecParametersDialog::loadParameters() {
    QPointer<EmbyCodecParametersDialog> safeThis(this);
    if (m_isLoading || !m_core || !m_core->adminService() || m_codecId.isEmpty()) {
        if (!m_codecId.isEmpty()) {
            qWarning() << "[EmbyCodecParametersDialog] Unable to load codec parameters"
                       << "| codecId=" << m_codecId
                       << "| hasCore=" << (m_core != nullptr);
        }
        m_feedbackText = tr("Failed to load codec parameters");
        refreshUiState();
        co_return;
    }

    m_isLoading = true;
    m_feedbackText.clear();
    refreshUiState();

    try {
        const QJsonObject response =
            co_await m_core->adminService()->getCodecParameters(m_codecId);
        if (!safeThis) {
            co_return;
        }

        m_editorContainer = response;
        m_parameterObject = response.value(QStringLiteral("Object")).toObject();
        m_defaultObject = response.value(QStringLiteral("DefaultObject")).toObject();
        rebuildForm();
        m_hasLoaded = true;
        m_feedbackText.clear();

        qDebug() << "[EmbyCodecParametersDialog] Loaded codec parameters"
                 << "| codecId=" << m_codecId
                 << "| fieldCount=" << m_fields.size()
                 << "| objectKeys=" << m_parameterObject.keys();
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[EmbyCodecParametersDialog] Failed to load codec parameters"
                   << "| codecId=" << m_codecId
                   << "| error=" << e.what();
        clearFieldBindings();
        m_editorContainer = QJsonObject();
        m_parameterObject = QJsonObject();
        m_defaultObject = QJsonObject();
        m_hasLoaded = false;
        m_feedbackText = tr("Failed to load codec parameters");
        ModernToast::showMessage(
            tr("Failed to load codec parameters: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (!safeThis) {
        co_return;
    }

    m_isLoading = false;
    refreshUiState();
}

QCoro::Task<void> EmbyCodecParametersDialog::saveParameters() {
    QPointer<EmbyCodecParametersDialog> safeThis(this);
    if (m_isLoading || m_isSaving || !m_hasLoaded || m_fields.isEmpty() || !m_core ||
        !m_core->adminService() || m_codecId.isEmpty()) {
        co_return;
    }

    m_isSaving = true;
    refreshUiState();

    const QJsonObject parameters = collectParameters();

    qDebug() << "[EmbyCodecParametersDialog] Saving codec parameters"
             << "| codecId=" << m_codecId
             << "| keyCount=" << parameters.keys().size()
             << "| keys=" << parameters.keys();

    try {
        co_await m_core->adminService()->updateCodecParameters(m_codecId, parameters);
        if (!safeThis) {
            co_return;
        }

        m_parameterObject = parameters;
        m_isSaving = false;
        refreshUiState();
        ModernToast::showMessage(tr("Codec parameters saved"), 2000);
        accept();
        co_return;
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[EmbyCodecParametersDialog] Failed to save codec parameters"
                   << "| codecId=" << m_codecId
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to save codec parameters: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (!safeThis) {
        co_return;
    }

    m_isSaving = false;
    refreshUiState();
}
