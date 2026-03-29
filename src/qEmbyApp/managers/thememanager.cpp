#include "thememanager.h"
#include <QApplication>
#include <QGuiApplication>
#include <QStyleHints>
#include <QFile>
#include <QThread>
#include <QMetaObject>
#include <QDebug>
#include <QPalette>
#include <QPixmap>
#include <QPainter>
#include <QSettings>
#include <QProcess>
#include <QRegularExpression>
#include "config/configstore.h"
#include "config/config_keys.h"

namespace {

QString resolveThemeIconPath(QString svgPath)
{
    if (svgPath.isEmpty()) {
        return svgPath;
    }

    const bool isDarkMode = ThemeManager::instance()->isDarkMode();
    QString candidatePath = svgPath;

    if (isDarkMode && svgPath.contains(QStringLiteral("/light/"))) {
        candidatePath.replace(QStringLiteral("/light/"), QStringLiteral("/dark/"));
    } else if (!isDarkMode && svgPath.contains(QStringLiteral("/dark/"))) {
        candidatePath.replace(QStringLiteral("/dark/"), QStringLiteral("/light/"));
    }

    return QFile::exists(candidatePath) ? candidatePath : svgPath;
}

} 

ThemeManager* ThemeManager::instance()
{
    static ThemeManager s_instance;
    return &s_instance;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent), m_currentTheme(Dark)
{
}

QIcon ThemeManager::getAdaptiveIcon(const QString& svgPath, QColor customColor)
{
    const QString themedSvgPath = resolveThemeIconPath(svgPath);

    
    QColor targetColor = customColor;
    if (!targetColor.isValid()) {
        
        targetColor = ThemeManager::instance()->isDarkMode() ? QColor("#CBD5E1") : QColor("#475569");
    }

    
    
    
    const int renderSize = 128;
    QPixmap pixmap(renderSize, renderSize);
    pixmap.fill(Qt::transparent);

    QIcon originalIcon(themedSvgPath);
    QPixmap sourcePixmap = originalIcon.pixmap(renderSize, renderSize);

    if (sourcePixmap.isNull()) {
        return originalIcon; 
    }

    
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform); 
    
    painter.drawPixmap(0, 0, sourcePixmap);
    painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
    painter.fillRect(pixmap.rect(), targetColor);
    painter.end();

    return QIcon(pixmap);
}

void ThemeManager::setTheme(Theme theme)
{
    
    m_currentTheme.store(theme, std::memory_order_relaxed);

    
    
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(this, [this, theme]() {
            applyStyleSheet(theme);
            Q_EMIT themeChanged(theme);
        }, Qt::QueuedConnection);
    } else {
        
        applyStyleSheet(theme);
        Q_EMIT themeChanged(theme);
    }
}

void ThemeManager::applyThemeMode(const QString &mode)
{
    Theme target;
    if (mode == QLatin1String("light")) {
        target = Light;
    } else if (mode == QLatin1String("dark")) {
        target = Dark;
    } else {
        
        target = detectSystemTheme();
    }
    setTheme(target);
}

ThemeManager::Theme ThemeManager::detectSystemTheme()
{
#if (QT_VERSION >= QT_VERSION_CHECK(6, 5, 0))
    
    auto scheme = QGuiApplication::styleHints()->colorScheme();
    return (scheme == Qt::ColorScheme::Light) ? Light : Dark;
#elif defined(Q_OS_WIN)
    
    
    QSettings reg("HKEY_CURRENT_USER\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                  QSettings::NativeFormat);
    QVariant val = reg.value("AppsUseLightTheme", 0);
    return val.toInt() == 1 ? Light : Dark;
#elif defined(Q_OS_MACOS)
    
    
    QProcess process;
    process.start("defaults", {"read", "-g", "AppleInterfaceStyle"});
    process.waitForFinished(500);
    QString output = process.readAllStandardOutput().trimmed();
    return (output.compare("Dark", Qt::CaseInsensitive) == 0) ? Dark : Light;
#elif defined(Q_OS_LINUX)
    
    
    QProcess process;
    process.start("gsettings", {"get", "org.gnome.desktop.interface", "color-scheme"});
    process.waitForFinished(500);
    QString output = process.readAllStandardOutput().trimmed();
    if (output.contains("dark", Qt::CaseInsensitive)) {
        return Dark;
    }
    
    QProcess gtkProcess;
    gtkProcess.start("gsettings", {"get", "org.gnome.desktop.interface", "gtk-theme"});
    gtkProcess.waitForFinished(500);
    QString gtkTheme = gtkProcess.readAllStandardOutput().trimmed();
    return gtkTheme.contains("dark", Qt::CaseInsensitive) ? Dark : Light;
#else
    return Dark;
#endif
}

ThemeManager::Theme ThemeManager::currentTheme() const
{
    return m_currentTheme.load(std::memory_order_relaxed);
}

bool ThemeManager::isDarkMode() const
{
    return m_currentTheme.load(std::memory_order_relaxed) == Dark;
}

qreal ThemeManager::fontScaleFactor()
{
    QString sizeKey = ConfigStore::instance()->get<QString>(ConfigKeys::FontSize, "medium");
    if (sizeKey == QLatin1String("small"))  return 0.85;
    if (sizeKey == QLatin1String("large"))  return 1.15;
    return 1.0;
}

void ThemeManager::applyStyleSheet(Theme theme)
{
    QString qssPath = (theme == Dark) ? ":/qss/dark-style.qss" : ":/qss/light-style.qss";

    QFile file(qssPath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QString qss = QString::fromUtf8(file.readAll());
        file.close();

        
        QString sizeKey = ConfigStore::instance()->get<QString>(ConfigKeys::FontSize, "medium");
        qss = scaleFontSizes(qss, sizeKey);

        qApp->setStyleSheet(qss);

        QPalette palette = qApp->palette();
        if (theme == Dark) {
            palette.setColor(QPalette::Highlight, QColor("#3B82F6"));
            palette.setColor(QPalette::HighlightedText, QColor("#FFFFFF"));
        } else {
            palette.setColor(QPalette::Highlight, QColor("#93C5FD"));
            palette.setColor(QPalette::HighlightedText, QColor("#0F172A"));
        }
        qApp->setPalette(palette);
    } else {
        qWarning() << "ThemeManager failed to load stylesheet:" << qssPath;
    }
}

QString ThemeManager::scaleFontSizes(const QString &qss, const QString &sizeKey)
{
    
    qreal factor = 1.0;
    if (sizeKey == QLatin1String("small")) {
        factor = 0.85;
    } else if (sizeKey == QLatin1String("large")) {
        factor = 1.15;
    }

    
    if (qFuzzyCompare(factor, 1.0)) {
        return qss;
    }

    
    static QRegularExpression re(QStringLiteral("font-size:\\s*(\\d+)px"),
                                 QRegularExpression::CaseInsensitiveOption);

    QString result;
    result.reserve(qss.size() + 64);
    qsizetype lastEnd = 0;

    auto it = re.globalMatch(qss);
    while (it.hasNext()) {
        auto match = it.next();
        
        result.append(qss.mid(lastEnd, match.capturedStart() - lastEnd));

        
        int originalSize = match.captured(1).toInt();
        int scaledSize = qMax(8, qRound(originalSize * factor));

        result.append(QStringLiteral("font-size: %1px").arg(scaledSize));
        lastEnd = match.capturedEnd();
    }
    
    result.append(qss.mid(lastEnd));

    return result;
}
