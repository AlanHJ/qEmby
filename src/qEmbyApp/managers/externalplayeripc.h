#ifndef EXTERNALPLAYERIPC_H
#define EXTERNALPLAYERIPC_H

#include <QObject>
#include <QElapsedTimer>

class QLocalSocket;
class QTcpSocket;
class QTimer;




enum class PlayerType {
    MPV,
    IINA,      
    VLC,
    PotPlayer,
    MPC_HC,
    MPC_BE,
    MPC_Qt,
    Unknown
};





class ExternalPlayerIPC : public QObject
{
    Q_OBJECT
public:
    explicit ExternalPlayerIPC(QObject* parent = nullptr) : QObject(parent) {}
    virtual ~ExternalPlayerIPC() = default;

    
    virtual void start(long long startPositionTicks) = 0;

    
    virtual void stop() = 0;

    
    virtual long long currentPositionTicks() const = 0;

    
    virtual bool isPrecise() const = 0;

    
    static ExternalPlayerIPC* create(PlayerType type, const QString& ipcEndpoint, QObject* parent = nullptr);

signals:
    void positionUpdated(long long ticks);
};





class MpvIPC : public ExternalPlayerIPC
{
    Q_OBJECT
public:
    explicit MpvIPC(const QString& pipePath, QObject* parent = nullptr);
    ~MpvIPC() override;

    void start(long long startPositionTicks) override;
    void stop() override;
    long long currentPositionTicks() const override;
    bool isPrecise() const override { return true; }

private slots:
    void onReadyRead();
    void tryConnect();

private:
    QString m_pipePath;
    QLocalSocket* m_socket = nullptr;
    QTimer* m_pollTimer = nullptr;     
    QTimer* m_connectTimer = nullptr;  
    long long m_lastPositionTicks = 0;
    int m_connectAttempts = 0;
    bool m_connected = false;

    void sendQuery();
};





class VlcRcIPC : public ExternalPlayerIPC
{
    Q_OBJECT
public:
    explicit VlcRcIPC(int port, QObject* parent = nullptr);
    ~VlcRcIPC() override;

    void start(long long startPositionTicks) override;
    void stop() override;
    long long currentPositionTicks() const override;
    bool isPrecise() const override { return true; }

private slots:
    void onConnected();
    void onReadyRead();
    void tryConnect();

private:
    int m_port;
    QTcpSocket* m_socket = nullptr;
    QTimer* m_pollTimer = nullptr;
    QTimer* m_connectTimer = nullptr;
    long long m_lastPositionTicks = 0;
    int m_connectAttempts = 0;
    bool m_connected = false;

    void sendQuery();
};





#ifdef Q_OS_WIN

#include <QWidget>

class MpcSlaveWindow : public QWidget
{
    Q_OBJECT
public:
    explicit MpcSlaveWindow(QWidget* parent = nullptr);

signals:
    void positionReceived(long long positionMs);
    void playbackEnded();

protected:
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
};

class MpcSlaveIPC : public ExternalPlayerIPC
{
    Q_OBJECT
public:
    explicit MpcSlaveIPC(QObject* parent = nullptr);
    ~MpcSlaveIPC() override;

    void start(long long startPositionTicks) override;
    void stop() override;
    long long currentPositionTicks() const override;
    bool isPrecise() const override { return true; }

    
    QString hwndString() const;

private:
    MpcSlaveWindow* m_window = nullptr;
    long long m_lastPositionTicks = 0;
};

#endif 




class EstimationIPC : public ExternalPlayerIPC
{
    Q_OBJECT
public:
    explicit EstimationIPC(QObject* parent = nullptr);
    ~EstimationIPC() override;

    void start(long long startPositionTicks) override;
    void stop() override;
    long long currentPositionTicks() const override;
    bool isPrecise() const override { return false; }

private:
    long long m_startTicks = 0;
    QElapsedTimer m_elapsed;
    bool m_running = false;
};

#endif 
