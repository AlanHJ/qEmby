#include "usereditdialog.h"

#include "collapsiblesection.h"
#include "moderncombobox.h"
#include "modernswitch.h"
#include "moderntaginput.h"
#include "moderntoast.h"
#include "slidingstackedwidget.h"
#include "../utils/layoututils.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDebug>
#include <QDoubleValidator>
#include <QFrame>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QStyle>
#include <QTime>
#include <QVBoxLayout>

namespace {

constexpr int ProfilePageIndex = 0;
constexpr int PasswordPageIndex = 3;
constexpr int MaxStreamLimit = 50;

QWidget *createSwitchRow(const QString &label, ModernSwitch *&sw,
                         QWidget *parent) {
    auto *rowWidget = new QWidget(parent);

    auto *rowLayout = new QHBoxLayout(rowWidget);
    rowLayout->setContentsMargins(0, 0, 6, 0);
    rowLayout->setSpacing(12);

    auto *lbl = new QLabel(label, rowWidget);
    lbl->setObjectName("ManageInfoKey");
    lbl->setWordWrap(true);
    rowLayout->addWidget(lbl, 1);

    sw = new ModernSwitch(rowWidget);
    rowLayout->addWidget(sw, 0, Qt::AlignVCenter);
    return rowWidget;
}

QWidget *createScrollablePage(QVBoxLayout *&innerLayout, QWidget *parent) {
    auto *scroll = new QScrollArea(parent);
    scroll->setObjectName("LibCreateScrollArea");
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);

    auto *container = new QWidget(scroll);
    container->setAttribute(Qt::WA_StyledBackground, true);

    innerLayout = new QVBoxLayout(container);
    innerLayout->setContentsMargins(0, 0, 12, 0);
    innerLayout->setSpacing(8);
    innerLayout->setAlignment(Qt::AlignTop);

    scroll->setWidget(container);
    return scroll;
}

QWidget *createPanelCard(QWidget *parent, QVBoxLayout *&bodyLayout) {
    auto *card = new QWidget(parent);
    card->setObjectName("LibAdvancedPanel");
    card->setAttribute(Qt::WA_StyledBackground, true);

    bodyLayout = new QVBoxLayout(card);
    bodyLayout->setContentsMargins(12, 10, 12, 10);
    bodyLayout->setSpacing(10);
    return card;
}

CollapsibleSection *createSectionCard(const QString &title, QWidget *parent,
                                      QVBoxLayout *&bodyLayout) {
    auto *section = new CollapsibleSection(title, parent);
    section->setExpanded(true);

    bodyLayout = section->contentLayout();
    bodyLayout->setSpacing(10);
    return section;
}

QLabel *createFieldLabel(const QString &text, QWidget *parent) {
    auto *label = new QLabel(text, parent);
    label->setObjectName("ManageInfoKey");
    label->setWordWrap(true);
    return label;
}

QPushButton *createNavButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setObjectName("SettingsCardButton");
    button->setCursor(Qt::PointingHandCursor);
    button->setProperty("navActive", false);
    button->setFixedHeight(30);
    return button;
}

QPushButton *createInlineButton(const QString &text, QWidget *parent) {
    auto *button = new QPushButton(text, parent);
    button->setObjectName("SettingsCardButton");
    button->setCursor(Qt::PointingHandCursor);
    button->setFixedHeight(28);
    return button;
}

QString formatHourValue(int hour) {
    if (hour >= 24) {
        return QStringLiteral("24:00");
    }
    return QTime(hour, 0).toString(QStringLiteral("HH:mm"));
}

QStringList splitCommaSeparatedValues(const QString &value) {
    QStringList result;
    const QStringList parts = value.split(',', Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty() && !result.contains(trimmed)) {
            result.append(trimmed);
        }
    }
    return result;
}

} 

UserEditDialog::UserEditDialog(QEmbyCore *core, const QString &userId,
                               QWidget *parent)
    : ModernDialogBase(parent), m_core(core), m_userId(userId) {
    setTitle(tr("User Settings"));
    setMinimumWidth(480);
    setMaximumHeight(700);
    resize(520, 560);

    if (m_core && m_core->serverManager()) {
        const ServerProfile activeProfile =
            m_core->serverManager()->activeProfile();
        m_supportsConnectLink =
            activeProfile.isValid() &&
            activeProfile.type == ServerProfile::Emby;
    }

    contentLayout()->setContentsMargins(20, 10, 0, 20);
    contentLayout()->setSpacing(12);

    auto *navRow = new QHBoxLayout();
    navRow->setContentsMargins(0, 0, 12, 0);
    navRow->setSpacing(8);

    m_pageStack = new SlidingStackedWidget(this);
    m_pageStack->setObjectName("UserEditPageStack");
    m_pageStack->setSpeed(260);
    m_pageStack->setEasingCurve(QEasingCurve::OutCubic);

    setupProfileTab();
    setupAccessTab();
    setupParentalTab();
    setupPasswordTab();

    const QStringList navTitles = {
        tr("Profile"),
        tr("Access"),
        tr("Parental Control"),
        tr("Password")
    };
    for (int i = 0; i < navTitles.size(); ++i) {
        auto *button = createNavButton(navTitles[i], this);
        connect(button, &QPushButton::clicked, this,
                [this, i]() { onNavigationChanged(i); });
        m_navButtons.append(button);
        navRow->addWidget(button);
    }
    navRow->addStretch(1);

    contentLayout()->addLayout(navRow);
    contentLayout()->addWidget(m_pageStack, 1);

    auto *bottomRow = new QHBoxLayout();
    bottomRow->setContentsMargins(0, 0, 12, 0);
    bottomRow->setSpacing(8);
    bottomRow->addStretch(1);

    m_btnCancel = new QPushButton(tr("Cancel"), this);
    m_btnCancel->setObjectName("SettingsCardButton");
    m_btnCancel->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnCancel);

    m_btnPrimaryAction = new QPushButton(tr("Save"), this);
    m_btnPrimaryAction->setObjectName("SettingsCardButton");
    m_btnPrimaryAction->setCursor(Qt::PointingHandCursor);
    bottomRow->addWidget(m_btnPrimaryAction);

    contentLayout()->addLayout(bottomRow);

    connect(m_btnCancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(m_btnPrimaryAction, &QPushButton::clicked, this,
            [this]() { triggerPrimaryAction(); });
    connect(m_editUserName, &QLineEdit::textChanged, this,
            [this]() { updatePrimaryActionState(); });
    connect(m_editNewPassword, &QLineEdit::textChanged, this,
            [this]() { updatePrimaryActionState(); });
    connect(m_editConfirmPassword, &QLineEdit::textChanged, this,
            [this]() { updatePrimaryActionState(); });

    connect(m_swAdmin, &ModernSwitch::toggled, this,
            [this](bool) { updateConditionalSections(); });
    connect(m_swDeleteAllFolders, &ModernSwitch::toggled, this,
            [this](bool) { updateConditionalSections(); });
    connect(m_swAllFolders, &ModernSwitch::toggled, this,
            [this](bool) { updateConditionalSections(); });
    connect(m_swAllChannels, &ModernSwitch::toggled, this,
            [this](bool) { updateConditionalSections(); });
    connect(m_swAllDevices, &ModernSwitch::toggled, this,
            [this](bool) { updateConditionalSections(); });
    connect(m_comboMaxRating, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateParentalSectionState(); });
    connect(m_comboTagMode, qOverload<int>(&QComboBox::currentIndexChanged),
            this, [this](int) { updateParentalSectionState(); });
    connect(m_btnAddSchedule, &QPushButton::clicked, this, [this]() {
        appendAccessScheduleRow(QStringLiteral("Sunday"), 0, 24);
    });

    m_currentPageIndex = ProfilePageIndex;
    m_pageStack->setCurrentIndex(ProfilePageIndex);
    for (int i = 1; i < m_pageStack->count(); ++i) {
        if (QWidget *page = m_pageStack->widget(i)) {
            page->hide();
        }
    }

    updateNavigationState();
    updateConditionalSections();
    updateParentalSectionState();
    updatePrimaryActionState();

    m_pendingTask = loadUserData();
}

