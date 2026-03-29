#include "pagetranscoding.h"

#include "../../components/embycodecinfodialog.h"
#include "../../components/embycodecparametersdialog.h"
#include "../../components/elidedlabel.h"
#include "../../components/flowlayout.h"
#include "../../components/moderncombobox.h"
#include "../../components/moderntaginput.h"
#include "../../components/moderntoast.h"
#include "../../components/modernswitch.h"
#include "../../components/transcodingcodeclistwidget.h"
#include "../../components/transcodingcodecrowwidget.h"
#include "../../utils/layoututils.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>

#include <QComboBox>
#include <QDebug>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QEasingCurve>
#include <QPixmap>
#include <QPushButton>
#include <QScrollArea>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QTransform>
#include <QVariantAnimation>
#include <QVBoxLayout>
#include <algorithm>
#include <limits>
#include <utility>

PageTranscoding::PageTranscoding(QEmbyCore* core, QWidget* parent)
    : ManagePageBase(core, tr("Transcoding"), parent) {
    if (m_core && m_core->serverManager()) {
        m_isJellyfin =
            m_core->serverManager()->activeProfile().type == ServerProfile::Jellyfin;
    }

    m_mainLayout->setContentsMargins(16, 30, 0, 0);

    setupToolbar();
    setupContentArea();
    buildPageLayout();
    connectInputs();

    m_contentContainer->hide();
}

void PageTranscoding::setupToolbar() {
    auto* titleRow = new QHBoxLayout();
    titleRow->setContentsMargins(0, 0, 20, 0);
    titleRow->setSpacing(6);

    m_btnRefresh = new QPushButton(this);
    m_btnRefresh->setObjectName("ManageRefreshBtn");
    m_btnRefresh->setCursor(Qt::PointingHandCursor);
    m_btnRefresh->setFixedSize(32, 32);
    m_btnRefresh->setToolTip(tr("Refresh"));
    titleRow->addWidget(m_btnRefresh);

    titleRow->addSpacing(6);

    m_btnSave = new QPushButton(this);
    m_btnSave->setObjectName("ManageSaveBtn");
    m_btnSave->setCursor(Qt::PointingHandCursor);
    m_btnSave->setFixedSize(32, 32);
    m_btnSave->setToolTip(tr("Save"));
    m_btnSave->setEnabled(false);
    titleRow->addWidget(m_btnSave);
    titleRow->addStretch(1);

    m_statsLabel = new QLabel(tr("Loading..."), this);
    m_statsLabel->setObjectName("ManageInfoValue");
    m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    titleRow->addWidget(m_statsLabel);

    m_mainLayout->addLayout(titleRow);
}

void PageTranscoding::setupContentArea() {
    m_scrollContentWidget = new QWidget(this);
    m_scrollContentWidget->setAttribute(Qt::WA_StyledBackground, true);

    auto* scrollLayout = new QVBoxLayout(m_scrollContentWidget);
    scrollLayout->setContentsMargins(0, 0, 16, 40);
    scrollLayout->setSpacing(16);

    m_contentContainer = new QWidget(m_scrollContentWidget);
    m_contentLayout = new QVBoxLayout(m_contentContainer);
    m_contentLayout->setContentsMargins(0, 0, 0, 0);
    m_contentLayout->setSpacing(16);
    scrollLayout->addWidget(m_contentContainer, 0, Qt::AlignTop);

    m_emptyStateContainer = new QWidget(m_scrollContentWidget);
    m_emptyStateContainer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    auto* emptyLayout = new QVBoxLayout(m_emptyStateContainer);
    emptyLayout->setContentsMargins(0, 24, 0, 0);
    emptyLayout->addStretch(1);

    m_emptyLabel =
        new QLabel(tr("No transcoding configuration available"), m_emptyStateContainer);
    m_emptyLabel->setObjectName("ManageEmptyLabel");
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLayout->addWidget(m_emptyLabel);
    emptyLayout->addStretch(1);
    m_emptyStateContainer->hide();
    scrollLayout->addWidget(m_emptyStateContainer, 1);

    m_scrollArea = new QScrollArea(this);
    m_scrollArea->setObjectName("SettingsScrollArea");
    m_scrollArea->setWidget(m_scrollContentWidget);
    m_scrollArea->setWidgetResizable(true);
    m_scrollArea->setFrameShape(QFrame::NoFrame);
    m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scrollArea->viewport()->setAutoFillBackground(false);
    m_mainLayout->addWidget(m_scrollArea, 1);
}

void PageTranscoding::buildPageLayout() {
    if (!m_contentLayout) {
        return;
    }

    if (m_isJellyfin) {
        buildJellyfinLayout(m_contentLayout);
    } else {
        buildEmbyLayout(m_contentLayout);
    }

    m_contentLayout->addStretch(1);
}

QWidget* PageTranscoding::createSectionCard(const QString& title,
                                            const QString& subtitle,
                                            QVBoxLayout*& bodyLayout,
                                            QWidget* parent) {
    auto* card = new QFrame(parent);
    card->setObjectName("TranscodingSectionCard");
    card->setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(card);
    layout->setContentsMargins(18, 16, 18, 18);
    layout->setSpacing(14);

    auto* titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("TranscodingSectionTitle");
    layout->addWidget(titleLabel);

    if (!subtitle.isEmpty()) {
        auto* subtitleLabel = new QLabel(subtitle, card);
        subtitleLabel->setObjectName("TranscodingSectionSubtitle");
        subtitleLabel->setWordWrap(true);
        layout->addWidget(subtitleLabel);
    }

    bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(12);
    layout->addLayout(bodyLayout);

    m_sectionCards.append(card);
    return card;
}

QWidget* PageTranscoding::createFieldBlock(const QString& title,
                                           const QString& description,
                                           QWidget* control,
                                           QWidget* parent,
                                           bool compactControl) {
    auto* block = new QFrame(parent);
    block->setObjectName("TranscodingFieldBlock");
    block->setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(block);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(8);

    if (description.trimmed().isEmpty()) {
        auto* topRow = new QHBoxLayout();
        topRow->setContentsMargins(0, 0, 0, 0);
        topRow->setSpacing(12);

        auto* titleLabel = new ElidedLabel(block);
        titleLabel->setObjectName("TranscodingFieldTitle");
        titleLabel->setFullText(title);
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        topRow->addWidget(titleLabel, 1, Qt::AlignVCenter);

        if (control) {
            topRow->addWidget(control, 0, compactControl ? Qt::AlignLeft : Qt::AlignVCenter);
        }

        layout->addLayout(topRow);
        return block;
    }

    auto* titleLabel = new QLabel(title, block);
    titleLabel->setObjectName("TranscodingFieldTitle");
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    if (!description.isEmpty()) {
        auto* descriptionLabel = new QLabel(description, block);
        descriptionLabel->setObjectName("TranscodingFieldDescription");
        descriptionLabel->setWordWrap(true);
        layout->addWidget(descriptionLabel);
    }

    if (control) {
        layout->addWidget(control, 0, compactControl ? Qt::AlignLeft : Qt::AlignTop);
    }

    return block;
}

QWidget* PageTranscoding::createSwitchBlock(const QString& title,
                                            const QString& description,
                                            ModernSwitch* control,
                                            QWidget* parent) {
    auto* block = new QFrame(parent);
    block->setObjectName("TranscodingFieldBlock");
    block->setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QHBoxLayout(block);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(12);

    auto* textLayout = new QVBoxLayout();
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(4);

    if (description.trimmed().isEmpty()) {
        auto* titleLabel = new ElidedLabel(block);
        titleLabel->setObjectName("TranscodingFieldTitle");
        titleLabel->setFullText(title);
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        textLayout->addWidget(titleLabel);
    } else {
        auto* titleLabel = new QLabel(title, block);
        titleLabel->setObjectName("TranscodingFieldTitle");
        titleLabel->setWordWrap(true);
        textLayout->addWidget(titleLabel);
    }

    if (!description.isEmpty()) {
        auto* descriptionLabel = new QLabel(description, block);
        descriptionLabel->setObjectName("TranscodingFieldDescription");
        descriptionLabel->setWordWrap(true);
        textLayout->addWidget(descriptionLabel);
    }

    layout->addLayout(textLayout, 1);
    if (control) {
        layout->addWidget(control, 0,
                          description.trimmed().isEmpty() ? Qt::AlignVCenter
                                                          : Qt::AlignTop);
    }

    return block;
}

QWidget* PageTranscoding::createGroupBlock(const QString& title,
                                           const QString& description,
                                           QVBoxLayout*& bodyLayout,
                                           QWidget* parent) {
    auto* block = new QFrame(parent);
    block->setObjectName("TranscodingGroupBlock");
    block->setAttribute(Qt::WA_StyledBackground, true);

    auto* layout = new QVBoxLayout(block);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* titleLabel = new QLabel(title, block);
    titleLabel->setObjectName("TranscodingGroupTitle");
    titleLabel->setWordWrap(true);
    layout->addWidget(titleLabel);

    if (!description.isEmpty()) {
        auto* descriptionLabel = new QLabel(description, block);
        descriptionLabel->setObjectName("TranscodingGroupDescription");
        descriptionLabel->setWordWrap(true);
        layout->addWidget(descriptionLabel);
    }

    bodyLayout = new QVBoxLayout();
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(10);
    layout->addLayout(bodyLayout);

    return block;
}

QLineEdit* PageTranscoding::createTextInput(int width, bool readOnly) {
    auto* edit = new QLineEdit(this);
    edit->setObjectName("ManageLibInput");
    edit->setMinimumWidth(width);
    edit->setClearButtonEnabled(!readOnly);
    edit->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    edit->setReadOnly(readOnly);
    if (readOnly) {
        edit->setFocusPolicy(Qt::NoFocus);
    }
    return edit;
}

QSpinBox* PageTranscoding::createSpinBox(int minimum, int maximum, int singleStep, int width) {
    auto* spin = new QSpinBox(this);
    spin->setObjectName("ManageLibSpinBox");
    spin->setRange(minimum, maximum);
    spin->setSingleStep(singleStep);
    spin->setFixedWidth(width);
    spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return spin;
}

QDoubleSpinBox* PageTranscoding::createDoubleSpinBox(double minimum,
                                                     double maximum,
                                                     double singleStep,
                                                     int decimals,
                                                     int width) {
    auto* spin = new QDoubleSpinBox(this);
    spin->setObjectName("ManageLibSpinBox");
    spin->setRange(minimum, maximum);
    spin->setSingleStep(singleStep);
    spin->setDecimals(decimals);
    spin->setFixedWidth(width);
    spin->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    return spin;
}

ModernComboBox* PageTranscoding::createComboBox(int width) {
    auto* combo = LayoutUtils::createStyledCombo(this);
    combo->setFixedWidth(width);
    combo->setMaxTextWidth(width - 42);
    combo->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return combo;
}

ModernTagInput* PageTranscoding::createTagInput(const QStringList& presetValues) {
    auto* input = new ModernTagInput(this);
    input->setFixedWidth(252);
    input->setPopupMode(ModernTagInput::ForcePopup);
    input->setMaxPopupHeight(240);
    for (const QString& presetValue : presetValues) {
        input->addPreset(presetValue, presetValue);
    }
    return input;
}

void PageTranscoding::addComboOption(ModernComboBox* combo,
                                     const QString& text,
                                     const QVariant& value) {
    if (combo) {
        combo->addItem(text, value);
    }
}

void PageTranscoding::setComboCurrentValue(ModernComboBox* combo, const QVariant& value) {
    if (!combo) {
        return;
    }

    int index = combo->findData(value);
    if (index < 0) {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            combo->addItem(text, value);
            index = combo->count() - 1;
        }
    }

    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else if (combo->count() > 0) {
        combo->setCurrentIndex(0);
    }
}

QVariant PageTranscoding::comboCurrentValue(ModernComboBox* combo) const {
    if (!combo || combo->currentIndex() < 0) {
        return {};
    }
    return combo->currentData();
}

QStringList PageTranscoding::stringListFromJsonValue(const QJsonValue& value) const {
    QStringList values;
    if (value.isArray()) {
        for (const QJsonValue& entry : value.toArray()) {
            const QString text = entry.toString().trimmed();
            if (!text.isEmpty() && !values.contains(text)) {
                values.append(text);
            }
        }
    } else {
        const QString text = value.toString().trimmed();
        if (!text.isEmpty()) {
            values.append(text);
        }
    }
    return values;
}

