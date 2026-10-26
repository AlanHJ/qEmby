#ifndef APICLIENT_H
#define APICLIENT_H

#include "../qEmbyCore_global.h"
#include "networkmanager.h"
#include "../models/profile/serverprofile.h"
#include <QByteArray>
#include <qcorotask.h>
#include <QJsonArray>
#include <QUrlQuery>

class QEMBYCORE_EXPORT ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(const ServerProfile& profile, NetworkManager* nm, QObject* parent = nullptr);

    
    void setCancellationContext(QObject* context);

    
    QCoro::Task<QJsonObject> get(const QString& path);
    QCoro::Task<QJsonObject> get(QString path, QPointer<QObject> cancellationContext);
    QCoro::Task<QString> getText(const QString& path);
    QCoro::Task<QJsonObject> post(const QString& path, const QJsonObject& payload);
    QCoro::Task<QJsonObject> post(QString path, QJsonObject payload, QPointer<QObject> cancellationContext);
    QCoro::Task<QJsonObject> postArray(const QString& path, const QJsonArray& payload);
    QCoro::Task<QJsonObject> postBytes(const QString& path, QByteArray payload,
                                       QString contentType);
    QCoro::Task<QJsonObject> postForm(const QString& path, const QUrlQuery& formData);
    QCoro::Task<QJsonObject> deleteResource(const QString& path);

private:
    ServerProfile m_profile;
    NetworkManager* m_network;
    QPointer<QObject> m_cancellationContext;

    
    QMap<QString, QString> getAuthHeaders() const;
    NetworkRequestOptions requestOptions() const;
};

#endif
