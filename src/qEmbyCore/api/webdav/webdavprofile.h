#ifndef WEBDAVPROFILE_H
#define WEBDAVPROFILE_H

#include "../../qEmbyCore_global.h"

#include <QJsonObject>
#include <QString>














struct QEMBYCORE_EXPORT WebdavProfile
{
    QString id;            
    QString name;          
    QString baseUrl;       
    QString username;
    QString password;      
    QString rootDir;       
    bool    ignoreSsl = false;  
    qint64  lastSyncAt = 0;     

    
    bool isValid() const;

    
    
    void normalize();

    QJsonObject toJson() const;
    static WebdavProfile fromJson(const QJsonObject& obj);

    
    static WebdavProfile defaults();
};

#endif 
