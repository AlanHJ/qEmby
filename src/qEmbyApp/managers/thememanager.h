#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QIcon>
#include <QColor>
#include <atomic> 

class ThemeManager : public QObject
{
    Q_OBJECT
public:
    enum Theme {
        Light,
        Dark
    };
    Q_ENUM(Theme)

    
    static ThemeManager* instance();

    
    
    static QIcon getAdaptiveIcon(const QString& svgPath, QColor customColor = QColor());

    
    void setTheme(Theme theme);

    
    void applyThemeMode(const QString &mode);

    
    static Theme detectSystemTheme();

    
    Theme currentTheme() const;
    bool isDarkMode() const;

    
    
    static qreal fontScaleFactor();

signals:
    void themeChanged(Theme theme);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() = default;

    void applyStyleSheet(Theme theme);

    
    static QString scaleFontSizes(const QString &qss, const QString &sizeKey);

    
    std::atomic<Theme> m_currentTheme;
};

#endif 
