#ifndef UPDATEMANAGER_H
#define UPDATEMANAGER_H

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;

struct UpdateInfo {
    QString version;
    QString name;
    QString releaseNotes;
    QDateTime publishedAt;
    QUrl releaseUrl;
    QUrl downloadUrl;
    QString assetName;
    QString sha256Digest;
    qint64 assetSize = 0;
};
Q_DECLARE_METATYPE(UpdateInfo)

class UpdateManager : public QObject
{
    Q_OBJECT

public:
    enum class CheckMode { Automatic, Manual };
    Q_ENUM(CheckMode)

    static UpdateManager *instance();

    void checkForUpdates(CheckMode mode);
    bool openUpdate(const UpdateInfo &info);
    bool isChecking(CheckMode mode) const;

Q_SIGNALS:
    void updateAvailable(const UpdateInfo &info, UpdateManager::CheckMode mode);
    void noUpdateAvailable(UpdateManager::CheckMode mode);
    void checkFailed(const QString &error, UpdateManager::CheckMode mode);
    void updateOpened(const QString &version);

private:
    explicit UpdateManager(QObject *parent = nullptr);
    void handleReply(QNetworkReply *reply, CheckMode mode);

    QNetworkAccessManager *m_networkManager = nullptr;
    bool m_automaticCheckInProgress = false;
    bool m_manualCheckInProgress = false;
    QString m_openedVersion;
};

#endif 
