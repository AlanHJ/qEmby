#include "embycodecinfodialog.h"

#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QScrollArea>
#include <QStringList>
#include <QVBoxLayout>
#include <utility>

namespace {

QWidget* createSummaryRow(const QString& title, const QString& value, QWidget* parent) {
    auto* row = new QWidget(parent);
    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel(title, row);
    titleLabel->setObjectName("ManageTranscodingFieldTitle");
    titleLabel->setWordWrap(true);
    titleLabel->setMinimumWidth(118);
    layout->addWidget(titleLabel, 0, Qt::AlignTop);

    auto* valueLabel = new QLabel(value, row);
    valueLabel->setObjectName("ManageInfoValue");
    valueLabel->setWordWrap(true);
    valueLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(valueLabel, 1, Qt::AlignTop);

    return row;
}

QLabel* createTableLabel(const QString& text,
                         const char* objectName,
                         QWidget* parent,
                         bool header = false) {
    auto* label = new QLabel(text, parent);
    label->setObjectName(objectName);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    if (header) {
        label->setMinimumWidth(72);
    }
    return label;
}

} 

EmbyCodecInfoDialog::EmbyCodecInfoDialog(QJsonObject codecInfo, QWidget* parent)
    : ModernDialogBase(parent), m_codecInfo(std::move(codecInfo)) {
    setTitle(codecTitle());
    setMinimumWidth(700);
    resize(760, 560);
    setupUi();
}

void EmbyCodecInfoDialog::setupUi() {
    contentLayout()->setContentsMargins(20, 10, 0, 20);
    contentLayout()->setSpacing(12);

    auto* scrollArea = new QScrollArea(this);
    scrollArea->setObjectName("SettingsScrollArea");
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->viewport()->setAutoFillBackground(false);
    contentLayout()->addWidget(scrollArea, 1);

    auto* page = new QWidget(scrollArea);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 12, 0);
    pageLayout->setSpacing(18);
    scrollArea->setWidget(page);

    const QString description = codecDescription();
    if (!description.isEmpty()) {
        auto* descriptionLabel = new QLabel(description, page);
        descriptionLabel->setObjectName("ManageInfoValue");
        descriptionLabel->setWordWrap(true);
        descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pageLayout->addWidget(descriptionLabel);
    }

    struct SummaryItem {
        QString title;
        QString value;
    };

    const QList<SummaryItem> summaryItems = {
        {tr("Max Bitrate:"), maxBitrateText()},
        {tr("Frame Sizes:"), frameSizesText()},
        {tr("Frame Rates:"), frameRatesText()},
        {tr("Max Instances:"), maxInstancesText()},
        {tr("Color Formats:"), colorFormatsText()}};

    auto* summaryContainer = new QWidget(page);
    auto* summaryLayout = new QVBoxLayout(summaryContainer);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(10);

    bool hasSummaryRows = false;
    for (const SummaryItem& item : summaryItems) {
        if (item.value.trimmed().isEmpty()) {
            continue;
        }
        hasSummaryRows = true;
        summaryLayout->addWidget(createSummaryRow(item.title, item.value, summaryContainer));
    }
    if (hasSummaryRows) {
        pageLayout->addWidget(summaryContainer);
    } else {
        summaryContainer->deleteLater();
    }

    const QJsonArray profileRows =
        m_codecInfo.value(QStringLiteral("ProfileAndLevelInformation")).toArray();
    if (!profileRows.isEmpty()) {
        auto* tableContainer = new QWidget(page);
        auto* tableLayout = new QGridLayout(tableContainer);
        tableLayout->setContentsMargins(0, 0, 0, 0);
        tableLayout->setHorizontalSpacing(18);
        tableLayout->setVerticalSpacing(12);
        tableLayout->setColumnStretch(4, 1);

        const QList<QString> headers = {tr("Profile"),
                                        tr("Max Level"),
                                        tr("Max Bitrate"),
                                        tr("Bit Depths"),
                                        tr("Resolutions")};
        for (int column = 0; column < headers.size(); ++column) {
            auto* label =
                createTableLabel(headers.at(column), "ManageTranscodingFieldTitle",
                                 tableContainer, true);
            tableLayout->addWidget(label, 0, column);
        }

        int rowIndex = 1;
        for (const QJsonValue& rowValue : profileRows) {
            const QJsonObject rowObject = rowValue.toObject();
            const QJsonObject profile = rowObject.value(QStringLiteral("Profile")).toObject();
            const QJsonObject level = rowObject.value(QStringLiteral("Level")).toObject();

            const QList<QString> cells = {profileDescription(profile),
                                          levelDescription(level),
                                          levelBitrate(level),
                                          bitDepthsText(profile),
                                          levelResolutions(level)};
            for (int column = 0; column < cells.size(); ++column) {
                auto* label =
                    createTableLabel(cells.at(column), "ManageInfoValue", tableContainer);
                tableLayout->addWidget(label, rowIndex, column);
            }
            ++rowIndex;
        }

        pageLayout->addWidget(tableContainer);
    }

    pageLayout->addStretch(1);
}

