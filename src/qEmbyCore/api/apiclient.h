#ifndef APICLIENT_H
#define APICLIENT_H

#include "../qEmbyCore_global.h"
#include "networkmanager.h"
#include "../models/profile/serverprofile.h"
#include <qcorotask.h>
#include <QJsonArray>
#include <QUrlQuery>

class QEMBYCORE_EXPORT ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(const ServerProfile& profile, NetworkManager* nm, QObject* parent = nullptr);

    
    QCoro::Task<QJsonObject> get(const QString& path);
    QCoro::Task<QString> getText(const QString& path);
    QCoro::Task<QJsonObject> post(const QString& path, const QJsonObject& payload);
    QCoro::Task<QJsonObject> postArray(const QString& path, const QJsonArray& payload);
    QCoro::Task<QJsonObject> postForm(const QString& path, const QUrlQuery& formData);
    QCoro::Task<QJsonObject> deleteResource(const QString& path);

private:
    ServerProfile m_profile;
    NetworkManager* m_network;

    
    QMap<QString, QString> getAuthHeaders() const;
    NetworkRequestOptions requestOptions() const;
};

#endif
