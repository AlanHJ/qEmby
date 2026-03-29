#include "languagemanager.h"
#include <QCoreApplication>
#include <QDebug>
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

    if (langCode == "system" || langCode.isEmpty()) {
        const QStringList uiLanguages = QLocale::system().uiLanguages();
        bool loaded = false;
        for (const QString &locale : uiLanguages) {
            const QString baseName = "qEmby_" + QLocale(locale).name();
            if (m_translator.load(":/i18n/" + baseName)) {
                app->installTranslator(&m_translator);
                m_translatorInstalled = true;
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
        if (m_translator.load(":/i18n/" + baseName)) {
            app->installTranslator(&m_translator);
            m_translatorInstalled = true;
            qDebug() << "LanguageManager: Loaded translation" << baseName;
        } else {
            qWarning() << "LanguageManager: Failed to load translation" << baseName << ", falling back to internal default.";
        }
    }

    Q_EMIT languageChanged(langCode);
}