QJsonArray PageTranscoding::jsonArrayFromStringList(const QStringList& values) const {
    QJsonArray result;
    QStringList deduplicated;
    for (const QString& rawValue : values) {
        const QString value = rawValue.trimmed();
        if (!value.isEmpty() && !deduplicated.contains(value)) {
            deduplicated.append(value);
            result.append(value);
        }
    }
    return result;
}

void PageTranscoding::addBlock(QVBoxLayout* body,
                               QWidget* sectionCard,
                               const QString& key,
                               QWidget* block,
                               bool actualKey) {
    if (!body || !sectionCard || !block || key.isEmpty()) {
        return;
    }

    body->addWidget(block);
    m_blocks.insert(key, block);
    m_sectionBlocks[sectionCard].append(key);
    if (actualKey) {
        m_actualBlockKeys.insert(key);
    }
}

void PageTranscoding::setBlockVisible(const QString& key, bool visible) {
    if (auto* block = m_blocks.value(key)) {
        block->setVisible(visible);
    }
}

void PageTranscoding::buildJellyfinLayout(QVBoxLayout* contentLayout) {
    auto createCompactDecoderSwitchBlock =
        [](const QString& title, ModernSwitch* control, QWidget* parent) -> QWidget* {
        auto* block = new QFrame(parent);
        block->setObjectName("TranscodingFieldBlock");
        block->setAttribute(Qt::WA_StyledBackground, true);
        block->setFixedWidth(128);
        block->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

        auto* layout = new QHBoxLayout(block);
        layout->setContentsMargins(12, 10, 12, 10);
        layout->setSpacing(10);

        auto* titleLabel = new ElidedLabel(block);
        titleLabel->setObjectName("TranscodingFieldTitle");
        titleLabel->setFullText(title);
        titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
        layout->addWidget(titleLabel, 1, Qt::AlignVCenter);

        if (control) {
            control->setParent(block);
            layout->addWidget(control, 0, Qt::AlignVCenter);
        }

        return block;
    };

    QVBoxLayout* hardwareBody = nullptr;
    QWidget* hardwareCard =
        createSectionCard(tr("Hardware Acceleration"), QString(), hardwareBody, m_contentContainer);
    contentLayout->addWidget(hardwareCard);

    auto* hardwareTypeCombo = createComboBox();
    addComboOption(hardwareTypeCombo, tr("None"), QStringLiteral("none"));
    addComboOption(hardwareTypeCombo, tr("AMF"), QStringLiteral("amf"));
    addComboOption(hardwareTypeCombo, tr("NVENC"), QStringLiteral("nvenc"));
    addComboOption(hardwareTypeCombo, tr("QSV"), QStringLiteral("qsv"));
    addComboOption(hardwareTypeCombo, tr("VAAPI"), QStringLiteral("vaapi"));
    addComboOption(hardwareTypeCombo, tr("RKMPP"), QStringLiteral("rkmpp"));
    addComboOption(hardwareTypeCombo, tr("VideoToolbox"), QStringLiteral("videotoolbox"));
    addComboOption(hardwareTypeCombo, tr("V4L2M2M"), QStringLiteral("v4l2m2m"));
    m_comboBoxes.insert(QStringLiteral("HardwareAccelerationType"), hardwareTypeCombo);
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("HardwareAccelerationType"),
             createFieldBlock(tr("Hardware Acceleration Type"),
                              QString(),
                              hardwareTypeCombo,
                              hardwareCard));

    m_lineEdits.insert(QStringLiteral("VaapiDevice"), createTextInput());
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("VaapiDevice"),
             createFieldBlock(tr("VAAPI Device"),
                              QString(),
                              m_lineEdits.value(QStringLiteral("VaapiDevice")),
                              hardwareCard));

    m_lineEdits.insert(QStringLiteral("QsvDevice"), createTextInput());
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("QsvDevice"),
             createFieldBlock(tr("QSV Device"),
                              QString(),
                              m_lineEdits.value(QStringLiteral("QsvDevice")),
                              hardwareCard));

    QVBoxLayout* decodingGroupBody = nullptr;
    QWidget* decodingGroup =
        createGroupBlock(tr("Hardware Decoding"), QString(), decodingGroupBody, hardwareCard);
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("@jellyfin_decoding_codecs"),
             decodingGroup,
             false);

    auto* decodingFlow = new FlowLayout(0, 10, 10);
    const QStringList decoderCodecs = knownJellyfinDecoderCodecs();
    for (const QString& codec : decoderCodecs) {
        auto* codecSwitch = new ModernSwitch(decodingGroup);
        codecSwitch->setCursor(Qt::PointingHandCursor);
        m_jellyfinCodecSwitches.insert(codec, codecSwitch);

        QWidget* codecBlock =
            createCompactDecoderSwitchBlock(jellyfinCodecLabel(codec), codecSwitch, decodingGroup);
        m_jellyfinCodecBlocks.insert(codec, codecBlock);
        decodingFlow->addWidget(codecBlock);
    }
    decodingGroupBody->addLayout(decodingFlow);

    QVBoxLayout* depthGroupBody = nullptr;
    QWidget* depthGroup =
        createGroupBlock(tr("10-bit Decoding"), QString(), depthGroupBody, hardwareCard);
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("@jellyfin_depth_group"),
             depthGroup,
             false);

    m_switches.insert(QStringLiteral("EnableDecodingColorDepth10Hevc"),
                      new ModernSwitch(depthGroup));
    addBlock(depthGroupBody,
             hardwareCard,
             QStringLiteral("EnableDecodingColorDepth10Hevc"),
             createSwitchBlock(tr("HEVC 10-bit"), QString(),
                               m_switches.value(QStringLiteral("EnableDecodingColorDepth10Hevc")),
                               depthGroup));

    m_switches.insert(QStringLiteral("EnableDecodingColorDepth10Vp9"),
                      new ModernSwitch(depthGroup));
    addBlock(depthGroupBody,
             hardwareCard,
             QStringLiteral("EnableDecodingColorDepth10Vp9"),
             createSwitchBlock(tr("VP9 10-bit"), QString(),
                               m_switches.value(QStringLiteral("EnableDecodingColorDepth10Vp9")),
                               depthGroup));

    QVBoxLayout* rextGroupBody = nullptr;
    QWidget* rextGroup =
        createGroupBlock(tr("HEVC RExt Decoding"), QString(), rextGroupBody, hardwareCard);
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("@jellyfin_rext_group"),
             rextGroup,
             false);

    m_switches.insert(QStringLiteral("EnableDecodingColorDepth10HevcRext"),
                      new ModernSwitch(rextGroup));
    addBlock(rextGroupBody,
             hardwareCard,
             QStringLiteral("EnableDecodingColorDepth10HevcRext"),
             createSwitchBlock(
                 tr("HEVC RExt 8/10-bit"),
                 QString(),
                 m_switches.value(QStringLiteral("EnableDecodingColorDepth10HevcRext")),
                 rextGroup));

    m_switches.insert(QStringLiteral("EnableDecodingColorDepth12HevcRext"),
                      new ModernSwitch(rextGroup));
    addBlock(rextGroupBody,
             hardwareCard,
             QStringLiteral("EnableDecodingColorDepth12HevcRext"),
             createSwitchBlock(
                 tr("HEVC RExt 12-bit"),
                 QString(),
                 m_switches.value(QStringLiteral("EnableDecodingColorDepth12HevcRext")),
                 rextGroup));

    m_switches.insert(QStringLiteral("PreferSystemNativeHwDecoder"),
                      new ModernSwitch(hardwareCard));
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("PreferSystemNativeHwDecoder"),
             createSwitchBlock(tr("Prefer System Native Decoder"),
                               QString(),
                               m_switches.value(QStringLiteral("PreferSystemNativeHwDecoder")),
                               hardwareCard));

    m_switches.insert(QStringLiteral("EnableEnhancedNvdecDecoder"),
                      new ModernSwitch(hardwareCard));
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("EnableEnhancedNvdecDecoder"),
             createSwitchBlock(tr("Enhanced NVDEC"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableEnhancedNvdecDecoder")),
                               hardwareCard));

    QVBoxLayout* encodingGroupBody = nullptr;
    QWidget* encodingGroup =
        createGroupBlock(tr("Hardware Encoding"), QString(), encodingGroupBody, hardwareCard);
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("@jellyfin_hardware_encoding_group"),
             encodingGroup,
             false);

    m_switches.insert(QStringLiteral("EnableHardwareEncoding"),
                      new ModernSwitch(encodingGroup));
    addBlock(encodingGroupBody,
             hardwareCard,
             QStringLiteral("EnableHardwareEncoding"),
             createSwitchBlock(tr("Enable Hardware Encoding"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableHardwareEncoding")),
                               encodingGroup));

    m_switches.insert(QStringLiteral("EnableIntelLowPowerH264HwEncoder"),
                      new ModernSwitch(encodingGroup));
    addBlock(
        encodingGroupBody,
        hardwareCard,
        QStringLiteral("EnableIntelLowPowerH264HwEncoder"),
        createSwitchBlock(tr("Intel Low Power H.264"),
                          QString(),
                          m_switches.value(QStringLiteral("EnableIntelLowPowerH264HwEncoder")),
                          encodingGroup));

    m_switches.insert(QStringLiteral("EnableIntelLowPowerHevcHwEncoder"),
                      new ModernSwitch(encodingGroup));
    addBlock(
        encodingGroupBody,
        hardwareCard,
        QStringLiteral("EnableIntelLowPowerHevcHwEncoder"),
        createSwitchBlock(tr("Intel Low Power HEVC"),
                          QString(),
                          m_switches.value(QStringLiteral("EnableIntelLowPowerHevcHwEncoder")),
                          encodingGroup));

    QVBoxLayout* formatsGroupBody = nullptr;
    QWidget* formatsGroup =
        createGroupBlock(tr("Encoding Formats"), QString(), formatsGroupBody, hardwareCard);
    addBlock(hardwareBody,
             hardwareCard,
             QStringLiteral("@jellyfin_encoding_formats_group"),
             formatsGroup,
             false);

    m_switches.insert(QStringLiteral("AllowHevcEncoding"), new ModernSwitch(formatsGroup));
    addBlock(formatsGroupBody,
             hardwareCard,
             QStringLiteral("AllowHevcEncoding"),
             createSwitchBlock(tr("Allow HEVC Encoding"),
                               QString(),
                               m_switches.value(QStringLiteral("AllowHevcEncoding")),
                               formatsGroup));

    m_switches.insert(QStringLiteral("AllowAv1Encoding"), new ModernSwitch(formatsGroup));
    addBlock(formatsGroupBody,
             hardwareCard,
             QStringLiteral("AllowAv1Encoding"),
             createSwitchBlock(tr("Allow AV1 Encoding"),
                               QString(),
                               m_switches.value(QStringLiteral("AllowAv1Encoding")),
                               formatsGroup));

    QVBoxLayout* toneBody = nullptr;
    QWidget* toneCard =
        createSectionCard(tr("Tone Mapping"), QString(), toneBody, m_contentContainer);
    contentLayout->addWidget(toneCard);

    m_switches.insert(QStringLiteral("EnableTonemapping"), new ModernSwitch(toneCard));
    addBlock(toneBody,
             toneCard,
             QStringLiteral("EnableTonemapping"),
             createSwitchBlock(tr("Enable Tonemapping"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableTonemapping")),
                               toneCard));

    m_switches.insert(QStringLiteral("EnableVppTonemapping"), new ModernSwitch(toneCard));
    addBlock(toneBody,
             toneCard,
             QStringLiteral("EnableVppTonemapping"),
             createSwitchBlock(tr("Enable VPP Tonemapping"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableVppTonemapping")),
                               toneCard));

    m_switches.insert(QStringLiteral("EnableVideoToolboxTonemapping"),
                      new ModernSwitch(toneCard));
    addBlock(toneBody,
             toneCard,
             QStringLiteral("EnableVideoToolboxTonemapping"),
             createSwitchBlock(tr("Enable VideoToolbox Tonemapping"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableVideoToolboxTonemapping")),
                               toneCard));

    auto* algorithmCombo = createComboBox();
    addComboOption(algorithmCombo, tr("None"), QStringLiteral("none"));
    addComboOption(algorithmCombo, tr("BT.2390"), QStringLiteral("bt2390"));
    addComboOption(algorithmCombo, tr("Hable"), QStringLiteral("hable"));
    addComboOption(algorithmCombo, tr("Reinhard"), QStringLiteral("reinhard"));
    addComboOption(algorithmCombo, tr("Mobius"), QStringLiteral("mobius"));
    addComboOption(algorithmCombo, tr("Clip"), QStringLiteral("clip"));
    addComboOption(algorithmCombo, tr("Linear"), QStringLiteral("linear"));
    addComboOption(algorithmCombo, tr("Gamma"), QStringLiteral("gamma"));
    m_comboBoxes.insert(QStringLiteral("TonemappingAlgorithm"), algorithmCombo);
    addBlock(toneBody,
             toneCard,
             QStringLiteral("TonemappingAlgorithm"),
             createFieldBlock(tr("Algorithm"), QString(), algorithmCombo, toneCard, true));

    auto* toneModeCombo = createComboBox(180);
    addComboOption(toneModeCombo, tr("Auto"), QStringLiteral("auto"));
    addComboOption(toneModeCombo, tr("MAX"), QStringLiteral("max"));
    addComboOption(toneModeCombo, tr("RGB"), QStringLiteral("rgb"));
    addComboOption(toneModeCombo, tr("LUM"), QStringLiteral("lum"));
    addComboOption(toneModeCombo, tr("ITP"), QStringLiteral("itp"));
    m_comboBoxes.insert(QStringLiteral("TonemappingMode"), toneModeCombo);
    addBlock(toneBody,
             toneCard,
             QStringLiteral("TonemappingMode"),
             createFieldBlock(tr("Mode"),
                              QString(),
                              toneModeCombo,
                              toneCard,
                              true));

    auto* toneRangeCombo = createComboBox(180);
    addComboOption(toneRangeCombo, tr("Auto"), QStringLiteral("auto"));
    addComboOption(toneRangeCombo, tr("TV"), QStringLiteral("tv"));
    addComboOption(toneRangeCombo, tr("PC"), QStringLiteral("pc"));
    m_comboBoxes.insert(QStringLiteral("TonemappingRange"), toneRangeCombo);
    addBlock(toneBody,
             toneCard,
             QStringLiteral("TonemappingRange"),
             createFieldBlock(tr("Range"),
                              QString(),
                              toneRangeCombo,
                              toneCard,
                              true));

    m_doubleSpinBoxes.insert(QStringLiteral("TonemappingDesat"),
                             createDoubleSpinBox(0.0, 1000000.0, 0.00001, 5));
    addBlock(toneBody,
             toneCard,
             QStringLiteral("TonemappingDesat"),
             createFieldBlock(tr("Desaturation"),
                              QString(),
                              m_doubleSpinBoxes.value(QStringLiteral("TonemappingDesat")),
                              toneCard,
                              true));

    m_doubleSpinBoxes.insert(QStringLiteral("TonemappingPeak"),
                             createDoubleSpinBox(0.0, 1000000.0, 0.00001, 5));
    addBlock(toneBody,
             toneCard,
             QStringLiteral("TonemappingPeak"),
             createFieldBlock(tr("Peak"),
                              QString(),
                              m_doubleSpinBoxes.value(QStringLiteral("TonemappingPeak")),
                              toneCard,
                              true));

    m_doubleSpinBoxes.insert(QStringLiteral("TonemappingParam"),
                             createDoubleSpinBox(-1000000.0, 1000000.0, 0.00001, 5));
    addBlock(toneBody,
             toneCard,
             QStringLiteral("TonemappingParam"),
             createFieldBlock(tr("Parameter"),
                              QString(),
                              m_doubleSpinBoxes.value(QStringLiteral("TonemappingParam")),
                              toneCard,
                              true));

    QVBoxLayout* vppBody = nullptr;
    QWidget* vppGroup =
        createGroupBlock(tr("VPP Tone Mapping"), QString(), vppBody, toneCard);
    addBlock(toneBody, toneCard, QStringLiteral("@jellyfin_vpp_group"), vppGroup, false);

    m_doubleSpinBoxes.insert(QStringLiteral("VppTonemappingBrightness"),
                             createDoubleSpinBox(0.0, 100.0, 0.00001, 5));
    addBlock(vppBody,
             toneCard,
             QStringLiteral("VppTonemappingBrightness"),
             createFieldBlock(tr("VPP Brightness"),
                              QString(),
                              m_doubleSpinBoxes.value(QStringLiteral("VppTonemappingBrightness")),
                              vppGroup,
                              true));

    m_doubleSpinBoxes.insert(QStringLiteral("VppTonemappingContrast"),
                             createDoubleSpinBox(1.0, 2.0, 0.00001, 5));
    addBlock(vppBody,
             toneCard,
             QStringLiteral("VppTonemappingContrast"),
             createFieldBlock(tr("VPP Contrast"),
                              QString(),
                              m_doubleSpinBoxes.value(QStringLiteral("VppTonemappingContrast")),
                              vppGroup,
                              true));

    QVBoxLayout* pathsBody = nullptr;
    QWidget* pathsCard =
        createSectionCard(tr("Paths and Threads"), QString(), pathsBody, m_contentContainer);
    contentLayout->addWidget(pathsCard);

    auto* encodingThreads = createSpinBox(-1, 256, 1, 168);
    encodingThreads->setSpecialValueText(tr("Auto"));
    m_spinBoxes.insert(QStringLiteral("EncodingThreadCount"), encodingThreads);
    addBlock(pathsBody,
             pathsCard,
             QStringLiteral("EncodingThreadCount"),
             createFieldBlock(tr("Encoding Threads"),
                              QString(),
                              encodingThreads,
                              pathsCard,
                              true));

    m_lineEdits.insert(QStringLiteral("TranscodingTempPath"), createTextInput());
    addBlock(pathsBody,
             pathsCard,
             QStringLiteral("TranscodingTempPath"),
             createFieldBlock(tr("Transcoding Temp Path"),
                              QString(),
                              m_lineEdits.value(QStringLiteral("TranscodingTempPath")),
                              pathsCard));

    m_encoderAppPathDisplay = createTextInput(240, true);
    addBlock(pathsBody,
             pathsCard,
             QStringLiteral("EncoderAppPathDisplay"),
             createFieldBlock(tr("FFmpeg Path"), QString(), m_encoderAppPathDisplay, pathsCard));

    m_lineEdits.insert(QStringLiteral("FallbackFontPath"), createTextInput());
    addBlock(pathsBody,
             pathsCard,
             QStringLiteral("FallbackFontPath"),
             createFieldBlock(tr("Fallback Font Path"),
                              QString(),
                              m_lineEdits.value(QStringLiteral("FallbackFontPath")),
                              pathsCard));

    m_switches.insert(QStringLiteral("EnableFallbackFont"), new ModernSwitch(pathsCard));
    addBlock(pathsBody,
             pathsCard,
             QStringLiteral("EnableFallbackFont"),
             createSwitchBlock(tr("Enable Fallback Font"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableFallbackFont")),
                               pathsCard));

    m_spinBoxes.insert(QStringLiteral("MaxMuxingQueueSize"), createSpinBox(0, 1000000));
    addBlock(pathsBody,
             pathsCard,
             QStringLiteral("MaxMuxingQueueSize"),
             createFieldBlock(tr("Max Muxing Queue"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("MaxMuxingQueueSize")),
                              pathsCard,
                              true));

    QVBoxLayout* qualityBody = nullptr;
    QWidget* qualityCard =
        createSectionCard(tr("Quality and Processing"), QString(), qualityBody, m_contentContainer);
    contentLayout->addWidget(qualityCard);

    m_switches.insert(QStringLiteral("EnableAudioVbr"), new ModernSwitch(qualityCard));
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("EnableAudioVbr"),
             createSwitchBlock(tr("Audio VBR"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableAudioVbr")),
                               qualityCard));

    auto* downMixStereoCombo = createComboBox();
    addComboOption(downMixStereoCombo, tr("None"), QStringLiteral("None"));
    addComboOption(downMixStereoCombo, tr("Dave750"), QStringLiteral("Dave750"));
    addComboOption(downMixStereoCombo,
                   tr("NightmodeDialogue"),
                   QStringLiteral("NightmodeDialogue"));
    addComboOption(downMixStereoCombo, tr("RFC7845"), QStringLiteral("Rfc7845"));
    addComboOption(downMixStereoCombo, tr("AC-4"), QStringLiteral("Ac4"));
    m_comboBoxes.insert(QStringLiteral("DownMixStereoAlgorithm"), downMixStereoCombo);
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("DownMixStereoAlgorithm"),
             createFieldBlock(tr("Stereo Downmix"),
                              QString(),
                              downMixStereoCombo,
                              qualityCard));

    m_doubleSpinBoxes.insert(QStringLiteral("DownMixAudioBoost"),
                             createDoubleSpinBox(0.5, 3.0, 0.1, 1));
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("DownMixAudioBoost"),
             createFieldBlock(tr("Downmix Audio Boost"),
                              QString(),
                              m_doubleSpinBoxes.value(QStringLiteral("DownMixAudioBoost")),
                              qualityCard,
                              true));

    auto* presetCombo = createComboBox();
    addComboOption(presetCombo, tr("Auto"), QStringLiteral("auto"));
    addComboOption(presetCombo, tr("Ultra Fast"), QStringLiteral("ultrafast"));
    addComboOption(presetCombo, tr("Super Fast"), QStringLiteral("superfast"));
    addComboOption(presetCombo, tr("Very Fast"), QStringLiteral("veryfast"));
    addComboOption(presetCombo, tr("Faster"), QStringLiteral("faster"));
    addComboOption(presetCombo, tr("Fast"), QStringLiteral("fast"));
    addComboOption(presetCombo, tr("Medium"), QStringLiteral("medium"));
    addComboOption(presetCombo, tr("Slow"), QStringLiteral("slow"));
    addComboOption(presetCombo, tr("Slower"), QStringLiteral("slower"));
    addComboOption(presetCombo, tr("Very Slow"), QStringLiteral("veryslow"));
    m_comboBoxes.insert(QStringLiteral("EncoderPreset"), presetCombo);
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("EncoderPreset"),
             createFieldBlock(tr("Encoder Preset"),
                              QString(),
                              presetCombo,
                              qualityCard));

    m_spinBoxes.insert(QStringLiteral("H264Crf"), createSpinBox(0, 51));
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("H264Crf"),
             createFieldBlock(tr("H.264 CRF"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("H264Crf")),
                              qualityCard,
                              true));

    m_spinBoxes.insert(QStringLiteral("H265Crf"), createSpinBox(0, 51));
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("H265Crf"),
             createFieldBlock(tr("H.265 CRF"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("H265Crf")),
                              qualityCard,
                              true));

    auto* deinterlaceCombo = createComboBox();
    addComboOption(deinterlaceCombo, tr("YADIF"), QStringLiteral("yadif"));
    addComboOption(deinterlaceCombo, tr("BWDIF"), QStringLiteral("bwdif"));
    m_comboBoxes.insert(QStringLiteral("DeinterlaceMethod"), deinterlaceCombo);
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("DeinterlaceMethod"),
             createFieldBlock(tr("Deinterlace Method"),
                              QString(),
                              deinterlaceCombo,
                              qualityCard));

    m_switches.insert(QStringLiteral("DeinterlaceDoubleRate"), new ModernSwitch(qualityCard));
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("DeinterlaceDoubleRate"),
             createSwitchBlock(tr("Double-Rate Deinterlace"),
                               QString(),
                               m_switches.value(QStringLiteral("DeinterlaceDoubleRate")),
                               qualityCard));

    m_switches.insert(QStringLiteral("EnableSubtitleExtraction"),
                      new ModernSwitch(qualityCard));
    addBlock(qualityBody,
             qualityCard,
             QStringLiteral("EnableSubtitleExtraction"),
             createSwitchBlock(tr("Enable Subtitle Extraction"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableSubtitleExtraction")),
                               qualityCard));

    QVBoxLayout* throttlingBody = nullptr;
    QWidget* throttlingCard = createSectionCard(
        tr("Throttling and Segments"), QString(), throttlingBody, m_contentContainer);
    contentLayout->addWidget(throttlingCard);

    m_switches.insert(QStringLiteral("EnableThrottling"), new ModernSwitch(throttlingCard));
    addBlock(throttlingBody,
             throttlingCard,
             QStringLiteral("EnableThrottling"),
             createSwitchBlock(tr("Enable Throttling"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableThrottling")),
                               throttlingCard));

    m_spinBoxes.insert(QStringLiteral("ThrottleDelaySeconds"), createSpinBox(0, 100000));
    addBlock(throttlingBody,
             throttlingCard,
             QStringLiteral("ThrottleDelaySeconds"),
             createFieldBlock(tr("Throttle Delay"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("ThrottleDelaySeconds")),
                              throttlingCard,
                              true));

    m_switches.insert(QStringLiteral("EnableSegmentDeletion"),
                      new ModernSwitch(throttlingCard));
    addBlock(throttlingBody,
             throttlingCard,
             QStringLiteral("EnableSegmentDeletion"),
             createSwitchBlock(tr("Enable Segment Deletion"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableSegmentDeletion")),
                               throttlingCard));

    m_spinBoxes.insert(QStringLiteral("SegmentKeepSeconds"), createSpinBox(0, 1000000));
    addBlock(throttlingBody,
             throttlingCard,
             QStringLiteral("SegmentKeepSeconds"),
             createFieldBlock(tr("Segment Keep Seconds"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("SegmentKeepSeconds")),
                              throttlingCard,
                              true));

    m_tagInputs.insert(
        QStringLiteral("AllowOnDemandMetadataBasedKeyframeExtractionForExtensions"),
        createTagInput({QStringLiteral("mkv"), QStringLiteral("ts"), QStringLiteral("mpg")}));
    addBlock(
        throttlingBody,
        throttlingCard,
        QStringLiteral("AllowOnDemandMetadataBasedKeyframeExtractionForExtensions"),
        createFieldBlock(
            tr("Metadata Keyframe Extensions"),
            QString(),
            m_tagInputs.value(
                QStringLiteral("AllowOnDemandMetadataBasedKeyframeExtractionForExtensions")),
            throttlingCard));
}

void PageTranscoding::buildEmbyLayout(QVBoxLayout* contentLayout) {
    auto* modeCombo = createComboBox();
    addComboOption(modeCombo, tr("Yes"), 1);
    addComboOption(modeCombo, tr("No"), 0);
    addComboOption(modeCombo, tr("Advanced"), 2);
    m_comboBoxes.insert(QStringLiteral("HardwareAccelerationMode"), modeCombo);

    auto* hardwareCard = new QFrame(m_contentContainer);
    hardwareCard->setObjectName("TranscodingSectionCard");
    hardwareCard->setAttribute(Qt::WA_StyledBackground, true);

    auto* hardwareLayout = new QVBoxLayout(hardwareCard);
    hardwareLayout->setContentsMargins(18, 16, 18, 18);
    hardwareLayout->setSpacing(0);

    auto* hardwareRow = new QWidget(hardwareCard);
    auto* hardwareRowLayout = new QHBoxLayout(hardwareRow);
    hardwareRowLayout->setContentsMargins(0, 0, 0, 0);
    hardwareRowLayout->setSpacing(16);

    auto* hardwareTitleLabel = new QLabel(tr("Hardware Acceleration"), hardwareRow);
    hardwareTitleLabel->setObjectName("TranscodingSectionTitle");
    hardwareTitleLabel->setWordWrap(true);
    hardwareRowLayout->addWidget(hardwareTitleLabel, 1);
    hardwareRowLayout->addWidget(modeCombo, 0, Qt::AlignTop);

    hardwareLayout->addWidget(hardwareRow);
    contentLayout->addWidget(hardwareCard);

    m_sectionCards.append(hardwareCard);
    m_blocks.insert(QStringLiteral("HardwareAccelerationMode"), hardwareRow);
    m_sectionBlocks[hardwareCard].append(QStringLiteral("HardwareAccelerationMode"));
    m_actualBlockKeys.insert(QStringLiteral("HardwareAccelerationMode"));

    QVBoxLayout* decoderBody = nullptr;
    QWidget* decoderCard = createSectionCard(
        tr("Preferred Hardware Decoders"), QString(), decoderBody, m_contentContainer);
    contentLayout->addWidget(decoderCard);

    auto* decoderContainer = new QWidget(decoderCard);
    m_embyDecoderLayout = new QVBoxLayout(decoderContainer);
    m_embyDecoderLayout->setContentsMargins(0, 0, 0, 0);
    m_embyDecoderLayout->setSpacing(12);
    addBlock(decoderBody,
             decoderCard,
             QStringLiteral("@emby_decoder_codecs"),
             decoderContainer,
             false);

    QVBoxLayout* encoderBody = nullptr;
    QWidget* encoderCard = createSectionCard(
        tr("Preferred Hardware Encoders"), QString(), encoderBody, m_contentContainer);
    contentLayout->addWidget(encoderCard);

    auto* encoderContainer = new QWidget(encoderCard);
    m_embyEncoderLayout = new QVBoxLayout(encoderContainer);
    m_embyEncoderLayout->setContentsMargins(0, 0, 0, 0);
    m_embyEncoderLayout->setSpacing(12);
    addBlock(encoderBody,
             encoderCard,
             QStringLiteral("@emby_encoder_codecs"),
             encoderContainer,
             false);

    QVBoxLayout* softwareBody = nullptr;
    QWidget* softwareCard =
        createSectionCard(tr("Software Encoders"), QString(), softwareBody, m_contentContainer);
    contentLayout->addWidget(softwareCard);

    auto* softwareContainer = new QWidget(softwareCard);
    m_embySoftwareLayout = new QVBoxLayout(softwareContainer);
    m_embySoftwareLayout->setContentsMargins(0, 0, 0, 0);
    m_embySoftwareLayout->setSpacing(12);
    addBlock(softwareBody,
             softwareCard,
             QStringLiteral("@emby_software_encoders"),
             softwareContainer,
             false);

    QVBoxLayout* transcodingBody = nullptr;
    QWidget* transcodingCard =
        createSectionCard(tr("Transcoding"), QString(), transcodingBody, m_contentContainer);
    contentLayout->addWidget(transcodingCard);

    m_lineEdits.insert(QStringLiteral("TranscodingTempPath"), createTextInput());
    addBlock(transcodingBody,
             transcodingCard,
             QStringLiteral("TranscodingTempPath"),
             createFieldBlock(tr("Transcoding Temp Path"),
                              QString(),
                              m_lineEdits.value(QStringLiteral("TranscodingTempPath")),
                              transcodingCard));

    m_switches.insert(QStringLiteral("EnableThrottling"),
                      new ModernSwitch(transcodingCard));
    addBlock(transcodingBody,
             transcodingCard,
             QStringLiteral("EnableThrottling"),
             createSwitchBlock(tr("Enable Throttling"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableThrottling")),
                               transcodingCard));

    m_doubleSpinBoxes.insert(QStringLiteral("DownMixAudioBoost"),
                             createDoubleSpinBox(0.5, 3.0, 0.1, 1));
    addBlock(transcodingBody,
             transcodingCard,
             QStringLiteral("DownMixAudioBoost"),
             createFieldBlock(tr("Downmix Audio Boost"),
                              QString(),
                              m_doubleSpinBoxes.value(QStringLiteral("DownMixAudioBoost")),
                              transcodingCard,
                              true));

    auto* toneCombo = createComboBox();
    addComboOption(toneCombo, tr("Disabled"), QStringLiteral("disabled"));
    addComboOption(toneCombo, tr("Hardware"), QStringLiteral("hardware"));
    addComboOption(toneCombo, tr("Software"), QStringLiteral("software"));
    addComboOption(toneCombo, tr("Hardware + Software"), QStringLiteral("mixed"));
    m_comboBoxes.insert(QStringLiteral("EmbyToneMappingMode"), toneCombo);
    addBlock(transcodingBody,
             transcodingCard,
             QStringLiteral("@emby_tone_mapping_mode"),
             createFieldBlock(tr("Tone Mapping"), QString(), toneCombo, transcodingCard),
             false);

    m_switches.insert(QStringLiteral("EnableSubtitleExtraction"),
                      new ModernSwitch(transcodingCard));
    addBlock(transcodingBody,
             transcodingCard,
             QStringLiteral("EnableSubtitleExtraction"),
             createSwitchBlock(tr("Enable Subtitle Extraction"),
                               QString(),
                               m_switches.value(QStringLiteral("EnableSubtitleExtraction")),
                               transcodingCard));

    QVBoxLayout* advancedBody = nullptr;
    QWidget* advancedCard =
        createSectionCard(tr("Advanced"), QString(), advancedBody, m_contentContainer);
    contentLayout->addWidget(advancedCard);

    auto* encodingThreads = createSpinBox(-1, 256, 1, 168);
    encodingThreads->setSpecialValueText(tr("Auto"));
    m_spinBoxes.insert(QStringLiteral("EncodingThreadCount"), encodingThreads);
    addBlock(advancedBody,
             advancedCard,
             QStringLiteral("EncodingThreadCount"),
             createFieldBlock(tr("Encoding Threads"),
                              QString(),
                              encodingThreads,
                              advancedCard,
                              true));

    auto* extractionThreads = createSpinBox(-1, 256, 1, 168);
    extractionThreads->setSpecialValueText(tr("Auto"));
    m_spinBoxes.insert(QStringLiteral("ExtractionThreadCount"), extractionThreads);
    addBlock(advancedBody,
             advancedCard,
             QStringLiteral("ExtractionThreadCount"),
             createFieldBlock(tr("Extraction Threads"),
                              QString(),
                              extractionThreads,
                              advancedCard,
                              true));

    m_spinBoxes.insert(QStringLiteral("H264Crf"), createSpinBox(0, 51));
    addBlock(advancedBody,
             advancedCard,
             QStringLiteral("H264Crf"),
             createFieldBlock(tr("H.264 CRF"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("H264Crf")),
                              advancedCard,
                              true));

    m_spinBoxes.insert(QStringLiteral("ThrottleBufferSize"), createSpinBox(0, 100000));
    addBlock(advancedBody,
             advancedCard,
             QStringLiteral("ThrottleBufferSize"),
             createFieldBlock(tr("Throttle Buffer"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("ThrottleBufferSize")),
                              advancedCard,
                              true));

    m_spinBoxes.insert(QStringLiteral("ThrottleHysteresis"), createSpinBox(0, 100000));
    addBlock(advancedBody,
             advancedCard,
             QStringLiteral("ThrottleHysteresis"),
             createFieldBlock(tr("Throttle Hysteresis"),
                              QString(),
                              m_spinBoxes.value(QStringLiteral("ThrottleHysteresis")),
                              advancedCard,
                              true));

    auto* throttlingMethodCombo = createComboBox();
    addComboOption(throttlingMethodCombo,
                   tr("By Segment Request"),
                   QStringLiteral("BySegmentRequest"));
    m_comboBoxes.insert(QStringLiteral("ThrottlingMethod"), throttlingMethodCombo);
    addBlock(advancedBody,
             advancedCard,
             QStringLiteral("ThrottlingMethod"),
             createFieldBlock(tr("Throttling Method"),
                              QString(),
                              throttlingMethodCombo,
                              advancedCard));
}

void PageTranscoding::connectInputs() {
    connect(m_btnRefresh, &QPushButton::clicked, this,
            [this]() { m_pendingTask = loadData(true); });
    connect(m_btnSave, &QPushButton::clicked, this,
            [this]() { m_pendingTask = saveData(); });

    auto refreshStatusOnly = [this]() {
        if (m_rawConfig.isEmpty()) {
            m_statsLabel->setText(tr("No transcoding configuration available"));
            return;
        }
        updateStatusText(collectConfig());
    };

    for (auto it = m_lineEdits.cbegin(); it != m_lineEdits.cend(); ++it) {
        connect(it.value(), &QLineEdit::textChanged, this, [this]() { refreshDerivedUi(); });
    }
    for (auto it = m_spinBoxes.cbegin(); it != m_spinBoxes.cend(); ++it) {
        connect(it.value(), qOverload<int>(&QSpinBox::valueChanged), this,
                [this](int) { refreshDerivedUi(); });
    }
    for (auto it = m_doubleSpinBoxes.cbegin(); it != m_doubleSpinBoxes.cend(); ++it) {
        connect(it.value(), qOverload<double>(&QDoubleSpinBox::valueChanged), this,
                [this](double) { refreshDerivedUi(); });
    }
    for (auto it = m_comboBoxes.cbegin(); it != m_comboBoxes.cend(); ++it) {
        connect(it.value(), qOverload<int>(&QComboBox::currentIndexChanged), this,
                [this](int) { refreshDerivedUi(); });
    }
    for (auto it = m_switches.cbegin(); it != m_switches.cend(); ++it) {
        const QString switchKey = it.key();
        connect(it.value(), &ModernSwitch::toggled, this,
                [this, switchKey, refreshStatusOnly](bool) {
                    if (!m_isJellyfin) {
                        refreshDerivedUi();
                        return;
                    }

                    if (switchKey == QStringLiteral("EnableTonemapping") ||
                        switchKey == QStringLiteral("EnableVppTonemapping") ||
                        switchKey == QStringLiteral("EnableThrottling") ||
                        switchKey == QStringLiteral("EnableSegmentDeletion")) {
                        refreshDerivedUi();
                        return;
                    }

                    if (switchKey == QStringLiteral("PreferSystemNativeHwDecoder") ||
                        switchKey == QStringLiteral("EnableVideoToolboxTonemapping")) {
                        refreshStatusOnly();
                    }
                });
    }
    for (auto it = m_tagInputs.cbegin(); it != m_tagInputs.cend(); ++it) {
        connect(it.value(), &ModernTagInput::valueChanged, this,
                [this](const QString&) { refreshDerivedUi(); });
    }
    for (auto it = m_jellyfinCodecSwitches.cbegin(); it != m_jellyfinCodecSwitches.cend(); ++it) {
        connect(it.value(), &ModernSwitch::toggled, this,
                [refreshStatusOnly](bool) { refreshStatusOnly(); });
    }
}

void PageTranscoding::showEvent(QShowEvent* event) {
    ManagePageBase::showEvent(event);
    if (!m_loaded) {
        m_pendingTask = loadData();
        m_loaded = true;
    }
}

QCoro::Task<void> PageTranscoding::loadData(bool isManual) {
    QPointer<PageTranscoding> safeThis(this);
    if (m_isLoading) {
        co_return;
    }

    m_isLoading = true;
    m_btnRefresh->setEnabled(false);
    m_btnSave->setEnabled(false);
    m_statsLabel->setText(tr("Loading..."));
    if (isManual) {
        playRefreshAnimation();
    }

    try {
        if (m_core && m_core->serverManager()) {
            const ServerProfile profile = m_core->serverManager()->activeProfile();
            qDebug() << "[PageTranscoding] Loading transcoding data"
                     << "| profileUrl=" << profile.url
                     << "| serverType="
                     << (profile.type == ServerProfile::Jellyfin ? "Jellyfin" : "Emby");
        }

        const QJsonObject config =
            co_await m_core->adminService()->getEncodingConfiguration();
        if (!safeThis) {
            co_return;
        }

        m_embyCodecInfo = QJsonArray();
        m_embyDefaultCodecConfigs = QJsonArray();
        if (!m_isJellyfin) {
            try {
                m_embyCodecInfo =
                    co_await m_core->adminService()->getVideoCodecInformation();
            } catch (const std::exception& e) {
                qWarning() << "[PageTranscoding] Failed to load Emby codec information"
                           << "| error=" << e.what();
            }
            if (!safeThis) {
                co_return;
            }
            try {
                m_embyDefaultCodecConfigs =
                    co_await m_core->adminService()->getDefaultCodecConfigurations();
            } catch (const std::exception& e) {
                qWarning() << "[PageTranscoding] Failed to load Emby default codec configurations"
                           << "| error=" << e.what();
            }
            if (!safeThis) {
                co_return;
            }
        }

        qDebug() << "[PageTranscoding] Loaded transcoding settings"
                 << "| serverType=" << (m_isJellyfin ? "Jellyfin" : "Emby")
                 << "| keyCount=" << config.keys().size()
                 << "| embyCodecInfoCount=" << m_embyCodecInfo.size()
                 << "| embyDefaultCodecCount=" << m_embyDefaultCodecConfigs.size();

        m_emptyLabel->setText(tr("No transcoding configuration available"));
        populateForm(config);
        m_hasLoadedData = true;
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[PageTranscoding] Failed to load transcoding settings"
                   << "| error=" << e.what();
        m_rawConfig = QJsonObject();
        m_supportedKeys.clear();
        m_embyCodecInfo = QJsonArray();
        m_embyDefaultCodecConfigs = QJsonArray();

        if (isManual || !m_hasLoadedData) {
            m_statsLabel->setText(tr("Refresh failed"));
        }
        if (!m_hasLoadedData) {
            m_emptyLabel->setText(tr("Failed to load transcoding settings"));
            updateContentState(false);
        }

        ModernToast::showMessage(
            tr("Failed to load transcoding settings: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (!safeThis) {
        co_return;
    }

    m_isLoading = false;
    m_btnRefresh->setEnabled(true);
    m_btnSave->setEnabled(m_hasLoadedData && !m_rawConfig.isEmpty() && !m_isSaving);
}

QCoro::Task<void> PageTranscoding::saveData() {
    QPointer<PageTranscoding> safeThis(this);
    if (!m_hasLoadedData || m_isLoading || m_isSaving || m_rawConfig.isEmpty()) {
        co_return;
    }

    m_isSaving = true;
    m_btnRefresh->setEnabled(false);
    m_btnSave->setEnabled(false);
    m_btnSave->setToolTip(tr("Saving..."));
    m_statsLabel->setText(tr("Saving..."));

    const QJsonObject config = collectConfig();
    bool saved = false;

    qDebug() << "[PageTranscoding] Saving transcoding settings"
             << "| serverType=" << (m_isJellyfin ? "Jellyfin" : "Emby")
             << "| keyCount=" << config.keys().size();

    try {
        co_await m_core->adminService()->updateEncodingConfiguration(config);
        if (!safeThis) {
            co_return;
        }
        saved = true;
        ModernToast::showMessage(tr("Transcoding settings saved"), 2000);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }
        qWarning() << "[PageTranscoding] Failed to save transcoding settings"
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to save transcoding settings: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (!safeThis) {
        co_return;
    }

    m_isSaving = false;
    m_btnSave->setToolTip(tr("Save"));
    m_btnRefresh->setEnabled(true);
    m_btnSave->setEnabled(m_hasLoadedData && !m_rawConfig.isEmpty());
    if (saved) {
        co_await loadData(false);
    } else {
        refreshDerivedUi();
    }
}

void PageTranscoding::updateSectionVisibility() {
    for (QWidget* card : std::as_const(m_sectionCards)) {
        bool hasVisibleBlock = false;
        for (const QString& key : m_sectionBlocks.value(card)) {
            
            
            if (auto* block = m_blocks.value(key); block && !block->isHidden()) {
                hasVisibleBlock = true;
                break;
            }
        }
        card->setVisible(hasVisibleBlock);
    }
}

void PageTranscoding::updateDynamicVisibility() {
    for (const QString& key : std::as_const(m_actualBlockKeys)) {
        setBlockVisible(key, m_supportedKeys.contains(key));
    }

    if (m_isJellyfin) {
        const QString hardwareType =
            comboCurrentValue(m_comboBoxes.value(QStringLiteral("HardwareAccelerationType")))
                .toString()
                .trimmed()
                .toLower();
        const bool hardwareAvailable =
            !hardwareType.isEmpty() && hardwareType != QStringLiteral("none");
        const bool toneEnabled =
            m_switches.value(QStringLiteral("EnableTonemapping")) &&
            m_switches.value(QStringLiteral("EnableTonemapping"))->isChecked();
        const bool vppEnabled =
            m_switches.value(QStringLiteral("EnableVppTonemapping")) &&
            m_switches.value(QStringLiteral("EnableVppTonemapping"))->isChecked();
        const bool throttlingEnabled =
            m_switches.value(QStringLiteral("EnableThrottling")) &&
            m_switches.value(QStringLiteral("EnableThrottling"))->isChecked();
        const bool segmentDeletionEnabled =
            m_switches.value(QStringLiteral("EnableSegmentDeletion")) &&
            m_switches.value(QStringLiteral("EnableSegmentDeletion"))->isChecked();

        setBlockVisible(QStringLiteral("VaapiDevice"),
                        m_supportedKeys.contains(QStringLiteral("VaapiDevice")) &&
                            hardwareType == QStringLiteral("vaapi"));
        setBlockVisible(QStringLiteral("QsvDevice"),
                        m_supportedKeys.contains(QStringLiteral("QsvDevice")) &&
                            hardwareType == QStringLiteral("qsv"));
        setBlockVisible(
            QStringLiteral("@jellyfin_decoding_codecs"),
            m_supportedKeys.contains(QStringLiteral("HardwareDecodingCodecs")) &&
                hardwareAvailable);
        setBlockVisible(
            QStringLiteral("@jellyfin_depth_group"),
            (m_supportedKeys.contains(QStringLiteral("EnableDecodingColorDepth10Hevc")) ||
             m_supportedKeys.contains(QStringLiteral("EnableDecodingColorDepth10Vp9"))) &&
                hardwareAvailable);
        setBlockVisible(
            QStringLiteral("@jellyfin_rext_group"),
            (m_supportedKeys.contains(QStringLiteral("EnableDecodingColorDepth10HevcRext")) ||
             m_supportedKeys.contains(QStringLiteral("EnableDecodingColorDepth12HevcRext"))) &&
                hardwareAvailable);
        setBlockVisible(
            QStringLiteral("PreferSystemNativeHwDecoder"),
            m_supportedKeys.contains(QStringLiteral("PreferSystemNativeHwDecoder")) &&
                hardwareAvailable);
        setBlockVisible(
            QStringLiteral("EnableEnhancedNvdecDecoder"),
            m_supportedKeys.contains(QStringLiteral("EnableEnhancedNvdecDecoder")) &&
                hardwareType == QStringLiteral("nvenc"));
        setBlockVisible(
            QStringLiteral("@jellyfin_hardware_encoding_group"),
            hardwareAvailable &&
                (m_supportedKeys.contains(QStringLiteral("EnableHardwareEncoding")) ||
                 m_supportedKeys.contains(
                     QStringLiteral("EnableIntelLowPowerH264HwEncoder")) ||
                 m_supportedKeys.contains(
                     QStringLiteral("EnableIntelLowPowerHevcHwEncoder"))));
        setBlockVisible(
            QStringLiteral("EnableIntelLowPowerH264HwEncoder"),
            m_supportedKeys.contains(QStringLiteral("EnableIntelLowPowerH264HwEncoder")) &&
                hardwareType == QStringLiteral("qsv"));
        setBlockVisible(
            QStringLiteral("EnableIntelLowPowerHevcHwEncoder"),
            m_supportedKeys.contains(QStringLiteral("EnableIntelLowPowerHevcHwEncoder")) &&
                hardwareType == QStringLiteral("qsv"));
        setBlockVisible(
            QStringLiteral("@jellyfin_encoding_formats_group"),
            hardwareAvailable &&
                (m_supportedKeys.contains(QStringLiteral("AllowHevcEncoding")) ||
                 m_supportedKeys.contains(QStringLiteral("AllowAv1Encoding"))));

        for (auto it = m_jellyfinCodecBlocks.begin(); it != m_jellyfinCodecBlocks.end(); ++it) {
            it.value()->setVisible(
                jellyfinCodecSupportedByHardware(it.key(), hardwareType));
        }

        setBlockVisible(QStringLiteral("EnableVppTonemapping"),
                        m_supportedKeys.contains(QStringLiteral("EnableVppTonemapping")) &&
                            toneEnabled && hardwareType == QStringLiteral("qsv"));
        setBlockVisible(
            QStringLiteral("EnableVideoToolboxTonemapping"),
            m_supportedKeys.contains(QStringLiteral("EnableVideoToolboxTonemapping")) &&
                toneEnabled && hardwareType == QStringLiteral("videotoolbox"));
        setBlockVisible(QStringLiteral("TonemappingAlgorithm"),
                        m_supportedKeys.contains(QStringLiteral("TonemappingAlgorithm")) &&
                            toneEnabled);
        setBlockVisible(QStringLiteral("TonemappingMode"),
                        m_supportedKeys.contains(QStringLiteral("TonemappingMode")) &&
                            toneEnabled);
        setBlockVisible(QStringLiteral("TonemappingRange"),
                        m_supportedKeys.contains(QStringLiteral("TonemappingRange")) &&
                            toneEnabled);
        setBlockVisible(QStringLiteral("TonemappingDesat"),
                        m_supportedKeys.contains(QStringLiteral("TonemappingDesat")) &&
                            toneEnabled);
        setBlockVisible(QStringLiteral("TonemappingPeak"),
                        m_supportedKeys.contains(QStringLiteral("TonemappingPeak")) &&
                            toneEnabled);
        setBlockVisible(QStringLiteral("TonemappingParam"),
                        m_supportedKeys.contains(QStringLiteral("TonemappingParam")) &&
                            toneEnabled);
        setBlockVisible(QStringLiteral("@jellyfin_vpp_group"),
                        (m_supportedKeys.contains(QStringLiteral("VppTonemappingBrightness")) ||
                         m_supportedKeys.contains(
                             QStringLiteral("VppTonemappingContrast"))) &&
                            toneEnabled && vppEnabled &&
                            hardwareType == QStringLiteral("qsv"));
        setBlockVisible(QStringLiteral("ThrottleDelaySeconds"),
                        m_supportedKeys.contains(QStringLiteral("ThrottleDelaySeconds")) &&
                            throttlingEnabled);
        setBlockVisible(QStringLiteral("SegmentKeepSeconds"),
                        m_supportedKeys.contains(QStringLiteral("SegmentKeepSeconds")) &&
                            segmentDeletionEnabled);
    } else {
        const QJsonObject currentConfig = collectConfig();
        const bool advancedMode = isEmbyAdvancedMode(currentConfig);
        const bool toneMappingAvailable =
            m_supportedKeys.contains(QStringLiteral("EnableHardwareToneMapping")) ||
            m_supportedKeys.contains(QStringLiteral("EnableSoftwareToneMapping"));
        const bool throttlingEnabled =
            m_switches.value(QStringLiteral("EnableThrottling")) &&
            m_switches.value(QStringLiteral("EnableThrottling"))->isChecked();

        setBlockVisible(QStringLiteral("@emby_tone_mapping_mode"), toneMappingAvailable);
        setBlockVisible(QStringLiteral("@emby_decoder_codecs"),
                        advancedMode && m_embyDecoderLayout && m_embyDecoderLayout->count() > 0);
        setBlockVisible(QStringLiteral("@emby_encoder_codecs"),
                        advancedMode && m_embyEncoderLayout && m_embyEncoderLayout->count() > 0);
        setBlockVisible(QStringLiteral("@emby_software_encoders"),
                        m_embySoftwareLayout && m_embySoftwareLayout->count() > 0);
        setBlockVisible(QStringLiteral("ThrottleBufferSize"),
                        m_supportedKeys.contains(QStringLiteral("ThrottleBufferSize")) &&
                            throttlingEnabled);
        setBlockVisible(QStringLiteral("ThrottleHysteresis"),
                        m_supportedKeys.contains(QStringLiteral("ThrottleHysteresis")) &&
                            throttlingEnabled);
        setBlockVisible(QStringLiteral("ThrottlingMethod"),
                        m_supportedKeys.contains(QStringLiteral("ThrottlingMethod")) &&
                            throttlingEnabled);
    }

    updateSectionVisibility();
}

void PageTranscoding::refreshDerivedUi() {
    if (m_rawConfig.isEmpty()) {
        m_statsLabel->setText(tr("No transcoding configuration available"));
        return;
    }

    const QJsonObject currentConfig = collectConfig();
    updateDynamicVisibility();
    updateStatusText(currentConfig);
}

void PageTranscoding::updateStatusText(const QJsonObject& config) {
    if (config.isEmpty()) {
        m_statsLabel->setText(tr("No transcoding configuration available"));
        return;
    }

    QStringList parts;
    parts.append(serverTypeText());
    parts.append(accelerationSummary(config));
    parts.append(decoderSummary(config));
    parts.append(toneMappingSummary(config));
    m_statsLabel->setText(parts.join(QStringLiteral(" · ")));
}

void PageTranscoding::updateContentState(bool hasConfig) {
    m_contentContainer->setVisible(hasConfig);
    m_emptyStateContainer->setVisible(!hasConfig);
}

void PageTranscoding::populateForm(const QJsonObject& config) {
    m_rawConfig = config;
    m_supportedKeys.clear();
    for (const QString& key : config.keys()) {
        m_supportedKeys.insert(key);
    }

    for (auto it = m_switches.cbegin(); it != m_switches.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            QSignalBlocker blocker(it.value());
            it.value()->setChecked(config.value(it.key()).toBool(false));
        }
    }
    for (auto it = m_spinBoxes.cbegin(); it != m_spinBoxes.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            QSignalBlocker blocker(it.value());
            it.value()->setValue(config.value(it.key()).toInt(it.value()->minimum()));
        }
    }
    for (auto it = m_doubleSpinBoxes.cbegin(); it != m_doubleSpinBoxes.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            QSignalBlocker blocker(it.value());
            it.value()->setValue(config.value(it.key()).toDouble(it.value()->minimum()));
        }
    }
    for (auto it = m_lineEdits.cbegin(); it != m_lineEdits.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            QSignalBlocker blocker(it.value());
            it.value()->setText(config.value(it.key()).toString());
        }
    }
    for (auto it = m_comboBoxes.cbegin(); it != m_comboBoxes.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            QSignalBlocker blocker(it.value());
            setComboCurrentValue(it.value(), config.value(it.key()).toVariant());
        }
    }
    for (auto it = m_tagInputs.cbegin(); it != m_tagInputs.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            QSignalBlocker blocker(it.value());
            it.value()->setValue(stringListFromJsonValue(config.value(it.key())).join(','));
        }
    }

    if (m_encoderAppPathDisplay &&
        m_supportedKeys.contains(QStringLiteral("EncoderAppPathDisplay"))) {
        QSignalBlocker blocker(m_encoderAppPathDisplay);
        m_encoderAppPathDisplay->setText(
            config.value(QStringLiteral("EncoderAppPathDisplay")).toString());
    }

    if (m_isJellyfin) {
        populateJellyfinCodecSwitches(config);
    } else {
        if (auto* combo = m_comboBoxes.value(QStringLiteral("EmbyToneMappingMode"))) {
            QSignalBlocker blocker(combo);
            const bool hardwareToneMapping =
                config.value(QStringLiteral("EnableHardwareToneMapping")).toBool(false);
            const bool softwareToneMapping =
                config.value(QStringLiteral("EnableSoftwareToneMapping")).toBool(false);
            QString mode = QStringLiteral("disabled");
            if (hardwareToneMapping && softwareToneMapping) {
                mode = QStringLiteral("mixed");
            } else if (hardwareToneMapping) {
                mode = QStringLiteral("hardware");
            } else if (softwareToneMapping) {
                mode = QStringLiteral("software");
            }
            setComboCurrentValue(combo, mode);
        }
        populateEmbyCodecLists(config);
    }

    updateContentState(!config.isEmpty());
    refreshDerivedUi();
    m_btnSave->setEnabled(!config.isEmpty() && !m_isLoading && !m_isSaving);
}

