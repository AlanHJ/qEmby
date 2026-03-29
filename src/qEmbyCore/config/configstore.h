#ifndef CONFIGSTORE_H
#define CONFIGSTORE_H

#include <QObject>
#include <QVariant>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QSettings>
#include <QMutex>
#include <QMutexLocker>


#include "../qEmbyCore_global.h"

class QEMBYCORE_EXPORT ConfigStore : public QObject {
    Q_OBJECT
public:
    
    static ConfigStore* instance();

    
    template<typename T>
    T get(const QString& key, const T& defaultValue = T()) const {
        QMutexLocker locker(&m_mutex);

        
        if (m_cache.contains(key)) {
            return m_cache.value(key).value<T>();
        }

        
        QVariant val = m_settings->value(key, QVariant::fromValue(defaultValue));
        return val.value<T>();
    }

    
    void set(const QString& key, const QVariant& value);
    QStringList allKeys() const;

signals:
    
    void valueChanged(const QString& key, const QVariant& newValue);

private:
    explicit ConfigStore(QObject* parent = nullptr);
    ~ConfigStore() override;

    
    Q_DISABLE_COPY(ConfigStore)

    QSettings* m_settings;
    mutable QMutex m_mutex;             
    QHash<QString, QVariant> m_cache;   
};

#endif 
