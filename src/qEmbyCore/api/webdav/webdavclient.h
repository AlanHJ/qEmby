#ifndef WEBDAVCLIENT_H
#define WEBDAVCLIENT_H

#include "../../qEmbyCore_global.h"
#include "../networkmanager.h"
#include "webdaventry.h"
#include "webdavprofile.h"

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

#include <qcorotask.h>

class QNetworkAccessManager;
class QNetworkRequest;


















class QEMBYCORE_EXPORT WebdavClient : public QObject
{
    Q_OBJECT
public:
    explicit WebdavClient(WebdavProfile profile, QObject* parent = nullptr);
    ~WebdavClient() override;

    
    void setProfile(WebdavProfile profile);
    const WebdavProfile& profile() const { return m_profile; }

    
    QCoro::Task<bool> testConnection();

    
    
    QCoro::Task<bool> ensureRootDir();

    
    
    QCoro::Task<QList<WebdavEntry>> list(QString relPath = QString());

    
    QCoro::Task<QByteArray> getFile(QString relPath);

    
    QCoro::Task<bool> putFile(QString relPath, QByteArray bytes,
                              QString contentType = QStringLiteral("application/json"));

    
    QCoro::Task<bool> remove(QString relPath);

    
    
    QCoro::Task<bool> mkcol(QString relPath);

private:
    QNetworkAccessManager* m_nam = nullptr;
    WebdavProfile m_profile;

    
    
    QUrl absoluteUrl(QString relPath) const;
    QUrl absoluteUrlForSegments(const QStringList &segments) const;

    
    QCoro::Task<bool> ensureCollection(QStringList segments);

    NetworkRequestOptions networkOptions() const;

    
    void applyAuthHeader(QNetworkRequest& request) const;
};

#endif 