QJsonObject PageTranscoding::collectConfig() const {
    QJsonObject config = m_rawConfig;

    for (auto it = m_switches.cbegin(); it != m_switches.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            config.insert(it.key(), it.value()->isChecked());
        }
    }
    for (auto it = m_spinBoxes.cbegin(); it != m_spinBoxes.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            config.insert(it.key(), it.value()->value());
        }
    }
    for (auto it = m_doubleSpinBoxes.cbegin(); it != m_doubleSpinBoxes.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            config.insert(it.key(), it.value()->value());
        }
    }
    for (auto it = m_lineEdits.cbegin(); it != m_lineEdits.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            config.insert(it.key(), it.value()->text().trimmed());
        }
    }
    for (auto it = m_comboBoxes.cbegin(); it != m_comboBoxes.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            config.insert(it.key(), QJsonValue::fromVariant(comboCurrentValue(it.value())));
        }
    }
    for (auto it = m_tagInputs.cbegin(); it != m_tagInputs.cend(); ++it) {
        if (m_supportedKeys.contains(it.key())) {
            config.insert(it.key(),
                          jsonArrayFromStringList(
                              it.value()->value().split(',', Qt::SkipEmptyParts)));
        }
    }

    if (m_isJellyfin &&
        m_supportedKeys.contains(QStringLiteral("HardwareDecodingCodecs"))) {
        config.insert(QStringLiteral("HardwareDecodingCodecs"),
                      jsonArrayFromStringList(collectJellyfinCodecValues()));
    }

    if (!m_isJellyfin) {
        const QString toneMode =
            comboCurrentValue(m_comboBoxes.value(QStringLiteral("EmbyToneMappingMode")))
                .toString()
                .trimmed();
        if (m_supportedKeys.contains(QStringLiteral("EnableHardwareToneMapping"))) {
            config.insert(QStringLiteral("EnableHardwareToneMapping"),
                          toneMode == QStringLiteral("hardware") ||
                              toneMode == QStringLiteral("mixed"));
        }
        if (m_supportedKeys.contains(QStringLiteral("EnableSoftwareToneMapping"))) {
            config.insert(QStringLiteral("EnableSoftwareToneMapping"),
                          toneMode == QStringLiteral("software") ||
                              toneMode == QStringLiteral("mixed"));
        }
        if (m_supportedKeys.contains(QStringLiteral("CodecConfigurations"))) {
            config.insert(QStringLiteral("CodecConfigurations"),
                          isEmbyAdvancedMode(config)
                              ? collectEmbyCodecConfigurations()
                              : QJsonArray());
        }
    }

    return config;
}