void UserEditDialog::setupProfileTab() {
    QVBoxLayout *layout = nullptr;
    QWidget *page = createScrollablePage(layout, m_pageStack);

    QVBoxLayout *summaryBody = nullptr;
    auto *summaryCard = createPanelCard(page, summaryBody);
    QWidget *summaryContent = summaryBody->parentWidget();

    auto *nameTitle = new QLabel(tr("Name"), summaryContent);
    nameTitle->setObjectName("ManageCardTitle");
    summaryBody->addWidget(nameTitle);

    auto *nameDescription = new QLabel(
        tr("This is how the user will appear on your server."),
        summaryContent);
    nameDescription->setObjectName("ManageInfoKey");
    nameDescription->setWordWrap(true);
    summaryBody->addWidget(nameDescription);

    m_editUserName = new QLineEdit(summaryContent);
    m_editUserName->setObjectName("ManageLibInput");
    m_editUserName->setPlaceholderText(tr("Enter user name"));
    m_editUserName->setClearButtonEnabled(true);
    summaryBody->addWidget(m_editUserName);

    m_connectInfoContainer = new QWidget(summaryContent);
    auto *connectLayout = new QVBoxLayout(m_connectInfoContainer);
    connectLayout->setContentsMargins(0, 0, 0, 0);
    connectLayout->setSpacing(8);
    connectLayout->addWidget(
        LayoutUtils::createSeparator(m_connectInfoContainer));
    connectLayout->addWidget(
        createFieldLabel(tr("Emby Connect email"), m_connectInfoContainer));

    m_editConnectUserName = new QLineEdit(m_connectInfoContainer);
    m_editConnectUserName->setObjectName("ManageLibInput");
    m_editConnectUserName->setPlaceholderText(tr("Optional connect email"));
    m_editConnectUserName->setClearButtonEnabled(true);
    connectLayout->addWidget(m_editConnectUserName);
    connectLayout->addWidget(createFieldLabel(
        tr("Link this user to an Emby Connect account by email."),
        m_connectInfoContainer));
    summaryBody->addWidget(m_connectInfoContainer);
    layout->addWidget(summaryCard);

    QVBoxLayout *accessBody = nullptr;
    auto *accessCard = createSectionCard(tr("Access"), page, accessBody);
    QWidget *accessContent = accessBody->parentWidget();
    accessBody->addWidget(
        createSwitchRow(tr("Administrator"), m_swAdmin, accessContent));
    accessBody->addWidget(
        createSwitchRow(tr("Disable user"), m_swDisabled, accessContent));
    accessBody->addWidget(createSwitchRow(tr("Allow remote access"),
                                          m_swRemoteAccess, accessContent));
    accessBody->addWidget(createSwitchRow(
        tr("Allow changing image and password"), m_swAllowChangeProfile,
        accessContent));

    m_authProviderRow = new QWidget(accessContent);
    auto *authProviderLayout = new QHBoxLayout(m_authProviderRow);
    authProviderLayout->setContentsMargins(0, 0, 0, 0);
    authProviderLayout->setSpacing(12);
    authProviderLayout->addWidget(
        createFieldLabel(tr("Authentication provider"), m_authProviderRow));
    authProviderLayout->addStretch(1);
    m_comboAuthProvider = LayoutUtils::createStyledCombo(m_authProviderRow);
    m_comboAuthProvider->setMinimumWidth(220);
    authProviderLayout->addWidget(m_comboAuthProvider);
    accessBody->addWidget(m_authProviderRow);
    layout->addWidget(accessCard);

    QVBoxLayout *playbackBody = nullptr;
    auto *playbackCard = createSectionCard(tr("Playback"), page, playbackBody);
    QWidget *playbackContent = playbackBody->parentWidget();
    playbackBody->addWidget(createSwitchRow(tr("Allow media playback"),
                                            m_swMediaPlayback,
                                            playbackContent));
    playbackBody->addWidget(createSwitchRow(tr("Allow audio transcoding"),
                                            m_swAudioTranscode,
                                            playbackContent));
    playbackBody->addWidget(createSwitchRow(tr("Allow video transcoding"),
                                            m_swVideoTranscode,
                                            playbackContent));
    playbackBody->addWidget(createSwitchRow(tr("Allow video remuxing"),
                                            m_swVideoRemuxing,
                                            playbackContent));

    auto *streamLimitRow = new QHBoxLayout();
    streamLimitRow->setContentsMargins(0, 0, 0, 0);
    streamLimitRow->setSpacing(12);
    streamLimitRow->addWidget(
        createFieldLabel(tr("Simultaneous stream limit"), playbackContent));
    streamLimitRow->addStretch(1);
    m_spinStreamLimit = new QSpinBox(playbackContent);
    m_spinStreamLimit->setObjectName("ManageLibSpinBox");
    m_spinStreamLimit->setRange(0, MaxStreamLimit);
    m_spinStreamLimit->setSpecialValueText(tr("Unlimited"));
    m_spinStreamLimit->setValue(0);
    m_spinStreamLimit->setFixedWidth(120);
    streamLimitRow->addWidget(m_spinStreamLimit);
    playbackBody->addLayout(streamLimitRow);

    auto *bitrateRow = new QHBoxLayout();
    bitrateRow->setContentsMargins(0, 0, 0, 0);
    bitrateRow->setSpacing(12);
    bitrateRow->addWidget(
        createFieldLabel(tr("Remote client bitrate limit"), playbackContent));
    bitrateRow->addStretch(1);
    m_editRemoteBitrateLimit = new QLineEdit(playbackContent);
    m_editRemoteBitrateLimit->setObjectName("ManageLibInput");
    m_editRemoteBitrateLimit->setValidator(
        new QDoubleValidator(0.0, 1000000.0, 2, m_editRemoteBitrateLimit));
    m_editRemoteBitrateLimit->setPlaceholderText(QStringLiteral("0"));
    m_editRemoteBitrateLimit->setMaximumWidth(110);
    bitrateRow->addWidget(m_editRemoteBitrateLimit);
    bitrateRow->addWidget(createFieldLabel(tr("Mbps"), playbackContent));
    playbackBody->addLayout(bitrateRow);
    layout->addWidget(playbackCard);

    QVBoxLayout *featureBody = nullptr;
    auto *featureCard =
        createSectionCard(tr("Feature Access"), page, featureBody);
    QWidget *featureContent = featureBody->parentWidget();
    featureBody->addWidget(
        createSwitchRow(tr("Live TV access"), m_swLiveTv, featureContent));
    featureBody->addWidget(createSwitchRow(
        tr("Live TV recording management"), m_swLiveTvManage, featureContent));

    m_dynamicFeatureSection = new QWidget(featureContent);
    auto *dynamicFeatureLayout = new QVBoxLayout(m_dynamicFeatureSection);
    dynamicFeatureLayout->setContentsMargins(0, 0, 0, 0);
    dynamicFeatureLayout->setSpacing(8);
    m_dynamicFeatureLayout = dynamicFeatureLayout;
    featureBody->addWidget(m_dynamicFeatureSection);

    featureBody->addWidget(createSwitchRow(
        tr("Remote control of other users"), m_swRemoteControl,
        featureContent));
    featureBody->addWidget(createSwitchRow(
        tr("Shared device remote control"), m_swSharedDevice,
        featureContent));
    featureBody->addWidget(createSwitchRow(tr("Allow media conversion"),
                                           m_swMediaConversion,
                                           featureContent));
    featureBody->addWidget(createSwitchRow(tr("Allow link sharing"),
                                           m_swLinkSharing,
                                           featureContent));
    layout->addWidget(featureCard);

    QVBoxLayout *downloadsBody = nullptr;
    auto *downloadsCard =
        createSectionCard(tr("Downloads"), page, downloadsBody);
    QWidget *downloadsContent = downloadsBody->parentWidget();
    downloadsBody->addWidget(createSwitchRow(tr("Allow media downloading"),
                                             m_swContentDownload,
                                             downloadsContent));
    downloadsBody->addWidget(createSwitchRow(tr("Allow sync transcoding"),
                                             m_swSyncTranscoding,
                                             downloadsContent));
    layout->addWidget(downloadsCard);

    QVBoxLayout *subtitleBody = nullptr;
    auto *subtitleCard =
        createSectionCard(tr("Subtitles"), page, subtitleBody);
    QWidget *subtitleContent = subtitleBody->parentWidget();
    subtitleBody->addWidget(createSwitchRow(tr("Allow subtitle downloading"),
                                            m_swSubtitleDownload,
                                            subtitleContent));
    subtitleBody->addWidget(createSwitchRow(tr("Allow subtitle management"),
                                            m_swSubtitleManagement,
                                            subtitleContent));
    layout->addWidget(subtitleCard);

    QVBoxLayout *deleteBody = nullptr;
    auto *deleteCard =
        createSectionCard(tr("Media Deletion"), page, deleteBody);
    QWidget *deleteContent = deleteBody->parentWidget();
    deleteBody->addWidget(createSwitchRow(
        tr("Allow media deletion from all libraries"), m_swDeleteAllFolders,
        deleteContent));

    m_deleteTargetsContainer = new QWidget(deleteContent);
    m_deleteTargetsContainer->setObjectName("LibAdvancedPanel");
    m_deleteTargetsContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *deleteTargetsOuter = new QVBoxLayout(m_deleteTargetsContainer);
    deleteTargetsOuter->setContentsMargins(12, 10, 12, 10);
    deleteTargetsOuter->setSpacing(8);
    m_deleteTargetsLayout = new QVBoxLayout();
    m_deleteTargetsLayout->setContentsMargins(0, 0, 0, 0);
    m_deleteTargetsLayout->setSpacing(8);
    deleteTargetsOuter->addLayout(m_deleteTargetsLayout);
    deleteBody->addWidget(m_deleteTargetsContainer);
    layout->addWidget(deleteCard);

    QVBoxLayout *visibilityBody = nullptr;
    auto *visibilityCard =
        createSectionCard(tr("Visibility"), page, visibilityBody);
    QWidget *visibilityContent = visibilityBody->parentWidget();
    visibilityBody->addWidget(createSwitchRow(tr("Hide from login screen"),
                                              m_swHidden,
                                              visibilityContent));
    visibilityBody->addWidget(createSwitchRow(
        tr("Hide from login screen (remote)"), m_swHiddenRemotely,
        visibilityContent));
    visibilityBody->addWidget(createSwitchRow(tr("Hide from unused devices"),
                                              m_swHiddenUnused,
                                              visibilityContent));
    layout->addWidget(visibilityCard);
    layout->addStretch(1);

    m_pageStack->addWidget(page);
}

