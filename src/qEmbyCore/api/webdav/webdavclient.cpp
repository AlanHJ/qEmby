#include "webdavclient.h"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include <QBuffer>
#include <QByteArray>
#include <QDateTime>
#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStringList>
#include <QUrl>
#include <QXmlStreamReader>

#include <qcoronetwork.h>

namespace
{

constexpr int kDepthListing = 1;
constexpr int kDepthSelf = 0;










QString humanReadableHttpError(const QString &verb, int status,
                                QNetworkReply::NetworkError qtErr,
                                const QString &qtErrString,
                                const QByteArray &body = QByteArray())
{
    Q_UNUSED(qtErr);

    QString reason;
    switch (status)
    {
    case 401:
        reason = WebdavClient::tr(
            "Authentication failed. Please check the username and password.");
        break;
    case 403:
        reason = WebdavClient::tr(
            "Access denied. The account does not have permission for this operation.");
        break;
    case 404:
        reason = WebdavClient::tr("Resource not found on the WebDAV server.");
        break;
    case 405:
        reason = WebdavClient::tr(
            "The WebDAV server does not allow this method on this resource.");
        break;
    case 409:
        reason = WebdavClient::tr(
            "Conflict. The parent directory may not exist on the WebDAV server.");
        break;
    case 412:
        reason = WebdavClient::tr("Precondition failed (HTTP 412).");
        break;
    case 423:
        reason = WebdavClient::tr("The resource is locked by another client.");
        break;
    case 500:
    case 502:
    case 503:
    case 504:
        reason = WebdavClient::tr(
            "WebDAV server is temporarily unavailable. Please try again later.");
        break;
    case 507:
        reason = WebdavClient::tr("Insufficient storage on the WebDAV server.");
        break;
    case 0:
        
        reason = qtErrString.isEmpty()
                     ? WebdavClient::tr("Could not reach the WebDAV server.")
                     : WebdavClient::tr("Network error: %1").arg(qtErrString);
        break;
    default:
        if (status >= 200 && status < 300)
        {
            
            reason = QStringLiteral("Unexpected success state");
        }
        else if (!qtErrString.isEmpty())
        {
            reason = qtErrString;
        }
        else
        {
            reason = WebdavClient::tr("Unexpected response from the WebDAV server.");
        }
        break;
    }

    QString full;
    if (status > 0)
    {
        full = WebdavClient::tr("%1 failed (HTTP %2): %3")
                   .arg(verb)
                   .arg(status)
                   .arg(reason);
    }
    else
    {
        full = WebdavClient::tr("%1 failed: %2").arg(verb, reason);
    }

    
    Q_UNUSED(body);
    return full;
}

const QByteArray &propfindBody()
{
    static const QByteArray body = QByteArrayLiteral(
        "<?xml version=\"1.0\" encoding=\"utf-8\"?>"
        "<d:propfind xmlns:d=\"DAV:\">"
        "<d:prop>"
        "<d:displayname/>"
        "<d:resourcetype/>"
        "<d:getcontentlength/>"
        "<d:getcontenttype/>"
        "<d:getlastmodified/>"
        "<d:getetag/>"
        "</d:prop>"
        "</d:propfind>");
    return body;
}

QString stripQuotes(QString s)
{
    s = s.trimmed();
    if (s.size() >= 2 && s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"')))
    {
        return s.mid(1, s.size() - 2);
    }
    return s;
}

QString sanitizeRelPath(QString p)
{
    QString t = p.trimmed();
    while (t.startsWith(QLatin1Char('/')))
        t.remove(0, 1);
    while (t.endsWith(QLatin1Char('/')))
        t.chop(1);
    return t;
}

QString joinPathSegments(const QStringList &parts)
{
    QStringList encoded;
    encoded.reserve(parts.size());
    for (const QString &p : parts)
    {
        if (p.isEmpty())
            continue;
        encoded.append(QString::fromUtf8(QUrl::toPercentEncoding(p)));
    }
    return encoded.join(QLatin1Char('/'));
}

int httpStatusOf(QNetworkReply *reply)
{
    return reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
}



QDateTime parseHttpDate(QString s)
{
    QString w = s.trimmed();
    if (w.endsWith(QStringLiteral("GMT"), Qt::CaseInsensitive))
    {
        w.chop(3);
        w = w.trimmed() + QStringLiteral(" +0000");
    }
    QDateTime dt = QDateTime::fromString(w, Qt::RFC2822Date);
    if (!dt.isValid())
    {
        dt = QDateTime::fromString(s.trimmed(), Qt::ISODate);
    }
    return dt;
}

QString hrefBasename(QString href)
{
    if (href.isEmpty())
        return QString();
    QString path = QUrl(href).path();
    while (path.endsWith(QLatin1Char('/')))
        path.chop(1);
    const qsizetype idx = path.lastIndexOf(QLatin1Char('/'));
    return idx >= 0 ? path.mid(idx + 1) : path;
}

QString normalizePathForCompare(QString p)
{
    while (p.endsWith(QLatin1Char('/')))
        p.chop(1);
    return p;
}



QList<WebdavEntry> parseMultiStatusXml(const QByteArray &body, const QString &parentRelPath)
{
    QList<WebdavEntry> entries;
    QXmlStreamReader xml(body);

    WebdavEntry cur;
    bool inResponse = false;
    bool inProp = false;

    while (!xml.atEnd() && !xml.hasError())
    {
        xml.readNext();

        if (xml.isStartElement())
        {
            const QString name = xml.name().toString().toLower();
            if (name == QStringLiteral("response"))
            {
                inResponse = true;
                cur = WebdavEntry();
                cur.parentRelPath = parentRelPath;
            }
            else if (inResponse && name == QStringLiteral("href"))
            {
                const QString raw = xml.readElementText().trimmed();
                cur.href = QUrl::fromPercentEncoding(raw.toUtf8());
            }
            else if (inResponse && name == QStringLiteral("prop"))
            {
                inProp = true;
            }
            else if (inProp)
            {
                if (name == QStringLiteral("displayname"))
                {
                    const QString v = xml.readElementText();
                    if (!v.isEmpty())
                        cur.displayName = v;
                }
                else if (name == QStringLiteral("getcontentlength"))
                {
                    bool ok = false;
                    const qint64 v = xml.readElementText().toLongLong(&ok);
                    if (ok)
                        cur.contentLength = v;
                }
                else if (name == QStringLiteral("getcontenttype"))
                {
                    cur.contentType = xml.readElementText();
                }
                else if (name == QStringLiteral("getlastmodified"))
                {
                    const QDateTime dt = parseHttpDate(xml.readElementText());
                    if (dt.isValid())
                        cur.lastModified = dt;
                }
                else if (name == QStringLiteral("getetag"))
                {
                    cur.etag = stripQuotes(xml.readElementText());
                }
                else if (name == QStringLiteral("resourcetype"))
                {
                    int depth = 1;
                    while (!xml.atEnd() && depth > 0)
                    {
                        xml.readNext();
                        if (xml.isStartElement())
                        {
                            ++depth;
                            if (xml.name().toString().toLower() == QStringLiteral("collection"))
                            {
                                cur.isCollection = true;
                            }
                        }
                        else if (xml.isEndElement())
                        {
                            --depth;
                        }
                    }
                }
            }
        }
        else if (xml.isEndElement())
        {
            const QString name = xml.name().toString().toLower();
            if (name == QStringLiteral("prop"))
            {
                inProp = false;
            }
            else if (name == QStringLiteral("response"))
            {
                inResponse = false;
                if (cur.displayName.isEmpty())
                {
                    cur.displayName = hrefBasename(cur.href);
                }
                entries.append(cur);
            }
        }
    }

    if (xml.hasError())
    {
        qWarning() << "[WebdavClient] parseMultiStatus XML error:" << xml.errorString()
                   << "| line:" << xml.lineNumber()
                   << "| col:" << xml.columnNumber();
    }

    return entries;
}

} 





WebdavClient::WebdavClient(WebdavProfile profile, QObject *parent)
    : QObject(parent), m_profile(std::move(profile))
{
    m_profile.normalize();
    m_nam = new QNetworkAccessManager(this);
}

WebdavClient::~WebdavClient() = default;

void WebdavClient::setProfile(WebdavProfile profile)
{
    m_profile = std::move(profile);
    m_profile.normalize();
}





QUrl WebdavClient::absoluteUrl(QString relPath) const
{
    const QStringList rootParts =
        sanitizeRelPath(m_profile.rootDir).split(QLatin1Char('/'), Qt::SkipEmptyParts);
    const QStringList relParts =
        sanitizeRelPath(relPath).split(QLatin1Char('/'), Qt::SkipEmptyParts);

    QStringList allParts;
    allParts.reserve(rootParts.size() + relParts.size());
    allParts.append(rootParts);
    allParts.append(relParts);

    return absoluteUrlForSegments(allParts);
}

QUrl WebdavClient::absoluteUrlForSegments(const QStringList &segments) const
{
    QUrl base(m_profile.baseUrl);
    if (!base.isValid())
    {
        qWarning() << "[WebdavClient] absoluteUrl: invalid baseUrl"
                   << "| baseUrl:" << m_profile.baseUrl;
        return base;
    }

    QString basePath = base.path();
    while (basePath.endsWith(QLatin1Char('/')))
        basePath.chop(1);

    const QString encoded = joinPathSegments(segments);
    QString fullPath = basePath;
    if (!encoded.isEmpty())
    {
        if (!fullPath.endsWith(QLatin1Char('/')))
            fullPath += QLatin1Char('/');
        fullPath += encoded;
    }
    base.setPath(fullPath, QUrl::StrictMode);
    return base;
}

NetworkRequestOptions WebdavClient::networkOptions() const
{
    NetworkRequestOptions options;
    options.ignoreSslErrors = m_profile.ignoreSsl;
    return options;
}

void WebdavClient::applyAuthHeader(QNetworkRequest &request) const
{
    const QByteArray cred =
        (m_profile.username + QLatin1Char(':') + m_profile.password).toUtf8();
    request.setRawHeader(QByteArrayLiteral("Authorization"),
                         QByteArrayLiteral("Basic ") + cred.toBase64());
    request.setRawHeader(QByteArrayLiteral("User-Agent"),
                         QByteArrayLiteral("qEmby-Webdav/1.0"));
    request.setRawHeader(QByteArrayLiteral("Accept"),
                         QByteArrayLiteral("application/xml, text/xml, */*"));
}





QCoro::Task<bool> WebdavClient::testConnection()
{
    const QUrl url = absoluteUrl(QString());
    qDebug() << "[WebdavClient] testConnection START"
             << "| url:" << url.toString()
             << "| user:" << m_profile.username
             << "| ignoreSsl:" << m_profile.ignoreSsl;

    QNetworkRequest req(url);
    applyAuthHeader(req);
    req.setRawHeader(QByteArrayLiteral("Depth"), QByteArray::number(kDepthSelf));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/xml; charset=utf-8"));
    NetworkManager::applyRequestOptions(req, networkOptions());

    auto *buffer = new QBuffer();
    buffer->setData(propfindBody());
    buffer->open(QIODevice::ReadOnly);

    QNetworkReply *reply = m_nam->sendCustomRequest(req, "PROPFIND", buffer);
    buffer->setParent(reply);
    NetworkManager::attachReplyHandlers(reply, networkOptions(),
                                        QStringLiteral("WEBDAV_PROPFIND_SELF"));
    co_await reply;

    const int status = httpStatusOf(reply);
    const auto qtErr = reply->error();
    const QString errString = reply->errorString();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    const bool ok = (qtErr == QNetworkReply::NoError) && (status == 207 || status == 200);

    qDebug() << "[WebdavClient] testConnection DONE"
             << "| url:" << url.toString()
             << "| status:" << status
             << "| ok:" << ok
             << "| qtError:" << qtErr
             << "| errString:" << errString
             << "| bodyHead:" << body.left(200);

    if (!ok && qtErr != QNetworkReply::NoError)
    {
        throw std::runtime_error(
            humanReadableHttpError(QStringLiteral("PROPFIND"), status, qtErr, errString)
                .toUtf8()
                .toStdString());
    }
    co_return ok;
}

QCoro::Task<bool> WebdavClient::ensureRootDir()
{
    qDebug() << "[WebdavClient] ensureRootDir START"
             << "| rootDir:" << m_profile.rootDir;

    const QStringList rootParts =
        sanitizeRelPath(m_profile.rootDir).split(QLatin1Char('/'), Qt::SkipEmptyParts);

    if (rootParts.isEmpty())
    {
        const bool ok = co_await ensureCollection(QStringList());
        co_return ok;
    }

    QStringList currentParts;
    currentParts.reserve(rootParts.size());
    for (const QString &part : rootParts)
    {
        currentParts.append(part);
        co_await ensureCollection(currentParts);
    }

    qDebug() << "[WebdavClient] ensureRootDir DONE"
             << "| rootDir:" << m_profile.rootDir;
    co_return true;
}

QCoro::Task<bool> WebdavClient::ensureCollection(QStringList segments)
{
    const QUrl url = absoluteUrlForSegments(segments);
    const QString segmentPath = segments.join(QLatin1Char('/'));
    const QString pathLabel = segmentPath.isEmpty()
                                  ? QStringLiteral("/")
                                  : QStringLiteral("/") + segmentPath;

    qDebug() << "[WebdavClient] ensureCollection probe START"
             << "| path:" << pathLabel
             << "| url:" << url.toString();

    QNetworkRequest req(url);
    applyAuthHeader(req);
    req.setRawHeader(QByteArrayLiteral("Depth"), QByteArray::number(kDepthSelf));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/xml; charset=utf-8"));
    NetworkManager::applyRequestOptions(req, networkOptions());

    auto *buffer = new QBuffer();
    buffer->setData(propfindBody());
    buffer->open(QIODevice::ReadOnly);

    QNetworkReply *reply = m_nam->sendCustomRequest(req, "PROPFIND", buffer);
    buffer->setParent(reply);
    NetworkManager::attachReplyHandlers(reply, networkOptions(),
                                        QStringLiteral("WEBDAV_PROPFIND_PROBE"));
    co_await reply;

    const int probeStatus = httpStatusOf(reply);
    const auto probeErr = reply->error();
    const QString probeErrString = reply->errorString();
    const QByteArray probeBody = reply->readAll();
    reply->deleteLater();

    qDebug() << "[WebdavClient] ensureCollection probe DONE"
             << "| path:" << pathLabel
             << "| status:" << probeStatus
             << "| qtErr:" << probeErr
             << "| bodyHead:" << probeBody.left(200);

    if (probeErr == QNetworkReply::NoError && (probeStatus == 207 || probeStatus == 200))
    {
        qDebug() << "[WebdavClient] ensureCollection exists"
                 << "| path:" << pathLabel;
        co_return true;
    }

    
    if (probeStatus == 404 || probeStatus == 409 || probeStatus == 405)
    {
        qDebug() << "[WebdavClient] ensureCollection MKCOL START"
                 << "| path:" << pathLabel
                 << "| url:" << url.toString();

        QNetworkRequest mkcolReq(url);
        applyAuthHeader(mkcolReq);
        NetworkManager::applyRequestOptions(mkcolReq, networkOptions());

        QNetworkReply *mkcolReply = m_nam->sendCustomRequest(mkcolReq, "MKCOL");
        NetworkManager::attachReplyHandlers(mkcolReply, networkOptions(),
                                            QStringLiteral("WEBDAV_MKCOL_ROOT_SEGMENT"));
        co_await mkcolReply;

        const int mkcolStatus = httpStatusOf(mkcolReply);
        const auto mkcolErr = mkcolReply->error();
        const QString mkcolErrString = mkcolReply->errorString();
        const QByteArray mkcolBody = mkcolReply->readAll();
        mkcolReply->deleteLater();

        const bool ok = mkcolStatus == 405 ||
                        ((mkcolErr == QNetworkReply::NoError) &&
                         (mkcolStatus == 200 || mkcolStatus == 201 ||
                          mkcolStatus == 204));

        qDebug() << "[WebdavClient] ensureCollection MKCOL DONE"
                 << "| path:" << pathLabel
                 << "| status:" << mkcolStatus
                 << "| ok:" << ok
                 << "| bodyHead:" << mkcolBody.left(200);

        if (!ok)
        {
            throw std::runtime_error(
                humanReadableHttpError(QStringLiteral("MKCOL"), mkcolStatus, mkcolErr,
                                       mkcolErrString, mkcolBody)
                    .toUtf8()
                    .toStdString());
        }
        co_return true;
    }

    throw std::runtime_error(
        humanReadableHttpError(QStringLiteral("PROPFIND"), probeStatus, probeErr, probeErrString)
            .toUtf8()
            .toStdString());
}

QCoro::Task<QList<WebdavEntry>> WebdavClient::list(QString relPath)
{
    const QString cleanRel = sanitizeRelPath(relPath);
    const QUrl url = absoluteUrl(cleanRel);

    qDebug() << "[WebdavClient] list START"
             << "| url:" << url.toString()
             << "| relPath:" << cleanRel;

    QNetworkRequest req(url);
    applyAuthHeader(req);
    req.setRawHeader(QByteArrayLiteral("Depth"), QByteArray::number(kDepthListing));
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/xml; charset=utf-8"));
    NetworkManager::applyRequestOptions(req, networkOptions());

    auto *buffer = new QBuffer();
    buffer->setData(propfindBody());
    buffer->open(QIODevice::ReadOnly);

    QNetworkReply *reply = m_nam->sendCustomRequest(req, "PROPFIND", buffer);
    buffer->setParent(reply);
    NetworkManager::attachReplyHandlers(reply, networkOptions(),
                                        QStringLiteral("WEBDAV_PROPFIND_LIST"));
    co_await reply;

    const int status = httpStatusOf(reply);
    const auto qtErr = reply->error();
    const QString errString = reply->errorString();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    qDebug() << "[WebdavClient] list response"
             << "| url:" << url.toString()
             << "| status:" << status
             << "| qtError:" << qtErr
             << "| bodyLen:" << body.size();

    if (qtErr != QNetworkReply::NoError || status < 200 || status >= 300)
    {
        throw std::runtime_error(
            humanReadableHttpError(QStringLiteral("PROPFIND"), status, qtErr, errString, body)
                .toUtf8()
                .toStdString());
    }

    QList<WebdavEntry> result = parseMultiStatusXml(body, cleanRel);

    
    const QString parentPathNorm = normalizePathForCompare(url.path(QUrl::FullyDecoded));
    auto isSelf = [&parentPathNorm](const WebdavEntry &e)
    {
        QUrl entryUrl(e.href);
        const QString p = entryUrl.isValid() ? entryUrl.path(QUrl::FullyDecoded) : e.href;
        return normalizePathForCompare(p) == parentPathNorm;
    };
    result.erase(std::remove_if(result.begin(), result.end(), isSelf), result.end());

    qDebug() << "[WebdavClient] list DONE"
             << "| entries:" << result.size();
    co_return result;
}

QCoro::Task<QByteArray> WebdavClient::getFile(QString relPath)
{
    const QString cleanRel = sanitizeRelPath(relPath);
    if (cleanRel.isEmpty())
    {
        throw std::runtime_error(
            tr("Cannot download: relative path is empty.").toUtf8().toStdString());
    }
    const QUrl url = absoluteUrl(cleanRel);

    qDebug() << "[WebdavClient] getFile START | url:" << url.toString();

    QNetworkRequest req(url);
    applyAuthHeader(req);
    NetworkManager::applyRequestOptions(req, networkOptions());

    QNetworkReply *reply = m_nam->get(req);
    NetworkManager::attachReplyHandlers(reply, networkOptions(),
                                        QStringLiteral("WEBDAV_GET"));
    co_await reply;

    const int status = httpStatusOf(reply);
    const auto qtErr = reply->error();
    const QString errString = reply->errorString();
    const QByteArray body = reply->readAll();
    reply->deleteLater();

    qDebug() << "[WebdavClient] getFile DONE"
             << "| url:" << url.toString()
             << "| status:" << status
             << "| size:" << body.size();

    if (qtErr != QNetworkReply::NoError || status < 200 || status >= 300)
    {
        throw std::runtime_error(
            humanReadableHttpError(QStringLiteral("GET"), status, qtErr, errString)
                .toUtf8()
                .toStdString());
    }
    co_return body;
}

QCoro::Task<bool> WebdavClient::putFile(QString relPath, QByteArray bytes, QString contentType)
{
    const QString cleanRel = sanitizeRelPath(relPath);
    if (cleanRel.isEmpty())
    {
        throw std::runtime_error(
            tr("Cannot upload: relative path is empty.").toUtf8().toStdString());
    }
    const QUrl url = absoluteUrl(cleanRel);

    qDebug() << "[WebdavClient] putFile START"
             << "| url:" << url.toString()
             << "| size:" << bytes.size()
             << "| contentType:" << contentType;

    bool retried = false;

    for (;;)
    {
        QNetworkRequest req(url);
        applyAuthHeader(req);
        if (!contentType.trimmed().isEmpty())
        {
            req.setHeader(QNetworkRequest::ContentTypeHeader, contentType.trimmed());
        }
        NetworkManager::applyRequestOptions(req, networkOptions());

        QNetworkReply *reply = m_nam->put(req, bytes);
        NetworkManager::attachReplyHandlers(reply, networkOptions(),
                                            QStringLiteral("WEBDAV_PUT"));
        co_await reply;

        const int status = httpStatusOf(reply);
        const auto qtErr = reply->error();
        const QString errString = reply->errorString();
        const QByteArray respBody = reply->readAll();
        reply->deleteLater();

        const bool ok = (qtErr == QNetworkReply::NoError) &&
                        (status == 200 || status == 201 || status == 204);

        if (ok)
        {
            qDebug() << "[WebdavClient] putFile DONE"
                     << "| url:" << url.toString()
                     << "| status:" << status
                     << "| retried:" << retried;
            co_return true;
        }

        
        if (!retried && (status == 404 || status == 409))
        {
            retried = true;
            qDebug() << "[WebdavClient] putFile received" << status
                     << ", forcing MKCOL on parent and retrying...";

            const int lastSlash = cleanRel.lastIndexOf(QLatin1Char('/'));
            if (lastSlash >= 0)
            {
                co_await mkcol(cleanRel.left(lastSlash));
            }
            else
            {
                co_await mkcol(QString());
            }
            continue;
        }

        throw std::runtime_error(
            humanReadableHttpError(QStringLiteral("PUT"), status, qtErr, errString, respBody)
                .toUtf8()
                .toStdString());
    }
}

QCoro::Task<bool> WebdavClient::remove(QString relPath)
{
    const QString cleanRel = sanitizeRelPath(relPath);
    if (cleanRel.isEmpty())
    {
        throw std::runtime_error(
            tr("Refused to delete the WebDAV root directory itself.")
                .toUtf8()
                .toStdString());
    }
    const QUrl url = absoluteUrl(cleanRel);

    qDebug() << "[WebdavClient] remove START | url:" << url.toString();

    QNetworkRequest req(url);
    applyAuthHeader(req);
    NetworkManager::applyRequestOptions(req, networkOptions());

    QNetworkReply *reply = m_nam->deleteResource(req);
    NetworkManager::attachReplyHandlers(reply, networkOptions(),
                                        QStringLiteral("WEBDAV_DELETE"));
    co_await reply;

    const int status = httpStatusOf(reply);
    const auto qtErr = reply->error();
    const QString errString = reply->errorString();
    reply->deleteLater();

    
    const bool ok = (qtErr == QNetworkReply::NoError) &&
                    (status == 200 || status == 204 || status == 404);

    qDebug() << "[WebdavClient] remove DONE"
             << "| status:" << status
             << "| ok:" << ok;

    if (!ok)
    {
        throw std::runtime_error(
            humanReadableHttpError(QStringLiteral("DELETE"), status, qtErr, errString)
                .toUtf8()
                .toStdString());
    }
    co_return true;
}

QCoro::Task<bool> WebdavClient::mkcol(QString relPath)
{
    const QString cleanRel = sanitizeRelPath(relPath);
    const QUrl url = absoluteUrl(cleanRel);

    qDebug() << "[WebdavClient] mkcol START | url:" << url.toString();

    QNetworkRequest req(url);
    applyAuthHeader(req);
    NetworkManager::applyRequestOptions(req, networkOptions());

    QNetworkReply *reply = m_nam->sendCustomRequest(req, "MKCOL");
    NetworkManager::attachReplyHandlers(reply, networkOptions(),
                                        QStringLiteral("WEBDAV_MKCOL"));
    co_await reply;

    const int status = httpStatusOf(reply);
    const auto qtErr = reply->error();
    const QString errString = reply->errorString();
    reply->deleteLater();

    
    
    const bool ok = status == 405 ||
                    ((qtErr == QNetworkReply::NoError) &&
                     (status == 200 || status == 201));

    qDebug() << "[WebdavClient] mkcol DONE"
             << "| status:" << status
             << "| ok:" << ok;

    if (!ok)
    {
        throw std::runtime_error(
            humanReadableHttpError(QStringLiteral("MKCOL"), status, qtErr, errString)
                .toUtf8()
                .toStdString());
    }
    co_return true;
}
