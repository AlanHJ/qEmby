#include "mpvhttpstreamrelay.h"

#include "../utils/logredactionutils.h"

#include <QAbstractSocket>
#include <QDebug>
#include <QHostAddress>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
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

QUrl MpvHttpStreamRelay::prepare(const QUrl &targetUrl, const QString &serverId, const QNetworkProxy &proxy)
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
    m_serverId = serverId;
    m_streamToken = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_network->setProxy(proxy);
    m_bytesRelayedSinceLastTick = 0;
    m_speedTimer->start();

    QUrl localUrl;
    localUrl.setScheme(QStringLiteral("http"));
    localUrl.setHost(QStringLiteral("127.0.0.1"));
    localUrl.setPort(m_server->serverPort());
    localUrl.setPath(QStringLiteral("/%1/stream").arg(m_streamToken));

    qInfo() << "[MpvHttpStreamRelay] prepared"
            << "| target:" << LogRedactionUtils::url(m_targetUrl) << "| local:" << localUrl.toString(QUrl::FullyEncoded)
            << "| serverId:" << (m_serverId.isEmpty() ? QStringLiteral("<none>") : m_serverId)
            << "| proxyType:" << proxy.type();

    return localUrl;
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
    if (it == m_connections.end() || it->reply)
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
    if (!(method == "GET" || method == "HEAD") || !path.startsWith(expectedPrefix))
    {
        writeError(socket, 404, "Not found");
        return;
    }

    QNetworkRequest request(m_targetUrl);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

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
        if (lowerName == "host" || isHopByHopHeader(lowerName))
        {
            continue;
        }
        if (lowerName == "range" || lowerName == "user-agent" || lowerName == "accept" || lowerName == "icy-metadata")
        {
            request.setRawHeader(name, value);
        }
    }

    auto it = m_connections.find(socket);
    if (it == m_connections.end())
    {
        return;
    }
    it->headOnly = method == "HEAD";
    it->reply = it->headOnly ? m_network->head(request) : m_network->get(request);
    it->reply->setReadBufferSize(kReplyReadBufferBytes);

    qDebug() << "[MpvHttpStreamRelay] request"
             << "| method:" << method << "| target:" << LogRedactionUtils::url(m_targetUrl)
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

                if (reply->error() != QNetworkReply::NoError &&
                    reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 0)
                {
                    qWarning() << "[MpvHttpStreamRelay] upstream failed"
                               << "| target:" << LogRedactionUtils::url(m_targetUrl)
                               << "| error:" << reply->errorString();
                }

                pumpReplyToSocket(socket);
            });
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
        response += name + ": " + pair.second + "\r\n";
    }
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