void PageTranscoding::populateJellyfinCodecSwitches(const QJsonObject& config) {
    const QStringList selectedCodecs =
        stringListFromJsonValue(config.value(QStringLiteral("HardwareDecodingCodecs")));

    m_unknownJellyfinCodecValues.clear();
    for (const QString& codec : selectedCodecs) {
        if (!m_jellyfinCodecSwitches.contains(codec)) {
            m_unknownJellyfinCodecValues.append(codec);
        }
    }

    for (auto it = m_jellyfinCodecSwitches.cbegin(); it != m_jellyfinCodecSwitches.cend(); ++it) {
        const QSignalBlocker blocker(it.value());
        it.value()->setChecked(selectedCodecs.contains(it.key()));
    }
}

QStringList PageTranscoding::collectJellyfinCodecValues() const {
    QStringList values = m_unknownJellyfinCodecValues;
    for (const QString& codec : knownJellyfinDecoderCodecs()) {
        if (auto* codecSwitch = m_jellyfinCodecSwitches.value(codec);
            codecSwitch && codecSwitch->isChecked() && !values.contains(codec)) {
            values.append(codec);
        }
    }
    return values;
}

QStringList PageTranscoding::knownJellyfinDecoderCodecs() const {
    return {QStringLiteral("h264"),
            QStringLiteral("hevc"),
            QStringLiteral("mpeg1video"),
            QStringLiteral("mpeg2video"),
            QStringLiteral("mpeg4"),
            QStringLiteral("vc1"),
            QStringLiteral("vp8"),
            QStringLiteral("vp9"),
            QStringLiteral("av1")};
}

