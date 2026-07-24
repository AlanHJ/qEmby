#ifndef WINDOWSUPDATEMANAGER_H
#define WINDOWSUPDATEMANAGER_H

#include "updatemanager.h"
#include <QObject>

class QCryptographicHash;
class QNetworkAccessManager;
class QNetworkReply;
class QSaveFile;

class WindowsUpdateManager : public QObject
{
    Q_OBJECT

public:
    enum class Stage { Downloading, VerifyingDigest, Ready };
    Q_ENUM(Stage)

    static WindowsUpdateManager *instance();

    void startDownload(const UpdateInfo &info);
    void cancel();
    bool launchInstaller(bool restartAfterUpdate, QString *errorMessage = nullptr);
    bool isBusy() const;

Q_SIGNALS:
    void stageChanged(WindowsUpdateManager::Stage stage, const QString &text);
    void downloadProgress(qint64 received, qint64 total);
    void readyToInstall(const QString &installerPath);
    void failed(const QString &error);
    void cancelled();

private:
    explicit WindowsUpdateManager(QObject *parent = nullptr);
    void finishDownload();
    void fail(const QString &error);
    QNetworkAccessManager *m_networkManager = nullptr;
    QNetworkReply *m_reply = nullptr;
    QSaveFile *m_outputFile = nullptr;
    QCryptographicHash *m_hash = nullptr;
    QString m_expectedDigest;
    QString m_targetPath;
    QString m_verifiedInstallerPath;
    bool m_cancelled = false;
};

#endif 
