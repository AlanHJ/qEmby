#ifndef WEBDAVSYNCSERVICE_H
#define WEBDAVSYNCSERVICE_H

#include "../../api/webdav/webdaventry.h"
#include "../../config/webdavprofilestore.h"
#include "../../models/sync/configbundle.h"
#include "../../qEmbyCore_global.h"

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <stdexcept>

#include <qcoro/qcorotask.h>

class WebdavClient;
















struct WebdavSnapshot
{
    QString fileName;
    QString appVersion;
    QString osName;
    QString deviceHint;
    QString customTag;
    QDateTime createdAt;
    QDateTime mtime;
    qint64 size = 0;
};

class QEMBYCORE_EXPORT WebdavPassphraseRequiredError : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

















class QEMBYCORE_EXPORT WebdavSyncService : public QObject
{
    Q_OBJECT
public:
    explicit WebdavSyncService(WebdavProfileStore *store, QObject *parent = nullptr);
    ~WebdavSyncService() override;

    

    
    QCoro::Task<bool> testConnection();

    
    
    QCoro::Task<QString> uploadSnapshot(QString customTag, bool encrypt, QString passphrase);

    
    QCoro::Task<QList<WebdavSnapshot>> listSnapshots();

    
    
    QCoro::Task<ConfigBundle> downloadSnapshot(QString fileName, QString passphrase = QString());

    
    QCoro::Task<bool> deleteSnapshot(QString fileName);

    

    
    
    
    bool applyBundle(const ConfigBundle &bundle, MergeStrategy strategy);

    
    static QString proposedSnapshotFileName(const ConfigBundleMetadata &metadata,
                                            QString customTag);

    
    static bool isQEmbySnapshotName(const QString &fileName);

signals:
    
    void snapshotListChanged();

    
    void appliedBundle(MergeStrategy strategy);

private:
    
    WebdavClient *buildClient();

    
    static void parseSnapshotName(const QString &fileName,
                                  QString &appVersionOut,
                                  QString &osNameOut,
                                  QString &deviceHintOut,
                                  QString &customTagOut,
                                  QDateTime &createdAtOut);

    QPointer<WebdavProfileStore> m_store;
};

#endif 
