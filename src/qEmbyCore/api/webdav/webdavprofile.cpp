#include "webdavprofile.h"

#include <QUrl>
#include <QUuid>






WebdavProfile WebdavProfile::defaults()
{
    WebdavProfile p;
    p.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    p.name.clear();
    p.baseUrl.clear();
    p.username.clear();
    p.password.clear();
    p.rootDir = QStringLiteral("/qEmby");
    p.ignoreSsl = false;
    p.lastSyncAt = 0;
    return p;
}

bool WebdavProfile::isValid() const
{
    if (baseUrl.trimmed().isEmpty() ||
        username.trimmed().isEmpty() ||
        password.isEmpty())
    {
        return false;
    }

    const QUrl url(baseUrl.trimmed());
    if (!url.isValid())
    {
        return false;
    }

    const QString scheme = url.scheme().toLower();
    if (scheme != QStringLiteral("http") && scheme != QStringLiteral("https"))
    {
        return false;
    }
    if (url.host().trimmed().isEmpty())
    {
        return false;
    }

    return true;
}

void WebdavProfile::normalize()
{
    if (id.trimmed().isEmpty())
    {
        id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }

    name = name.trimmed();
    username = username.trimmed();
    

    
    QString trimmed = baseUrl.trimmed();
    while (trimmed.endsWith('/') && !trimmed.endsWith(QStringLiteral("://")))
    {
        trimmed.chop(1);
    }
    baseUrl = trimmed;

    
    QString root = rootDir.trimmed();
    if (root.isEmpty())
    {
        root = QStringLiteral("/qEmby");
    }
    if (!root.startsWith('/'))
    {
        root.prepend('/');
    }
    while (root.size() > 1 && root.endsWith('/'))
    {
        root.chop(1);
    }
    rootDir = root;
}

QJsonObject WebdavProfile::toJson() const
{
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), id);
    obj.insert(QStringLiteral("name"), name);
    obj.insert(QStringLiteral("baseUrl"), baseUrl);
    obj.insert(QStringLiteral("username"), username);
    obj.insert(QStringLiteral("password"), password);
    obj.insert(QStringLiteral("rootDir"), rootDir);
    obj.insert(QStringLiteral("ignoreSsl"), ignoreSsl);
    obj.insert(QStringLiteral("lastSyncAt"), static_cast<double>(lastSyncAt));
    return obj;
}

WebdavProfile WebdavProfile::fromJson(const QJsonObject& obj)
{
    WebdavProfile p = defaults();
    if (obj.contains(QStringLiteral("id")))
    {
        const QString stored = obj.value(QStringLiteral("id")).toString().trimmed();
        if (!stored.isEmpty())
        {
            p.id = stored;
        }
    }
    p.name = obj.value(QStringLiteral("name")).toString();
    p.baseUrl = obj.value(QStringLiteral("baseUrl")).toString();
    p.username = obj.value(QStringLiteral("username")).toString();
    p.password = obj.value(QStringLiteral("password")).toString();
    if (obj.contains(QStringLiteral("rootDir")))
    {
        const QString stored = obj.value(QStringLiteral("rootDir")).toString();
        if (!stored.trimmed().isEmpty())
        {
            p.rootDir = stored;
        }
    }
    p.ignoreSsl = obj.value(QStringLiteral("ignoreSsl")).toBool(false);
    p.lastSyncAt = static_cast<qint64>(obj.value(QStringLiteral("lastSyncAt")).toDouble(0));
    p.normalize();
    return p;
}
