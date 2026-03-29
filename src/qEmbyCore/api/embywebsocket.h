#ifndef EMBYWEBSOCKET_H
#define EMBYWEBSOCKET_H

#include "../qEmbyCore_global.h"
#include "../models/profile/serverprofile.h"
#include <QObject>
#include <QSslError>
#include <QWebSocket>
#include <QTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>










class QEMBYCORE_EXPORT EmbyWebSocket : public QObject
{
    Q_OBJECT
public:
    explicit EmbyWebSocket(const ServerProfile& profile, QObject* parent = nullptr);
    ~EmbyWebSocket() override;

    
    void connectToServer();

    
    void disconnectFromServer();

    
    bool isConnected() const;

Q_SIGNALS:
    
    void connected();
    void disconnected();
    void connectionError(const QString& error);

    

    
    void refreshProgress(const QString& itemId, int progress);

    
    void libraryChanged();

    
    void scheduledTasksInfoChanged(const QJsonArray& tasks);

    
    void sessionsUpdated(const QJsonArray& sessions);

    
    void userDataChanged(const QJsonObject& data);

    
    void messageReceived(const QString& messageType, const QJsonValue& data);

private Q_SLOTS:
    void onConnected();
    void onDisconnected();
    void onTextMessageReceived(const QString& message);
    void onError(QAbstractSocket::SocketError error);
    void onSslErrors(const QList<QSslError>& errors);
    void sendKeepAlive();
    void attemptReconnect();

private:
    
    QString buildWebSocketUrl() const;

    
    void processMessage(const QString& messageType, const QJsonValue& data);

    ServerProfile m_profile;
    QWebSocket*   m_socket;
    QTimer*       m_keepAliveTimer;     
    QTimer*       m_reconnectTimer;     
    int           m_reconnectAttempts = 0;
    bool          m_intentionalDisconnect = false;

    static constexpr int KeepAliveIntervalMs  = 30000;  
    static constexpr int MaxReconnectAttempts  = 10;
};

#endif 
