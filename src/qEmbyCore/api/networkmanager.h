#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include "../qEmbyCore_global.h"
#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMap>
#include <QSslError>
#include <QUrlQuery>
#include <qcorotask.h>

struct NetworkRequestOptions {
    bool ignoreSslErrors = false;
};

class QEMBYCORE_EXPORT NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    
    QCoro::Task<QJsonObject> get(const QString& url,
                                 const QMap<QString, QString>& headers,
                                 const NetworkRequestOptions& options = {});

    
    QCoro::Task<QString> getText(const QString& url,
                                 const QMap<QString, QString>& headers,
                                 const NetworkRequestOptions& options = {});

    
    QCoro::Task<QByteArray> getBytes(const QString& url,
                                     const QMap<QString, QString>& headers,
                                     const NetworkRequestOptions& options = {});

    
    QCoro::Task<QJsonObject> post(const QString& url,
                                  const QMap<QString, QString>& headers,
                                  const QJsonObject& payload,
                                  const NetworkRequestOptions& options = {});
    QCoro::Task<QJsonObject> postArray(const QString& url,
                                       const QMap<QString, QString>& headers,
                                       const QJsonArray& payload,
                                       const NetworkRequestOptions& options = {});
    QCoro::Task<QJsonObject> postForm(const QString& url,
                                      const QMap<QString, QString>& headers,
                                      const QUrlQuery& formData,
                                      const NetworkRequestOptions& options = {});

    
    QCoro::Task<QJsonObject> deleteResource(const QString& url,
                                            const QMap<QString, QString>& headers,
                                            const NetworkRequestOptions& options = {});

private:
    QNetworkAccessManager* m_networkManager;

    
    void applyHeaders(QNetworkRequest& request, const QMap<QString, QString>& headers);
    void applyRequestOptions(QNetworkRequest& request,
                             const NetworkRequestOptions& options);
    void attachReplyHandlers(QNetworkReply* reply,
                             const NetworkRequestOptions& options,
                             const QString& requestKind);
    QString buildReplyErrorMessage(QNetworkReply* reply, int httpStatus) const;

    
    QJsonObject parseReply(QNetworkReply* reply);
    QString parseReplyAsText(QNetworkReply* reply);
    QByteArray parseReplyAsBytes(QNetworkReply* reply, const QString& requestKind);
};

#endif 
