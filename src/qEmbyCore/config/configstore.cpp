#include "configstore.h"
#include "config_keys.h"
#include <QDir>
#include <QStandardPaths>
#include <QCoreApplication>

ConfigStore* ConfigStore::instance() {
    
    static ConfigStore* s_instance = new ConfigStore(qApp);
    return s_instance;
}

ConfigStore::ConfigStore(QObject* parent) : QObject(parent) {
    
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QDir().mkpath(configPath);
    QString configFile = configPath + "/config.ini";

    
    m_settings = new QSettings(configFile, QSettings::IniFormat, this);

    
    if (!m_settings->contains(ConfigKeys::ShowLatestAdded)) {
        m_settings->setValue(ConfigKeys::ShowLatestAdded, true);
    }
    if (!m_settings->contains(ConfigKeys::ShowMediaLibraries)) {
        m_settings->setValue(ConfigKeys::ShowMediaLibraries, true);
    }
    if (!m_settings->contains(ConfigKeys::ShowEachLibrary)) {
        m_settings->setValue(ConfigKeys::ShowEachLibrary, true);
    }
    if (!m_settings->contains(ConfigKeys::ImageQuality)) {
        m_settings->setValue(ConfigKeys::ImageQuality, "high");
    }
    if (!m_settings->contains(ConfigKeys::DataCacheDuration)) {
        m_settings->setValue(ConfigKeys::DataCacheDuration, "24");
    }
    if (!m_settings->contains(ConfigKeys::ImageCacheDuration)) {
        m_settings->setValue(ConfigKeys::ImageCacheDuration, "7");
    }
}

ConfigStore::~ConfigStore() {
    if (m_settings) {
        m_settings->sync(); 
    }
}

void ConfigStore::set(const QString& key, const QVariant& value) {
    m_mutex.lock();

    
    QVariant oldValue;
    if (m_cache.contains(key)) {
        oldValue = m_cache.value(key);
    } else {
        oldValue = m_settings->value(key);
    }

    
    if (oldValue == value && oldValue.isValid()) {
        m_mutex.unlock();
        return;
    }

    
    m_cache.insert(key, value);
    
    m_settings->setValue(key, value);

    
    m_mutex.unlock();

    
    emit valueChanged(key, value);
}

QStringList ConfigStore::allKeys() const {
    QMutexLocker locker(&m_mutex);

    QStringList keys = m_settings->allKeys();
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it) {
        if (!keys.contains(it.key())) {
            keys.append(it.key());
        }
    }
    return keys;
}
