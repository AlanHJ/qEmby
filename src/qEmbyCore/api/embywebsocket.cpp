#include "embywebsocket.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QUrl>
#include <QDebug>

EmbyWebSocket::EmbyWebSocket(const ServerProfile& profile, QObject* parent)
    : QObject(parent)
    , m_profile(profile)
    , m_socket(new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this))
    , m_keepAliveTimer(new QTimer(this))
    , m_reconnectTimer(new QTimer(this))
{
    
    m_keepAliveTimer->setInterval(KeepAliveIntervalMs);
    connect(m_keepAliveTimer, &QTimer::timeout, this, &EmbyWebSocket::sendKeepAlive);

    
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &EmbyWebSocket::attemptReconnect);

    
    connect(m_socket, &QWebSocket::connected, this, &EmbyWebSocket::onConnected);
    connect(m_socket, &QWebSocket::disconnected, this, &EmbyWebSocket::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived,
            this, &EmbyWebSocket::onTextMessageReceived);
    connect(m_socket, &QWebSocket::errorOccurred, this, &EmbyWebSocket::onError);
    connect(m_socket, &QWebSocket::sslErrors, this, &EmbyWebSocket::onSslErrors);
}

EmbyWebSocket::~EmbyWebSocket()
{
    m_intentionalDisconnect = true;
    m_keepAliveTimer->stop();
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->close();
    }
}





void EmbyWebSocket::connectToServer()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        return; 
    }

    m_intentionalDisconnect = false;
    m_reconnectAttempts = 0;
    QString url = buildWebSocketUrl();
    qDebug() << "[EmbyWebSocket] Connecting to:" << url;
    if (m_profile.ignoreSslVerification &&
        url.startsWith("wss://", Qt::CaseInsensitive)) {
        qWarning() << "[EmbyWebSocket] SSL certificate verification is DISABLED"
                   << "| url:" << url;
    }
    m_socket->open(QUrl(url));
}

void EmbyWebSocket::disconnectFromServer()
{
    m_intentionalDisconnect = true;
    m_keepAliveTimer->stop();
    m_reconnectTimer->stop();
    m_reconnectAttempts = 0;

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->close();
    }
}

bool EmbyWebSocket::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}





void EmbyWebSocket::onConnected()
{
    qDebug() << "[EmbyWebSocket] Connected successfully";
    m_reconnectAttempts = 0;
    m_keepAliveTimer->start();
    Q_EMIT connected();
}

void EmbyWebSocket::onDisconnected()
{
    qDebug() << "[EmbyWebSocket] Disconnected";
    m_keepAliveTimer->stop();
    Q_EMIT disconnected();

    
    if (!m_intentionalDisconnect && m_reconnectAttempts < MaxReconnectAttempts) {
        
        int delayMs = qMin(5000 * (1 << m_reconnectAttempts), 30000);
        qDebug() << "[EmbyWebSocket] Reconnect in" << delayMs << "ms (attempt"
                 << m_reconnectAttempts + 1 << ")";
        m_reconnectTimer->start(delayMs);
    }
}

void EmbyWebSocket::onError(QAbstractSocket::SocketError error)
{
    Q_UNUSED(error)
    QString errMsg = m_socket->errorString();
    qDebug() << "[EmbyWebSocket] Error:" << errMsg;
    Q_EMIT connectionError(errMsg);
}

void EmbyWebSocket::onSslErrors(const QList<QSslError>& errors)
{
    QStringList errorMessages;
    for (const QSslError& error : errors) {
        const QString errorText = error.errorString().trimmed();
        if (!errorText.isEmpty() && !errorMessages.contains(errorText)) {
            errorMessages.append(errorText);
        }
    }

    const QString summary = errorMessages.join("; ");
    qWarning() << "[EmbyWebSocket] TLS validation errors"
               << "| url:" << buildWebSocketUrl()
               << "| ignoreSslVerification:"
               << m_profile.ignoreSslVerification
               << "| errors:" << summary;

    if (m_profile.ignoreSslVerification) {
        qWarning() << "[EmbyWebSocket] Ignoring TLS validation errors for "
                      "trusted server";
        m_socket->ignoreSslErrors();
    }
}

