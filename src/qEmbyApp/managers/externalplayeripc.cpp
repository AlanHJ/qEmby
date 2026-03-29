#include "externalplayeripc.h"
#include <QLocalSocket>
#include <QTcpSocket>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>


static constexpr long long TICKS_PER_SECOND = 10000000LL;
static constexpr int POLL_INTERVAL_MS = 5000;      
static constexpr int CONNECT_RETRY_MS = 1000;       
static constexpr int MAX_CONNECT_ATTEMPTS = 15;      




ExternalPlayerIPC* ExternalPlayerIPC::create(PlayerType type, const QString& ipcEndpoint, QObject* parent)
{
    switch (type) {
    case PlayerType::MPV:
    case PlayerType::IINA:
        return new MpvIPC(ipcEndpoint, parent);

    case PlayerType::VLC:

#ifdef Q_OS_WIN
    case PlayerType::MPC_HC:
        return new MpcSlaveIPC(parent);
#endif

    case PlayerType::MPC_BE:

    default:
        return new EstimationIPC(parent);
    }
}




MpvIPC::MpvIPC(const QString& pipePath, QObject* parent)
    : ExternalPlayerIPC(parent)
    , m_pipePath(pipePath)
{
}

MpvIPC::~MpvIPC()
{
    stop();
}

void MpvIPC::start(long long startPositionTicks)
{
    m_lastPositionTicks = startPositionTicks;
    m_connectAttempts = 0;
    m_connected = false;

    m_socket = new QLocalSocket(this);
    connect(m_socket, &QLocalSocket::readyRead, this, &MpvIPC::onReadyRead);
    connect(m_socket, &QLocalSocket::connected, this, [this]() {
        m_connected = true;
        m_connectAttempts = 0;
        qDebug() << "[MpvIPC] Connected to pipe:" << m_pipePath;

        
        if (!m_pollTimer) {
            m_pollTimer = new QTimer(this);
            connect(m_pollTimer, &QTimer::timeout, this, &MpvIPC::sendQuery);
        }
        m_pollTimer->start(POLL_INTERVAL_MS);
        sendQuery(); 
    });
    connect(m_socket, &QLocalSocket::disconnected, this, [this]() {
        m_connected = false;
        if (m_pollTimer) m_pollTimer->stop();
        qDebug() << "[MpvIPC] Disconnected from pipe";
    });

    
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, &MpvIPC::tryConnect);
    m_connectTimer->start(CONNECT_RETRY_MS);
}

void MpvIPC::tryConnect()
{
    if (m_connected || m_connectAttempts >= MAX_CONNECT_ATTEMPTS) {
        if (!m_connected) {
            qDebug() << "[MpvIPC] Max connect attempts reached, giving up";
        }
        return;
    }

    m_connectAttempts++;
    qDebug() << "[MpvIPC] Attempting to connect (" << m_connectAttempts << "/" << MAX_CONNECT_ATTEMPTS << ")";
    m_socket->connectToServer(m_pipePath);

    if (!m_socket->waitForConnected(500)) {
        
        if (m_connectTimer)
            m_connectTimer->start(CONNECT_RETRY_MS);
    }
}

void MpvIPC::stop()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
    if (m_connectTimer) {
        m_connectTimer->stop();
        m_connectTimer->deleteLater();
        m_connectTimer = nullptr;
    }
    if (m_socket) {
        if (m_socket->state() == QLocalSocket::ConnectedState) {
            m_socket->disconnectFromServer();
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_connected = false;
}

long long MpvIPC::currentPositionTicks() const
{
    return m_lastPositionTicks;
}

void MpvIPC::sendQuery()
{
    if (!m_socket || !m_connected) return;

    
    QJsonObject cmd;
    cmd["command"] = QJsonArray{"get_property", "time-pos"};
    QByteArray data = QJsonDocument(cmd).toJson(QJsonDocument::Compact) + "\n";
    m_socket->write(data);
    m_socket->flush();
}

void MpvIPC::onReadyRead()
{
    while (m_socket && m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty()) continue;

        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(line, &err);
        if (err.error != QJsonParseError::NoError) continue;

        QJsonObject obj = doc.object();

        
        if (obj.contains("data") && obj["error"].toString() == "success") {
            double seconds = obj["data"].toDouble(-1.0);
            if (seconds >= 0.0) {
                m_lastPositionTicks = static_cast<long long>(seconds * TICKS_PER_SECOND);
                Q_EMIT positionUpdated(m_lastPositionTicks);
            }
        }
    }
}




