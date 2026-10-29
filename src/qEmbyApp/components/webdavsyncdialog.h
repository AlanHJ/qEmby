#ifndef WEBDAVSYNCDIALOG_H
#define WEBDAVSYNCDIALOG_H

#include "moderndialogbase.h"
#include "models/sync/configbundle.h"
#include "services/sync/webdavsyncservice.h"

#include <QPointer>
#include <QString>

class QAction;
class QButtonGroup;
class QLabel;
class QLineEdit;
class QPushButton;
class QStandardItemModel;
class QTableView;
class QTimer;
class InlineToggleButton;
class SslVerificationButton;
class TrailingWidgetLineEdit;
class ServerManager;
class WebdavProfileStore;




















class WebdavSyncDialog : public ModernDialogBase
{
    Q_OBJECT
public:
    WebdavSyncDialog(WebdavProfileStore *store, ServerManager *serverManager,
                     QWidget *parent = nullptr);
    ~WebdavSyncDialog() override;

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onTestConnectionClicked();
    void onSaveProfileClicked();
    void onTogglePasswordVisibility();
    void onUploadClicked();
    void onRefreshSnapshotsClicked();
    void onApplySelectedClicked();
    void onDeleteSelectedClicked();
    void onSnapshotSelectionChanged();
    void updateProposedFileName();

private:
    void buildUi();
    void wireSignals();
    void loadProfileToUi();
    void updateSslOptionsVisibility();
    WebdavProfile collectProfileFromUi() const;

    void refreshSnapshotList();
    void requestSnapshotRefresh();
    void clearSnapshotModel();
    void appendSnapshotRow(const WebdavSnapshot &s);

    void setBusy(bool busy, const QString &hint = QString());
    void showError(const QString &title, const QString &text);
    void showToast(const QString &text);

    void setTestButtonIdle();
    void setTestButtonLoading();
    void setTestButtonResult(bool success);

    QString currentSnapshotTag() const;
    QString currentSelectedSnapshotName() const;
    MergeStrategy currentMergeStrategy() const;

    
    QPointer<WebdavProfileStore> m_store;
    QPointer<ServerManager> m_serverManager;
    WebdavSyncService *m_service = nullptr;

    
    TrailingWidgetLineEdit *m_baseUrlEdit = nullptr;
    QLineEdit *m_userEdit = nullptr;
    QLineEdit *m_passwordEdit = nullptr;
    QAction *m_togglePwdAction = nullptr;
    QLineEdit *m_rootDirEdit = nullptr;
    SslVerificationButton *m_ignoreSslButton = nullptr;
    QPushButton *m_testBtn = nullptr;
    QPushButton *m_saveProfileBtn = nullptr;

    
    TrailingWidgetLineEdit *m_snapshotTagEdit = nullptr;
    InlineToggleButton *m_encryptSnapshotButton = nullptr;
    QLabel *m_proposedFileLabel = nullptr;
    QPushButton *m_uploadBtn = nullptr;

    
    QTableView *m_snapshotTable = nullptr;
    QStandardItemModel *m_snapshotModel = nullptr;
    QLabel *m_lastSyncLabel = nullptr;
    QLabel *m_snapshotStatusLabel = nullptr;
    QPushButton *m_refreshBtn = nullptr;
    QPushButton *m_applyBtn = nullptr;
    QPushButton *m_deleteBtn = nullptr;
    QButtonGroup *m_strategyGroup = nullptr;
    QPushButton *m_strategyReplaceBtn = nullptr;
    QPushButton *m_strategyMergeBtn = nullptr;
    QPushButton *m_strategyLocalWinsBtn = nullptr;

    
    bool m_busy = false;
    bool m_pendingSnapshotRefresh = false;
    bool m_autoRefreshedOnShow = false;
    QTimer *m_testSpinnerTimer = nullptr;
    qreal m_testSpinnerAngle = 0.0;
};

#endif 
