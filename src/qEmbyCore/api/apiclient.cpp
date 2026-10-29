#include "apiclient.h"

#include <utility>

ApiClient::ApiClient(const ServerProfile& profile, NetworkManager* nm, QObject* parent)
    : QObject(parent), m_profile(profile), m_network(nm) {}

void ApiClient::setCancellationContext(QObject* context) {
    m_cancellationContext = context;
}

QMap<QString, QString> ApiClient::getAuthHeaders() const {
    QMap<QString, QString> headers;

    
    
    QString auth = QString("MediaBrowser Client=\"qEmby\", Device=\"Desktop\", "
                           "DeviceId=\"%1\", Version=\"0.1\"")
                       .arg(m_profile.deviceId);

    if (!m_profile.accessToken.isEmpty()) {
        auth += QString(", Token=\"%1\"").arg(m_profile.accessToken);
    }

    
    headers.insert("X-Emby-Authorization", auth);
    return headers;
}

NetworkRequestOptions ApiClient::requestOptions() const {
    NetworkRequestOptions options;
    options.ignoreSslErrors = m_profile.ignoreSslVerification;
    options.userAgentServerId = m_profile.id;
    options.cancellationContext = m_cancellationContext;
    return options;
}

QCoro::Task<QJsonObject> ApiClient::get(const QString& path) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->get(fullUrl, getAuthHeaders(),
                                      requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::get(QString path, QPointer<QObject> cancellationContext) {
    const QString fullUrl = m_profile.url + path;
    auto options = requestOptions();
    if (cancellationContext) options.cancellationContext = cancellationContext;
    co_return co_await m_network->get(fullUrl, getAuthHeaders(), options);
}

QCoro::Task<QString> ApiClient::getText(const QString& path) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->getText(fullUrl, getAuthHeaders(),
                                          requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::post(const QString& path, const QJsonObject& payload) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->post(fullUrl, getAuthHeaders(), payload,
                                       requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::post(QString path, QJsonObject payload, QPointer<QObject> cancellationContext) {
    const QString fullUrl = m_profile.url + path;
    auto options = requestOptions();
    if (cancellationContext) options.cancellationContext = cancellationContext;
    co_return co_await m_network->post(fullUrl, getAuthHeaders(), payload, options);
}

QCoro::Task<QJsonObject> ApiClient::postArray(const QString& path, const QJsonArray& payload) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->postArray(fullUrl, getAuthHeaders(), payload,
                                            requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::postBytes(const QString& path,
                                              QByteArray payload,
                                              QString contentType) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->postBytes(fullUrl, getAuthHeaders(),
                                            std::move(payload),
                                            std::move(contentType),
                                            requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::postForm(const QString& path,
                                             const QUrlQuery& formData) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->postForm(fullUrl, getAuthHeaders(), formData,
                                           requestOptions());
}

QCoro::Task<QJsonObject> ApiClient::deleteResource(const QString& path) {
    QString fullUrl = m_profile.url + path;
    co_return co_await m_network->deleteResource(fullUrl, getAuthHeaders(),
                                                 requestOptions());
}