QString EmbyCodecInfoDialog::codecTitle() const {
    const QString title = m_codecInfo.value(QStringLiteral("Name")).toString().trimmed();
    if (!title.isEmpty()) {
        return title;
    }
    return m_codecInfo.value(QStringLiteral("MediaTypeName")).toString().trimmed();
}

QString EmbyCodecInfoDialog::codecDescription() const {
    const QString description =
        m_codecInfo.value(QStringLiteral("Description")).toString().trimmed();
    if (!description.isEmpty()) {
        return description;
    }

    const QJsonObject deviceInfo = m_codecInfo.value(QStringLiteral("CodecDeviceInfo")).toObject();
    const QString fallbackDescription =
        deviceInfo.value(QStringLiteral("Desription")).toString().trimmed();
    if (!fallbackDescription.isEmpty()) {
        return fallbackDescription;
    }

    return deviceInfo.value(QStringLiteral("Description")).toString().trimmed();
}

QString EmbyCodecInfoDialog::maxBitrateText() const {
    return m_codecInfo.value(QStringLiteral("MaxBitRate")).toString().trimmed();
}

QString EmbyCodecInfoDialog::frameSizesText() const {
    const int minWidth = m_codecInfo.value(QStringLiteral("MinWidth")).toInt(0);
    const int minHeight = m_codecInfo.value(QStringLiteral("MinHeight")).toInt(0);
    const int maxWidth = m_codecInfo.value(QStringLiteral("MaxWidth")).toInt(0);
    const int maxHeight = m_codecInfo.value(QStringLiteral("MaxHeight")).toInt(0);

    if (minWidth > 0 && minHeight > 0 && maxWidth > 0 && maxHeight > 0) {
        return QStringLiteral("%1x%2...%3x%4")
            .arg(minWidth)
            .arg(minHeight)
            .arg(maxWidth)
            .arg(maxHeight);
    }
    if (maxWidth > 0 && maxHeight > 0) {
        return tr("max %1x%2").arg(maxWidth).arg(maxHeight);
    }
    if (minWidth > 0 && minHeight > 0) {
        return QStringLiteral("%1x%2").arg(minWidth).arg(minHeight);
    }
    return {};
}

QString EmbyCodecInfoDialog::frameRatesText() const {
    const int minFrameRate = m_codecInfo.value(QStringLiteral("MinFrameRate")).toInt(0);
    const int maxFrameRate = m_codecInfo.value(QStringLiteral("MaxFrameRate")).toInt(0);
    if (minFrameRate > 0 && maxFrameRate > 0) {
        return tr("%1fps...%2fps").arg(minFrameRate).arg(maxFrameRate);
    }
    if (maxFrameRate > 0) {
        return tr("max %1fps").arg(maxFrameRate);
    }
    if (minFrameRate > 0) {
        return tr("%1fps").arg(minFrameRate);
    }
    return {};
}

QString EmbyCodecInfoDialog::maxInstancesText() const {
    const int maxInstances = m_codecInfo.value(QStringLiteral("MaxInstanceCount")).toInt(0);
    return maxInstances > 0 ? QString::number(maxInstances) : QString();
}

QString EmbyCodecInfoDialog::colorFormatsText() const {
    QStringList formats;
    for (const QJsonValue& value :
         m_codecInfo.value(QStringLiteral("SupportedColorFormatStrings")).toArray()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            formats.append(text);
        }
    }
    return formats.join(QStringLiteral(", "));
}

QString EmbyCodecInfoDialog::profileDescription(const QJsonObject& profile) const {
    QString description = profile.value(QStringLiteral("Description")).toString().trimmed();
    description.replace(QStringLiteral(" Profile"), QString());
    if (!description.isEmpty()) {
        return description;
    }
    return profile.value(QStringLiteral("ShortName")).toString().trimmed();
}

QString EmbyCodecInfoDialog::levelDescription(const QJsonObject& level) const {
    const QString description = level.value(QStringLiteral("Description")).toString().trimmed();
    if (!description.isEmpty()) {
        return description;
    }
    return level.value(QStringLiteral("ShortName")).toString().trimmed();
}

QString EmbyCodecInfoDialog::levelBitrate(const QJsonObject& level) const {
    const QString display = level.value(QStringLiteral("MaxBitRateDisplay")).toString().trimmed();
    if (!display.isEmpty()) {
        return display;
    }
    return level.value(QStringLiteral("MaxBitRate")).toString().trimmed();
}

QString EmbyCodecInfoDialog::levelResolutions(const QJsonObject& level) const {
    QStringList values;
    for (const QJsonValue& value : level.value(QStringLiteral("ResolutionRateStrings")).toArray()) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }

    if (values.isEmpty()) {
        return level.value(QStringLiteral("ResolutionRatesDisplay")).toString().trimmed();
    }
    if (values.size() == 1) {
        return values.constFirst();
    }
    return QStringLiteral("%1 - %2").arg(values.constFirst(), values.constLast());
}

QString EmbyCodecInfoDialog::bitDepthsText(const QJsonObject& profile) const {
    QStringList values;
    for (const QJsonValue& value : profile.value(QStringLiteral("BitDepths")).toArray()) {
        if (value.isDouble()) {
            values.append(QString::number(value.toInt()));
        }
    }
    return values.join(QStringLiteral(", "));
}