QString PageTranscoding::jellyfinCodecLabel(const QString& codec) const {
    if (codec == QStringLiteral("h264")) {
        return tr("H264");
    }
    if (codec == QStringLiteral("hevc")) {
        return tr("HEVC");
    }
    if (codec == QStringLiteral("mpeg1video")) {
        return tr("MPEG1");
    }
    if (codec == QStringLiteral("mpeg2video")) {
        return tr("MPEG2");
    }
    if (codec == QStringLiteral("mpeg4")) {
        return tr("MPEG4");
    }
    if (codec == QStringLiteral("vc1")) {
        return tr("VC1");
    }
    if (codec == QStringLiteral("vp8")) {
        return tr("VP8");
    }
    if (codec == QStringLiteral("vp9")) {
        return tr("VP9");
    }
    if (codec == QStringLiteral("av1")) {
        return tr("AV1");
    }
    return codec.toUpper();
}

bool PageTranscoding::jellyfinCodecSupportedByHardware(const QString& codec,
                                                       const QString& hardwareType) const {
    const QString type = hardwareType.trimmed().toLower();
    if (type.isEmpty() || type == QStringLiteral("none")) {
        return false;
    }
    if (codec == QStringLiteral("h264")) {
        return type == QStringLiteral("amf") || type == QStringLiteral("nvenc") ||
               type == QStringLiteral("qsv") || type == QStringLiteral("vaapi") ||
               type == QStringLiteral("rkmpp") ||
               type == QStringLiteral("videotoolbox") ||
               type == QStringLiteral("v4l2m2m");
    }
    if (codec == QStringLiteral("hevc")) {
        return type == QStringLiteral("amf") || type == QStringLiteral("nvenc") ||
               type == QStringLiteral("qsv") || type == QStringLiteral("vaapi") ||
               type == QStringLiteral("rkmpp") ||
               type == QStringLiteral("videotoolbox");
    }
    if (codec == QStringLiteral("mpeg1video")) {
        return type == QStringLiteral("rkmpp");
    }
    if (codec == QStringLiteral("mpeg2video")) {
        return type == QStringLiteral("amf") || type == QStringLiteral("nvenc") ||
               type == QStringLiteral("qsv") || type == QStringLiteral("vaapi") ||
               type == QStringLiteral("rkmpp");
    }
    if (codec == QStringLiteral("mpeg4")) {
        return type == QStringLiteral("nvenc") || type == QStringLiteral("rkmpp");
    }
    if (codec == QStringLiteral("vc1")) {
        return type == QStringLiteral("amf") || type == QStringLiteral("nvenc") ||
               type == QStringLiteral("qsv") || type == QStringLiteral("vaapi");
    }
    if (codec == QStringLiteral("vp8")) {
        return type == QStringLiteral("nvenc") || type == QStringLiteral("qsv") ||
               type == QStringLiteral("vaapi") || type == QStringLiteral("rkmpp") ||
               type == QStringLiteral("videotoolbox");
    }
    if (codec == QStringLiteral("vp9")) {
        return type == QStringLiteral("amf") || type == QStringLiteral("nvenc") ||
               type == QStringLiteral("qsv") || type == QStringLiteral("vaapi") ||
               type == QStringLiteral("rkmpp") ||
               type == QStringLiteral("videotoolbox");
    }
    if (codec == QStringLiteral("av1")) {
        return type == QStringLiteral("amf") || type == QStringLiteral("nvenc") ||
               type == QStringLiteral("qsv") || type == QStringLiteral("vaapi") ||
               type == QStringLiteral("rkmpp");
    }
    return true;
}

