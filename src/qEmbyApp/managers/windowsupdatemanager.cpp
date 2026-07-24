#include "windowsupdatemanager.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QSaveFile>
#include <QStandardPaths>

WindowsUpdateManager *WindowsUpdateManager::instance()
{
    static auto *manager =
        new WindowsUpdateManager(QCoreApplication::instance());
    return manager;
}

WindowsUpdateManager::WindowsUpdateManager(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
}

bool WindowsUpdateManager::isBusy() const
{
    return m_reply != nullptr;
}

void WindowsUpdateManager::startDownload(const UpdateInfo &info)
{
    cancel();
    m_cancelled = false;
    m_verifiedInstallerPath.clear();

    if (!info.downloadUrl.isValid() ||
        info.downloadUrl.scheme() != QStringLiteral("https") ||
        !info.assetName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        fail(tr("No compatible Windows installer is available for this release."));
        return;
    }
    if (info.sha256Digest.size() != 64) {
        fail(tr("The release does not provide a valid SHA-256 digest."));
        return;
    }

    const QString updateDirectory =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QStringLiteral("/qEmby-updates/") + info.version;
    if (!QDir().mkpath(updateDirectory)) {
        fail(tr("Could not create the temporary update directory."));
        return;
    }

    const QString safeFileName = QFileInfo(info.assetName).fileName();
    if (safeFileName.isEmpty()) {
        fail(tr("The Windows installer has an invalid file name."));
        return;
    }
    m_targetPath = QDir(updateDirectory).filePath(safeFileName);
    m_expectedDigest = info.sha256Digest.toLower();
    QFile::remove(m_targetPath);

    m_outputFile = new QSaveFile(m_targetPath, this);
    if (!m_outputFile->open(QIODevice::WriteOnly)) {
        fail(tr("Could not open the temporary update file for writing."));
        return;
    }
    m_hash = new QCryptographicHash(QCryptographicHash::Sha256);

    qInfo() << "WindowsUpdateManager: starting download"
            << "| version=" << info.version << "| asset=" << safeFileName
            << "| expectedBytes=" << info.assetSize;

    QNetworkRequest request(info.downloadUrl);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("qEmby/%1")
                          .arg(QCoreApplication::applicationVersion()));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);
    m_reply = m_networkManager->get(request);

    Q_EMIT stageChanged(Stage::Downloading, tr("Downloading update…"));
    connect(m_reply, &QNetworkReply::downloadProgress, this,
            &WindowsUpdateManager::downloadProgress);
    connect(m_reply, &QIODevice::readyRead, this, [this]() {
        if (!m_reply || !m_outputFile || !m_hash) {
            return;
        }
        const QByteArray data = m_reply->readAll();
        if (m_outputFile->write(data) != data.size()) {
            fail(tr("Failed to write the downloaded update to disk."));
            return;
        }
        m_hash->addData(data);
    });
    connect(m_reply, &QNetworkReply::finished, this,
            &WindowsUpdateManager::finishDownload);
}

void WindowsUpdateManager::cancel()
{
    const bool hadWork = m_reply || m_outputFile || m_hash ||
                         !m_verifiedInstallerPath.isEmpty();
    m_cancelled = true;
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_outputFile) {
        m_outputFile->cancelWriting();
        delete m_outputFile;
        m_outputFile = nullptr;
    }
    delete m_hash;
    m_hash = nullptr;
    if (!m_targetPath.isEmpty()) {
        QFile::remove(m_targetPath);
    }
    m_verifiedInstallerPath.clear();
    if (hadWork) {
        Q_EMIT cancelled();
    }
}

