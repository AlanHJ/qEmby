#ifndef USEREDITDIALOG_H
#define USEREDITDIALOG_H

#include "moderndialogbase.h"
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QStringList>
#include <optional>
#include <qcorotask.h>

struct AdminAuthProviderInfo;
struct AdminChannelInfo;
struct AdminDeviceInfo;
struct AdminFeatureInfo;
struct AdminMediaFolderInfo;
struct AdminParentalRatingInfo;
struct SelectableMediaFolderInfo;

class QCheckBox;
class QEmbyCore;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QVBoxLayout;
class QWidget;
class SlidingStackedWidget;
class ModernComboBox;
class ModernSwitch;
class ModernTagInput;
class CollapsibleSection;

class UserEditDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit UserEditDialog(QEmbyCore *core, const QString &userId,
                            QWidget *parent = nullptr);

signals:
    void userUpdated();
    void userDeleted();

private:
    void setupProfileTab();
    void setupAccessTab();
    void setupParentalTab();
    void setupPasswordTab();

    QCoro::Task<void> loadUserData();
    QCoro::Task<void> saveProfileAndAccess();
    QCoro::Task<void> savePassword();

    void populateFromJson(const QJsonObject &userJson);
    void populateDeletionTargets(const QList<AdminMediaFolderInfo> &folders,
                                 const QList<AdminChannelInfo> &channels);
    void populateSelectableFolders(
        const QList<SelectableMediaFolderInfo> &folders);
    void populateChannels(const QList<AdminChannelInfo> &channels);
    void populateDevices(const QList<AdminDeviceInfo> &devices);
    void populateAuthProviders(
        const QList<AdminAuthProviderInfo> &providers);
    void populateDynamicFeatures(const QList<AdminFeatureInfo> &features);
    void populateParentalRatings(
        const QList<AdminParentalRatingInfo> &ratings);
    void populateAccessSchedules(const QJsonArray &schedules);
    void appendAccessScheduleRow(const QString &dayOfWeek,
                                 int startHour,
                                 int endHour);

    void onNavigationChanged(int index);
    void triggerPrimaryAction();
    void updatePrimaryActionState();
    void updateNavigationState();
    void updateConditionalSections();
    void updateParentalSectionState();

    QStringList collectBlockedTags() const;
    QJsonArray collectAccessSchedules(QString *errorText = nullptr) const;

    QEmbyCore *m_core = nullptr;
    QString m_userId;
    QJsonObject m_fullUserJson;
    QJsonObject m_policyJson;
    QString m_loadedUserName;
    QString m_loadedConnectUserName;

    SlidingStackedWidget *m_pageStack = nullptr;
    QList<QPushButton *> m_navButtons;
    QPushButton *m_btnCancel = nullptr;
    QPushButton *m_btnPrimaryAction = nullptr;
    int m_currentPageIndex = 0;
    bool m_isDataLoaded = false;
    bool m_isSubmitting = false;
    bool m_accessFoldersLoaded = false;
    bool m_channelsLoaded = false;
    bool m_devicesLoaded = false;
    bool m_deletionTargetsLoaded = false;
    bool m_authProvidersLoaded = false;
    bool m_dynamicFeaturesLoaded = false;
    bool m_supportsConnectLink = false;
    bool m_hasChannelOptions = false;
    bool m_hasDeviceOptions = false;

    
    QLineEdit *m_editUserName = nullptr;
    QWidget *m_connectInfoContainer = nullptr;
    QLineEdit *m_editConnectUserName = nullptr;
    QWidget *m_authProviderRow = nullptr;
    ModernComboBox *m_comboAuthProvider = nullptr;
    ModernSwitch *m_swAdmin = nullptr;
    ModernSwitch *m_swDisabled = nullptr;
    ModernSwitch *m_swRemoteAccess = nullptr;
    ModernSwitch *m_swAllowChangeProfile = nullptr;
    ModernSwitch *m_swMediaPlayback = nullptr;
    ModernSwitch *m_swAudioTranscode = nullptr;
    ModernSwitch *m_swVideoTranscode = nullptr;
    ModernSwitch *m_swVideoRemuxing = nullptr;
    QSpinBox *m_spinStreamLimit = nullptr;
    QLineEdit *m_editRemoteBitrateLimit = nullptr;
    ModernSwitch *m_swLiveTv = nullptr;
    ModernSwitch *m_swLiveTvManage = nullptr;
    QWidget *m_dynamicFeatureSection = nullptr;
    QVBoxLayout *m_dynamicFeatureLayout = nullptr;
    QList<QCheckBox *> m_dynamicFeatureCheckboxes;
    ModernSwitch *m_swRemoteControl = nullptr;
    ModernSwitch *m_swSharedDevice = nullptr;
    ModernSwitch *m_swMediaConversion = nullptr;
    ModernSwitch *m_swLinkSharing = nullptr;
    ModernSwitch *m_swContentDownload = nullptr;
    ModernSwitch *m_swSyncTranscoding = nullptr;
    ModernSwitch *m_swSubtitleDownload = nullptr;
    ModernSwitch *m_swSubtitleManagement = nullptr;
    ModernSwitch *m_swDeleteAllFolders = nullptr;
    QWidget *m_deleteTargetsContainer = nullptr;
    QVBoxLayout *m_deleteTargetsLayout = nullptr;
    QList<QCheckBox *> m_deleteTargetCheckboxes;
    QStringList m_enabledDeletionTargetIds;
    ModernSwitch *m_swHidden = nullptr;
    ModernSwitch *m_swHiddenRemotely = nullptr;
    ModernSwitch *m_swHiddenUnused = nullptr;

    
    ModernSwitch *m_swAllFolders = nullptr;
    QWidget *m_folderListContainer = nullptr;
    QVBoxLayout *m_folderListLayout = nullptr;
    QList<QCheckBox *> m_folderCheckboxes;
    QList<QCheckBox *> m_subFolderCheckboxes;
    QStringList m_enabledFolderIds;
    QStringList m_excludedSubFolderIds;
    CollapsibleSection *m_channelSection = nullptr;
    ModernSwitch *m_swAllChannels = nullptr;
    QWidget *m_channelListContainer = nullptr;
    QVBoxLayout *m_channelListLayout = nullptr;
    QList<QCheckBox *> m_channelCheckboxes;
    QStringList m_enabledChannelIds;
    CollapsibleSection *m_deviceSection = nullptr;
    ModernSwitch *m_swAllDevices = nullptr;
    QWidget *m_deviceListContainer = nullptr;
    QVBoxLayout *m_deviceListLayout = nullptr;
    QList<QCheckBox *> m_deviceCheckboxes;
    QStringList m_enabledDeviceIds;

    
    ModernComboBox *m_comboMaxRating = nullptr;
    QWidget *m_unratedItemsContainer = nullptr;
    QVBoxLayout *m_unratedItemsLayout = nullptr;
    QList<QCheckBox *> m_unratedItemCheckboxes;
    ModernTagInput *m_tagInputBlockedTags = nullptr;
    ModernComboBox *m_comboTagMode = nullptr;
    QWidget *m_multiRestrictionModeRow = nullptr;
    ModernComboBox *m_comboMultiRestrictionMode = nullptr;
    QWidget *m_scheduleSection = nullptr;
    QVBoxLayout *m_scheduleListLayout = nullptr;
    QPushButton *m_btnAddSchedule = nullptr;

    
    QLineEdit *m_editNewPassword = nullptr;
    QLineEdit *m_editConfirmPassword = nullptr;

    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
