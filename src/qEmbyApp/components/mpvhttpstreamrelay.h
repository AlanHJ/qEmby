#ifndef MPVHTTPSTREAMRELAY_H
#define MPVHTTPSTREAMRELAY_H

#include <QHash>
#include <QNetworkProxy>
#include <QObject>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;
class QTcpServer;
class QTcpSocket;

class MpvHttpStreamRelay : public QObject {
    Q_OBJECT
public:
    explicit MpvHttpStreamRelay(QObject *parent = nullptr);
    ~MpvHttpStreamRelay() override;

    QUrl prepare(const QUrl &targetUrl, const QString &serverId,
                 const QNetworkProxy &proxy);
    void stop();

Q_SIGNALS:
    void upstreamSpeedChanged(qint64 bytesPerSecond);

private:
    struct ConnectionState {
        QByteArray buffer;
        QNetworkReply *reply = nullptr;
        bool headersSent = false;
        bool headOnly = false;
        bool upstreamFinished = false;
    };

    void onNewConnection();
    void onSocketReadyRead(QTcpSocket *socket);
    void onSocketDisconnected(QTcpSocket *socket);
    void processRequest(QTcpSocket *socket, const QByteArray &requestData);
    void sendReplyHeaders(QTcpSocket *socket);
    void pumpReplyToSocket(QTcpSocket *socket);
    void writeError(QTcpSocket *socket, int statusCode, const QByteArray &message);
    void closeConnection(QTcpSocket *socket);
    void recordRelayedBytes(qint64 bytes);

    static QByteArray reasonPhrase(int statusCode);
    static bool isHopByHopHeader(QByteArray name);

    QTcpServer *m_server = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_speedTimer = nullptr;
    QHash<QTcpSocket *, ConnectionState> m_connections;
    QUrl m_targetUrl;
    QString m_serverId;
    QString m_streamToken;
    qint64 m_bytesRelayedSinceLastTick = 0;
};

#endif 
