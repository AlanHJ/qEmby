#ifndef QEMBYCORE_H
#define QEMBYCORE_H

#include "qEmbyCore_global.h"
#include <QObject>


class NetworkManager;
class ServerManager;
class AuthService;
class MediaService; 
class AdminService; 
class DanmakuService;
class IntroDBService;

class QEMBYCORE_EXPORT QEmbyCore : public QObject
{
    Q_OBJECT
public:
    explicit QEmbyCore(QObject *parent = nullptr);
    ~QEmbyCore() override;

    
    ServerManager* serverManager() const;
    AuthService* authService() const;
    MediaService* mediaService() const; 
    AdminService* adminService() const; 
    DanmakuService* danmakuService() const;
    IntroDBService* introDBService() const;

private:
    
    NetworkManager* m_networkManager;
    ServerManager* m_serverManager;
    AuthService* m_authService;
    MediaService* m_mediaService; 
    AdminService* m_adminService; 
    DanmakuService* m_danmakuService;
    IntroDBService* m_introDBService;
};

#endif 