void UserEditDialog::setupAccessTab() {
    QVBoxLayout *layout = nullptr;
    QWidget *page = createScrollablePage(layout, m_pageStack);

    QVBoxLayout *libraryBody = nullptr;
    auto *libraryCard = createPanelCard(page, libraryBody);
    QWidget *libraryContent = libraryBody->parentWidget();
    libraryBody->addWidget(createSwitchRow(tr("Allow access to all libraries"),
                                           m_swAllFolders, libraryContent));

    m_folderListContainer = new QWidget(libraryContent);
    m_folderListContainer->setObjectName("LibAdvancedPanel");
    m_folderListContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *folderOuterLayout = new QVBoxLayout(m_folderListContainer);
    folderOuterLayout->setContentsMargins(12, 10, 12, 10);
    folderOuterLayout->setSpacing(8);
    m_folderListLayout = new QVBoxLayout();
    m_folderListLayout->setContentsMargins(0, 0, 0, 0);
    m_folderListLayout->setSpacing(8);
    folderOuterLayout->addLayout(m_folderListLayout);
    libraryBody->addWidget(m_folderListContainer);
    layout->addWidget(libraryCard);

    QVBoxLayout *channelBody = nullptr;
    m_channelSection =
        createSectionCard(tr("Channel Access"), page, channelBody);
    QWidget *channelContent = channelBody->parentWidget();
    channelBody->addWidget(createSwitchRow(tr("Allow access to all channels"),
                                           m_swAllChannels, channelContent));

    m_channelListContainer = new QWidget(channelContent);
    m_channelListContainer->setObjectName("LibAdvancedPanel");
    m_channelListContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *channelOuterLayout = new QVBoxLayout(m_channelListContainer);
    channelOuterLayout->setContentsMargins(12, 10, 12, 10);
    channelOuterLayout->setSpacing(8);
    m_channelListLayout = new QVBoxLayout();
    m_channelListLayout->setContentsMargins(0, 0, 0, 0);
    m_channelListLayout->setSpacing(8);
    channelOuterLayout->addLayout(m_channelListLayout);
    channelBody->addWidget(m_channelListContainer);
    layout->addWidget(m_channelSection);

    QVBoxLayout *deviceBody = nullptr;
    m_deviceSection =
        createSectionCard(tr("Device Access"), page, deviceBody);
    QWidget *deviceContent = deviceBody->parentWidget();
    deviceBody->addWidget(createSwitchRow(tr("Allow access on all devices"),
                                          m_swAllDevices, deviceContent));

    m_deviceListContainer = new QWidget(deviceContent);
    m_deviceListContainer->setObjectName("LibAdvancedPanel");
    m_deviceListContainer->setAttribute(Qt::WA_StyledBackground, true);
    auto *deviceOuterLayout = new QVBoxLayout(m_deviceListContainer);
    deviceOuterLayout->setContentsMargins(12, 10, 12, 10);
    deviceOuterLayout->setSpacing(8);
    m_deviceListLayout = new QVBoxLayout();
    m_deviceListLayout->setContentsMargins(0, 0, 0, 0);
    m_deviceListLayout->setSpacing(8);
    deviceOuterLayout->addLayout(m_deviceListLayout);
    deviceBody->addWidget(m_deviceListContainer);
    layout->addWidget(m_deviceSection);

    layout->addStretch(1);
    m_pageStack->addWidget(page);
}

void UserEditDialog::setupParentalTab() {
    QVBoxLayout *layout = nullptr;
    QWidget *page = createScrollablePage(layout, m_pageStack);

    QVBoxLayout *ratingBody = nullptr;
    auto *ratingCard = createPanelCard(page, ratingBody);
    QWidget *ratingContent = ratingBody->parentWidget();

    m_comboMaxRating = LayoutUtils::createStyledCombo(ratingContent);
    m_comboMaxRating->setMaximumWidth(220);
    m_comboMaxRating->addItem(tr("None"), 0);
    m_comboMaxRating->addItem(QStringLiteral("G"), 1);
    m_comboMaxRating->addItem(QStringLiteral("PG"), 5);
    m_comboMaxRating->addItem(QStringLiteral("PG-13"), 7);
    m_comboMaxRating->addItem(QStringLiteral("R"), 9);
    m_comboMaxRating->addItem(QStringLiteral("NC-17"), 12);

    auto *ratingRow = new QHBoxLayout();
    ratingRow->setContentsMargins(0, 0, 0, 0);
    ratingRow->setSpacing(12);
    ratingRow->addWidget(
        createFieldLabel(tr("Maximum parental rating"), ratingContent));
    ratingRow->addStretch(1);
    ratingRow->addWidget(m_comboMaxRating);
    ratingBody->addLayout(ratingRow);

    m_unratedItemsContainer = new QWidget(ratingContent);
    auto *unratedItemsOuter = new QVBoxLayout(m_unratedItemsContainer);
    unratedItemsOuter->setContentsMargins(0, 0, 0, 0);
    unratedItemsOuter->setSpacing(8);
    unratedItemsOuter->addWidget(
        createFieldLabel(tr("Block unrated items"), m_unratedItemsContainer));

    auto *unratedPanel = new QWidget(m_unratedItemsContainer);
    unratedPanel->setObjectName("LibAdvancedPanel");
    unratedPanel->setAttribute(Qt::WA_StyledBackground, true);
    auto *unratedPanelLayout = new QVBoxLayout(unratedPanel);
    unratedPanelLayout->setContentsMargins(12, 10, 12, 10);
    unratedPanelLayout->setSpacing(8);
    m_unratedItemsLayout = unratedPanelLayout;

    struct UnratedItemOption {
        const char *type;
        QString label;
    };
    const QList<UnratedItemOption> unratedOptions = {
        {"Book", tr("Books")},
        {"Game", tr("Games")},
        {"ChannelContent", tr("Channel content")},
        {"LiveTvChannel", tr("Live TV")},
        {"Movie", tr("Movies")},
        {"Music", tr("Music")},
        {"Trailer", tr("Trailers")},
        {"Series", tr("TV shows")}
    };
    for (const UnratedItemOption &option : unratedOptions) {
        auto *cb = new QCheckBox(option.label, unratedPanel);
        cb->setObjectName("ManageLibCheckBox");
        cb->setProperty("itemType", QString::fromUtf8(option.type));
        unratedPanelLayout->addWidget(cb);
        m_unratedItemCheckboxes.append(cb);
    }
    unratedItemsOuter->addWidget(unratedPanel);
    ratingBody->addWidget(m_unratedItemsContainer);
    layout->addWidget(ratingCard);

    QVBoxLayout *tagBody = nullptr;
    auto *tagCard = createSectionCard(tr("Tag Restrictions"), page, tagBody);
    QWidget *tagContent = tagBody->parentWidget();
    tagBody->addWidget(createFieldLabel(tr("Blocked tags"), tagContent));

    m_tagInputBlockedTags = new ModernTagInput(tagContent);
    tagBody->addWidget(m_tagInputBlockedTags);

    auto *tagModeRow = new QHBoxLayout();
    tagModeRow->setContentsMargins(0, 0, 0, 0);
    tagModeRow->setSpacing(12);
    tagModeRow->addWidget(
        createFieldLabel(tr("Tag restriction mode"), tagContent));
    tagModeRow->addStretch(1);
    m_comboTagMode = LayoutUtils::createStyledCombo(tagContent);
    m_comboTagMode->setMinimumWidth(220);
    m_comboTagMode->addItem(tr("Block items with these tags"), QString());
    m_comboTagMode->addItem(tr("Allow items with these tags"),
                            QStringLiteral("include"));
    tagModeRow->addWidget(m_comboTagMode);
    tagBody->addLayout(tagModeRow);

    m_multiRestrictionModeRow = new QWidget(tagContent);
    auto *multiModeLayout = new QHBoxLayout(m_multiRestrictionModeRow);
    multiModeLayout->setContentsMargins(0, 0, 0, 0);
    multiModeLayout->setSpacing(12);
    multiModeLayout->addWidget(createFieldLabel(
        tr("Restriction match mode"), m_multiRestrictionModeRow));
    multiModeLayout->addStretch(1);
    m_comboMultiRestrictionMode =
        LayoutUtils::createStyledCombo(m_multiRestrictionModeRow);
    m_comboMultiRestrictionMode->setMinimumWidth(220);
    m_comboMultiRestrictionMode->addItem(tr("Match all restrictions"),
                                         QStringLiteral("all"));
    m_comboMultiRestrictionMode->addItem(tr("Match any restriction"),
                                         QStringLiteral("any"));
    multiModeLayout->addWidget(m_comboMultiRestrictionMode);
    tagBody->addWidget(m_multiRestrictionModeRow);
    layout->addWidget(tagCard);

    QVBoxLayout *scheduleBody = nullptr;
    m_scheduleSection =
        createSectionCard(tr("Access Schedule"), page, scheduleBody);
    QWidget *scheduleContent = scheduleBody->parentWidget();

    auto *scheduleHeaderRow = new QHBoxLayout();
    scheduleHeaderRow->setContentsMargins(0, 0, 0, 0);
    scheduleHeaderRow->setSpacing(12);
    scheduleHeaderRow->addWidget(createFieldLabel(
        tr("Limit when this user can access the server."),
        scheduleContent));
    scheduleHeaderRow->addStretch(1);
    m_btnAddSchedule = createInlineButton(tr("Add"), scheduleContent);
    scheduleHeaderRow->addWidget(m_btnAddSchedule);
    scheduleBody->addLayout(scheduleHeaderRow);

    auto *scheduleListContainer = new QWidget(scheduleContent);
    auto *scheduleListOuter = new QVBoxLayout(scheduleListContainer);
    scheduleListOuter->setContentsMargins(0, 0, 0, 0);
    scheduleListOuter->setSpacing(8);
    m_scheduleListLayout = new QVBoxLayout();
    m_scheduleListLayout->setContentsMargins(0, 0, 0, 0);
    m_scheduleListLayout->setSpacing(8);
    scheduleListOuter->addLayout(m_scheduleListLayout);
    scheduleBody->addWidget(scheduleListContainer);
    layout->addWidget(m_scheduleSection);

    layout->addStretch(1);
    m_pageStack->addWidget(page);
}

