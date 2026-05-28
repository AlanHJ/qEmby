#ifndef LOGREDACTIONUTILS_H
#define LOGREDACTIONUTILS_H

#include <QString>
#include <QStringList>
#include <QUrl>

class LogRedactionUtils
{
public:
    static QString url(const QUrl &url);
    static QString url(const QString &value);
    static QString proxy(const QString &value);
    static QStringList stringList(const QStringList &values);
};

#endif 
