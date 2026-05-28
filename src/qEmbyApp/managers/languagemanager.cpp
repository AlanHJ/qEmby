#include "languagemanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QLocale>
#include <config/config_keys.h>
#include <config/configstore.h>

LanguageManager* LanguageManager::instance() {
    static LanguageManager s_instance;
    return &s_instance;
}

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent), m_currentLang("") {
}

LanguageManager::~LanguageManager() = default;

QString LanguageManager::currentLanguage() const {
    return m_currentLang;
}

void LanguageManager::init() {
    if (m_initialized) {
        return;
    }
    m_initialized = true;

    if (auto *app = QCoreApplication::instance()) {
        connect(app, &QCoreApplication::aboutToQuit, this, &LanguageManager::removeCurrentTranslator, Qt::DirectConnection);
    }

    
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this, [this](const QString& key, const QVariant& value) {
        if (key == ConfigKeys::Language) {
            applyLanguage(value.toString());
        }
    });

    
    QString savedLang = ConfigStore::instance()->get<QString>(ConfigKeys::Language, "system");
    qDebug() << "LanguageManager: saved language" << savedLang
             << "| config:" << ConfigStore::instance()->filePath();
    applyLanguage(savedLang);
}

void LanguageManager::removeCurrentTranslator() {
    if (!m_translatorInstalled) {
        return;
    }

    auto *app = QCoreApplication::instance();
    if (!app) {
        m_translatorInstalled = false;
        return;
    }

    app->removeTranslator(&m_translator);
    m_translatorInstalled = false;
}

void LanguageManager::applyLanguage(const QString& langCode) {
    if (m_currentLang == langCode && !m_translator.isEmpty()) {
        return;
    }

    auto *app = QCoreApplication::instance();
    if (!app) {
        qWarning() << "LanguageManager: QCoreApplication is not available, skipping language change.";
        return;
    }

    m_currentLang = langCode;

    
    removeCurrentTranslator();

    const auto loadTranslation = [this, app](const QString &baseName) {
        const QString resourcePath = QStringLiteral(":/i18n/%1.qm").arg(baseName);
        const bool resourceExists = QFile::exists(resourcePath);
        if (resourceExists && m_translator.load(resourcePath)) {
            app->installTranslator(&m_translator);
            m_translatorInstalled = true;
            qDebug() << "LanguageManager: Loaded translation"
                     << resourcePath
                     << "| language:" << m_translator.language()
                     << "| filePath:" << m_translator.filePath();
            return true;
        }

        qWarning() << "LanguageManager: Failed to load translation"
                   << resourcePath << "| resourceExists:" << resourceExists;
        return false;
    };

    if (langCode == "system" || langCode.isEmpty()) {
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        bool loaded = false;
        for (const QString &locale : uiLanguages) {
            const QString baseName = "qEmby_" + QLocale(locale).name();
            if (loadTranslation(baseName)) {
                qDebug() << "LanguageManager: Loaded system translation" << baseName;
                loaded = true;
                break;
            }
        }
        if (!loaded) {
            qWarning() << "LanguageManager: Failed to load system translation, falling back to internal default.";
        }
    } else {
        const QString baseName = "qEmby_" + langCode;
        if (!loadTranslation(baseName)) {
            qWarning() << "LanguageManager: Falling back to internal default for" << baseName;
        }
    }

    Q_EMIT languageChanged(langCode);
}