void EmbyWebSocket::onTextMessageReceived(const QString& message)
{
    QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8());
    if (!doc.isObject()) return;

    QJsonObject obj = doc.object();
    QString messageType = obj["MessageType"].toString();
    QJsonValue data = obj["Data"];

    
    QString dataPreview;
    if (data.isArray()) {
        QJsonDocument dataDoc(data.toArray());
        dataPreview = QString::fromUtf8(dataDoc.toJson(QJsonDocument::Compact));
    } else if (data.isObject()) {
        QJsonDocument dataDoc(data.toObject());
        dataPreview = QString::fromUtf8(dataDoc.toJson(QJsonDocument::Compact));
    } else {
        dataPreview = data.toVariant().toString();
    }
    if (dataPreview.length() > 300) {
        dataPreview = dataPreview.left(300) + "...";
    }
    qDebug() << "[WS RAW] <<" << messageType << "| Data:" << dataPreview;

    
    processMessage(messageType, data);

    
    Q_EMIT messageReceived(messageType, data);
}





void EmbyWebSocket::sendKeepAlive()
{
    if (!isConnected()) return;

    QJsonObject msg;
    msg["MessageType"] = QStringLiteral("KeepAlive");
    m_socket->sendTextMessage(QJsonDocument(msg).toJson(QJsonDocument::Compact));
}

void EmbyWebSocket::attemptReconnect()
{
    if (m_intentionalDisconnect) return;

    m_reconnectAttempts++;
    qDebug() << "[EmbyWebSocket] Reconnect attempt" << m_reconnectAttempts;
    QString url = buildWebSocketUrl();
    m_socket->open(QUrl(url));
}





QString EmbyWebSocket::buildWebSocketUrl() const
{
    
    QString baseUrl = m_profile.url;
    if (baseUrl.startsWith("https://", Qt::CaseInsensitive)) {
        baseUrl.replace(0, 5, "wss");
    } else if (baseUrl.startsWith("http://", Qt::CaseInsensitive)) {
        baseUrl.replace(0, 4, "ws");
    }

    
    while (baseUrl.endsWith('/')) {
        baseUrl.chop(1);
    }

    
    QString path;
    if (m_profile.type == ServerProfile::Emby) {
        path = "/embywebsocket";
    } else {
        path = "/socket";
    }

    return QString("%1%2?api_key=%3&deviceId=%4")
        .arg(baseUrl, path, m_profile.accessToken, m_profile.deviceId);
}

void EmbyWebSocket::processMessage(const QString& messageType, const QJsonValue& data)
{
    if (messageType == "RefreshProgress") {
        
        QJsonObject obj = data.toObject();
        QString itemId = obj["ItemId"].toString();
        
        QJsonValue progressVal = obj["Progress"];
        int progress;
        if (progressVal.isString()) {
            progress = static_cast<int>(progressVal.toString().toDouble());
        } else {
            progress = static_cast<int>(progressVal.toDouble());
        }
        qDebug() << "[EmbyWebSocket] RefreshProgress itemId=" << itemId << "progress=" << progress;
        Q_EMIT refreshProgress(itemId, progress);
    }
    else if (messageType == "ScheduledTasksInfo" || messageType == "ScheduledTasksInfoChanged") {
        
        QJsonArray tasks = data.toArray();
        Q_EMIT scheduledTasksInfoChanged(tasks);

        
        for (const auto& val : tasks) {
            QJsonObject taskObj = val.toObject();
            QString category = taskObj["Category"].toString();
            QString state = taskObj["State"].toString();
            if (category == "Library" && state == "Running") {
                
                
                if (taskObj.contains("CurrentProgressPercentage")
                    && !taskObj["CurrentProgressPercentage"].isNull()) {
                    double progressPct = taskObj["CurrentProgressPercentage"].toDouble();
                    int progress = qBound(0, static_cast<int>(progressPct), 100);
                    qDebug() << "[EmbyWebSocket] ScheduledTask Library running, progress=" << progress;
                    Q_EMIT refreshProgress(QString(), progress);
                } else {
                    qDebug() << "[EmbyWebSocket] ScheduledTask Library running, no progress yet";
                }
            }
        }
    }
    else if (messageType == "ScheduledTaskEnded") {
        
        QJsonObject taskObj = data.toObject();
        QString category;
        
        if (taskObj.contains("Category")) {
            category = taskObj["Category"].toString();
        } else if (taskObj.contains("Key")) {
            category = taskObj["Key"].toString();
        }
        qDebug() << "[EmbyWebSocket] ScheduledTaskEnded category=" << category;
        Q_EMIT libraryChanged();
    }
    else if (messageType == "LibraryChanged") {
        qDebug() << "[EmbyWebSocket] LibraryChanged";
        Q_EMIT libraryChanged();
    }
    else if (messageType == "Sessions") {
        QJsonArray sessions = data.toArray();
        Q_EMIT sessionsUpdated(sessions);
    }
    else if (messageType == "UserDataChanged") {
        QJsonObject obj = data.toObject();
        Q_EMIT userDataChanged(obj);
    }
    
}
