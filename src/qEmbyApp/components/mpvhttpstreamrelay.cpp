#include "mpvhttpstreamrelay.h"

#include "../utils/logredactionutils.h"
#include "../utils/hlsplaylistutils.h"

#include <QAbstractSocket>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include "api/useragentmanager.h"
#include "api/networkmanager.h"
#include <QPointer>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUuid>

namespace
{

constexpr qint64 kReplyReadBufferBytes = 4 * 1024 * 1024;
constexpr qint64 kSocketQueuedBytesHighWater = 4 * 1024 * 1024;
constexpr qint64 kRelayPumpChunkBytes = 256 * 1024;

bool canWriteToSocket(QTcpSocket *socket)
{
    return socket && socket->isOpen() && socket->isWritable() && socket->state() != QAbstractSocket::UnconnectedState;
}

} 

MpvHttpStreamRelay::MpvHttpStreamRelay(QObject *parent)
    : QObject(parent), m_server(new QTcpServer(this)), m_network(new QNetworkAccessManager(this)),
      m_speedTimer(new QTimer(this))
{
    m_cache = new HttpRangeCache(m_network, this);
    connect(m_cache, &HttpRangeCache::upstreamBytes, this, &MpvHttpStreamRelay::recordRelayedBytes);
    connect(m_cache, &HttpRangeCache::blockChanged, this, [this](const QString &key) {
        const auto sockets = m_connections.keys();
        for (auto *socket : sockets) {
            const auto it = m_connections.constFind(socket);
            if (it != m_connections.cend() && it->cached && it->blockKey == key) pumpCachedToSocket(socket);
        }
    });
    connect(m_cache, &HttpRangeCache::blockFailed, this,
            [this](const QString &key, bool fallback, const QString &reason) {
        if (fallback && reason != QLatin1String("request-rejected")) { disableSharedCache(reason); return; }
        const auto sockets = m_connections.keys();
        for (auto *socket : sockets) {
            const auto it = m_connections.constFind(socket);
            if (it == m_connections.cend() || !it->cached || it->blockKey != key) continue;
            if (it->headersSent) closeConnection(socket);
            else if (fallback) startPassthrough(socket);
            else writeError(socket, 502, "Unable to read media range");
        }
    });
    connect(m_server, &QTcpServer::newConnection, this, [this]() { onNewConnection(); });
    m_speedTimer->setInterval(1000);
    connect(m_speedTimer, &QTimer::timeout, this,
            [this]()
            {
                const qint64 speed = m_bytesRelayedSinceLastTick;
                m_bytesRelayedSinceLastTick = 0;
                Q_EMIT upstreamSpeedChanged(speed);
            });
}

MpvHttpStreamRelay::~MpvHttpStreamRelay()
{
    stop();
}

QUrl MpvHttpStreamRelay::prepare(const QUrl &targetUrl, const QString &serverId, const QNetworkProxy &proxy,
                                const QMap<QString, QString>& headers, const QUrl& trustedTlsOrigin, bool ignoreSslErrors)
{
    if (!targetUrl.isValid() || targetUrl.scheme().isEmpty())
    {
        return {};
    }

    stop();
    if (!m_server->isListening() && !m_server->listen(QHostAddress::LocalHost, 0))
    {
        qWarning() << "[MpvHttpStreamRelay] failed to listen"
                   << "| error:" << m_server->errorString();
        return {};
    }

    m_targetUrl = targetUrl;
    m_headers = headers;
    m_trustedTlsOrigin = trustedTlsOrigin;
    m_ignoreSslErrors = ignoreSslErrors;
    m_serverId = serverId;
    m_streamToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_sharedCacheEnabled = !HttpRangeUtils::isPlaylist(targetUrl) &&
        !(ignoreSslErrors && HlsPlaylistUtils::sameOrigin(targetUrl, trustedTlsOrigin));
    m_network->setProxy(proxy);
    m_bytesRelayedSinceLastTick = 0;
    m_speedTimer->start();

    QUrl localUrl;
    localUrl.setScheme(QStringLiteral("http"));
    localUrl.setHost(QStringLiteral("127.0.0.1"));
    localUrl.setPort(m_server->serverPort());
    localUrl.setPath(QStringLiteral("/%1/stream").arg(m_streamToken));

    qInfo() << "[MpvHttpStreamRelay] prepared"
            << "| target:" << LogRedactionUtils::url(m_targetUrl) << "| sharedCache:" << m_sharedCacheEnabled
            << "| serverId:" << (m_serverId.isEmpty() ? QStringLiteral("<none>") : m_serverId)
            << "| proxyType:" << proxy.type();

    return localUrl;
}