void PageTranscoding::populateEmbyCodecLists(const QJsonObject& config) {
    m_embyCodecStates.clear();
    m_embyCodecOrders.clear();
    m_embyCodecGroupOrder.clear();

    LayoutUtils::clearLayout(m_embyDecoderLayout);
    LayoutUtils::clearLayout(m_embyEncoderLayout);
    LayoutUtils::clearLayout(m_embySoftwareLayout);

    const QJsonArray codecConfigs =
        !config.value(QStringLiteral("CodecConfigurations")).toArray().isEmpty()
            ? config.value(QStringLiteral("CodecConfigurations")).toArray()
            : m_embyDefaultCodecConfigs;

    auto addHardwareGroup = [this, &codecConfigs](QVBoxLayout* targetLayout,
                                                  const QString& direction,
                                                  const QString& mediaType) {
        const QStringList codecIds = sortedEmbyCodecIds(direction, mediaType, codecConfigs);
        if (!targetLayout || codecIds.isEmpty()) {
            qDebug() << "[PageTranscoding] Skip empty Emby codec group"
                     << "| direction=" << direction
                     << "| mediaType=" << mediaType;
            return;
        }

        qDebug() << "[PageTranscoding] Build Emby codec group"
                 << "| direction=" << direction
                 << "| mediaType=" << mediaType
                 << "| codecCount=" << codecIds.size()
                 << "| codecIds=" << codecIds;

        const QString groupKey = embyGroupKey(direction, mediaType);
        m_embyCodecGroupOrder.append(groupKey);
        m_embyCodecOrders.insert(groupKey, codecIds);

        QVBoxLayout* groupBody = nullptr;
        QWidget* groupBlock = createGroupBlock(mediaType,
                                               QString(),
                                               groupBody,
                                               targetLayout->parentWidget());
        targetLayout->addWidget(groupBlock);

        auto* listWidget = new TranscodingCodecListWidget(groupBlock);
        groupBody->addWidget(listWidget);

        QList<TranscodingCodecRowWidget*> rows;
        rows.reserve(codecIds.size());

        for (const QString& codecId : codecIds) {
            const QJsonObject codecInfo = embyCodecInfoById(codecId);
            const QString title =
                codecInfo.value(QStringLiteral("Name")).toString(codecId).trimmed();
            const QString description =
                codecInfo.value(QStringLiteral("Description")).toString().trimmed();

            bool enabled = codecConfigs.isEmpty()
                               ? codecInfo.value(QStringLiteral("IsEnabledByDefault")).toBool(false)
                               : false;
            int priority = codecConfigs.isEmpty()
                               ? codecInfo.value(QStringLiteral("DefaultPriority")).toInt(0)
                               : 0;
            for (const QJsonValue& value : codecConfigs) {
                const QJsonObject item = value.toObject();
                if (item.value(QStringLiteral("CodecId")).toString() == codecId) {
                    enabled = item.value(QStringLiteral("IsEnabled")).toBool(enabled);
                    priority = item.value(QStringLiteral("Priority")).toInt(priority);
                    break;
                }
            }

            auto* row = new TranscodingCodecRowWidget(codecId,
                                                      title,
                                                      description,
                                                      tr("Codec Information"),
                                                      tr("Drag to Reorder"),
                                                      listWidget);
            row->setCodecEnabled(enabled);
            rows.append(row);

            EmbyCodecState state;
            state.codecId = codecId;
            state.direction = direction;
            state.mediaType = mediaType;
            state.title = title;
            state.description = description;
            state.priority = priority;
            state.enabled = enabled;
            state.row = row;
            m_embyCodecStates.insert(codecId, state);

            qDebug() << "[PageTranscoding] Emby codec row"
                     << "| direction=" << direction
                     << "| mediaType=" << mediaType
                     << "| codecId=" << codecId
                     << "| title=" << title
                     << "| enabled=" << enabled
                     << "| priority=" << priority;

            connect(row, &TranscodingCodecRowWidget::enabledChanged, this,
                    [this, codecId](const QString&, bool enabledState) {
                        if (m_embyCodecStates.contains(codecId)) {
                            EmbyCodecState state = m_embyCodecStates.value(codecId);
                            state.enabled = enabledState;
                            m_embyCodecStates.insert(codecId, state);
                        }
                    });
            connect(row, &TranscodingCodecRowWidget::infoRequested, this,
                    [this](const QString& requestedCodecId) {
                        showEmbyCodecInfo(requestedCodecId);
                    });
        }

        listWidget->setCodecRows(rows);
        connect(listWidget, &TranscodingCodecListWidget::orderChanged, this,
                [this, groupKey](const QStringList& orderedCodecIds) {
                    m_embyCodecOrders.insert(groupKey, orderedCodecIds);
                });
    };

    QStringList decoderGroups;
    QStringList encoderGroups;
    for (const QJsonValue& value : m_embyCodecInfo) {
        const QJsonObject codecInfo = value.toObject();
        if (codecInfo.isEmpty() ||
            !codecInfo.value(QStringLiteral("IsHardwareCodec")).toBool(false)) {
            continue;
        }

        const QString direction = codecInfo.value(QStringLiteral("Direction")).toString().trimmed();
        const QString mediaType =
            codecInfo.value(QStringLiteral("MediaTypeName")).toString().trimmed();
        if (mediaType.isEmpty()) {
            continue;
        }

        if (direction.compare(QStringLiteral("Decoder"), Qt::CaseInsensitive) == 0 &&
            !decoderGroups.contains(mediaType)) {
            decoderGroups.append(mediaType);
        }
        if (direction.compare(QStringLiteral("Encoder"), Qt::CaseInsensitive) == 0 &&
            !encoderGroups.contains(mediaType)) {
            encoderGroups.append(mediaType);
        }
    }

    std::sort(decoderGroups.begin(), decoderGroups.end());
    std::sort(encoderGroups.begin(), encoderGroups.end());

    for (const QString& mediaType : std::as_const(decoderGroups)) {
        addHardwareGroup(m_embyDecoderLayout, QStringLiteral("Decoder"), mediaType);
    }
    for (const QString& mediaType : std::as_const(encoderGroups)) {
        addHardwareGroup(m_embyEncoderLayout, QStringLiteral("Encoder"), mediaType);
    }

    QList<QJsonObject> softwareEncoders;
    for (const QJsonValue& value : m_embyCodecInfo) {
        const QJsonObject codecInfo = value.toObject();
        if (codecInfo.isEmpty() ||
            codecInfo.value(QStringLiteral("IsHardwareCodec")).toBool(false) ||
            codecInfo.value(QStringLiteral("Direction")).toString().compare(
                QStringLiteral("Encoder"), Qt::CaseInsensitive) != 0 ||
            !codecInfo.value(QStringLiteral("SupportsParameters")).toBool(false)) {
            continue;
        }
        softwareEncoders.append(codecInfo);
    }

    std::sort(softwareEncoders.begin(), softwareEncoders.end(), [](const QJsonObject& left,
                                                                   const QJsonObject& right) {
        const QString leftTitle = left.value(QStringLiteral("MediaTypeName"))
                                      .toString(left.value(QStringLiteral("Name")).toString())
                                      .trimmed();
        const QString rightTitle = right.value(QStringLiteral("MediaTypeName"))
                                       .toString(right.value(QStringLiteral("Name")).toString())
                                       .trimmed();
        return leftTitle.localeAwareCompare(rightTitle) < 0;
    });

    for (const QJsonObject& codecInfo : std::as_const(softwareEncoders)) {
        const QString codecId =
            codecInfo.value(QStringLiteral("Id")).toString().trimmed();
        const QString title = codecInfo.value(QStringLiteral("MediaTypeName"))
                                  .toString(codecInfo.value(QStringLiteral("Name")).toString())
                                  .trimmed();
        QStringList descriptionParts;
        const QString name = codecInfo.value(QStringLiteral("Name")).toString().trimmed();
        const QString description =
            codecInfo.value(QStringLiteral("Description")).toString().trimmed();
        if (!name.isEmpty() && name != title) {
            descriptionParts.append(name);
        }
        if (!description.isEmpty() && !descriptionParts.contains(description)) {
            descriptionParts.append(description);
        }

        auto* row = new QFrame(m_embySoftwareLayout ? m_embySoftwareLayout->parentWidget()
                                                    : nullptr);
        row->setObjectName("TranscodingCodecItem");
        row->setAttribute(Qt::WA_StyledBackground, true);

        auto* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 10, 12, 10);
        rowLayout->setSpacing(10);

        auto* textLayout = new QVBoxLayout();
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(4);

        auto* titleLabel = new QLabel(title, row);
        titleLabel->setObjectName("TranscodingCodecName");
        titleLabel->setWordWrap(true);
        textLayout->addWidget(titleLabel);

        auto* descriptionLabel =
            new QLabel(descriptionParts.join(QStringLiteral(" · ")), row);
        descriptionLabel->setObjectName("TranscodingCodecDescription");
        descriptionLabel->setWordWrap(true);
        descriptionLabel->setVisible(!descriptionLabel->text().trimmed().isEmpty());
        textLayout->addWidget(descriptionLabel);

        rowLayout->addLayout(textLayout, 1);

        auto* settingsButton = new QPushButton(tr("Settings"), row);
        settingsButton->setObjectName("SettingsCardButton");
        settingsButton->setCursor(Qt::PointingHandCursor);
        settingsButton->setFixedHeight(30);
        settingsButton->setToolTip(tr("Settings"));
        rowLayout->addWidget(settingsButton, 0, Qt::AlignVCenter);

        m_embySoftwareLayout->addWidget(row);

        connect(settingsButton, &QPushButton::clicked, this,
                [this, codecId]() { showEmbyCodecParameters(codecId); });
    }

}

