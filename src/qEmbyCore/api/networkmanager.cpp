#include "networkmanager.h"
#include <QJsonArray>
#include <QJsonParseError>
#include <QJsonValue>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringList>
#include <QUrl>
#include <qcoronetwork.h>
#include <limits>
#include <stdexcept>

namespace {

QString extractCharsetName(const QString &contentType)
{
    static const QRegularExpression charsetRe(
        QStringLiteral("charset\\s*=\\s*\"?([^;\"\\s]+)\"?"),
        QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = charsetRe.match(contentType);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

bool isUtf8Charset(const QString &charset)
{
    const QString normalized = charset.trimmed().toLower();
    return normalized.isEmpty() || normalized == "utf-8" || normalized == "utf8";
}

int countReplacementCharacters(const QJsonValue &value)
{
    switch (value.type()) {
    case QJsonValue::String:
        return value.toString().count(QChar::ReplacementCharacter);
    case QJsonValue::Array: {
        int total = 0;
        const QJsonArray array = value.toArray();
        for (const QJsonValue &entry : array) {
            total += countReplacementCharacters(entry);
        }
        return total;
    }
    case QJsonValue::Object: {
        int total = 0;
        const QJsonObject object = value.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            total += countReplacementCharacters(it.value());
        }
        return total;
    }
    default:
        return 0;
    }
}

int countReplacementCharacters(const QJsonDocument &document)
{
    if (document.isArray()) {
        return countReplacementCharacters(QJsonValue(document.array()));
    }
    return countReplacementCharacters(QJsonValue(document.object()));
}

bool tryParseJsonWithEncoding(const QByteArray &responseData,
                              const QString &encodingName,
                              QJsonDocument &jsonDoc,
                              QJsonParseError &parseError)
{
    const auto encoding =
        QStringConverter::encodingForName(encodingName);
    if (!encoding.has_value()) {
        return false;
    }

    QStringDecoder decoder(*encoding);
    const QString decodedText = decoder(responseData);
    const QByteArray utf8Data = decodedText.toUtf8();
    jsonDoc = QJsonDocument::fromJson(utf8Data, &parseError);
    return parseError.error == QJsonParseError::NoError;
}

} 

NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
}

NetworkManager::~NetworkManager() {}

void NetworkManager::applyHeaders(QNetworkRequest& request, const QMap<QString, QString>& headers) {
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    
    QMapIterator<QString, QString> i(headers);
    while (i.hasNext()) {
        i.next();
        request.setRawHeader(i.key().toUtf8(), i.value().toUtf8());
    }
}

void NetworkManager::applyRequestOptions(QNetworkRequest& request,
                                         const NetworkRequestOptions& options) {
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    if (options.ignoreSslErrors &&
        request.url().scheme().compare(QStringLiteral("https"),
                                       Qt::CaseInsensitive) == 0) {
        qWarning() << "[NetworkManager] SSL certificate verification is DISABLED"
                   << "| url:" << request.url().toString();
    }
}

void NetworkManager::attachReplyHandlers(QNetworkReply* reply,
                                         const NetworkRequestOptions& options,
                                         const QString& requestKind) {
    reply->setProperty("ignoreSslErrors", options.ignoreSslErrors);

    connect(reply, &QNetworkReply::sslErrors, this,
            [reply, options, requestKind](const QList<QSslError>& errors) {
                QStringList errorMessages;
                for (const QSslError& error : errors) {
                    const QString errorText = error.errorString().trimmed();
                    if (!errorText.isEmpty() &&
                        !errorMessages.contains(errorText)) {
                        errorMessages.append(errorText);
                    }
                }

                const QString summary = errorMessages.join("; ");
                reply->setProperty("sslErrorsSummary", summary);

                qWarning() << "[NetworkManager]" << requestKind
                           << "TLS validation errors"
                           << "| url:" << reply->url().toString()
                           << "| ignoreSslErrors:" << options.ignoreSslErrors
                           << "| errors:" << summary;

                if (options.ignoreSslErrors) {
                    qWarning() << "[NetworkManager]" << requestKind
                               << "Ignoring TLS validation errors for trusted "
                                  "server"
                               << "| url:" << reply->url().toString();
                    reply->ignoreSslErrors();
                    reply->setProperty("sslErrorsIgnored", true);
                }
            });
}

QString NetworkManager::buildReplyErrorMessage(QNetworkReply* reply,
                                               int httpStatus) const {
    QString errorMsg =
        QString(tr("请求失败(HTTP %1): %2")).arg(httpStatus).arg(reply->errorString());

    const QString sslSummary = reply->property("sslErrorsSummary").toString();
    if (!sslSummary.isEmpty() &&
        !reply->property("sslErrorsIgnored").toBool()) {
        errorMsg +=
            QStringLiteral("\n") + tr("TLS/SSL 证书错误: %1").arg(sslSummary);
        errorMsg += QStringLiteral("\n") +
                    tr("如果这是可信任的自签名 HTTPS 服务器，请启用“忽略 SSL "
                       "证书验证”。");
    }

    return errorMsg;
}