void UserEditDialog::setupPasswordTab() {
    QVBoxLayout *layout = nullptr;
    QWidget *page = createScrollablePage(layout, m_pageStack);

    QVBoxLayout *passwordBody = nullptr;
    auto *passwordCard = createPanelCard(page, passwordBody);
    QWidget *passwordContent = passwordBody->parentWidget();

    passwordBody->addWidget(createFieldLabel(tr("New password"),
                                             passwordContent));
    m_editNewPassword = new QLineEdit(passwordContent);
    m_editNewPassword->setObjectName("ManageLibInput");
    m_editNewPassword->setEchoMode(QLineEdit::Password);
    m_editNewPassword->setPlaceholderText(tr("Enter new password"));
    m_editNewPassword->setClearButtonEnabled(true);
    passwordBody->addWidget(m_editNewPassword);

    passwordBody->addWidget(
        createFieldLabel(tr("Confirm password"), passwordContent));
    m_editConfirmPassword = new QLineEdit(passwordContent);
    m_editConfirmPassword->setObjectName("ManageLibInput");
    m_editConfirmPassword->setEchoMode(QLineEdit::Password);
    m_editConfirmPassword->setPlaceholderText(tr("Re-enter new password"));
    m_editConfirmPassword->setClearButtonEnabled(true);
    passwordBody->addWidget(m_editConfirmPassword);

    layout->addWidget(passwordCard);
    layout->addStretch(1);

    m_pageStack->addWidget(page);
}

void UserEditDialog::populateDeletionTargets(
    const QList<AdminMediaFolderInfo> &folders,
    const QList<AdminChannelInfo> &channels) {
    LayoutUtils::clearLayout(m_deleteTargetsLayout);
    m_deleteTargetCheckboxes.clear();

    const bool deleteAll = m_policyJson["EnableContentDeletion"].toBool();
    const auto addTarget = [this, deleteAll](const QString &id,
                                             const QString &name) {
        if (id.trimmed().isEmpty()) {
            return;
        }
        auto *cb = new QCheckBox(name, m_deleteTargetsContainer);
        cb->setObjectName("ManageLibCheckBox");
        cb->setProperty("targetId", id);
        cb->setChecked(deleteAll || m_enabledDeletionTargetIds.contains(id));
        m_deleteTargetsLayout->addWidget(cb);
        m_deleteTargetCheckboxes.append(cb);
    };

    for (const AdminMediaFolderInfo &folder : folders) {
        addTarget(folder.id, folder.name);
    }
    for (const AdminChannelInfo &channel : channels) {
        addTarget(channel.id, channel.name);
    }
}

void UserEditDialog::populateSelectableFolders(
    const QList<SelectableMediaFolderInfo> &folders) {
    LayoutUtils::clearLayout(m_folderListLayout);
    m_folderCheckboxes.clear();
    m_subFolderCheckboxes.clear();

    const bool enableAllFolders = m_policyJson["EnableAllFolders"].toBool(true);

    for (const SelectableMediaFolderInfo &folder : folders) {
        if (folder.id.trimmed().isEmpty()) {
            continue;
        }

        const bool folderChecked =
            enableAllFolders || m_enabledFolderIds.contains(folder.id);

        auto *groupWidget = new QWidget(m_folderListContainer);
        auto *groupLayout = new QVBoxLayout(groupWidget);
        groupLayout->setContentsMargins(0, 0, 0, 0);
        groupLayout->setSpacing(6);

        auto *folderCheck = new QCheckBox(folder.name, groupWidget);
        folderCheck->setObjectName("ManageLibCheckBox");
        folderCheck->setProperty("folderId", folder.id);
        folderCheck->setChecked(folderChecked);
        folderCheck->setEnabled(folder.isUserAccessConfigurable);
        groupLayout->addWidget(folderCheck);
        m_folderCheckboxes.append(folderCheck);

        connect(folderCheck, &QCheckBox::toggled, this,
                [this, folderId = folder.id](bool checked) {
            for (QCheckBox *subFolderCheck : m_subFolderCheckboxes) {
                if (!subFolderCheck ||
                    subFolderCheck->property("folderId").toString() != folderId) {
                    continue;
                }
                subFolderCheck->setChecked(checked);
            }
        });

        for (const AdminMediaSubFolderInfo &subFolder : folder.subFolders) {
            auto *subRow = new QWidget(groupWidget);
            auto *subRowLayout = new QHBoxLayout(subRow);
            subRowLayout->setContentsMargins(22, 0, 0, 0);
            subRowLayout->setSpacing(0);

            auto *subFolderCheck = new QCheckBox(subFolder.path, subRow);
            subFolderCheck->setObjectName("ManageLibCheckBox");
            subFolderCheck->setProperty("folderId", folder.id);
            const QString subFolderId =
                folder.id + QStringLiteral("_") + subFolder.id;
            const QString altSubFolderId =
                folder.id + QStringLiteral("_") + subFolder.effectiveId();
            subFolderCheck->setProperty("subFolderId", subFolderId);
            subFolderCheck->setProperty("subFolderAltId", altSubFolderId);
            subFolderCheck->setEnabled(subFolder.isUserAccessConfigurable);

            const bool subFolderChecked =
                enableAllFolders ||
                (folderChecked &&
                 !m_excludedSubFolderIds.contains(subFolderId) &&
                 !m_excludedSubFolderIds.contains(altSubFolderId));
            subFolderCheck->setChecked(subFolderChecked);
            subRowLayout->addWidget(subFolderCheck);
            groupLayout->addWidget(subRow);
            m_subFolderCheckboxes.append(subFolderCheck);

            connect(subFolderCheck, &QCheckBox::toggled, this,
                    [this, folderId = folder.id](bool) {
                QCheckBox *folderCheckBox = nullptr;
                for (QCheckBox *cb : m_folderCheckboxes) {
                    if (cb && cb->property("folderId").toString() == folderId) {
                        folderCheckBox = cb;
                        break;
                    }
                }
                if (!folderCheckBox) {
                    return;
                }

                bool anyChecked = false;
                bool hasChildren = false;
                for (QCheckBox *cb : m_subFolderCheckboxes) {
                    if (!cb ||
                        cb->property("folderId").toString() != folderId) {
                        continue;
                    }
                    hasChildren = true;
                    if (cb->isChecked()) {
                        anyChecked = true;
                        break;
                    }
                }

                if (hasChildren) {
                    const QSignalBlocker blocker(folderCheckBox);
                    folderCheckBox->setChecked(anyChecked);
                }
            });
        }

        m_folderListLayout->addWidget(groupWidget);
    }
}