void WindowsUpdateManager::finishDownload()
{
    if (!m_reply) {
        return;
    }
    QNetworkReply *reply = m_reply;
    m_reply = nullptr;
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    const int status =
        reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (m_cancelled) {
        return;
    }
    if (networkError != QNetworkReply::NoError || status < 200 || status >= 300) {
        fail(tr("Update download failed: %1").arg(networkErrorText));
        return;
    }
    if (m_outputFile && m_hash) {
        const QByteArray remainingData = reply->readAll();
        if (!remainingData.isEmpty()) {
            if (m_outputFile->write(remainingData) != remainingData.size()) {
                fail(tr("Failed to write the downloaded update to disk."));
                return;
            }
            m_hash->addData(remainingData);
        }
    }
    if (!m_outputFile || !m_hash || !m_outputFile->commit()) {
        fail(tr("Could not finalize the downloaded update file."));
        return;
    }
    delete m_outputFile;
    m_outputFile = nullptr;

    Q_EMIT stageChanged(Stage::VerifyingDigest,
                        tr("Verifying SHA-256 digest…"));
    const QString actualDigest = QString::fromLatin1(m_hash->result().toHex());
    delete m_hash;
    m_hash = nullptr;
    if (actualDigest.compare(m_expectedDigest, Qt::CaseInsensitive) != 0) {
        qWarning() << "WindowsUpdateManager: SHA-256 verification failed"
                   << "| asset=" << QFileInfo(m_targetPath).fileName();
        QFile::remove(m_targetPath);
        fail(tr("SHA-256 verification failed. The downloaded file may be corrupted."));
        return;
    }

    m_verifiedInstallerPath = m_targetPath;
    qInfo() << "WindowsUpdateManager: installer SHA-256 verified"
            << "| asset=" << QFileInfo(m_targetPath).fileName();
    Q_EMIT stageChanged(Stage::Ready, tr("Update verified and ready to install."));
    Q_EMIT readyToInstall(m_verifiedInstallerPath);
}

void WindowsUpdateManager::fail(const QString &error)
{
    if (m_reply) {
        disconnect(m_reply, nullptr, this, nullptr);
        m_reply->abort();
        m_reply->deleteLater();
        m_reply = nullptr;
    }
    if (m_outputFile) {
        m_outputFile->cancelWriting();
        delete m_outputFile;
        m_outputFile = nullptr;
    }
    delete m_hash;
    m_hash = nullptr;
    m_verifiedInstallerPath.clear();
    Q_EMIT failed(error);
}

bool WindowsUpdateManager::launchInstaller(bool restartAfterUpdate,
                                            QString *errorMessage)
{
#ifdef Q_OS_WIN
    if (m_verifiedInstallerPath.isEmpty() ||
        !QFileInfo::exists(m_verifiedInstallerPath)) {
        if (errorMessage) {
            *errorMessage = tr("The verified installer is no longer available.");
        }
        return false;
    }

    auto powerShellQuote = [](QString value) {
        value.replace(QLatin1Char('\''), QStringLiteral("''"));
        return QString(QLatin1Char('\'')) + value + QLatin1Char('\'');
    };

    
    
    QString script =
        QStringLiteral("$ErrorActionPreference='Stop';"
                       "Wait-Process -Id %1 -ErrorAction SilentlyContinue;"
                       "$p=Start-Process -FilePath %2 "
                       "-ArgumentList @('/SILENT','/SUPPRESSMSGBOXES','/NORESTART') "
                       "-Wait -PassThru;")
            .arg(QCoreApplication::applicationPid())
            .arg(powerShellQuote(m_verifiedInstallerPath));
    if (restartAfterUpdate) {
        script += QStringLiteral("if($p.ExitCode -eq 0){Start-Process -FilePath %1;}")
                      .arg(powerShellQuote(
                          QCoreApplication::applicationFilePath()));
    }
    const QByteArray encodedCommand =
        QByteArray(reinterpret_cast<const char *>(script.utf16()),
                   script.size() * static_cast<int>(sizeof(char16_t)))
            .toBase64();
    const QStringList powerShellArguments = {
        QStringLiteral("-NoProfile"), QStringLiteral("-NonInteractive"),
        QStringLiteral("-WindowStyle"), QStringLiteral("Hidden"),
        QStringLiteral("-EncodedCommand"),
        QString::fromLatin1(encodedCommand)};
    if (!QProcess::startDetached(QStringLiteral("powershell.exe"),
                                 powerShellArguments,
                                 QCoreApplication::applicationDirPath())) {
        if (errorMessage) {
            *errorMessage =
                tr("Could not start the Windows update helper.");
        }
        return false;
    }
    qInfo() << "WindowsUpdateManager: update helper started"
            << "| restartAfterUpdate=" << restartAfterUpdate;
    return true;
#else
    Q_UNUSED(restartAfterUpdate);
    if (errorMessage) {
        *errorMessage = tr("In-app installation is only available on Windows.");
    }
    return false;
#endif
}
