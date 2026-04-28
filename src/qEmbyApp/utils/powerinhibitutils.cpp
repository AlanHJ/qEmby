#include "powerinhibitutils.h"

#include <QCoreApplication>
#include <QDebug>

#if defined(Q_OS_WIN)
#include <windows.h>
#elif defined(Q_OS_MACOS)
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/pwr_mgt/IOPMLib.h>
#elif defined(QEMBY_HAS_QT_DBUS)
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#endif

namespace {

int g_refCount = 0;
bool g_platformInhibited = false;

#if defined(Q_OS_MACOS)
IOPMAssertionID g_displayAssertion = kIOPMNullAssertionID;
IOPMAssertionID g_systemAssertion = kIOPMNullAssertionID;
#elif defined(QEMBY_HAS_QT_DBUS) && !defined(Q_OS_WIN) && !defined(Q_OS_MACOS)
enum class DbusInhibitor {
    None,
    FreedesktopScreenSaver,
    FreedesktopPowerManagement,
    GnomeSessionManager
};

DbusInhibitor g_dbusInhibitor = DbusInhibitor::None;
quint32 g_dbusCookie = 0;
#endif

QString applicationName()
{
    const QString appName = QCoreApplication::applicationName().trimmed();
    return appName.isEmpty() ? QStringLiteral("qEmby") : appName;
}

QString inhibitionReason(QString reason)
{
    reason = reason.trimmed();
    return reason.isEmpty() ? QStringLiteral("Video playback is active") : reason;
}

#if defined(Q_OS_MACOS)
CFStringRef createCFString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return CFStringCreateWithCString(kCFAllocatorDefault, utf8.constData(),
                                     kCFStringEncodingUTF8);
}
#endif

bool activatePlatformInhibition(const QString &reason)
{
#if defined(Q_OS_WIN)
    const EXECUTION_STATE state =
        ES_CONTINUOUS | ES_DISPLAY_REQUIRED | ES_SYSTEM_REQUIRED;
    const EXECUTION_STATE previous = SetThreadExecutionState(state);
    if (previous == 0) {
        qWarning() << "[PowerInhibit] Failed to prevent display sleep on Windows";
        return false;
    }

    qDebug() << "[PowerInhibit] Display sleep inhibited on Windows";
    return true;
#elif defined(Q_OS_MACOS)
    const QString resolvedReason = inhibitionReason(reason);
    CFStringRef reasonRef = createCFString(resolvedReason);
    if (!reasonRef) {
        qWarning() << "[PowerInhibit] Failed to create macOS assertion reason";
        return false;
    }

    const IOReturn displayResult = IOPMAssertionCreateWithName(
        CFSTR("PreventUserIdleDisplaySleep"), kIOPMAssertionLevelOn,
        reasonRef, &g_displayAssertion);
    const IOReturn systemResult = IOPMAssertionCreateWithName(
        CFSTR("PreventUserIdleSystemSleep"), kIOPMAssertionLevelOn,
        reasonRef, &g_systemAssertion);
    CFRelease(reasonRef);

    if (displayResult != kIOReturnSuccess) {
        if (systemResult == kIOReturnSuccess &&
            g_systemAssertion != kIOPMNullAssertionID) {
            IOPMAssertionRelease(g_systemAssertion);
        }
        g_displayAssertion = kIOPMNullAssertionID;
        g_systemAssertion = kIOPMNullAssertionID;
        qWarning() << "[PowerInhibit] Failed to prevent display sleep on macOS"
                   << "| displayResult=" << displayResult
                   << "| systemResult=" << systemResult;
        return false;
    }

    qDebug() << "[PowerInhibit] Display sleep inhibited on macOS"
             << "| displayAssertion=" << (displayResult == kIOReturnSuccess)
             << "| systemAssertion=" << (systemResult == kIOReturnSuccess);
    return true;
#elif defined(QEMBY_HAS_QT_DBUS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) {
        qWarning() << "[PowerInhibit] Session D-Bus is unavailable; cannot inhibit display sleep";
        return false;
    }

    const QString appName = applicationName();
    const QString resolvedReason = inhibitionReason(reason);

    QDBusInterface screenSaver(
        QStringLiteral("org.freedesktop.ScreenSaver"),
        QStringLiteral("/org/freedesktop/ScreenSaver"),
        QStringLiteral("org.freedesktop.ScreenSaver"), bus);
    if (screenSaver.isValid()) {
        QDBusReply<quint32> reply =
            screenSaver.call(QStringLiteral("Inhibit"), appName, resolvedReason);
        if (reply.isValid()) {
            g_dbusInhibitor = DbusInhibitor::FreedesktopScreenSaver;
            g_dbusCookie = reply.value();
            qDebug() << "[PowerInhibit] Display sleep inhibited via org.freedesktop.ScreenSaver"
                     << "| cookie=" << g_dbusCookie;
            return true;
        }

        qWarning() << "[PowerInhibit] org.freedesktop.ScreenSaver.Inhibit failed"
                   << "|" << reply.error().message();
    }

    QDBusInterface powerManagement(
        QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
        QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
        QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), bus);
    if (powerManagement.isValid()) {
        QDBusReply<quint32> reply =
            powerManagement.call(QStringLiteral("Inhibit"), appName, resolvedReason);
        if (reply.isValid()) {
            g_dbusInhibitor = DbusInhibitor::FreedesktopPowerManagement;
            g_dbusCookie = reply.value();
            qDebug() << "[PowerInhibit] Display sleep inhibited via org.freedesktop.PowerManagement"
                     << "| cookie=" << g_dbusCookie;
            return true;
        }

        qWarning() << "[PowerInhibit] org.freedesktop.PowerManagement.Inhibit failed"
                   << "|" << reply.error().message();
    }

    QDBusInterface gnomeSession(
        QStringLiteral("org.gnome.SessionManager"),
        QStringLiteral("/org/gnome/SessionManager"),
        QStringLiteral("org.gnome.SessionManager"), bus);
    if (gnomeSession.isValid()) {
        constexpr quint32 inhibitIdle = 8;
        QDBusReply<quint32> reply =
            gnomeSession.call(QStringLiteral("Inhibit"), appName, quint32(0),
                              resolvedReason, inhibitIdle);
        if (reply.isValid()) {
            g_dbusInhibitor = DbusInhibitor::GnomeSessionManager;
            g_dbusCookie = reply.value();
            qDebug() << "[PowerInhibit] Display sleep inhibited via org.gnome.SessionManager"
                     << "| cookie=" << g_dbusCookie;
            return true;
        }

        qWarning() << "[PowerInhibit] org.gnome.SessionManager.Inhibit failed"
                   << "|" << reply.error().message();
    }

    qWarning() << "[PowerInhibit] No supported D-Bus inhibition service was available";
    return false;