void UserEditDialog::populateChannels(
    const QList<AdminChannelInfo> &channels) {
    LayoutUtils::clearLayout(m_channelListLayout);
    m_channelCheckboxes.clear();
    m_hasChannelOptions = !channels.isEmpty();

    const bool enableAllChannels =
        m_policyJson["EnableAllChannels"].toBool(true);
    for (const AdminChannelInfo &channel : channels) {
        if (channel.id.trimmed().isEmpty()) {
            continue;
        }
        auto *cb = new QCheckBox(channel.name, m_channelListContainer);
        cb->setObjectName("ManageLibCheckBox");
        cb->setProperty("channelId", channel.id);
        cb->setChecked(enableAllChannels ||
                       m_enabledChannelIds.contains(channel.id));
        m_channelListLayout->addWidget(cb);
        m_channelCheckboxes.append(cb);
    }
}

void UserEditDialog::populateDevices(const QList<AdminDeviceInfo> &devices) {
    LayoutUtils::clearLayout(m_deviceListLayout);
    m_deviceCheckboxes.clear();
    m_hasDeviceOptions = !devices.isEmpty();

    const bool enableAllDevices =
        m_policyJson["EnableAllDevices"].toBool(true);
    for (const AdminDeviceInfo &device : devices) {
        if (device.id.trimmed().isEmpty()) {
            continue;
        }
        QString text = device.name;
        if (!device.appName.trimmed().isEmpty()) {
            text += QStringLiteral(" - ") + device.appName.trimmed();
        }
        auto *cb = new QCheckBox(text, m_deviceListContainer);
        cb->setObjectName("ManageLibCheckBox");
        cb->setProperty("deviceId", device.id);
        cb->setChecked(enableAllDevices ||
                       m_enabledDeviceIds.contains(device.id));
        m_deviceListLayout->addWidget(cb);
        m_deviceCheckboxes.append(cb);
    }
}

void UserEditDialog::populateAuthProviders(
    const QList<AdminAuthProviderInfo> &providers) {
    m_comboAuthProvider->clear();

    const QString currentProviderId =
        m_policyJson["AuthenticationProviderId"].toString();
    for (const AdminAuthProviderInfo &provider : providers) {
        m_comboAuthProvider->addItem(provider.name, provider.id);
    }

    int selectedIndex = -1;
    for (int i = 0; i < m_comboAuthProvider->count(); ++i) {
        if (m_comboAuthProvider->itemData(i).toString() == currentProviderId) {
            selectedIndex = i;
            break;
        }
    }
    if (selectedIndex < 0 && m_comboAuthProvider->count() == 1) {
        selectedIndex = 0;
    }
    if (selectedIndex >= 0) {
        m_comboAuthProvider->setCurrentIndex(selectedIndex);
    }
}

void UserEditDialog::populateDynamicFeatures(
    const QList<AdminFeatureInfo> &features) {
    LayoutUtils::clearLayout(m_dynamicFeatureLayout);
    m_dynamicFeatureCheckboxes.clear();

    QStringList restrictedFeatureIds;
    for (const auto &value : m_policyJson["RestrictedFeatures"].toArray()) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty() && !restrictedFeatureIds.contains(id)) {
            restrictedFeatureIds.append(id);
        }
    }

    for (const AdminFeatureInfo &feature : features) {
        const QString featureId = feature.id.trimmed();
        if (featureId.isEmpty() || featureId.contains(QLatin1Char('.')) ||
            featureId == QStringLiteral("livetv") ||
            featureId == QStringLiteral("livetv_manage")) {
            continue;
        }

        auto *cb = new QCheckBox(feature.name, m_dynamicFeatureSection);
        cb->setObjectName("ManageLibCheckBox");
        cb->setProperty("featureId", featureId);
        cb->setChecked(!restrictedFeatureIds.contains(featureId));
        m_dynamicFeatureLayout->addWidget(cb);
        m_dynamicFeatureCheckboxes.append(cb);
    }
}

void UserEditDialog::populateParentalRatings(
    const QList<AdminParentalRatingInfo> &ratings) {
    if (ratings.isEmpty()) {
        return;
    }

    QList<AdminParentalRatingInfo> mergedRatings;
    for (const AdminParentalRatingInfo &rating : ratings) {
        if (!mergedRatings.isEmpty() &&
            mergedRatings.last().value == rating.value) {
            if (!rating.name.trimmed().isEmpty()) {
                mergedRatings.last().name +=
                    QStringLiteral("/") + rating.name.trimmed();
            }
            continue;
        }
        mergedRatings.append(rating);
    }

    m_comboMaxRating->clear();
    m_comboMaxRating->addItem(tr("None"), 0);
    for (const AdminParentalRatingInfo &rating : mergedRatings) {
        m_comboMaxRating->addItem(rating.name, rating.value);
    }

    const int maxRating = m_policyJson["MaxParentalRating"].toInt(0);
    int bestIndex = 0;
    for (int i = 0; i < m_comboMaxRating->count(); ++i) {
        if (m_comboMaxRating->itemData(i).toInt() <= maxRating) {
            bestIndex = i;
        }
    }
    m_comboMaxRating->setCurrentIndex(bestIndex);
}

void UserEditDialog::populateAccessSchedules(const QJsonArray &schedules) {
    LayoutUtils::clearLayout(m_scheduleListLayout);

    for (const auto &value : schedules) {
        const QJsonObject schedule = value.toObject();
        appendAccessScheduleRow(
            schedule["DayOfWeek"].toString(QStringLiteral("Sunday")),
            schedule["StartHour"].toVariant().toInt(),
            schedule["EndHour"].toVariant().toInt());
    }
}

void UserEditDialog::appendAccessScheduleRow(const QString &dayOfWeek,
                                             int startHour,
                                             int endHour) {
    auto *row = new QWidget(m_scheduleSection);
    row->setObjectName("LibAdvancedPanel");
    row->setAttribute(Qt::WA_StyledBackground, true);

    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(12, 10, 12, 10);
    rowLayout->setSpacing(8);

    auto *dayCombo = LayoutUtils::createStyledCombo(row);
    dayCombo->setObjectName("ScheduleDayCombo");
    dayCombo->setMinimumWidth(110);
    const QList<QPair<QString, QString>> dayOptions = {
        {tr("Sunday"), QStringLiteral("Sunday")},
        {tr("Monday"), QStringLiteral("Monday")},
        {tr("Tuesday"), QStringLiteral("Tuesday")},
        {tr("Wednesday"), QStringLiteral("Wednesday")},
        {tr("Thursday"), QStringLiteral("Thursday")},
        {tr("Friday"), QStringLiteral("Friday")},
        {tr("Saturday"), QStringLiteral("Saturday")}
    };
    for (const auto &option : dayOptions) {
        dayCombo->addItem(option.first, option.second);
    }

    auto *startCombo = LayoutUtils::createStyledCombo(row);
    startCombo->setObjectName("ScheduleStartCombo");
    startCombo->setMinimumWidth(100);
    auto *endCombo = LayoutUtils::createStyledCombo(row);
    endCombo->setObjectName("ScheduleEndCombo");
    endCombo->setMinimumWidth(100);
    for (int hour = 0; hour <= 24; ++hour) {
        const QString label = formatHourValue(hour);
        startCombo->addItem(label, hour);
        endCombo->addItem(label, hour);
    }

    int dayIndex = 0;
    for (int i = 0; i < dayCombo->count(); ++i) {
        if (dayCombo->itemData(i).toString() == dayOfWeek) {
            dayIndex = i;
            break;
        }
    }
    dayCombo->setCurrentIndex(dayIndex);

    const int safeStartHour = qBound(0, startHour, 24);
    const int safeEndHour = qBound(0, endHour, 24);
    startCombo->setCurrentIndex(safeStartHour);
    endCombo->setCurrentIndex(safeEndHour);

    rowLayout->addWidget(dayCombo);
    rowLayout->addWidget(startCombo);
    rowLayout->addWidget(endCombo);
    rowLayout->addStretch(1);

    auto *btnRemove = createInlineButton(tr("Remove"), row);
    rowLayout->addWidget(btnRemove);
    connect(btnRemove, &QPushButton::clicked, this, [this, row]() {
        m_scheduleListLayout->removeWidget(row);
        row->deleteLater();
    });

    m_scheduleListLayout->addWidget(row);
}