QJsonObject NetworkManager::parseReply(QNetworkReply* reply) {
    
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray responseBody = reply->readAll();
        qWarning() << "[NetworkManager] Request FAILED"
                   << "| url:" << reply->url().toString()
                   << "| httpStatus:" << httpStatus
                   << "| qtError:" << reply->error()
                   << "| errorString:" << reply->errorString()
                   << "| ignoreSslErrors:"
                   << reply->property("ignoreSslErrors").toBool()
                   << "| sslErrors:"
                   << reply->property("sslErrorsSummary").toString()
                   << "| responseBody:" << responseBody.left(500);
        
        QString errorMsg = buildReplyErrorMessage(reply, httpStatus);
        throw std::runtime_error(errorMsg.toStdString());
    }

    QByteArray responseData = reply->readAll();
    const QString contentType =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const QString charsetName = extractCharsetName(contentType);


    
    if (responseData.isEmpty()) {
        return QJsonObject();
    }

    QJsonParseError parseError;
    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData, &parseError);
    bool hasValidJson = parseError.error == QJsonParseError::NoError;
    QString parseSource = QStringLiteral("raw-utf8");
    int replacementCount =
        hasValidJson ? countReplacementCharacters(jsonDoc)
                     : std::numeric_limits<int>::max();

    
    
    
    
    
    
    auto tryAdoptDecodedJson = [&](const QString &encodingName,
                                   const QString &sourceLabel) {
        if (encodingName.isEmpty()) {
            return;
        }

        QJsonParseError decodedParseError;
        QJsonDocument decodedDoc;
        if (!tryParseJsonWithEncoding(responseData, encodingName, decodedDoc,
                                      decodedParseError)) {
            return;
        }

        const int decodedReplacementCount =
            countReplacementCharacters(decodedDoc);
        if (!hasValidJson || decodedReplacementCount < replacementCount) {
            jsonDoc = decodedDoc;
            parseError = decodedParseError;
            hasValidJson = true;
            replacementCount = decodedReplacementCount;
            parseSource = sourceLabel;
        }
    };

    
    if (!hasValidJson) {
        if (!isUtf8Charset(charsetName)) {
            tryAdoptDecodedJson(charsetName,
                                QStringLiteral("response-charset:%1").arg(charsetName));
        }
        tryAdoptDecodedJson(QStringLiteral("GB18030"),
                            QStringLiteral("fallback:GB18030"));
        tryAdoptDecodedJson(QStringLiteral("GBK"),
                            QStringLiteral("fallback:GBK"));
    }

    if (!hasValidJson) {
        qWarning() << "[NetworkManager] JSON parse failed"
                   << "| url:" << reply->url().toString()
                   << "| contentType:" << contentType
                   << "| charset:" << charsetName
                   << "| parseError:" << parseError.errorString()
                   << "| responseBody:" << responseData.left(500);
        QString errorMsg = tr("JSON 解析失败: ") + parseError.errorString();
        throw std::runtime_error(errorMsg.toStdString());
    }

    if (!charsetName.isEmpty() || parseSource != QStringLiteral("raw-utf8") ||
        replacementCount > 0) {
        qDebug() << "[NetworkManager] JSON decoded"
                 << "| url:" << reply->url().toString()
                 << "| contentType:" << contentType
                 << "| charset:" << charsetName
                 << "| source:" << parseSource
                 << "| replacementChars:" << replacementCount;
    }

    
    
    if (jsonDoc.isArray()) {
        QJsonObject wrapper;
        wrapper["data"] = jsonDoc.array();
        return wrapper;
    }

    return jsonDoc.object();
}

QCoro::Task<QJsonObject> NetworkManager::get(
    const QString& url, const QMap<QString, QString>& headers,
    const NetworkRequestOptions& options)
{
    QNetworkRequest request((QUrl(url)));
    applyHeaders(request, headers);
    applyRequestOptions(request, options);

    QNetworkReply* reply = m_networkManager->get(request);
    attachReplyHandlers(reply, options, QStringLiteral("GET"));
    co_await reply;

    co_return parseReply(reply);
}


QCoro::Task<QString> NetworkManager::getText(
    const QString& url, const QMap<QString, QString>& headers,
    const NetworkRequestOptions& options)
{
    QNetworkRequest request((QUrl(url)));
    applyHeaders(request, headers);
    applyRequestOptions(request, options);
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());

    QNetworkReply* reply = m_networkManager->get(request);
    attachReplyHandlers(reply, options, QStringLiteral("GET_TEXT"));
    co_await reply;

    co_return parseReplyAsText(reply);
}

