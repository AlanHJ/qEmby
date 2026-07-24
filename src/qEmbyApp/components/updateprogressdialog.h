#ifndef UPDATEPROGRESSDIALOG_H
#define UPDATEPROGRESSDIALOG_H

#include "../managers/updatemanager.h"
#include "moderndialogbase.h"

class QLabel;
class QProgressBar;
class QPushButton;
class ModernSwitch;

class UpdateProgressDialog : public ModernDialogBase
{
    Q_OBJECT

public:
    explicit UpdateProgressDialog(const UpdateInfo &info,
                                  QWidget *parent = nullptr);

    static void startUpdate(const UpdateInfo &info, QWidget *parent = nullptr);

private:
    void updateDownloadProgress(qint64 received, qint64 total);
    void setFailure(const QString &error);
    void installUpdate();

    UpdateInfo m_info;
    QLabel *m_statusLabel = nullptr;
    QLabel *m_detailLabel = nullptr;
    QProgressBar *m_progressBar = nullptr;
    ModernSwitch *m_restartSwitch = nullptr;
    QPushButton *m_cancelButton = nullptr;
    QPushButton *m_installButton = nullptr;
    bool m_installLaunched = false;
};

#endif 