QUrl MpvHttpStreamRelay::previewUrl() const
{
    if (!m_server->isListening() || m_streamToken.isEmpty()) return {};
    QUrl url;
    url.setScheme(QStringLiteral("http"));
    url.setHost(QStringLiteral("127.0.0.1"));
    url.setPort(m_server->serverPort());
    url.setPath(QStringLiteral("/%1/preview").arg(m_streamToken));
    return url;
}

QUrl MpvHttpStreamRelay::mapResource(const QUrl& remote)
{
    if (remote.scheme() == QLatin1String("data")) return remote;
    if (remote.scheme() != QLatin1String("http") && remote.scheme() != QLatin1String("https")) return {};
    const auto found = m_resourceUrls.constFind(remote);
    if (found != m_resourceUrls.cend()) return found.value();
    
    
    if (m_resources.size() >= 16384) {
        const auto key = m_resources.constBegin().key();
        m_resourceUrls.remove(m_resources.take(key));
    }
    QUrl local = previewUrl();
    local.setPath(QStringLiteral("/%1/resource/%2").arg(m_streamToken,
        QUuid::createUuid().toString(QUuid::WithoutBraces)));
    m_resources.insert(local.path().toUtf8(), remote);
    m_resourceUrls.insert(remote, local);
    return local;
}

void MpvHttpStreamRelay::stop()
{
    const auto sockets = m_connections.keys();
    for (QTcpSocket *socket : sockets)
    {
        closeConnection(socket);
    }
    if (m_server->isListening())
    {
        m_server->close();
    }
    if (m_speedTimer->isActive())
    {
        m_speedTimer->stop();
        m_bytesRelayedSinceLastTick = 0;
        Q_EMIT upstreamSpeedChanged(0);
    }
    m_connections.clear();
    m_resources.clear();
    m_resourceUrls.clear();
    m_headers.clear();
    m_playlistUrls.clear();
    m_trustedTlsOrigin.clear();
    m_ignoreSslErrors = false;
    m_cache->clear();
    m_targetUrl.clear();
    m_serverId.clear();
    m_streamToken.clear();
}

void MpvHttpStreamRelay::onNewConnection()
{
    while (QTcpSocket *socket = m_server->nextPendingConnection())
    {
        m_connections.insert(socket, ConnectionState{});
        QPointer<QTcpSocket> safeSocket(socket);
        connect(socket, &QTcpSocket::readyRead, this,
                [this, safeSocket]()
                {
                    if (safeSocket)
                    {
                        onSocketReadyRead(safeSocket.data());
                    }
                });
        connect(socket, &QTcpSocket::bytesWritten, this,
                [this, safeSocket](qint64)
                {
                    if (safeSocket)
                    {
                        pumpReplyToSocket(safeSocket.data());
                    }
                });
        connect(socket, &QTcpSocket::disconnected, this,
                [this, safeSocket]()
                {
                    if (safeSocket)
                    {
                        onSocketDisconnected(safeSocket.data());
                    }
                });
    }
}

void MpvHttpStreamRelay::onSocketReadyRead(QTcpSocket *socket)
{
    if (!socket || !socket->isOpen())
    {
        closeConnection(socket);
        return;
    }

    auto it = m_connections.find(socket);
    if (it == m_connections.end() || it->requestStarted)
    {
        return;
    }

    it->buffer += socket->readAll();
    const int headerEnd = it->buffer.indexOf("\r\n\r\n");
    if (headerEnd < 0)
    {
        if (it->buffer.size() > 64 * 1024)
        {
            writeError(socket, 431, "Request header too large");
        }
        return;
    }

    const QByteArray requestData = it->buffer.left(headerEnd + 4);
    it->requestStarted = true;
    it->buffer.clear();
    processRequest(socket, requestData);
}

