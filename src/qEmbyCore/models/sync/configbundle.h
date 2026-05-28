#ifndef CONFIGBUNDLE_H
#define CONFIGBUNDLE_H

#include "../../qEmbyCore_global.h"

#include <QByteArray>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>

















enum class MergeStrategy
{
    Replace = 0,   
    Merge = 1,     
    LocalWins = 2, 
};

struct QEMBYCORE_EXPORT ConfigBundleMetadata
{
    int schemaVersion = 1;
    QString appVersion;
    QString deviceName;
    QString osName;   
    QString osPretty; 
    qint64 exportedAt = 0; 

    QJsonObject toJson() const;
    static ConfigBundleMetadata fromJson(const QJsonObject &obj);

    
    static ConfigBundleMetadata current();
};

class QEMBYCORE_EXPORT ConfigBundle
{
public:
    ConfigBundleMetadata metadata;
    QJsonArray servers;        
    QJsonObject configEntries; 

    
    QJsonObject toJson() const;
    QByteArray serialize(bool indented = false) const; 

    static ConfigBundle fromJson(const QJsonObject &obj);
    static std::optional<ConfigBundle> deserialize(const QByteArray &bytes);

    
    
    
    static ConfigBundle collectFromLocal();

    
    
    static const QStringList &includedNamespaces();

    
    static bool shouldExcludeKey(const QString &key);
};

#endif 
