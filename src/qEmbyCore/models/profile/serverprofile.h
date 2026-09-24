#ifndef SERVERPROFILE_H
#define SERVERPROFILE_H

#include "proxyconfig.h"
#include "useragentconfig.h"
#include <QString>
#include <QUuid>

struct ServerProfile {
    enum ServerType { Emby, Jellyfin };

    QString id = QUuid::createUuid().toString(); 
    QString name;            
    QString url;             
    ServerType type = Emby;  
    bool ignoreSslVerification = false; 

    QString userId;
    QString userName;
    QString accessToken;
    QString deviceId;        
    bool isAdmin = false;    
    bool canDownloadMedia = false;

    QString iconBase64;

    
    
    
    
    bool useGlobalProxy = false;
    ProxyConfig proxy;

    
    bool useGlobalUserAgent = false;
    UserAgentConfig userAgent;

    bool isValid() const { return !accessToken.isEmpty(); }
};
#endif