VlcRcIPC::VlcRcIPC(int port, QObject* parent)
    : ExternalPlayerIPC(parent)
    , m_port(port)
{
}

VlcRcIPC::~VlcRcIPC()
{
    stop();
}

void VlcRcIPC::start(long long startPositionTicks)
{
    m_lastPositionTicks = startPositionTicks;
    m_connectAttempts = 0;
    m_connected = false;

    m_socket = new QTcpSocket(this);
    connect(m_socket, &QTcpSocket::connected, this, &VlcRcIPC::onConnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &VlcRcIPC::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected, this, [this]() {
        m_connected = false;
        if (m_pollTimer) m_pollTimer->stop();
        qDebug() << "[VlcRcIPC] Disconnected from RC interface";
    });

    
    m_connectTimer = new QTimer(this);
    m_connectTimer->setSingleShot(true);
    connect(m_connectTimer, &QTimer::timeout, this, &VlcRcIPC::tryConnect);
    m_connectTimer->start(2000); 
}

void VlcRcIPC::tryConnect()
{
    if (m_connected || m_connectAttempts >= MAX_CONNECT_ATTEMPTS) {
        if (!m_connected)
            qDebug() << "[VlcRcIPC] Max connect attempts reached";
        return;
    }

    m_connectAttempts++;
    qDebug() << "[VlcRcIPC] Attempting to connect to port" << m_port
             << "(" << m_connectAttempts << "/" << MAX_CONNECT_ATTEMPTS << ")";
    m_socket->connectToHost("127.0.0.1", m_port);

    if (!m_socket->waitForConnected(1000)) {
        if (m_connectTimer)
            m_connectTimer->start(CONNECT_RETRY_MS);
    }
}

void VlcRcIPC::onConnected()
{
    m_connected = true;
    m_connectAttempts = 0;
    qDebug() << "[VlcRcIPC] Connected to VLC RC on port" << m_port;

    if (!m_pollTimer) {
        m_pollTimer = new QTimer(this);
        connect(m_pollTimer, &QTimer::timeout, this, &VlcRcIPC::sendQuery);
    }
    m_pollTimer->start(POLL_INTERVAL_MS);
    
    QTimer::singleShot(500, this, &VlcRcIPC::sendQuery);
}

void VlcRcIPC::stop()
{
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->deleteLater();
        m_pollTimer = nullptr;
    }
    if (m_connectTimer) {
        m_connectTimer->stop();
        m_connectTimer->deleteLater();
        m_connectTimer = nullptr;
    }
    if (m_socket) {
        if (m_socket->state() == QTcpSocket::ConnectedState) {
            m_socket->write("quit\n");
            m_socket->flush();
            m_socket->disconnectFromHost();
        }
        m_socket->deleteLater();
        m_socket = nullptr;
    }
    m_connected = false;
}

long long VlcRcIPC::currentPositionTicks() const
{
    return m_lastPositionTicks;
}

void VlcRcIPC::sendQuery()
{
    if (!m_socket || !m_connected) return;

    
    m_socket->write("get_time\n");
    m_socket->flush();
}

void VlcRcIPC::onReadyRead()
{
    while (m_socket && m_socket->canReadLine()) {
        QByteArray line = m_socket->readLine().trimmed();
        if (line.isEmpty()) continue;

        
        
        bool ok;
        int seconds = line.toInt(&ok);
        if (ok && seconds >= 0) {
            m_lastPositionTicks = static_cast<long long>(seconds) * TICKS_PER_SECOND;
            Q_EMIT positionUpdated(m_lastPositionTicks);
        }
    }
}