void UserEditDialog::onNavigationChanged(int index) {
    if (index < 0 || index >= m_pageStack->count() ||
        index == m_currentPageIndex) {
        return;
    }

    qDebug() << "[UserEditDialog] Navigate page"
             << "| from=" << m_currentPageIndex
             << "| to=" << index;

    m_currentPageIndex = index;
    m_pageStack->slideInIdx(index, SlidingStackedWidget::Automatic);
    updateNavigationState();
    updatePrimaryActionState();
}

void UserEditDialog::triggerPrimaryAction() {
    if (m_currentPageIndex == PasswordPageIndex) {
        m_pendingTask = savePassword();
        return;
    }

    m_pendingTask = saveProfileAndAccess();
}

void UserEditDialog::updateNavigationState() {
    for (int i = 0; i < m_navButtons.size(); ++i) {
        QPushButton *button = m_navButtons[i];
        if (!button) {
            continue;
        }

        const bool active = (i == m_currentPageIndex);
        button->setProperty("navActive", active);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
}

void UserEditDialog::updatePrimaryActionState() {
    if (!m_btnPrimaryAction) {
        return;
    }

    const bool isPasswordPage = m_currentPageIndex == PasswordPageIndex;
    m_btnPrimaryAction->setText(isPasswordPage ? tr("Save Password")
                                               : tr("Save"));

    bool enabled = m_isDataLoaded && !m_isSubmitting;
    if (!isPasswordPage && m_editUserName) {
        enabled = enabled && !m_editUserName->text().trimmed().isEmpty();
    }
    if (isPasswordPage && m_editNewPassword && m_editConfirmPassword) {
        enabled = enabled && !m_editNewPassword->text().trimmed().isEmpty() &&
                  !m_editConfirmPassword->text().trimmed().isEmpty();
    }

    m_btnPrimaryAction->setEnabled(enabled);
    for (QPushButton *button : m_navButtons) {
        if (button) {
            button->setEnabled(!m_isSubmitting);
        }
    }
}

void UserEditDialog::updateConditionalSections() {
    const bool isAdmin = m_swAdmin && m_swAdmin->isChecked();

    if (m_connectInfoContainer) {
        m_connectInfoContainer->setVisible(m_supportsConnectLink);
    }
    if (m_authProviderRow) {
        const bool showAuthProvider =
            m_authProvidersLoaded && m_comboAuthProvider &&
            m_comboAuthProvider->count() > 1 && !isAdmin;
        m_authProviderRow->setVisible(showAuthProvider);
    }
    if (m_dynamicFeatureSection) {
        m_dynamicFeatureSection->setVisible(!m_dynamicFeatureCheckboxes.isEmpty());
    }
    if (m_deleteTargetsContainer) {
        m_deleteTargetsContainer->setVisible(
            m_deletionTargetsLoaded && m_swDeleteAllFolders &&
            !m_swDeleteAllFolders->isChecked());
    }
    if (m_folderListContainer) {
        m_folderListContainer->setVisible(
            m_accessFoldersLoaded && m_swAllFolders &&
            !m_swAllFolders->isChecked());
    }
    if (m_channelSection) {
        m_channelSection->setVisible(m_hasChannelOptions);
    }
    if (m_channelListContainer) {
        m_channelListContainer->setVisible(
            m_channelsLoaded && m_hasChannelOptions && m_swAllChannels &&
            !m_swAllChannels->isChecked());
    }
    if (m_deviceSection) {
        m_deviceSection->setVisible(m_hasDeviceOptions && !isAdmin);
    }
    if (m_deviceListContainer) {
        m_deviceListContainer->setVisible(
            m_devicesLoaded && m_hasDeviceOptions && m_swAllDevices &&
            !m_swAllDevices->isChecked() && !isAdmin);
    }
    if (m_scheduleSection) {
        m_scheduleSection->setVisible(!isAdmin);
    }

    updateParentalSectionState();
}

void UserEditDialog::updateParentalSectionState() {
    const bool hasMaxRating =
        m_comboMaxRating && m_comboMaxRating->currentData().toInt() > 0;
    if (m_unratedItemsContainer) {
        m_unratedItemsContainer->setVisible(hasMaxRating);
    }

    const bool inclusiveTagMode =
        m_comboTagMode &&
        m_comboTagMode->currentData().toString() == QStringLiteral("include");
    if (m_multiRestrictionModeRow) {
        m_multiRestrictionModeRow->setVisible(hasMaxRating && inclusiveTagMode);
    }
}

QCoro::Task<void> UserEditDialog::loadUserData() {
    QPointer<UserEditDialog> safeThis(this);

    try {
        const QJsonObject userJson =
            co_await m_core->adminService()->getUserById(m_userId);
        if (!safeThis) {
            co_return;
        }
        populateFromJson(userJson);
    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        ModernToast::showMessage(
            tr("Failed to load user data: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
        co_return;
    }

    QList<AdminMediaFolderInfo> deletionFolders;
    QList<AdminChannelInfo> deletionChannels;
    try {
        deletionFolders = co_await m_core->adminService()->getMediaFolders(false);
        if (!safeThis) {
            co_return;
        }
    } catch (const std::exception &e) {
        qWarning() << "[UserEditDialog] Failed to load media folders"
                   << "| error=" << e.what();
    }
    try {
        deletionChannels =
            co_await m_core->adminService()->getChannels(true);
        if (!safeThis) {
            co_return;
        }
    } catch (const std::exception &e) {
        qWarning() << "[UserEditDialog] Failed to load deletion channels"
                   << "| error=" << e.what();
    }
    if (!safeThis) {
        co_return;
    }
    populateDeletionTargets(deletionFolders, deletionChannels);
    m_deletionTargetsLoaded = !m_deleteTargetCheckboxes.isEmpty();

    try {
        const QList<SelectableMediaFolderInfo> folders =
            co_await m_core->adminService()->getSelectableMediaFolders(false);
        if (!safeThis) {
            co_return;
        }
        populateSelectableFolders(folders);
        m_accessFoldersLoaded = true;
    } catch (const std::exception &e) {
        qWarning() << "[UserEditDialog] Failed to load selectable folders"
                   << "| error=" << e.what();
    }

    try {
        const QList<AdminChannelInfo> channels =
            co_await m_core->adminService()->getChannels();
        if (!safeThis) {
            co_return;
        }
        populateChannels(channels);
        m_channelsLoaded = true;
    } catch (const std::exception &e) {
        qWarning() << "[UserEditDialog] Failed to load channels"
                   << "| error=" << e.what();
    }

    try {
        const QList<AdminDeviceInfo> devices =
            co_await m_core->adminService()->getDevices();
        if (!safeThis) {
            co_return;
        }
        populateDevices(devices);
        m_devicesLoaded = true;
    } catch (const std::exception &e) {
        qWarning() << "[UserEditDialog] Failed to load devices"
                   << "| error=" << e.what();
    }

    if (m_supportsConnectLink) {
        try {
            const QList<AdminAuthProviderInfo> providers =
                co_await m_core->adminService()->getAuthProviders();
            if (!safeThis) {
                co_return;
            }
            populateAuthProviders(providers);
            m_authProvidersLoaded = true;
        } catch (const std::exception &e) {
            qWarning() << "[UserEditDialog] Failed to load auth providers"
                       << "| error=" << e.what();
        }
    }

    try {
        const QList<AdminFeatureInfo> features =
            co_await m_core->adminService()->getUserFeatures();
        if (!safeThis) {
            co_return;
        }
        populateDynamicFeatures(features);
        m_dynamicFeaturesLoaded = true;
    } catch (const std::exception &e) {
        qWarning() << "[UserEditDialog] Failed to load feature access"
                   << "| error=" << e.what();
    }

    try {
        const QList<AdminParentalRatingInfo> ratings =
            co_await m_core->adminService()->getParentalRatings();
        if (!safeThis) {
            co_return;
        }
        populateParentalRatings(ratings);
    } catch (const std::exception &e) {
        qWarning() << "[UserEditDialog] Failed to load parental ratings"
                   << "| error=" << e.what();
    }

    if (!safeThis) {
        co_return;
    }

    updateConditionalSections();
    updateParentalSectionState();
    m_isDataLoaded = true;
    updatePrimaryActionState();
}

void UserEditDialog::populateFromJson(const QJsonObject &userJson) {
    m_fullUserJson = userJson;
    m_policyJson = userJson["Policy"].toObject();

    const QString userName = userJson["Name"].toString();
    m_loadedUserName = userName;
    setTitle(tr("User Settings — %1").arg(userName));
    m_editUserName->setText(userName);

    m_loadedConnectUserName = userJson["ConnectUserName"].toString().trimmed();
    if (m_editConnectUserName) {
        m_editConnectUserName->setText(m_loadedConnectUserName);
    }

    m_swAdmin->setChecked(m_policyJson["IsAdministrator"].toBool());
    m_swDisabled->setChecked(m_policyJson["IsDisabled"].toBool());
    m_swRemoteAccess->setChecked(
        m_policyJson["EnableRemoteAccess"].toBool(true));
    m_swAllowChangeProfile->setChecked(
        m_policyJson["EnableUserPreferenceAccess"].toBool(true));
    m_swMediaPlayback->setChecked(
        m_policyJson["EnableMediaPlayback"].toBool(true));
    m_swAudioTranscode->setChecked(
        m_policyJson["EnableAudioPlaybackTranscoding"].toBool(true));
    m_swVideoTranscode->setChecked(
        m_policyJson["EnableVideoPlaybackTranscoding"].toBool(true));
    m_swVideoRemuxing->setChecked(
        m_policyJson["EnablePlaybackRemuxing"].toBool(true));
    m_spinStreamLimit->setValue(
        m_policyJson["SimultaneousStreamLimit"].toVariant().toInt());

    const qint64 remoteBitrateLimit =
        m_policyJson["RemoteClientBitrateLimit"].toVariant().toLongLong();
    if (remoteBitrateLimit > 0) {
        m_editRemoteBitrateLimit->setText(
            QString::number(remoteBitrateLimit / 1000000.0, 'f', 2));
    } else {
        m_editRemoteBitrateLimit->clear();
    }

    m_swLiveTv->setChecked(m_policyJson["EnableLiveTvAccess"].toBool(true));
    m_swLiveTvManage->setChecked(
        m_policyJson["EnableLiveTvManagement"].toBool(true));
    m_swRemoteControl->setChecked(
        m_policyJson["EnableRemoteControlOfOtherUsers"].toBool(true));
    m_swSharedDevice->setChecked(
        m_policyJson["EnableSharedDeviceControl"].toBool(true));
    m_swMediaConversion->setChecked(
        m_policyJson["EnableMediaConversion"].toBool());
    m_swLinkSharing->setChecked(
        m_policyJson["EnablePublicSharing"].toBool());
    m_swContentDownload->setChecked(
        m_policyJson["EnableContentDownloading"].toBool(
            m_policyJson["EnableMediaDownloading"].toBool(true)));
    m_swSyncTranscoding->setChecked(
        m_policyJson["EnableSyncTranscoding"].toBool());
    m_swSubtitleDownload->setChecked(
        m_policyJson["EnableSubtitleDownloading"].toBool());
    m_swSubtitleManagement->setChecked(
        m_policyJson["EnableSubtitleManagement"].toBool());
    m_swDeleteAllFolders->setChecked(
        m_policyJson["EnableContentDeletion"].toBool());
    m_swHidden->setChecked(m_policyJson["IsHidden"].toBool());
    m_swHiddenRemotely->setChecked(
        m_policyJson["IsHiddenRemotely"].toBool());
    m_swHiddenUnused->setChecked(
        m_policyJson["IsHiddenFromUnusedDevices"].toBool());

    m_enabledDeletionTargetIds.clear();
    for (const auto &value :
         m_policyJson["EnableContentDeletionFromFolders"].toArray()) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty() && !m_enabledDeletionTargetIds.contains(id)) {
            m_enabledDeletionTargetIds.append(id);
        }
    }

    const bool allFolders = m_policyJson["EnableAllFolders"].toBool(true);
    m_swAllFolders->setChecked(allFolders);
    m_enabledFolderIds.clear();
    for (const QJsonValue &value : m_policyJson["EnabledFolders"].toArray()) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty() && !m_enabledFolderIds.contains(id)) {
            m_enabledFolderIds.append(id);
        }
    }
    m_excludedSubFolderIds.clear();
    for (const QJsonValue &value :
         m_policyJson["ExcludedSubFolders"].toArray()) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty() && !m_excludedSubFolderIds.contains(id)) {
            m_excludedSubFolderIds.append(id);
        }
    }

    const bool allChannels =
        m_policyJson["EnableAllChannels"].toBool(true);
    m_swAllChannels->setChecked(allChannels);
    m_enabledChannelIds.clear();
    for (const QJsonValue &value : m_policyJson["EnabledChannels"].toArray()) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty() && !m_enabledChannelIds.contains(id)) {
            m_enabledChannelIds.append(id);
        }
    }

    const bool allDevices =
        m_policyJson["EnableAllDevices"].toBool(true);
    m_swAllDevices->setChecked(allDevices);
    m_enabledDeviceIds.clear();
    for (const QJsonValue &value : m_policyJson["EnabledDevices"].toArray()) {
        const QString id = value.toString().trimmed();
        if (!id.isEmpty() && !m_enabledDeviceIds.contains(id)) {
            m_enabledDeviceIds.append(id);
        }
    }

    const int maxRating = m_policyJson["MaxParentalRating"].toInt(0);
    int bestIndex = 0;
    for (int i = 0; i < m_comboMaxRating->count(); ++i) {
        if (m_comboMaxRating->itemData(i).toInt() <= maxRating) {
            bestIndex = i;
        }
    }
    m_comboMaxRating->setCurrentIndex(bestIndex);

    QStringList blockedUnratedTypes;
    for (const auto &value : m_policyJson["BlockUnratedItems"].toArray()) {
        const QString itemType = value.toString().trimmed();
        if (!itemType.isEmpty() && !blockedUnratedTypes.contains(itemType)) {
            blockedUnratedTypes.append(itemType);
        }
    }
    for (QCheckBox *cb : m_unratedItemCheckboxes) {
        if (cb) {
            cb->setChecked(blockedUnratedTypes.contains(
                cb->property("itemType").toString()));
        }
    }

    QStringList blockedTags;
    for (const auto &value : m_policyJson["BlockedTags"].toArray()) {
        const QString tag = value.toString().trimmed();
        if (!tag.isEmpty() && !blockedTags.contains(tag)) {
            blockedTags.append(tag);
        }
    }
    m_tagInputBlockedTags->setValue(blockedTags.join(QStringLiteral(",")));
    m_comboTagMode->setCurrentIndex(
        m_policyJson["IsTagBlockingModeInclusive"].toBool() ? 1 : 0);
    m_comboMultiRestrictionMode->setCurrentIndex(
        m_policyJson["AllowTagOrRating"].toBool() ? 1 : 0);

    populateAccessSchedules(m_policyJson["AccessSchedules"].toArray());
}