QJsonObject PageTranscoding::embyCodecInfoById(const QString& codecId) const {
    for (const QJsonValue& value : m_embyCodecInfo) {
        const QJsonObject codecInfo = value.toObject();
        if (codecInfo.value(QStringLiteral("Id")).toString().trimmed() == codecId) {
            return codecInfo;
        }
    }
    return {};
}

QStringList PageTranscoding::sortedEmbyCodecIds(const QString& direction,
                                                const QString& mediaType,
                                                const QJsonArray& configs) const {
    struct Entry {
        QString codecId;
        QString title;
        int priority = std::numeric_limits<int>::max();
    };

    QHash<QString, QJsonObject> configById;
    for (const QJsonValue& value : configs) {
        const QJsonObject item = value.toObject();
        const QString codecId = item.value(QStringLiteral("CodecId")).toString().trimmed();
        if (!codecId.isEmpty()) {
            configById.insert(codecId, item);
        }
    }

    QList<Entry> entries;
    for (const QJsonValue& value : m_embyCodecInfo) {
        const QJsonObject codecInfo = value.toObject();
        if (codecInfo.isEmpty() ||
            !codecInfo.value(QStringLiteral("IsHardwareCodec")).toBool(false) ||
            codecInfo.value(QStringLiteral("Direction")).toString().compare(
                direction, Qt::CaseInsensitive) != 0 ||
            codecInfo.value(QStringLiteral("MediaTypeName")).toString().trimmed() != mediaType) {
            continue;
        }

        const QString codecId = codecInfo.value(QStringLiteral("Id")).toString().trimmed();
        if (codecId.isEmpty()) {
            continue;
        }

        const QJsonObject configItem = configById.value(codecId);
        Entry entry;
        entry.codecId = codecId;
        entry.title = codecInfo.value(QStringLiteral("Name")).toString(codecId);
        entry.priority = configItem.isEmpty()
                             ? codecInfo.value(QStringLiteral("DefaultPriority")).toInt(9999)
                             : configItem.value(QStringLiteral("Priority")).toInt(9999);
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(), [](const Entry& left, const Entry& right) {
        if (left.priority != right.priority) {
            return left.priority > right.priority;
        }
        return left.title.localeAwareCompare(right.title) < 0;
    });

    QStringList codecIds;
    for (const Entry& entry : std::as_const(entries)) {
        codecIds.append(entry.codecId);
    }
    return codecIds;
}