void MpvHttpStreamRelay::onSocketDisconnected(QTcpSocket *socket)
{
    closeConnection(socket);
}

void MpvHttpStreamRelay::processRequest(QTcpSocket *socket, const QByteArray &requestData)
{
    if (m_targetUrl.isEmpty() || m_streamToken.isEmpty())
    {
        writeError(socket, 503, "Relay target is not ready");
        return;
    }

    const QList<QByteArray> lines = requestData.split('\n');
    if (lines.isEmpty())
    {
        writeError(socket, 400, "Bad request");
        return;
    }

    const QList<QByteArray> requestParts = lines.first().trimmed().split(' ');
    if (requestParts.size() < 2)
    {
        writeError(socket, 400, "Bad request");
        return;
    }

    const QByteArray method = requestParts.at(0).toUpper();
    const QByteArray path = requestParts.at(1);
    const QByteArray expectedPrefix = QByteArray("/") + m_streamToken.toUtf8() + QByteArray("/");
    if (!(method == "GET" || method == "HEAD") ||
        (path != expectedPrefix + "stream" && path != expectedPrefix + "preview" && !m_resources.contains(path)))
    {
        writeError(socket, 404, "Not found");
        return;
    }

    const QUrl target = m_resources.value(path, m_targetUrl);
    QNetworkRequest request(target);
    if (HlsPlaylistUtils::sameOrigin(target, m_targetUrl)) {
        for (auto header = m_headers.cbegin(); header != m_headers.cend(); ++header)
            request.setRawHeader(header.key().toUtf8(), header.value().toUtf8());
    }
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);

    for (int i = 1; i < lines.size(); ++i)
    {
        QByteArray line = lines.at(i).trimmed();
        if (line.isEmpty())
        {
            continue;
        }
        const int colon = line.indexOf(':');
        if (colon <= 0)
        {
            continue;
        }

        QByteArray name = line.left(colon).trimmed();
        const QByteArray value = line.mid(colon + 1).trimmed();
        const QByteArray lowerName = name.toLower();
        if (lowerName == "host" || isHopByHopHeader(lowerName) ||
            lowerName == "content-length" || lowerName == "accept-encoding")
        {
            continue;
        }
        
        
        if (HlsPlaylistUtils::sameOrigin(target, m_targetUrl) || lowerName == "range" ||
            lowerName == "if-range" || lowerName == "accept" || lowerName == "user-agent" ||
            lowerName == "cache-control")
            request.setRawHeader(name, value);
    }

    auto it = m_connections.find(socket);
    if (it == m_connections.end())
    {
        return;
    }
    it->sourceUrl = target;
    it->requestId = ++m_nextRequestId;
    it->playlist = HttpRangeUtils::isPlaylist(target) || m_playlistUrls.contains(target);
    if (it->playlist) {
        request.setRawHeader("Range", QByteArray());
        request.setRawHeader("If-Range", QByteArray());
        request.setRawHeader("If-None-Match", QByteArray());
        request.setRawHeader("If-Modified-Since", QByteArray());
    }
    it->headOnly = method == "HEAD";
    it->preview = path.endsWith("/preview");
    UserAgentManager::instance()->applyToRequest(request, m_serverId);
    request.setRawHeader("Accept-Encoding", "identity");
    request.setPriority(it->preview ? QNetworkRequest::LowPriority : QNetworkRequest::HighPriority);
    it->request = request;
    const auto range = HttpRangeUtils::requestRange(request.rawHeader("Range"));
    const bool conditional = request.hasRawHeader("If-Match") ||
        request.hasRawHeader("If-None-Match") || request.hasRawHeader("If-Modified-Since") ||
        request.hasRawHeader("If-Unmodified-Since");
    if (m_sharedCacheEnabled && !m_resources.contains(path) && !it->headOnly && range && !conditional &&
        !HttpRangeUtils::forbidsCaching(request.rawHeader("Cache-Control"))) {
        it->cached = true;
        it->range = *range;
        it->offset = range->first;
        pumpCachedToSocket(socket);
        return;
    }
    startPassthrough(socket);
}

