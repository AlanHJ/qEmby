#ifndef ADDUSERDIALOG_H
#define ADDUSERDIALOG_H

#include "moderndialogbase.h"
#include <models/admin/adminmodels.h>
#include <qcorotask.h>
#include <optional>

class QEmbyCore;
class QCheckBox;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QWidget;
class ModernComboBox;
class ModernSwitch;
class CollapsibleSection;

class AddUserDialog : public ModernDialogBase {
    Q_OBJECT
public:
    explicit AddUserDialog(QEmbyCore* core, QWidget* parent = nullptr);

signals:
    void userCreated();

private:
    void setupUi();
    void updateCopyModeState();
    void updatePrimaryActionState();
    void populateCopyUserOptions(const QList<UserInfo>& users);
    void populateSelectableFolders(
        const QList<SelectableMediaFolderInfo>& folders);
    void populateChannels(const QList<AdminChannelInfo>& channels);

    QCoro::Task<void> loadInitialData();
    QCoro::Task<void> createUser();

    QEmbyCore* m_core;

    QLineEdit* m_editUserName = nullptr;
    ModernComboBox* m_comboCopyFromUser = nullptr;
    QCheckBox* m_chkCopyUserPolicy = nullptr;
    QCheckBox* m_chkCopyUserConfiguration = nullptr;
    ModernSwitch* m_swAdmin = nullptr;
    ModernSwitch* m_swAllFolders = nullptr;
    ModernSwitch* m_swAllChannels = nullptr;
    QWidget* m_copyOptionsSection = nullptr;
    CollapsibleSection* m_libraryAccessSection = nullptr;
    CollapsibleSection* m_channelAccessSection = nullptr;
    QWidget* m_folderListContainer = nullptr;
    QWidget* m_channelListContainer = nullptr;
    QVBoxLayout* m_folderListLayout = nullptr;
    QVBoxLayout* m_channelListLayout = nullptr;
    QPushButton* m_btnCancel = nullptr;
    QPushButton* m_btnCreate = nullptr;

    QList<QCheckBox*> m_folderCheckboxes;
    QList<QCheckBox*> m_channelCheckboxes;
    QStringList m_fixedEnabledFolderIds;
    bool m_hasChannelOptions = false;
    bool m_isDataLoaded = false;
    bool m_isSubmitting = false;

    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