QString PageTranscoding::embyGroupKey(const QString& direction, const QString& mediaType) const {
    return direction + QStringLiteral("::") + mediaType;
}

void PageTranscoding::showEmbyCodecInfo(const QString& codecId) {
    const QJsonObject codecInfo = embyCodecInfoById(codecId);
    if (codecInfo.isEmpty()) {
        return;
    }

    qDebug() << "[PageTranscoding] Opening codec info dialog"
             << "| codecId=" << codecId
             << "| name=" << codecInfo.value(QStringLiteral("Name")).toString()
             << "| mediaType="
             << codecInfo.value(QStringLiteral("MediaTypeName")).toString();

    EmbyCodecInfoDialog dialog(codecInfo, this);
    dialog.exec();
}

void PageTranscoding::showEmbyCodecParameters(const QString& codecId) {
    const QJsonObject codecInfo = embyCodecInfoById(codecId);
    if (codecInfo.isEmpty()) {
        qWarning() << "[PageTranscoding] Codec info missing for parameter editor"
                   << "| codecId=" << codecId;
        return;
    }

    qDebug() << "[PageTranscoding] Opening codec parameter editor"
             << "| codecId=" << codecId
             << "| title="
             << codecInfo.value(QStringLiteral("MediaTypeName"))
                    .toString(codecInfo.value(QStringLiteral("Name")).toString());

    EmbyCodecParametersDialog dialog(m_core, codecInfo, this);
    dialog.exec();
}

QJsonArray PageTranscoding::collectEmbyCodecConfigurations() const {
    QJsonArray configs = m_rawConfig.value(QStringLiteral("CodecConfigurations")).toArray();
    if (configs.isEmpty()) {
        configs = m_embyDefaultCodecConfigs;
    }

    QHash<QString, int> priorities;
    for (int groupIndex = 0; groupIndex < m_embyCodecGroupOrder.size(); ++groupIndex) {
        const QString groupKey = m_embyCodecGroupOrder.at(groupIndex);
        const QStringList codecIds = m_embyCodecOrders.value(groupKey);
        for (int index = 0; index < codecIds.size(); ++index) {
            priorities.insert(codecIds.at(index), 100 - index);
        }
    }

    QSet<QString> seenCodecIds;
    QJsonArray result;
    for (const QJsonValue& value : configs) {
        QJsonObject item = value.toObject();
        const QString codecId = item.value(QStringLiteral("CodecId")).toString().trimmed();
        if (codecId.isEmpty()) {
            result.append(item);
            continue;
        }

        seenCodecIds.insert(codecId);
        if (const EmbyCodecState state = m_embyCodecStates.value(codecId); state.row) {
            item.insert(QStringLiteral("IsEnabled"), state.row->isCodecEnabled());
            item.insert(QStringLiteral("Priority"),
                        priorities.value(codecId, state.priority));
        }
        result.append(item);
    }

    for (const QString& groupKey : m_embyCodecGroupOrder) {
        for (const QString& codecId : m_embyCodecOrders.value(groupKey)) {
            if (seenCodecIds.contains(codecId)) {
                continue;
            }
            const EmbyCodecState state = m_embyCodecStates.value(codecId);
            if (!state.row) {
                continue;
            }

            QJsonObject item;
            item.insert(QStringLiteral("CodecId"), codecId);
            item.insert(QStringLiteral("IsEnabled"), state.row->isCodecEnabled());
            item.insert(QStringLiteral("Priority"),
                        priorities.value(codecId, state.priority));
            result.append(item);
        }
    }

    return result;
}

QString PageTranscoding::serverTypeText() const {
    return m_isJellyfin ? tr("Jellyfin") : tr("Emby");
}

QString PageTranscoding::hardwareTypeText(QString type) const {
    type = type.trimmed().toLower();
    if (type.isEmpty()) {
        return tr("Unknown");
    }
    if (type == QStringLiteral("auto")) {
        return tr("Auto");
    }
    if (type == QStringLiteral("none")) {
        return tr("None");
    }
    if (auto* combo = m_comboBoxes.value(QStringLiteral("HardwareAccelerationType"))) {
        const int index = combo->findData(type);
        if (index >= 0) {
            return combo->itemText(index);
        }
    }
    return type.toUpper();
}

QString PageTranscoding::embyHardwareModeText(int mode) const {
    switch (mode) {
    case 0:
        return tr("No");
    case 1:
        return tr("Yes");
    case 2:
        return tr("Advanced");
    default:
        return tr("Mode %1").arg(mode);
    }
}

QString PageTranscoding::accelerationSummary(const QJsonObject& config) const {
    if (m_isJellyfin) {
        const QString type =
            config.value(QStringLiteral("HardwareAccelerationType")).toString();
        if (type.trimmed().isEmpty() || type.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
            return tr("Disabled");
        }
        return hardwareTypeText(type);
    }
    return embyHardwareModeText(
        config.value(QStringLiteral("HardwareAccelerationMode")).toInt(0));
}

QString PageTranscoding::decoderSummary(const QJsonObject& config) const {
    if (m_isJellyfin) {
        const QString hardwareType =
            config.value(QStringLiteral("HardwareAccelerationType")).toString();
        if (hardwareType.trimmed().isEmpty() ||
            hardwareType.compare(QStringLiteral("none"), Qt::CaseInsensitive) == 0) {
            return tr("Disabled");
        }
        if (config.value(QStringLiteral("PreferSystemNativeHwDecoder")).toBool(false)) {
            return tr("System Native");
        }
        QStringList preview =
            stringListFromJsonValue(config.value(QStringLiteral("HardwareDecodingCodecs")))
                .mid(0, 2);
        if (preview.isEmpty()) {
            return tr("Automatic");
        }
        for (QString& value : preview) {
            value = jellyfinCodecLabel(value);
        }
        return preview.join(QStringLiteral(", "));
    }

    const int mode = config.value(QStringLiteral("HardwareAccelerationMode")).toInt(0);
    if (mode <= 0) {
        return tr("Disabled");
    }
    if (mode == 2 && !m_embyCodecStates.isEmpty()) {
        return tr("Custom Order");
    }
    return tr("Automatic");
}

QString PageTranscoding::toneMappingSummary(const QJsonObject& config) const {
    if (m_isJellyfin) {
        if (!isToneMappingEnabled(config)) {
            return tr("Disabled");
        }
        if (config.value(QStringLiteral("EnableVppTonemapping")).toBool(false)) {
            return tr("VPP");
        }
        if (config.value(QStringLiteral("EnableVideoToolboxTonemapping")).toBool(false)) {
            return tr("VideoToolbox");
        }
        const QString algorithm = config.value(QStringLiteral("TonemappingAlgorithm")).toString();
        if (auto* combo = m_comboBoxes.value(QStringLiteral("TonemappingAlgorithm"))) {
            const int index = combo->findData(algorithm);
            if (index >= 0) {
                return combo->itemText(index);
            }
        }
        return algorithm.trimmed().isEmpty() ? tr("Enabled") : algorithm;
    }

    const bool hardwareToneMapping =
        config.value(QStringLiteral("EnableHardwareToneMapping")).toBool(false);
    const bool softwareToneMapping =
        config.value(QStringLiteral("EnableSoftwareToneMapping")).toBool(false);
    if (hardwareToneMapping && softwareToneMapping) {
        return tr("Hardware + Software");
    }
    if (hardwareToneMapping) {
        return tr("Hardware");
    }
    if (softwareToneMapping) {
        return tr("Software");
    }
    return tr("Disabled");
}

bool PageTranscoding::isToneMappingEnabled(const QJsonObject& config) const {
    if (m_isJellyfin) {
        return config.value(QStringLiteral("EnableTonemapping")).toBool(false);
    }
    return config.value(QStringLiteral("EnableHardwareToneMapping")).toBool(false) ||
           config.value(QStringLiteral("EnableSoftwareToneMapping")).toBool(false);
}

bool PageTranscoding::isEmbyAdvancedMode(const QJsonObject& config) const {
    return config.value(QStringLiteral("HardwareAccelerationMode")).toInt(0) == 2;
}

void PageTranscoding::playRefreshAnimation() {
    const QIcon originalIcon = m_btnRefresh->icon();
    const QSize iconSize = m_btnRefresh->iconSize();
    const QPixmap basePixmap = originalIcon.pixmap(iconSize);
    if (iconSize.isEmpty() || basePixmap.isNull()) {
        return;
    }

    auto* animation = new QVariantAnimation(this);
    animation->setStartValue(0.0);
    animation->setEndValue(360.0);
    animation->setDuration(800);
    animation->setEasingCurve(QEasingCurve::InOutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this,
            [this, basePixmap, iconSize](const QVariant& value) {
                QTransform transform;
                transform.translate(iconSize.width() / 2.0, iconSize.height() / 2.0);
                transform.rotate(value.toReal());
                transform.translate(-iconSize.width() / 2.0, -iconSize.height() / 2.0);
                const QPixmap rotated =
                    basePixmap.transformed(transform, Qt::SmoothTransformation);
                const int x = (rotated.width() - iconSize.width()) / 2;
                const int y = (rotated.height() - iconSize.height()) / 2;
                m_btnRefresh->setIcon(
                    QIcon(rotated.copy(x, y, iconSize.width(), iconSize.height())));
            });
    connect(animation, &QVariantAnimation::finished, this,
            [this, originalIcon]() { m_btnRefresh->setIcon(originalIcon); });
    animation->start(QAbstractAnimation::DeleteWhenStopped);
}