void MpvHttpStreamRelay::startPassthrough(QTcpSocket *socket)
{
    auto it = m_connections.find(socket);
    if (it == m_connections.end()) return;
    it->cached = false;
    it->block.reset();
    it->blockKey.clear();
    m_cache->release(socket);
    const auto request = it->request;
    it->reply = it->headOnly ? m_network->head(request) : m_network->get(request);
    NetworkRequestOptions options;
    options.ignoreSslErrors = m_ignoreSslErrors && HlsPlaylistUtils::sameOrigin(request.url(), m_trustedTlsOrigin);
    NetworkManager::attachReplyHandlers(it->reply, options, QStringLiteral("STREAM_RELAY"));
    it->reply->setReadBufferSize(kReplyReadBufferBytes);

    qDebug() << "[MpvHttpStreamRelay] request"
             << "| requestId:" << it->requestId << "| target:" << LogRedactionUtils::url(request.url())
             << "| head:" << it->headOnly << "| preview:" << it->preview
             << "| range:" << request.rawHeader("Range");

    QNetworkReply *reply = it->reply;
    QPointer<QTcpSocket> safeSocket(socket);
    QPointer<QNetworkReply> safeReply(reply);
    connect(reply, &QNetworkReply::readyRead, this,
            [this, safeSocket, safeReply]()
            {
                if (!safeSocket || !safeReply)
                {
                    return;
                }
                QTcpSocket *socket = safeSocket.data();
                QNetworkReply *reply = safeReply.data();
                auto it = m_connections.find(socket);
                if (it == m_connections.end() || it->reply != reply)
                {
                    return;
                }
                if (!canWriteToSocket(socket))
                {
                    closeConnection(socket);
                    return;
                }

                pumpReplyToSocket(socket);
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, safeSocket, safeReply]()
            {
                if (!safeSocket || !safeReply)
                {
                    return;
                }
                QTcpSocket *socket = safeSocket.data();
                QNetworkReply *reply = safeReply.data();
                auto it = m_connections.find(socket);
                if (it == m_connections.end() || it->reply != reply)
                {
                    return;
                }

                it->upstreamFinished = true;
                const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QUrl location = reply->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
                if (status >= 300 && status < 400 && status != 304 && !location.isEmpty()) {
                    const QUrl destination = reply->url().resolved(location);
                    if (++it->redirects > 8 || (destination.scheme() != "http" && destination.scheme() != "https") ||
                        (reply->url().scheme() == "https" && destination.scheme() == "http")) {
                        writeError(socket, 502, "Invalid upstream redirect"); return;
                    }
                    auto request = it->request;
                    if (!HlsPlaylistUtils::sameOrigin(request.url(), destination)) {
                        QNetworkRequest sanitized(destination);
                        sanitized.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
                        for (const auto& name : {"Range", "If-Range", "Accept", "User-Agent", "Accept-Encoding"})
                            if (request.hasRawHeader(name)) sanitized.setRawHeader(name, request.rawHeader(name));
                        request = sanitized;
                    }
                    request.setUrl(destination);
                    it->request = request;
                    it->reply = nullptr;
                    it->upstreamFinished = false;
                    disconnect(reply, nullptr, this, nullptr);
                    reply->deleteLater();
                    startPassthrough(socket);
                    return;
                }

                if (reply->error() != QNetworkReply::NoError &&
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 0)
                {
                    qWarning() << "[MpvHttpStreamRelay] upstream failed"
                               << "| requestId:" << it->requestId
                               << "| target:" << LogRedactionUtils::url(reply->url())
                               << "| resource:" << (it->sourceUrl == m_targetUrl ? "main" : "child")
                               << "| error:" << reply->error();
                }

                pumpReplyToSocket(socket);
            });
}

void MpvHttpStreamRelay::disableSharedCache(const QString &reason)
{
    qInfo() << "[MpvHttpStreamRelay] Shared cache disabled | reason:" << reason;
    m_sharedCacheEnabled = false;
    
    m_cache->clear();
    const auto sockets = m_connections.keys();
    for (auto *socket : sockets) {
        const auto it = m_connections.constFind(socket);
        if (it == m_connections.cend() || !it->cached) continue;
        
        
        if (it->headersSent) closeConnection(socket);
        else startPassthrough(socket);
    }
}