QString NetworkManager::parseReplyAsText(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qWarning() << "[NetworkManager] getText FAILED"
                   << "| url:" << reply->url().toString()
                   << "| httpStatus:" << httpStatus
                   << "| errorString:" << reply->errorString()
                   << "| ignoreSslErrors:"
                   << reply->property("ignoreSslErrors").toBool()
                   << "| sslErrors:"
                   << reply->property("sslErrorsSummary").toString();
        QString errorMsg = buildReplyErrorMessage(reply, httpStatus);
        throw std::runtime_error(errorMsg.toStdString());
    }

    QByteArray responseData = reply->readAll();
    const QString contentType =
        reply->header(QNetworkRequest::ContentTypeHeader).toString();
    const QString charsetName = extractCharsetName(contentType);

    
    if (!charsetName.isEmpty() && !isUtf8Charset(charsetName)) {
        auto encoding = QStringConverter::encodingForName(charsetName);
        if (encoding.has_value()) {
            QStringDecoder decoder(*encoding);
            return decoder(responseData);
        }
    }

    return QString::fromUtf8(responseData);
}

QCoro::Task<QByteArray> NetworkManager::getBytes(
    const QString& url, const QMap<QString, QString>& headers,
    const NetworkRequestOptions& options)
{
    QNetworkRequest request((QUrl(url)));
    applyHeaders(request, headers);
    applyRequestOptions(request, options);
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());

    QNetworkReply* reply = m_networkManager->get(request);
    attachReplyHandlers(reply, options, QStringLiteral("GET_BYTES"));
    co_await reply;

    co_return parseReplyAsBytes(reply, QStringLiteral("getBytes"));
}

QByteArray NetworkManager::parseReplyAsBytes(QNetworkReply* reply,
                                             const QString& requestKind) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        qWarning() << "[NetworkManager]" << requestKind << "FAILED"
                   << "| url:" << reply->url().toString()
                   << "| httpStatus:" << httpStatus
                   << "| errorString:" << reply->errorString()
                   << "| ignoreSslErrors:"
                   << reply->property("ignoreSslErrors").toBool()
                   << "| sslErrors:"
                   << reply->property("sslErrorsSummary").toString();
        QString errorMsg = buildReplyErrorMessage(reply, httpStatus);
        throw std::runtime_error(errorMsg.toStdString());
    }

    return reply->readAll();
}

QCoro::Task<QJsonObject> NetworkManager::post(
    const QString& url, const QMap<QString, QString>& headers,
    const QJsonObject& payload, const NetworkRequestOptions& options)
{
    QNetworkRequest request((QUrl(url)));
    applyHeaders(request, headers);
    applyRequestOptions(request, options);

    QByteArray data;
    
    
    
    if (!payload.isEmpty()) {
        data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    }

    QNetworkReply* reply = m_networkManager->post(request, data);
    attachReplyHandlers(reply, options, QStringLiteral("POST"));
    co_await reply;

    co_return parseReply(reply);
}

QCoro::Task<QJsonObject> NetworkManager::postArray(
    const QString& url, const QMap<QString, QString>& headers,
    const QJsonArray& payload, const NetworkRequestOptions& options)
{
    QNetworkRequest request((QUrl(url)));
    applyHeaders(request, headers);
    applyRequestOptions(request, options);

    QByteArray data;
    if (!payload.isEmpty()) {
        data = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    }

    QNetworkReply* reply = m_networkManager->post(request, data);
    attachReplyHandlers(reply, options, QStringLiteral("POST_ARRAY"));
    co_await reply;

    co_return parseReply(reply);
}

QCoro::Task<QJsonObject> NetworkManager::postForm(
    const QString& url, const QMap<QString, QString>& headers,
    const QUrlQuery& formData, const NetworkRequestOptions& options)
{
    QNetworkRequest request((QUrl(url)));
    applyHeaders(request, headers);
    applyRequestOptions(request, options);
    request.setHeader(QNetworkRequest::ContentTypeHeader,
                      "application/x-www-form-urlencoded");

    const QByteArray data = formData.toString(QUrl::FullyEncoded).toUtf8();
    QNetworkReply* reply = m_networkManager->post(request, data);
    attachReplyHandlers(reply, options, QStringLiteral("POST_FORM"));
    co_await reply;

    co_return parseReply(reply);
}

QCoro::Task<QJsonObject> NetworkManager::deleteResource(
    const QString& url, const QMap<QString, QString>& headers,
    const NetworkRequestOptions& options)
{
    QNetworkRequest request((QUrl(url)));
    applyHeaders(request, headers);
    applyRequestOptions(request, options);

    
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, QVariant());

    qDebug() << "[NetworkManager] DELETE encodedUrl:" << request.url().toEncoded();

    QNetworkReply* reply = m_networkManager->deleteResource(request);
    attachReplyHandlers(reply, options, QStringLiteral("DELETE"));
    co_await reply;

    co_return parseReply(reply);
}