QStringList UserEditDialog::collectBlockedTags() const {
    if (!m_tagInputBlockedTags) {
        return {};
    }
    return splitCommaSeparatedValues(m_tagInputBlockedTags->value());
}

QJsonArray UserEditDialog::collectAccessSchedules(QString *errorText) const {
    QJsonArray schedules;
    if (!m_scheduleListLayout) {
        return schedules;
    }

    for (int i = 0; i < m_scheduleListLayout->count(); ++i) {
        QWidget *row =
            m_scheduleListLayout->itemAt(i)
                ? m_scheduleListLayout->itemAt(i)->widget()
                : nullptr;
        if (!row) {
            continue;
        }

        auto *dayCombo = row->findChild<ModernComboBox *>("ScheduleDayCombo");
        auto *startCombo =
            row->findChild<ModernComboBox *>("ScheduleStartCombo");
        auto *endCombo = row->findChild<ModernComboBox *>("ScheduleEndCombo");
        if (!dayCombo || !startCombo || !endCombo) {
            continue;
        }

        const int startHour = startCombo->currentData().toInt();
        const int endHour = endCombo->currentData().toInt();
        if (startHour >= endHour) {
            if (errorText) {
                *errorText = tr("Schedule start time must be earlier than end time");
            }
            return {};
        }

        QJsonObject schedule;
        schedule["DayOfWeek"] = dayCombo->currentData().toString();
        schedule["StartHour"] = startHour;
        schedule["EndHour"] = endHour;
        schedules.append(schedule);
    }

    return schedules;
}