void MpvHttpStreamRelay::pumpCachedToSocket(QTcpSocket *socket)
{
    auto it = m_connections.find(socket);
    if (it == m_connections.end() || !it->cached) return;
    if (!canWriteToSocket(socket)) { closeConnection(socket); return; }
    while (socket->bytesToWrite() < kSocketQueuedBytesHighWater) {
        if (it->blockKey.isEmpty()) {
            const qint64 first = it->offset / HttpRangeUtils::BlockBytes * HttpRangeUtils::BlockBytes;
            auto request = it->request;
            if (!it->validator.isEmpty()) request.setRawHeader("If-Range", it->validator);
            it->blockKey = HttpRangeUtils::blockKey(request, first);
            m_cache->acquire(it->blockKey, request, first, socket, it->preview);
        }
        if (!it->block) it->block = m_cache->block(it->blockKey);
        const auto block = it->block;
        if (!block || block->total < 0) return;
        if (!it->headersSent) {
            const auto ifRange = it->request.rawHeader("If-Range");
            if (!ifRange.isEmpty() && ifRange != block->validator) {
                startPassthrough(socket);
                return;
            }
            if (it->range.first >= block->total) {
                socket->write("HTTP/1.1 416 Range Not Satisfiable\r\nContent-Range: bytes */" +
                    QByteArray::number(block->total) + "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                socket->disconnectFromHost();
                return;
            }
            it->total = block->total;
            it->validator = block->validator;
            it->responseEnd = it->range.last < 0 ? block->total - 1 : qMin(it->range.last, block->total - 1);
            QByteArray response = it->range.partial ? "HTTP/1.1 206 Partial Content\r\n" : "HTTP/1.1 200 OK\r\n";
            response += "Accept-Ranges: bytes\r\nContent-Length: " +
                        QByteArray::number(it->responseEnd - it->range.first + 1) + "\r\n";
            if (it->range.partial) response += "Content-Range: bytes " + QByteArray::number(it->range.first) + "-" +
                QByteArray::number(it->responseEnd) + "/" + QByteArray::number(block->total) + "\r\n";
            if (!block->contentType.isEmpty()) response += "Content-Type: " + block->contentType + "\r\n";
            response += (block->validator.startsWith('"') ? "ETag: " : "Last-Modified: ") + block->validator + "\r\n";
            response += "Connection: close\r\n\r\n";
            if (socket->write(response) < 0) { closeConnection(socket); return; }
            it->headersSent = true;
        } else if (block->total != it->total || block->validator != it->validator) {
            disableSharedCache(QStringLiteral("representation-changed"));
            return;
        }
        const qint64 available = block->first + block->data.size() - it->offset;
        const qint64 count = qMin(qMin(available, it->responseEnd - it->offset + 1),
                                 kSocketQueuedBytesHighWater - socket->bytesToWrite());
        if (count > 0) {
            const qint64 written = socket->write(block->data.constData() + (it->offset - block->first), count);
            if (written < 0) { closeConnection(socket); return; }
            it->offset += written;
            if (written == 0) return;
        }
        if (it->offset > it->responseEnd) {
            socket->disconnectFromHost();
            return;
        }
        if (!block->complete || it->offset <= block->last) return;
        it->block.reset();
        it->blockKey.clear();
    }
}

void MpvHttpStreamRelay::sendReplyHeaders(QTcpSocket *socket)
{
    auto it = m_connections.find(socket);
    if (it == m_connections.end() || !it->reply || it->headersSent || !canWriteToSocket(socket))
    {
        return;
    }

    QNetworkReply *reply = it->reply;
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (statusCode == 0)
    {
        statusCode = reply->error() == QNetworkReply::NoError ? 200 : 502;
    }

    qDebug() << "[MpvHttpStreamRelay] response"
             << "| requestId:" << it->requestId
             << "| status:" << statusCode << "| contentRange:" << reply->rawHeader("Content-Range")
             << "| contentLength:" << reply->rawHeader("Content-Length")
             << "| acceptRanges:" << reply->rawHeader("Accept-Ranges");

    QByteArray response = "HTTP/1.1 " + QByteArray::number(statusCode) + " " + reasonPhrase(statusCode) + "\r\n";
    const auto headerPairs = reply->rawHeaderPairs();
    for (const auto &pair : headerPairs)
    {
        QByteArray name = pair.first;
        const QByteArray lowerName = name.toLower();
        if (isHopByHopHeader(lowerName))
        {
            continue;
        }
        if (it->playlist && (lowerName == "content-length" || lowerName == "content-range" ||
            lowerName == "accept-ranges" || lowerName == "etag" || lowerName == "last-modified" || lowerName == "cache-control")) continue;
        response += name + ": " + pair.second + "\r\n";
    }
    if (it->playlist) response += "Cache-Control: no-store\r\n";
    response += "Connection: close\r\n";
    response += "\r\n";

    if (socket->write(response) >= 0)
    {
        it->headersSent = true;
    }
}

void MpvHttpStreamRelay::pumpReplyToSocket(QTcpSocket *socket)
{
    auto it = m_connections.find(socket);
    if (it != m_connections.end() && it->cached) { pumpCachedToSocket(socket); return; }
    if (it == m_connections.end() || !it->reply)
    {
        return;
    }
    if (!canWriteToSocket(socket))
    {
        closeConnection(socket);
        return;
    }

    QNetworkReply *reply = it->reply;
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 304) {
        sendReplyHeaders(socket);
        if (it->upstreamFinished) socket->disconnectFromHost();
        return;
    }
    if (!status || (status >= 300 && status < 400)) {
        if (it->upstreamFinished) writeError(socket, 502, "Invalid upstream response");
        return;
    }
    if (!it->inspected) {
        if (!it->headOnly && reply->bytesAvailable() < 7 && !it->upstreamFinished) return;
        const QByteArray type = reply->header(QNetworkRequest::ContentTypeHeader).toByteArray().toLower();
        it->playlist = it->playlist || reply->peek(64).trimmed().startsWith("#EXTM3U") ||
                       type.contains("mpegurl") || HttpRangeUtils::isPlaylist(reply->url());
        it->inspected = true;
        if (it->playlist) {
            if (!m_playlistUrls.contains(it->sourceUrl))
                qInfo() << "[MpvHttpStreamRelay] HLS manifest detected"
                        << "| requestId:" << it->requestId << "| finalUrl:" << LogRedactionUtils::url(reply->url());
            if (m_playlistUrls.size() >= 4096) m_playlistUrls.clear();
            m_playlistUrls.insert(it->sourceUrl);
            m_playlistUrls.insert(reply->url());
        }
    }
    if (it->playlist && !it->headOnly) {
        if (status == 206 && !it->playlistRetried && it->request.hasRawHeader("Range")) {
            it->playlistRetried = true;
            it->inspected = false;
            it->playlist = false;
            it->request.setRawHeader("Range", QByteArray());
            it->request.setRawHeader("If-Range", QByteArray());
            it->reply = nullptr;
            it->upstreamFinished = false;
            disconnect(reply, nullptr, this, nullptr);
            reply->abort(); reply->deleteLater();
            startPassthrough(socket);
            return;
        }
        it->playlistData += reply->readAll();
        if (it->playlistData.size() > 4 * 1024 * 1024) {
            writeError(socket, 502, "Playlist exceeds relay budget"); return;
        }
        if (!it->upstreamFinished) return;
        if (reply->error() != QNetworkReply::NoError) {
            writeError(socket, 502, "Unable to read playlist"); return;
        }
        const QByteArray body = HlsPlaylistUtils::rewrite(it->playlistData, reply->url(),
            [this](const QUrl& remote) { return mapResource(remote); });
        socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/vnd.apple.mpegurl\r\nCache-Control: no-store\r\nContent-Length: " +
                      QByteArray::number(body.size()) + "\r\nConnection: close\r\n\r\n" + body);
        recordRelayedBytes(body.size());
        socket->disconnectFromHost();
        return;
    }
    sendReplyHeaders(socket);

    if (!it->headOnly && reply->isOpen())
    {
        while (reply->bytesAvailable() > 0 && socket->bytesToWrite() < kSocketQueuedBytesHighWater)
        {
            const qint64 budget = kSocketQueuedBytesHighWater - socket->bytesToWrite();
            const qint64 readSize = qMin(kRelayPumpChunkBytes, qMin(budget, reply->bytesAvailable()));
            if (readSize <= 0)
            {
                break;
            }

            const QByteArray chunk = reply->read(readSize);
            if (chunk.isEmpty())
            {
                break;
            }

            const qint64 written = socket->write(chunk);
            if (written > 0)
            {
                recordRelayedBytes(written);
            }
            if (written < static_cast<qint64>(chunk.size()))
            {
                break;
            }

            it = m_connections.find(socket);
            if (it == m_connections.end() || it->reply != reply || !canWriteToSocket(socket))
            {
                return;
            }
        }
    }

    it = m_connections.find(socket);
    if (it == m_connections.end() || it->reply != reply)
    {
        return;
    }

    const bool allReplyDataDrained = it->headOnly || !reply->isOpen() || reply->bytesAvailable() == 0;
    if (!it->upstreamFinished || !allReplyDataDrained)
    {
        return;
    }

    it->reply = nullptr;
    disconnect(reply, nullptr, this, nullptr);
    reply->deleteLater();

    if (socket->state() != QAbstractSocket::UnconnectedState)
    {
        socket->flush();
        socket->disconnectFromHost();
    }
    else
    {
        closeConnection(socket);
    }
}

