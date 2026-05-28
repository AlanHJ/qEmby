#ifndef WEBDAVENTRY_H
#define WEBDAVENTRY_H

#include "../../qEmbyCore_global.h"

#include <QDateTime>
#include <QString>







struct QEMBYCORE_EXPORT WebdavEntry
{
    
    
    
    QString href;

    
    QString displayName;

    
    
    QString parentRelPath;

    
    
    bool isCollection = false;

    
    qint64 contentLength = -1;

    
    QDateTime lastModified;

    
    QString contentType;

    
    QString etag;
};

#endif 