#else
    Q_UNUSED(reason);
    qWarning() << "[PowerInhibit] Display sleep inhibition is unavailable on this platform";
    return false;
#endif
}

void deactivatePlatformInhibition()
{
#if defined(Q_OS_WIN)
    SetThreadExecutionState(ES_CONTINUOUS);
    qDebug() << "[PowerInhibit] Display sleep inhibition released on Windows";
#elif defined(Q_OS_MACOS)
    if (g_displayAssertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(g_displayAssertion);
        g_displayAssertion = kIOPMNullAssertionID;
    }
    if (g_systemAssertion != kIOPMNullAssertionID) {
        IOPMAssertionRelease(g_systemAssertion);
        g_systemAssertion = kIOPMNullAssertionID;
    }
    qDebug() << "[PowerInhibit] Display sleep inhibition released on macOS";
#elif defined(QEMBY_HAS_QT_DBUS)
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected() || g_dbusInhibitor == DbusInhibitor::None) {
        g_dbusInhibitor = DbusInhibitor::None;
        g_dbusCookie = 0;
        return;
    }

    if (g_dbusInhibitor == DbusInhibitor::FreedesktopScreenSaver) {
        QDBusInterface screenSaver(
            QStringLiteral("org.freedesktop.ScreenSaver"),
            QStringLiteral("/org/freedesktop/ScreenSaver"),
            QStringLiteral("org.freedesktop.ScreenSaver"), bus);
        screenSaver.call(QStringLiteral("UnInhibit"), g_dbusCookie);
        qDebug() << "[PowerInhibit] Released org.freedesktop.ScreenSaver inhibition"
                 << "| cookie=" << g_dbusCookie;
    } else if (g_dbusInhibitor == DbusInhibitor::FreedesktopPowerManagement) {
        QDBusInterface powerManagement(
            QStringLiteral("org.freedesktop.PowerManagement.Inhibit"),
            QStringLiteral("/org/freedesktop/PowerManagement/Inhibit"),
            QStringLiteral("org.freedesktop.PowerManagement.Inhibit"), bus);
        powerManagement.call(QStringLiteral("UnInhibit"), g_dbusCookie);
        qDebug() << "[PowerInhibit] Released org.freedesktop.PowerManagement inhibition"
                 << "| cookie=" << g_dbusCookie;
    } else if (g_dbusInhibitor == DbusInhibitor::GnomeSessionManager) {
        QDBusInterface gnomeSession(
            QStringLiteral("org.gnome.SessionManager"),
            QStringLiteral("/org/gnome/SessionManager"),
            QStringLiteral("org.gnome.SessionManager"), bus);
        gnomeSession.call(QStringLiteral("Uninhibit"), g_dbusCookie);
        qDebug() << "[PowerInhibit] Released org.gnome.SessionManager inhibition"
                 << "| cookie=" << g_dbusCookie;
    }

    g_dbusInhibitor = DbusInhibitor::None;
    g_dbusCookie = 0;
#endif
}

} 

namespace PowerInhibitUtils {

bool acquirePlaybackInhibition(const QString &reason)
{
    ++g_refCount;
    if (g_refCount > 1) {
        qDebug() << "[PowerInhibit] Reuse existing playback inhibition"
                 << "| refCount=" << g_refCount;
        return g_platformInhibited;
    }

    g_platformInhibited = activatePlatformInhibition(reason);
    if (!g_platformInhibited) {
        g_refCount = 0;
    }
    return g_platformInhibited;
}

void releasePlaybackInhibition()
{
    if (g_refCount <= 0) {
        g_refCount = 0;
        return;
    }

    --g_refCount;
    if (g_refCount > 0) {
        qDebug() << "[PowerInhibit] Keep playback inhibition"
                 << "| refCount=" << g_refCount;
        return;
    }

    if (g_platformInhibited) {
        deactivatePlatformInhibition();
    }
    g_platformInhibited = false;
}

bool isPlaybackInhibited()
{
    return g_refCount > 0 && g_platformInhibited;
}

} 
