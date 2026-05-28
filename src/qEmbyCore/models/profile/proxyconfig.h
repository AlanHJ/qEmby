#ifndef PROXYCONFIG_H
#define PROXYCONFIG_H

#include "../../qEmbyCore_global.h"
#include <QJsonObject>
#include <QNetworkProxy>
#include <QString>










struct QEMBYCORE_EXPORT ProxyConfig {
    enum Mode {
        None = 0,    
        System = 1,  
        Custom = 2,  
    };

    enum Type {
        Http = 0,    
        Socks5 = 1,  
    };

    Mode    mode = None;
    Type    type = Http;
    QString host;
    quint16 port = 0;
    QString username;
    QString password;
    bool    bypassLocalhost = true;  

    
    
    
    
    
    QNetworkProxy toQNetworkProxy() const;

    
    QJsonObject toJson() const;
    static ProxyConfig fromJson(const QJsonObject& obj);

    
    bool isExplicit() const { return mode != None; }

    
    QString summary() const;

    bool operator==(const ProxyConfig& other) const;
    bool operator!=(const ProxyConfig& other) const { return !(*this == other); }
};

#endif  
