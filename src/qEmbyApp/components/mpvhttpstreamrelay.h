#ifndef MPVHTTPSTREAMRELAY_H
#define MPVHTTPSTREAMRELAY_H

#include <QHash>
#include <QMap>
#include <QSet>
#include <QNetworkProxy>
#include <QObject>
#include <QUrl>
#include <QNetworkRequest>
#include "httprangecache.h"
#include "../utils/httprangeutils.h"

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
                 const QNetworkProxy &proxy, const QMap<QString, QString>& headers = {},
                 const QUrl& trustedTlsOrigin = {}, bool ignoreSslErrors = false);
    void stop();
    QUrl previewUrl() const;

Q_SIGNALS:
    void upstreamSpeedChanged(qint64 bytesPerSecond);

private:
    struct ConnectionState {
        QByteArray buffer;
        QNetworkReply *reply = nullptr;
        bool headersSent = false;
        bool headOnly = false;
        bool upstreamFinished = false;
        bool requestStarted = false;
        bool preview = false;
        bool cached = false;
        bool inspected = false;
        bool playlist = false;
        bool playlistRetried = false;
        QByteArray playlistData;
        int redirects = 0;
        quint64 requestId = 0;
        QNetworkRequest request;
        QUrl sourceUrl;
        HttpRangeUtils::Range range;
        qint64 offset = 0;
        qint64 responseEnd = -1;
        qint64 total = -1;
        QByteArray validator;
        QString blockKey;
        std::shared_ptr<const HttpRangeCache::Block> block;
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
    void startPassthrough(QTcpSocket *socket);
    void pumpCachedToSocket(QTcpSocket *socket);
    void disableSharedCache(const QString &reason);
    QUrl mapResource(const QUrl& remote);

    static QByteArray reasonPhrase(int statusCode);
    static bool isHopByHopHeader(QByteArray name);

    QTcpServer *m_server = nullptr;
    QNetworkAccessManager *m_network = nullptr;
    QTimer *m_speedTimer = nullptr;
    HttpRangeCache *m_cache = nullptr;
    QHash<QTcpSocket *, ConnectionState> m_connections;
    QUrl m_targetUrl;
    QString m_serverId;
    QString m_streamToken;
    QHash<QByteArray, QUrl> m_resources;
    QHash<QUrl, QUrl> m_resourceUrls;
    QMap<QString, QString> m_headers;
    QSet<QUrl> m_playlistUrls;
    QUrl m_trustedTlsOrigin;
    bool m_ignoreSslErrors = false;
    quint64 m_nextRequestId = 0;
    qint64 m_bytesRelayedSinceLastTick = 0;
    bool m_sharedCacheEnabled = true;
};

#endif 