#ifdef Q_OS_WIN

#include <Windows.h>



namespace MpcCmd {
    
    constexpr DWORD CMD_CURRENTPOSITION = 0x50000007;  
    constexpr DWORD CMD_NOWPLAYING       = 0x50000003;  
    constexpr DWORD CMD_PLAYMODE         = 0x50000004;  
    constexpr DWORD CMD_NOTIFYSEEK       = 0x50000008;  
    constexpr DWORD CMD_NOTIFYENDSTREAM  = 0x50000009;  

    
    constexpr DWORD CMD_GETCURRENTPOSITION = 0xA0003004;  
}

MpcSlaveWindow::MpcSlaveWindow(QWidget* parent)
    : QWidget(parent)
{
    
    setFixedSize(1, 1);
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_ShowWithoutActivating);
    
    
    winId();
}

bool MpcSlaveWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
    if (eventType == "windows_generic_MSG") {
        MSG* msg = static_cast<MSG*>(message);
        if (msg->message == WM_COPYDATA) {
            COPYDATASTRUCT* cds = reinterpret_cast<COPYDATASTRUCT*>(msg->lParam);
            if (!cds) return false;

            
            QString data = QString::fromWCharArray(
                reinterpret_cast<const wchar_t*>(cds->lpData),
                cds->cbData / sizeof(wchar_t));
            
            if (data.endsWith(QChar('\0')))
                data.chop(1);

            switch (cds->dwData) {
            case MpcCmd::CMD_CURRENTPOSITION:
            case MpcCmd::CMD_NOTIFYSEEK: {
                
                bool ok;
                
                double seconds = data.toDouble(&ok);
                if (!ok) {
                    
                    QStringList parts = data.split(':');
                    if (parts.size() == 3) {
                        seconds = parts[0].toDouble() * 3600 + parts[1].toDouble() * 60 + parts[2].toDouble();
                        ok = true;
                    }
                }
                if (ok && seconds >= 0.0) {
                    long long posMs = static_cast<long long>(seconds * 1000);
                    Q_EMIT positionReceived(posMs);
                }
                if (result) *result = 1;
                return true;
            }
            case MpcCmd::CMD_NOTIFYENDSTREAM:
                Q_EMIT playbackEnded();
                if (result) *result = 1;
                return true;
            default:
                break;
            }
        }
    }
    return QWidget::nativeEvent(eventType, message, result);
}

MpcSlaveIPC::MpcSlaveIPC(QObject* parent)
    : ExternalPlayerIPC(parent)
{
}

MpcSlaveIPC::~MpcSlaveIPC()
{
    stop();
}

void MpcSlaveIPC::start(long long startPositionTicks)
{
    m_lastPositionTicks = startPositionTicks;

    m_window = new MpcSlaveWindow();
    connect(m_window, &MpcSlaveWindow::positionReceived, this, [this](long long posMs) {
        m_lastPositionTicks = posMs * 10000LL; 
        Q_EMIT positionUpdated(m_lastPositionTicks);
    });
    
    
}

void MpcSlaveIPC::stop()
{
    if (m_window) {
        m_window->deleteLater();
        m_window = nullptr;
    }
}

long long MpcSlaveIPC::currentPositionTicks() const
{
    return m_lastPositionTicks;
}

QString MpcSlaveIPC::hwndString() const
{
    if (m_window)
        return QString::number(static_cast<quintptr>(m_window->winId()));
    return {};
}

#endif 




EstimationIPC::EstimationIPC(QObject* parent)
    : ExternalPlayerIPC(parent)
{
}

EstimationIPC::~EstimationIPC()
{
    stop();
}

void EstimationIPC::start(long long startPositionTicks)
{
    m_startTicks = startPositionTicks;
    m_elapsed.start();
    m_running = true;
}

void EstimationIPC::stop()
{
    m_running = false;
}

long long EstimationIPC::currentPositionTicks() const
{
    if (!m_running) return m_startTicks;
    
    long long elapsedMs = m_elapsed.elapsed();
    return m_startTicks + elapsedMs * 10000LL; 
}