QCoro::Task<void> UserEditDialog::saveProfileAndAccess() {
    QPointer<UserEditDialog> safeThis(this);

    const QString trimmedUserName = m_editUserName->text().trimmed();
    if (trimmedUserName.isEmpty()) {
        m_editUserName->setFocus();
        ModernToast::showMessage(tr("User name cannot be empty"), 2000);
        co_return;
    }

    QString scheduleError;
    const QJsonArray schedules = collectAccessSchedules(&scheduleError);
    if (!scheduleError.isEmpty()) {
        ModernToast::showMessage(scheduleError, 2500);
        co_return;
    }

    m_isSubmitting = true;
    updatePrimaryActionState();

    QJsonObject policy = m_policyJson;

    policy["IsAdministrator"] = m_swAdmin->isChecked();
    policy["IsDisabled"] = m_swDisabled->isChecked();
    policy["EnableRemoteAccess"] = m_swRemoteAccess->isChecked();
    policy["EnableUserPreferenceAccess"] =
        m_swAllowChangeProfile->isChecked();
    policy["EnableMediaPlayback"] = m_swMediaPlayback->isChecked();
    policy["EnableAudioPlaybackTranscoding"] = m_swAudioTranscode->isChecked();
    policy["EnableVideoPlaybackTranscoding"] = m_swVideoTranscode->isChecked();
    policy["EnablePlaybackRemuxing"] = m_swVideoRemuxing->isChecked();
    policy["SimultaneousStreamLimit"] = m_spinStreamLimit->value();

    const double bitrateLimitMbps =
        m_editRemoteBitrateLimit->text().trimmed().toDouble();
    policy["RemoteClientBitrateLimit"] =
        bitrateLimitMbps > 0.0
            ? static_cast<qint64>(bitrateLimitMbps * 1000000.0)
            : 0;

    policy["EnableLiveTvAccess"] = m_swLiveTv->isChecked();
    policy["EnableLiveTvManagement"] = m_swLiveTvManage->isChecked();
    policy["EnableRemoteControlOfOtherUsers"] =
        m_swRemoteControl->isChecked();
    policy["EnableSharedDeviceControl"] = m_swSharedDevice->isChecked();
    policy["EnableMediaConversion"] = m_swMediaConversion->isChecked();
    policy["EnablePublicSharing"] = m_swLinkSharing->isChecked();
    policy["EnableContentDownloading"] = m_swContentDownload->isChecked();
    policy.remove("EnableMediaDownloading");
    policy["EnableSyncTranscoding"] = m_swSyncTranscoding->isChecked();
    policy["EnableSubtitleDownloading"] = m_swSubtitleDownload->isChecked();
    policy["EnableSubtitleManagement"] = m_swSubtitleManagement->isChecked();
    policy["IsHidden"] = m_swHidden->isChecked();
    policy["IsHiddenRemotely"] = m_swHiddenRemotely->isChecked();
    policy["IsHiddenFromUnusedDevices"] = m_swHiddenUnused->isChecked();

    if (m_authProvidersLoaded && m_comboAuthProvider &&
        m_comboAuthProvider->count() > 0) {
        const QString providerId =
            m_comboAuthProvider->currentData().toString().trimmed();
        if (!providerId.isEmpty()) {
            policy["AuthenticationProviderId"] = providerId;
        } else {
            policy.remove("AuthenticationProviderId");
        }
    }

    if (m_dynamicFeaturesLoaded) {
        QJsonArray restrictedFeatures;
        for (QCheckBox *cb : m_dynamicFeatureCheckboxes) {
            if (cb && !cb->isChecked()) {
                restrictedFeatures.append(cb->property("featureId").toString());
            }
        }
        policy["RestrictedFeatures"] = restrictedFeatures;
    }

    if (m_deletionTargetsLoaded) {
        policy["EnableContentDeletion"] = m_swDeleteAllFolders->isChecked();
        QJsonArray deletionTargets;
        if (!m_swDeleteAllFolders->isChecked()) {
            for (QCheckBox *cb : m_deleteTargetCheckboxes) {
                if (cb && cb->isChecked()) {
                    deletionTargets.append(cb->property("targetId").toString());
                }
            }
        }
        policy["EnableContentDeletionFromFolders"] = deletionTargets;
    }

    if (m_accessFoldersLoaded) {
        policy["EnableAllFolders"] = m_swAllFolders->isChecked();
        QJsonArray enabledFolders;
        QJsonArray excludedSubFolders;
        if (!m_swAllFolders->isChecked()) {
            for (QCheckBox *cb : m_folderCheckboxes) {
                if (cb && cb->isChecked()) {
                    enabledFolders.append(cb->property("folderId").toString());
                }
            }
            for (QCheckBox *cb : m_subFolderCheckboxes) {
                if (cb && !cb->isChecked()) {
                    excludedSubFolders.append(
                        cb->property("subFolderId").toString());
                }
            }
        }
        policy["EnabledFolders"] = enabledFolders;
        policy["ExcludedSubFolders"] = excludedSubFolders;
    }

    if (m_channelsLoaded) {
        policy["EnableAllChannels"] = m_swAllChannels->isChecked();
        QJsonArray enabledChannels;
        if (!m_swAllChannels->isChecked()) {
            for (QCheckBox *cb : m_channelCheckboxes) {
                if (cb && cb->isChecked()) {
                    enabledChannels.append(
                        cb->property("channelId").toString());
                }
            }
        }
        policy["EnabledChannels"] = enabledChannels;
    }

    if (m_devicesLoaded) {
        policy["EnableAllDevices"] = m_swAllDevices->isChecked();
        QJsonArray enabledDevices;
        if (!m_swAllDevices->isChecked()) {
            for (QCheckBox *cb : m_deviceCheckboxes) {
                if (cb && cb->isChecked()) {
                    enabledDevices.append(
                        cb->property("deviceId").toString());
                }
            }
        }
        policy["EnabledDevices"] = enabledDevices;
    }

    const int selectedRating = m_comboMaxRating->currentData().toInt();
    if (selectedRating > 0) {
        policy["MaxParentalRating"] = selectedRating;
    } else {
        policy.remove("MaxParentalRating");
    }

    QJsonArray blockUnratedItems;
    for (QCheckBox *cb : m_unratedItemCheckboxes) {
        if (cb && cb->isChecked()) {
            blockUnratedItems.append(cb->property("itemType").toString());
        }
    }
    policy["BlockUnratedItems"] = blockUnratedItems;

    const QStringList blockedTags = collectBlockedTags();
    QJsonArray blockedTagsArray;
    for (const QString &tag : blockedTags) {
        blockedTagsArray.append(tag);
    }
    policy["BlockedTags"] = blockedTagsArray;
    policy["IsTagBlockingModeInclusive"] =
        m_comboTagMode->currentData().toString() == QStringLiteral("include");
    policy["AllowTagOrRating"] =
        m_comboMultiRestrictionMode->currentData().toString() ==
        QStringLiteral("any");
    policy["AccessSchedules"] = schedules;

    qDebug() << "[UserEditDialog] Saving policy"
             << "| userId=" << m_userId
             << "| userName=" << trimmedUserName
             << "| policyKeys=" << policy.keys().size();

    try {
        if (trimmedUserName != m_loadedUserName) {
            QJsonObject userPayload = m_fullUserJson;
            userPayload["Name"] = trimmedUserName;

            co_await m_core->adminService()->updateUser(m_userId, userPayload);
            if (!safeThis) {
                co_return;
            }

            m_loadedUserName = trimmedUserName;
            m_fullUserJson["Name"] = trimmedUserName;
            setTitle(tr("User Settings — %1").arg(trimmedUserName));
        }

        if (m_supportsConnectLink && m_editConnectUserName) {
            const QString connectUserName =
                m_editConnectUserName->text().trimmed();
            if (connectUserName != m_loadedConnectUserName) {
                if (connectUserName.isEmpty()) {
                    if (!m_loadedConnectUserName.isEmpty()) {
                        co_await m_core->adminService()->removeConnectLink(
                            m_userId);
                        if (!safeThis) {
                            co_return;
                        }
                    }
                } else {
                    co_await m_core->adminService()->updateConnectLink(
                        m_userId, connectUserName);
                    if (!safeThis) {
                        co_return;
                    }
                }
                m_loadedConnectUserName = connectUserName;
                m_fullUserJson["ConnectUserName"] = connectUserName;
            }
        }

        co_await m_core->adminService()->setUserPolicy(m_userId, policy);
        if (!safeThis) {
            co_return;
        }

        ModernToast::showMessage(tr("Settings saved"), 2000);
        m_policyJson = policy;
        m_fullUserJson["Policy"] = policy;
        emit userUpdated();

    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        ModernToast::showMessage(
            tr("Failed to save: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (safeThis) {
        m_isSubmitting = false;
        updateConditionalSections();
        updatePrimaryActionState();
    }
}

QCoro::Task<void> UserEditDialog::savePassword() {
    QPointer<UserEditDialog> safeThis(this);

    const QString newPw = m_editNewPassword->text();
    const QString confirmPw = m_editConfirmPassword->text();

    if (newPw != confirmPw) {
        ModernToast::showMessage(tr("Passwords do not match"), 2000);
        co_return;
    }

    m_isSubmitting = true;
    updatePrimaryActionState();

    try {
        co_await m_core->adminService()->updateUserPassword(
            m_userId, QString(), newPw);
        if (!safeThis) {
            co_return;
        }

        ModernToast::showMessage(tr("Password updated"), 2000);
        m_editNewPassword->clear();
        m_editConfirmPassword->clear();

    } catch (const std::exception &e) {
        if (!safeThis) {
            co_return;
        }
        ModernToast::showMessage(
            tr("Failed to update password: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }

    if (safeThis) {
        m_isSubmitting = false;
        updatePrimaryActionState();
    }
}
