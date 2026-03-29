#ifndef LANGUAGEMANAGER_H
#define LANGUAGEMANAGER_H

#include <QObject>
#include <QTranslator>
#include <QString>

class LanguageManager : public QObject {
    Q_OBJECT
public:
    static LanguageManager* instance();

    void init();
    QString currentLanguage() const;

signals:
    void languageChanged(const QString& langCode);

private:
    explicit LanguageManager(QObject *parent = nullptr);
    ~LanguageManager() override;

    Q_DISABLE_COPY(LanguageManager)

    void applyLanguage(const QString& langCode);
    void removeCurrentTranslator();

    QTranslator m_translator;
    QString m_currentLang;
    bool m_initialized = false;
    bool m_translatorInstalled = false;
};

#endif 
