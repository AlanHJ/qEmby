#include "updateprogressdialog.h"

#include "modernmessagebox.h"
#include "modernswitch.h"
#include "../managers/windowsupdatemanager.h"
#include <QApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>

namespace {

QString formatBytes(qint64 bytes)
{
    constexpr qreal kiloByte = 1024.0;
    constexpr qreal megaByte = kiloByte * 1024.0;
    if (bytes >= static_cast<qint64>(megaByte)) {
        return UpdateProgressDialog::tr("%1 MB")
            .arg(QLocale().toString(bytes / megaByte, 'f', 1));
    }
    return UpdateProgressDialog::tr("%1 KB")
        .arg(QLocale().toString(bytes / kiloByte, 'f', 1));
}

} 

UpdateProgressDialog::UpdateProgressDialog(const UpdateInfo &info,
                                           QWidget *parent)
    : ModernDialogBase(parent), m_info(info)
{
    setTitle(tr("Update qEmby"));
    setMinimumWidth(500);
    resize(560, 320);

    auto *layout = contentLayout();
    layout->setSpacing(12);

    auto *headline = new QLabel(tr("Updating to %1").arg(info.version), this);
    headline->setObjectName(QStringLiteral("UpdateProgressHeadline"));
    layout->addWidget(headline);

    m_statusLabel = new QLabel(tr("Preparing download…"), this);
    m_statusLabel->setObjectName(QStringLiteral("UpdateProgressStatus"));
    m_statusLabel->setWordWrap(true);
    layout->addWidget(m_statusLabel);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("UpdateDownloadProgressBar"));
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(false);
    layout->addWidget(m_progressBar);

    m_detailLabel = new QLabel(tr("Waiting for download…"), this);
    m_detailLabel->setObjectName(QStringLiteral("UpdateProgressDetail"));
    m_detailLabel->setWordWrap(true);
    m_detailLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_detailLabel);

    auto *restartCard = new QWidget(this);
    restartCard->setObjectName(QStringLiteral("UpdateRestartCard"));
    restartCard->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto *restartLayout = new QHBoxLayout(restartCard);
    restartLayout->setContentsMargins(15, 11, 15, 11);
    restartLayout->setSpacing(16);

    auto *restartTextLayout = new QVBoxLayout();
    restartTextLayout->setContentsMargins(0, 0, 0, 0);
    restartTextLayout->setSpacing(3);
    auto *restartTitle =
        new QLabel(tr("Start qEmby after updating"), restartCard);
    restartTitle->setObjectName(QStringLiteral("UpdateRestartTitle"));
    auto *restartDescription = new QLabel(
        tr("Automatically reopen the application after installation completes"),
        restartCard);
    restartDescription->setObjectName(
        QStringLiteral("UpdateRestartDescription"));
    restartDescription->setWordWrap(true);
    restartTextLayout->addWidget(restartTitle);
    restartTextLayout->addWidget(restartDescription);
    restartLayout->addLayout(restartTextLayout, 1);

    m_restartSwitch = new ModernSwitch(restartCard);
    m_restartSwitch->setChecked(true);
    m_restartSwitch->setToolTip(tr("Start qEmby after updating"));
    restartLayout->addWidget(m_restartSwitch);
    layout->addWidget(restartCard);

    auto *buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);
    buttonLayout->addStretch();

    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setObjectName(QStringLiteral("dialog-btn-cancel"));
    m_cancelButton->setCursor(Qt::PointingHandCursor);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(m_cancelButton);

    m_installButton = new QPushButton(tr("Install Update"), this);
    m_installButton->setObjectName(QStringLiteral("dialog-btn-primary"));
    m_installButton->setCursor(Qt::PointingHandCursor);
    m_installButton->setEnabled(false);
    connect(m_installButton, &QPushButton::clicked, this,
            &UpdateProgressDialog::installUpdate);
    buttonLayout->addWidget(m_installButton);
    layout->addLayout(buttonLayout);

    auto *manager = WindowsUpdateManager::instance();
    connect(manager, &WindowsUpdateManager::stageChanged, this,
            [this](WindowsUpdateManager::Stage stage, const QString &text) {
                m_statusLabel->setText(text);
                if (stage != WindowsUpdateManager::Stage::Downloading) {
                    m_progressBar->setRange(0, 0);
                }
            });
    connect(manager, &WindowsUpdateManager::downloadProgress, this,
            &UpdateProgressDialog::updateDownloadProgress);
    connect(manager, &WindowsUpdateManager::readyToInstall, this,
            [this](const QString &) {
                m_progressBar->setRange(0, 100);
                m_progressBar->setValue(100);
                m_detailLabel->setText(tr("SHA-256 digest verified."));
                m_installButton->setEnabled(true);
                m_installButton->setFocus();
            });
    connect(manager, &WindowsUpdateManager::failed, this,
            &UpdateProgressDialog::setFailure);
    connect(this, &QDialog::finished, this, [this, manager]() {
        if (!m_installLaunched) {
            manager->cancel();
        }
    });

    QTimer::singleShot(0, manager,
                       [manager, info]() { manager->startDownload(info); });
}

void UpdateProgressDialog::startUpdate(const UpdateInfo &info, QWidget *parent)
{
#ifdef Q_OS_WIN
    UpdateProgressDialog dialog(info, parent);
    dialog.exec();
#else
    if (!UpdateManager::instance()->openUpdate(info)) {
        ModernMessageBox::warning(
            parent, tr("Update Failed"),
            tr("The update page could not be opened. Please try again later."));
    }
#endif
}

void UpdateProgressDialog::updateDownloadProgress(qint64 received, qint64 total)
{
    if (total > 0) {
        m_progressBar->setRange(0, 100);
        m_progressBar->setValue(
            static_cast<int>((received * 100) / qMax<qint64>(1, total)));
        m_detailLabel->setText(
            tr("%1 of %2 (%3%)")
                .arg(formatBytes(received), formatBytes(total),
                     QString::number(m_progressBar->value())));
    } else {
        m_progressBar->setRange(0, 0);
        m_detailLabel->setText(tr("Downloaded %1").arg(formatBytes(received)));
    }
}

void UpdateProgressDialog::setFailure(const QString &error)
{
    m_statusLabel->setText(tr("Update failed"));
    m_detailLabel->setText(error);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_cancelButton->setText(tr("Close"));
    m_installButton->setEnabled(false);
    m_restartSwitch->setEnabled(false);
}

void UpdateProgressDialog::installUpdate()
{
    QString error;
    if (!WindowsUpdateManager::instance()->launchInstaller(
            m_restartSwitch->isChecked(), &error)) {
        setFailure(error);
        return;
    }

    m_installLaunched = true;
    m_statusLabel->setText(tr("Starting the Windows installer…"));
    m_detailLabel->setText(
        tr("qEmby will close now. Installation progress will be shown by the "
           "Windows installer."));
    m_cancelButton->setEnabled(false);
    m_installButton->setEnabled(false);
    m_restartSwitch->setEnabled(false);
    QTimer::singleShot(350, qApp, &QCoreApplication::quit);
}