void MpvHttpStreamRelay::writeError(QTcpSocket *socket, int statusCode, const QByteArray &message)
{
    const auto state = m_connections.constFind(socket);
    qWarning() << "[MpvHttpStreamRelay] request rejected"
               << "| requestId:" << (state == m_connections.cend() ? 0 : state->requestId)
               << "| status:" << statusCode << "| reason:" << message;
    if (!socket)
    {
        return;
    }

    const QByteArray body = message + "\n";
    QByteArray response = "HTTP/1.1 " + QByteArray::number(statusCode) + " " + reasonPhrase(statusCode) + "\r\n";
    response += "Content-Type: text/plain; charset=utf-8\r\n";
    response += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
    response += "Connection: close\r\n\r\n";
    response += body;
    if (canWriteToSocket(socket))
    {
        socket->write(response);
        socket->disconnectFromHost();
    }
    else
    {
        closeConnection(socket);
    }
}

void MpvHttpStreamRelay::closeConnection(QTcpSocket *socket)
{
    if (!socket)
    {
        return;
    }

    auto it = m_connections.find(socket);
    if (it != m_connections.end())
    {
        QNetworkReply *reply = it->reply;
        it->reply = nullptr;
        m_connections.erase(it);
        m_cache->release(socket);

        if (reply)
        {
            disconnect(reply, nullptr, this, nullptr);
            if (!reply->isFinished())
            {
                reply->abort();
            }
            reply->deleteLater();
        }
    }

    disconnect(socket, nullptr, this, nullptr);
    if (socket->state() != QAbstractSocket::UnconnectedState)
    {
        socket->disconnectFromHost();
    }
    socket->deleteLater();
}

void MpvHttpStreamRelay::recordRelayedBytes(qint64 bytes)
{
    if (bytes > 0)
    {
        m_bytesRelayedSinceLastTick += bytes;
    }
}

QByteArray MpvHttpStreamRelay::reasonPhrase(int statusCode)
{
    switch (statusCode)
    {
    case 200:
        return "OK";
    case 206:
        return "Partial Content";
    case 400:
        return "Bad Request";
    case 404:
        return "Not Found";
    case 431:
        return "Request Header Fields Too Large";
    case 502:
        return "Bad Gateway";
    case 503:
        return "Service Unavailable";
    default:
        return "Status";
    }
}

bool MpvHttpStreamRelay::isHopByHopHeader(QByteArray name)
{
    name = name.toLower();
    return name == "connection" || name == "keep-alive" || name == "proxy-authenticate" ||
           name == "proxy-authorization" || name == "te" || name == "trailer" || name == "transfer-encoding" ||
           name == "upgrade";
}
