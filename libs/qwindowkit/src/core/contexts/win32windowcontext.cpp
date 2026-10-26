// Copyright (C) 2023-2024 Stdware Collections (https://www.github.com/stdware)
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#include "win32windowcontext_p.h"

#include <optional>

#include <QtCore/QAbstractEventDispatcher>
#include <QtCore/QDateTime>
#include <QtCore/QDebug>
#include <QtCore/QHash>
#include <QtCore/QScopeGuard>
#include <QtCore/QTimer>
#include <QtGui/QGuiApplication>
#include <QtGui/QPainter>
#include <QtGui/QPalette>

#include <QtGui/qpa/qwindowsysteminterface.h>

#include <QtGui/private/qhighdpiscaling_p.h>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#  include <QtGui/private/qguiapplication_p.h>
#endif
#include <QtGui/qpa/qplatformwindow.h>
#if QT_VERSION < QT_VERSION_CHECK(6, 2, 0)
#  include <QtGui/qpa/qplatformnativeinterface.h>
#else
#  include <QtGui/qpa/qplatformwindow_p.h>
#endif

#include <QWKCore/qwkconfig.h>

#include "qwkglobal_p.h"
#include "qwkwindowsextra_p.h"

#include <shellapi.h>

#if (QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)) && (QT_VERSION <= QT_VERSION_CHECK(6, 6, 1))
#  error Current Qt version has a critical bug which will break QWK functionality. Please upgrade to > 6.6.1 or downgrade to < 6.6.0
#endif

#ifndef DWM_BB_ENABLE
#  define DWM_BB_ENABLE 0x00000001
#endif

#ifndef ABM_GETAUTOHIDEBAREX
#  define ABM_GETAUTOHIDEBAREX 0x0000000b
#endif

namespace QWK {

    enum IconButtonClickLevelFlag {
        IconButtonClicked = 1,
        IconButtonDoubleClicked = 2,
        IconButtonTriggersClose = 4,
    };

    
    static constexpr const quint8 kAutoHideTaskBarThickness = 2;

    QWK_USED static constexpr const struct {
        const uint32_t activeLight = MAKE_RGBA_COLOR(110, 110, 110, 255);   
        const uint32_t activeDark = MAKE_RGBA_COLOR(51, 51, 51, 255);       
        const uint32_t inactiveLight = MAKE_RGBA_COLOR(167, 167, 167, 255); 
        const uint32_t inactiveDark = MAKE_RGBA_COLOR(61, 61, 62, 255);     
    } kWindowsColorSet;

    
    using WndProcHash = QHash<HWND, Win32WindowContext *>;
    Q_GLOBAL_STATIC(WndProcHash, g_wndProcHash)

    
    
    using OriginalWndProcHash = QHash<HWND, WNDPROC>;
    Q_GLOBAL_STATIC(OriginalWndProcHash, g_originalWndProcs)

    static inline bool
#if !QWINDOWKIT_CONFIG(ENABLE_WINDOWS_SYSTEM_BORDERS)
        constexpr
#endif

        isSystemBorderEnabled() {
        return
#if QWINDOWKIT_CONFIG(ENABLE_WINDOWS_SYSTEM_BORDERS)
            isWin10OrGreater()
#else
            false
#endif
                ;
    }

    static inline void triggerFrameChange(HWND hwnd) {
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                           SWP_FRAMECHANGED);
    }

    static void setInternalWindowFrameMargins(QWindow *window, const QMargins &margins) {
        
        
        return;

        const QVariant marginsVar = QVariant::fromValue(margins);

        
        
        
        
        window->setProperty("_q_windowsCustomMargins", marginsVar);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        if (QPlatformWindow *platformWindow = window->handle()) {
            if (const auto ni = QGuiApplication::platformNativeInterface()) {
                ni->setWindowProperty(platformWindow, QStringLiteral("WindowsCustomMargins"),
                                      marginsVar);
            }
        }
#else
        if (const auto platformWindow =
                dynamic_cast<QNativeInterface::Private::QWindowsWindow *>(window->handle())) {
            platformWindow->setCustomMargins(margins);
        }
#endif
    }

    static inline MONITORINFOEXW getMonitorForWindow(HWND hwnd) {
        
        
        HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
        MONITORINFOEXW monitorInfo{};
        monitorInfo.cbSize = sizeof(monitorInfo);
        ::GetMonitorInfoW(monitor, &monitorInfo);
        return monitorInfo;
    }

    static inline void moveWindowToMonitor(HWND hwnd, const MONITORINFOEXW &activeMonitor) {
        RECT currentMonitorRect = getMonitorForWindow(hwnd).rcMonitor;
        RECT activeMonitorRect = activeMonitor.rcMonitor;
        
        if (currentMonitorRect == activeMonitorRect) {
            return;
        }
        RECT currentWindowRect{};
        ::GetWindowRect(hwnd, &currentWindowRect);
        auto newWindowX =
            activeMonitorRect.left + (currentWindowRect.left - currentMonitorRect.left);
        auto newWindowY = activeMonitorRect.top + (currentWindowRect.top - currentMonitorRect.top);
        ::SetWindowPos(hwnd, nullptr, newWindowX, newWindowY, RECT_WIDTH(currentWindowRect),
                       RECT_HEIGHT(currentWindowRect),
                       SWP_NOACTIVATE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
    }

    static inline bool isFullScreen(HWND hwnd) {
        RECT windowRect{};
        ::GetWindowRect(hwnd, &windowRect);
        
        return (windowRect == getMonitorForWindow(hwnd).rcMonitor);
    }

    static inline bool isMaximized(HWND hwnd) {
        return ::IsZoomed(hwnd);
    }

    static inline bool isMinimized(HWND hwnd) {
        return ::IsIconic(hwnd);
    }

    static inline bool isWindowNoState(HWND hwnd) {
#if 0
        WINDOWPLACEMENT wp{};
        wp.length = sizeof(wp);
        ::GetWindowPlacement(hwnd, &wp);
        return ((wp.showCmd == SW_NORMAL) || (wp.showCmd == SW_RESTORE));
#else
        if (isFullScreen(hwnd)) {
            return false;
        }
        const auto style = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_STYLE));
        return (!(style & (WS_MINIMIZE | WS_MAXIMIZE)));
#endif
    }

    static inline void bringWindowToFront(HWND hwnd) {
        HWND oldForegroundWindow = ::GetForegroundWindow();
        if (!oldForegroundWindow) {
            
            return;
        }
        MONITORINFOEXW activeMonitor = getMonitorForWindow(oldForegroundWindow);
        
        if (!::IsWindowVisible(hwnd)) {
            ::ShowWindow(hwnd, SW_SHOW);
        }
        if (isMinimized(hwnd)) {
            
            ::ShowWindow(hwnd, SW_RESTORE);
            
            moveWindowToMonitor(hwnd, activeMonitor);
            
            
            return;
        }
        
        
        
        if (!::SendMessageTimeoutW(oldForegroundWindow, WM_NULL, 0, 0,
                                   SMTO_BLOCK | SMTO_ABORTIFHUNG | SMTO_NOTIMEOUTIFNOTHUNG, 1000,
                                   nullptr)) {
            
            return;
        }
        DWORD windowThreadProcessId = ::GetWindowThreadProcessId(oldForegroundWindow, nullptr);
        DWORD currentThreadId = ::GetCurrentThreadId();
        
        
        ::AttachThreadInput(windowThreadProcessId, currentThreadId, TRUE);

        [[maybe_unused]] const auto &cleaner =
            qScopeGuard([windowThreadProcessId, currentThreadId]() {
                ::AttachThreadInput(windowThreadProcessId, currentThreadId, FALSE);
            });

        ::BringWindowToTop(hwnd);
        
        
        ::SetActiveWindow(hwnd);
        
        moveWindowToMonitor(hwnd, activeMonitor);
    }

    
    static bool showSystemMenu_sys(HWND hWnd, const POINT &pos, const bool selectFirstEntry,
                                   const bool fixedSize) {
        HMENU hMenu = ::GetSystemMenu(hWnd, FALSE);
        if (!hMenu) {
            
            
            
            return true;
        }

        const auto windowStyles = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
        const bool allowMaximize = windowStyles & WS_MAXIMIZEBOX;
        const bool allowMinimize = windowStyles & WS_MINIMIZEBOX;

        const bool maxOrFull = isMaximized(hWnd) || isFullScreen(hWnd);
        ::EnableMenuItem(hMenu, SC_CLOSE, (MF_BYCOMMAND | MFS_ENABLED));
        ::EnableMenuItem(
            hMenu, SC_MAXIMIZE,
            (MF_BYCOMMAND |
             ((maxOrFull || fixedSize || !allowMaximize) ? MFS_DISABLED : MFS_ENABLED)));
        ::EnableMenuItem(
            hMenu, SC_RESTORE,
            (MF_BYCOMMAND |
             ((maxOrFull && !fixedSize && allowMaximize) ? MFS_ENABLED : MFS_DISABLED)));
        
        
        
        
        
        
        
        
        
        ::HiliteMenuItem(hWnd, hMenu, SC_RESTORE,
                         (MF_BYCOMMAND | (selectFirstEntry ? MFS_HILITE : MFS_UNHILITE)));
        ::EnableMenuItem(hMenu, SC_MINIMIZE,
                         (MF_BYCOMMAND | (allowMinimize ? MFS_ENABLED : MFS_DISABLED)));
        ::EnableMenuItem(hMenu, SC_SIZE,
                         (MF_BYCOMMAND | ((maxOrFull || fixedSize) ? MFS_DISABLED : MFS_ENABLED)));
        ::EnableMenuItem(hMenu, SC_MOVE, (MF_BYCOMMAND | (maxOrFull ? MFS_DISABLED : MFS_ENABLED)));

        
        
        
        UINT defaultItemId = UINT_MAX;
        if (isWin11OrGreater()) {
            if (maxOrFull) {
                defaultItemId = SC_RESTORE;
            } else {
                defaultItemId = SC_MAXIMIZE;
            }
        }
        if (defaultItemId == UINT_MAX) {
            defaultItemId = SC_CLOSE;
        }
        ::SetMenuDefaultItem(hMenu, defaultItemId, FALSE);

        
        const auto result = ::TrackPopupMenu(
            hMenu,
            (TPM_RETURNCMD | (QGuiApplication::isRightToLeft() ? TPM_RIGHTALIGN : TPM_LEFTALIGN) |
             TPM_RIGHTBUTTON),
            pos.x, pos.y, 0, hWnd, nullptr);

        
        
        ::HiliteMenuItem(hWnd, hMenu, SC_RESTORE, (MF_BYCOMMAND | MFS_UNHILITE));

        if (!result) {
            
            return false;
        }

        
        ::PostMessageW(hWnd, WM_SYSCOMMAND, result, 0);
        return true;
    }

    static inline Win32WindowContext::WindowPart getHitWindowPart(int hitTestResult) {
        switch (hitTestResult) {
            case HTCLIENT:
                return Win32WindowContext::ClientArea;
            case HTCAPTION:
                return Win32WindowContext::TitleBar;
            case HTSYSMENU:
            case HTHELP:
            case HTREDUCE:
            case HTZOOM:
            case HTCLOSE:
                return Win32WindowContext::ChromeButton;
            case HTLEFT:
            case HTRIGHT:
            case HTTOP:
            case HTTOPLEFT:
            case HTTOPRIGHT:
            case HTBOTTOM:
            case HTBOTTOMLEFT:
            case HTBOTTOMRIGHT:
                return Win32WindowContext::ResizeBorder;
            case HTBORDER:
                return Win32WindowContext::FixedBorder;
            default:
                break;
        }
        return Win32WindowContext::Outside;
    }

    static bool isValidWindow(HWND hWnd, bool checkVisible, bool checkTopLevel) {
        if (!::IsWindow(hWnd)) {
            return false;
        }
        const LONG_PTR styles = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
        if (styles & WS_DISABLED) {
            return false;
        }
        const LONG_PTR exStyles = ::GetWindowLongPtrW(hWnd, GWL_EXSTYLE);
        if (exStyles & WS_EX_TOOLWINDOW) {
            return false;
        }
        RECT rect{};
        if (!::GetWindowRect(hWnd, &rect)) {
            return false;
        }
        if ((rect.left >= rect.right) || (rect.top >= rect.bottom)) {
            return false;
        }
        if (checkVisible) {
            if (!::IsWindowVisible(hWnd)) {
                return false;
            }
        }
        if (checkTopLevel) {
            if (::GetAncestor(hWnd, GA_ROOT) != hWnd) {
                return false;
            }
        }
        return true;
    }

    static inline constexpr bool isNonClientMessage(const UINT message) {
        if (((message >= WM_NCCREATE) && (message <= WM_NCACTIVATE)) ||
            ((message >= WM_NCMOUSEMOVE) && (message <= WM_NCMBUTTONDBLCLK)) ||
            ((message >= WM_NCXBUTTONDOWN) && (message <= WM_NCXBUTTONDBLCLK))
#if (WINVER >= _WIN32_WINNT_WIN8)
            || ((message >= WM_NCPOINTERUPDATE) && (message <= WM_NCPOINTERUP))
#endif
            || ((message == WM_NCMOUSEHOVER) || (message == WM_NCMOUSELEAVE))) {
            return true;
        } else {
            return false;
        }
    }

    static MSG createMessageBlock(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
        MSG msg;
        msg.hwnd = hWnd;
        msg.message = message;
        msg.wParam = wParam;
        msg.lParam = lParam;

        const DWORD dwScreenPos = ::GetMessagePos();
        msg.pt.x = GET_X_LPARAM(dwScreenPos);
        msg.pt.y = GET_Y_LPARAM(dwScreenPos);
        if (!isNonClientMessage(message)) {
            ::ScreenToClient(hWnd, &msg.pt);
        }

        msg.time = ::GetMessageTime();
        return msg;
    }

    static inline constexpr bool isInputMessage(UINT m) {
        switch (m) {
            case WM_IME_STARTCOMPOSITION:
            case WM_IME_ENDCOMPOSITION:
            case WM_IME_COMPOSITION:
            case WM_INPUT:
            case WM_TOUCH:
            case WM_MOUSEHOVER:
            case WM_MOUSELEAVE:
            case WM_NCMOUSEHOVER:
            case WM_NCMOUSELEAVE:
            case WM_SIZING:
            case WM_MOVING:
            case WM_SYSCOMMAND:
            case WM_COMMAND:
            case WM_DWMNCRENDERINGCHANGED:
            case WM_PAINT:
                return true;
            default:
                break;
        }
        return (m >= WM_MOUSEFIRST && m <= WM_MOUSELAST) ||
               (m >= WM_NCMOUSEMOVE && m <= WM_NCXBUTTONDBLCLK) ||
               (m >= WM_KEYFIRST && m <= WM_KEYLAST);
    }

    static inline QByteArray nativeEventType() {
        return QByteArrayLiteral("windows_generic_MSG");
    }

    
    static bool filterNativeEvent(MSG *msg, LRESULT *result) {
        auto dispatcher = QAbstractEventDispatcher::instance();
        QT_NATIVE_EVENT_RESULT_TYPE filterResult = *result;
        if (dispatcher && dispatcher->filterNativeEvent(nativeEventType(), msg, &filterResult)) {
            *result = LRESULT(filterResult);
            return true;
        }
        return false;
    }

    
    static bool filterNativeEvent(QWindow *window, MSG *msg, LRESULT *result) {
        QT_NATIVE_EVENT_RESULT_TYPE filterResult = *result;
        if (QWindowSystemInterface::handleNativeEvent(window, nativeEventType(), msg,
                                                      &filterResult)) {
            *result = LRESULT(filterResult);
            return true;
        }
        return false;
    }

    static inline bool forwardFilteredEvent(QWindow *window, HWND hWnd, UINT message, WPARAM wParam,
                                            LPARAM lParam, LRESULT *result) {
        MSG msg = createMessageBlock(hWnd, message, wParam, lParam);

        
        

        
        
        if (!isInputMessage(msg.message) && filterNativeEvent(&msg, result))
            return true;

        auto platformWindow = window->handle();
        if (platformWindow && filterNativeEvent(platformWindow->window(), &msg, result))
            return true;

        return false;
    }

    
    
    
    
    
    
    
    class WindowsNativeEventFilter : public AppNativeEventFilter {
    public:
        bool nativeEventFilter(const QByteArray &eventType, void *message,
                               QT_NATIVE_EVENT_RESULT_TYPE *result) override {
            Q_UNUSED(eventType)

            
            
            if (!result) {
                return false;
            }

            auto msg = static_cast<const MSG *>(message);
            switch (msg->message) {
                case WM_NCCALCSIZE: {
                    
                    
                    
                    if (lastMessageContext) {
                        LRESULT res;
                        if (lastMessageContext->nonClientCalcSizeHandler(
                                msg->hwnd, msg->message, msg->wParam, msg->lParam, &res)) {
                            *result = decltype(*result)(res);
                            return true;
                        }
                    }
                    break;
                }

                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
                    
            }
            return false;
        }

        static inline WindowsNativeEventFilter *instance = nullptr;
        static inline Win32WindowContext *lastMessageContext = nullptr;

        static inline void install() {
            if (instance) {
                return;
            }
            instance = new WindowsNativeEventFilter();
        }

        static inline void uninstall() {
            if (!instance) {
                return;
            }
            delete instance;
            instance = nullptr;
        }
    };

    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    
    

    extern "C" LRESULT QT_WIN_CALLBACK QWKHookedWndProc(HWND hWnd, UINT message, WPARAM wParam,
                                                        LPARAM lParam) {
        Q_ASSERT(hWnd);
        if (!hWnd) {
            return FALSE;
        }

        
        
        const WNDPROC originalProc = g_originalWndProcs->value(hWnd, nullptr);
        const auto nativeDestroyCleanup = qScopeGuard([hWnd, message]() {
            if (message == WM_NCDESTROY) {
                g_originalWndProcs->remove(hWnd);
            }
        });
        auto ctx = g_wndProcHash->value(hWnd);
        if (!ctx || !originalProc) {
            return originalProc ? ::CallWindowProcW(originalProc, hWnd, message, wParam, lParam)
                                : ::DefWindowProcW(hWnd, message, wParam, lParam);
        }

        WindowsNativeEventFilter::lastMessageContext = ctx;
        const auto &contextCleaner = qScopeGuard([]() {
            WindowsNativeEventFilter::lastMessageContext = nullptr; 
        });

        
        
        if (message == WM_NCCALCSIZE) {
            return ::CallWindowProcW(originalProc, hWnd, message, wParam, lParam);
        }

        
        LRESULT result;
        if (ctx->windowProc(hWnd, message, wParam, lParam, &result)) {
            
            
            
            std::ignore =
                forwardFilteredEvent(ctx->window(), hWnd, message, wParam, lParam, &result);
            return result;
        }

        
        return ::CallWindowProcW(originalProc, hWnd, message, wParam, lParam);
    }

    static inline void addManagedWindow(QWindow *window, HWND hWnd, Win32WindowContext *ctx) {
        if (isSystemBorderEnabled()) {
            
            setInternalWindowFrameMargins(window, QMargins(0, -int(getTitleBarHeight(hWnd)), 0, 0));
        }

        
        
        if (!g_originalWndProcs->contains(hWnd)) {
            ::SetLastError(ERROR_SUCCESS);
            const auto previousProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(
                hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(QWKHookedWndProc)));
            if (!previousProc) {
                qWarning() << "[QWK] Failed to install window procedure"
                           << "| hwnd=" << quintptr(hWnd) << "| error=" << ::GetLastError();
                return;
            }
            if (previousProc == QWKHookedWndProc) {
                qWarning() << "[QWK] Missing original window procedure for existing hook"
                           << "| hwnd=" << quintptr(hWnd);
                return;
            }
            bool differsFromExistingWindow = false;
            for (auto it = g_originalWndProcs->cbegin(); it != g_originalWndProcs->cend(); ++it) {
                if (it.value() != previousProc) {
                    differsFromExistingWindow = true;
                    break;
                }
            }
            g_originalWndProcs->insert(hWnd, previousProc);
            qInfo() << "[QWK] Per-window procedure installed"
                    << "| hwnd=" << quintptr(hWnd)
                    << "| originalProc=" << quintptr(previousProc)
                    << "| classProc=" << quintptr(::GetClassLongPtrW(hWnd, GCLP_WNDPROC))
                    << "| surfaceType=" << (window ? int(window->surfaceType()) : -1)
                    << "| differsFromExistingWindow=" << differsFromExistingWindow;
        }

        
        WindowsNativeEventFilter::install();

        
        g_wndProcHash->insert(hWnd, ctx);

        
        
        
        triggerFrameChange(hWnd);
    }

    static inline void removeManagedWindow(HWND hWnd) {
        
        if (!g_wndProcHash->remove(hWnd))
            return;

        const WNDPROC originalProc = g_originalWndProcs->value(hWnd, nullptr);
        if (!::IsWindow(hWnd)) {
            g_originalWndProcs->remove(hWnd);
        } else if (originalProc && reinterpret_cast<WNDPROC>(
                       ::GetWindowLongPtrW(hWnd, GWLP_WNDPROC)) == QWKHookedWndProc) {
            
            
            if (::SetWindowLongPtrW(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(originalProc))) {
                g_originalWndProcs->remove(hWnd);
            } else {
                qWarning() << "[QWK] Failed to restore window procedure"
                           << "| hwnd=" << quintptr(hWnd) << "| error=" << ::GetLastError();
            }
        }

        
        if (g_wndProcHash->empty()) {
            WindowsNativeEventFilter::uninstall();
        }
    }

    Win32WindowContext::Win32WindowContext() : AbstractWindowContext() {
    }

    Win32WindowContext::~Win32WindowContext() {
        if (m_windowId) {
            removeManagedWindow(reinterpret_cast<HWND>(m_windowId));
        }
    }

    QString Win32WindowContext::key() const {
        return QStringLiteral("win32");
    }

    void Win32WindowContext::virtual_hook(int id, void *data) {
        switch (id) {
            case RaiseWindowHook: {
                if (!m_windowId)
                    return;
                m_delegate->setWindowVisible(m_host, true);
                const auto hwnd = reinterpret_cast<HWND>(m_windowId);
                bringWindowToFront(hwnd);
                return;
            }

            case ShowSystemMenuHook: {
                if (!m_windowId)
                    return;
                const auto &pos = *static_cast<const QPoint *>(data);
                auto hWnd = reinterpret_cast<HWND>(m_windowId);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
                const QPoint nativeGlobalPos =
                    QHighDpi::toNativeGlobalPosition(pos, m_windowHandle.data());
#else
                const QPoint nativeGlobalPos = QHighDpi::toNativePixels(pos, m_windowHandle.data());
#endif
                std::ignore = showSystemMenu_sys(hWnd, qpoint2point(nativeGlobalPos), false,
                                                 isHostSizeFixed());
                return;
            }

            case DefaultColorsHook: {
                auto &map = *static_cast<QMap<QString, QColor> *>(data);
                map.clear();
                map.insert(QStringLiteral("activeLight"), kWindowsColorSet.activeLight);
                map.insert(QStringLiteral("activeDark"), kWindowsColorSet.activeDark);
                map.insert(QStringLiteral("inactiveLight"), kWindowsColorSet.inactiveLight);
                map.insert(QStringLiteral("inactiveDark"), kWindowsColorSet.inactiveDark);
                return;
            }

#if QWINDOWKIT_CONFIG(ENABLE_WINDOWS_SYSTEM_BORDERS)
            
            case DrawWindows10BorderHook_Emulated: {
                if (!m_windowId)
                    return;

                auto args = static_cast<void **>(data);
                auto &painter = *static_cast<QPainter *>(args[0]);
                const auto &rect = *static_cast<const QRect *>(args[1]);
                const auto &region = *static_cast<const QRegion *>(args[2]);
                const auto hwnd = reinterpret_cast<HWND>(m_windowId);

                QPen pen;
#  if QT_VERSION_MAJOR < 6
                pen.setWidth(1);
#  else
                pen.setWidthF(1 / m_windowHandle->devicePixelRatio()); 
#  endif

                const bool dark = isDarkThemeActive() && isDarkWindowFrameEnabled(hwnd);
                if (m_delegate->isWindowActive(m_host)) {
                    if (isWindowFrameBorderColorized()) {
                        pen.setColor(getAccentColor());
                    } else {
                        static QColor frameBorderActiveColorLight(kWindowsColorSet.activeLight);
                        static QColor frameBorderActiveColorDark(kWindowsColorSet.activeDark);
                        pen.setColor(dark ? frameBorderActiveColorDark
                                          : frameBorderActiveColorLight);
                    }
                } else {
                    static QColor frameBorderInactiveColorLight(kWindowsColorSet.inactiveLight);
                    static QColor frameBorderInactiveColorDark(kWindowsColorSet.inactiveDark);
                    pen.setColor(dark ? frameBorderInactiveColorDark
                                      : frameBorderInactiveColorLight);
                }
                painter.save();

                
                painter.setRenderHint(QPainter::Antialiasing);

                painter.setPen(pen);
                painter.drawLine(QLine{
                    QPoint{0,                       0},
                    QPoint{m_windowHandle->width(), 0}
                });
                painter.restore();
                return;
            }

            case DrawWindows10BorderHook_Native: {
                if (!m_windowId)
                    return;

                
                
                

                auto hWnd = reinterpret_cast<HWND>(m_windowId);
                HDC hdc = ::GetDC(hWnd);
                RECT windowRect{};
                ::GetClientRect(hWnd, &windowRect);
                RECT rcTopBorder = {
                    0,
                    0,
                    RECT_WIDTH(windowRect),
                    1,
                };
                ::FillRect(hdc, &rcTopBorder,
                           reinterpret_cast<HBRUSH>(::GetStockObject(BLACK_BRUSH)));
                ::ReleaseDC(hWnd, hdc);
                return;
            }
#endif

            default:
                break;
        }
        AbstractWindowContext::virtual_hook(id, data);
    }

    QVariant Win32WindowContext::windowAttribute(const QString &key) const {
        if (key == QStringLiteral("window-rect")) {
            if (!m_windowId)
                return {};

            RECT frame{};
            auto hwnd = reinterpret_cast<HWND>(m_windowId);
            
            auto style = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_STYLE) & ~WS_OVERLAPPED);
            auto exStyle = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
            const DynamicApis &apis = DynamicApis::instance();
            if (apis.pAdjustWindowRectExForDpi) {
                apis.pAdjustWindowRectExForDpi(&frame, style, FALSE, exStyle,
                                               getDpiForWindow(hwnd));
            } else {
                ::AdjustWindowRectEx(&frame, style, FALSE, exStyle);
            }
            return QVariant::fromValue(rect2qrect(frame));
        }

        if (key == QStringLiteral("win10-border-needed")) {
            return isSystemBorderEnabled() && !isWin11OrGreater();
        }

        if (key == QStringLiteral("windows-system-border-enabled")) {
            return isSystemBorderEnabled();
        }

        if (key == QStringLiteral("border-thickness")) {
            return m_windowId
                       ? int(getWindowFrameBorderThickness(reinterpret_cast<HWND>(m_windowId)))
                       : 0;
        }

        if (key == QStringLiteral("title-bar-height")) {
            return m_windowId ? int(getTitleBarHeight(reinterpret_cast<HWND>(m_windowId))) : 0;
        }
        return AbstractWindowContext::windowAttribute(key);
    }

    void Win32WindowContext::winIdChanged(WId winId, WId oldWinId) {
        
        mouseLeaveBlocked = false;
        lastHitTestResult = WindowPart::Outside;
        lastHitTestResultRaw = HTNOWHERE;

        
        if (oldWinId) {
            removeManagedWindow(reinterpret_cast<HWND>(oldWinId));
        }
        if (!winId) {
            return;
        }

        
        auto hWnd = reinterpret_cast<HWND>(winId);
        if (!isSystemBorderEnabled()) {
            static auto margins = QVariant::fromValue(QMargins(1, 1, 1, 1));

            
            
            
            setWindowAttribute(QStringLiteral("extra-margins"), margins);
        }

        
        
        {
            auto style = ::GetWindowLongPtrW(hWnd, GWL_STYLE);
            if (isSystemBorderEnabled()) {
                ::SetWindowLongPtrW(hWnd, GWL_STYLE, style & (~WS_SYSMENU));
            } else {
                ::SetWindowLongPtrW(hWnd, GWL_STYLE,
                                    (style | WS_THICKFRAME | WS_CAPTION) & (~WS_SYSMENU));
            }
        }

        
        addManagedWindow(m_windowHandle, hWnd, this);
    }

    bool Win32WindowContext::windowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                        LRESULT *result) {
        *result = FALSE;

        
        
        switch (message) {
            case WM_DESTROY:
            case WM_CLOSE:
            case WM_NCDESTROY:
            
            case WM_UAHDESTROYWINDOW:
            case WM_UNREGISTER_WINDOW_SERVICES:
                return false;
            default:
                break;
        }

        if (!isValidWindow(hWnd, false, true)) {
            return false;
        }

        
        if (snapLayoutHandler(hWnd, message, wParam, lParam, result)) {
            return true;
        }

        
        if (customWindowHandler(hWnd, message, wParam, lParam, result)) {
            return true;
        }

        
        if (systemMenuHandler(hWnd, message, wParam, lParam, result)) {
            return true;
        }

        
        if (!m_nativeEventFilters.isEmpty()) {
            MSG msg = createMessageBlock(hWnd, message, wParam, lParam);
            QT_NATIVE_EVENT_RESULT_TYPE res = 0;
            if (nativeDispatch(nativeEventType(), &msg, &res)) {
                *result = LRESULT(res);
                return true;
            }
        }
        return false; 
    }

    bool Win32WindowContext::windowAttributeChanged(const QString &key, const QVariant &attribute,
                                                    const QVariant &oldAttribute) {
        Q_UNUSED(oldAttribute)

        const auto hwnd = reinterpret_cast<HWND>(m_windowId);
        Q_ASSERT(hwnd);

        const DynamicApis &apis = DynamicApis::instance();
        const auto &extendMargins = [this, &apis, hwnd]() {
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            
            static constexpr const MARGINS margins = {65536, 0, 0, 0};
            apis.pDwmExtendFrameIntoClientArea(hwnd, &margins);
        };
        const auto &restoreMargins = [this, &apis, hwnd]() {
            auto margins = qmargins2margins(
                windowAttribute(QStringLiteral("extra-margins")).value<QMargins>());
            apis.pDwmExtendFrameIntoClientArea(hwnd, &margins);
        };

        const auto &effectBugWorkaround = [this, hwnd]() {
            
            
            
            
            if (m_host->isWidgetType()) {
                return;
            }
            
            static const char *kPropKey = "_qwk_effectBugWorkaround1";
            if (property(kPropKey).toBool()) {
                return;
            }
            setProperty(kPropKey, true);

            RECT rect{};
            ::GetWindowRect(hwnd, &rect);
            ::MoveWindow(hwnd, rect.left, rect.top, 1, 1, FALSE);
            ::MoveWindow(hwnd, rect.right - 1, rect.bottom - 1, 1, 1, FALSE);
            ::MoveWindow(hwnd, rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top,
                         FALSE);
        };

        if (key == QStringLiteral("extra-margins")) {
            auto margins = qmargins2margins(attribute.value<QMargins>());
            return SUCCEEDED(apis.pDwmExtendFrameIntoClientArea(hwnd, &margins));
        }

        if (key == QStringLiteral("dark-mode")) {
            if (!isWin101809OrGreater()) {
                return false;
            }

            BOOL enable = attribute.toBool();
            if (isWin101903OrGreater()) {
                apis.pSetPreferredAppMode(enable ? PAM_AUTO : PAM_DEFAULT);
            } else {
                apis.pAllowDarkModeForApp(enable);
            }
            const auto attr = isWin1020H1OrGreater() ? _DWMWA_USE_IMMERSIVE_DARK_MODE
                                                     : _DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1;
            apis.pDwmSetWindowAttribute(hwnd, attr, &enable, sizeof(enable));

            apis.pFlushMenuThemes();
            return true;
        }

        
        if (key == QStringLiteral("mica")) {
            if (!isWin11OrGreater()) {
                return false;
            }
            if (attribute.toBool()) {
                extendMargins();
                if (isWin1122H2OrGreater()) {
                    
                    
                    const _DWM_SYSTEMBACKDROP_TYPE backdropType = _DWMSBT_MAINWINDOW;
                    apis.pDwmSetWindowAttribute(hwnd, _DWMWA_SYSTEMBACKDROP_TYPE, &backdropType,
                                                sizeof(backdropType));
                } else {
                    
                    
                    const BOOL enable = TRUE;
                    apis.pDwmSetWindowAttribute(hwnd, _DWMWA_MICA_EFFECT, &enable, sizeof(enable));
                }
            } else {
                if (isWin1122H2OrGreater()) {
                    const _DWM_SYSTEMBACKDROP_TYPE backdropType = _DWMSBT_AUTO;
                    apis.pDwmSetWindowAttribute(hwnd, _DWMWA_SYSTEMBACKDROP_TYPE, &backdropType,
                                                sizeof(backdropType));
                } else {
                    const BOOL enable = FALSE;
                    apis.pDwmSetWindowAttribute(hwnd, _DWMWA_MICA_EFFECT, &enable, sizeof(enable));
                }
                restoreMargins();
            }
            effectBugWorkaround();
            return true;
        }

        if (key == QStringLiteral("mica-alt")) {
            if (!isWin1122H2OrGreater()) {
                return false;
            }
            if (attribute.toBool()) {
                extendMargins();
                
                
                const _DWM_SYSTEMBACKDROP_TYPE backdropType = _DWMSBT_TABBEDWINDOW;
                apis.pDwmSetWindowAttribute(hwnd, _DWMWA_SYSTEMBACKDROP_TYPE, &backdropType,
                                            sizeof(backdropType));
            } else {
                const _DWM_SYSTEMBACKDROP_TYPE backdropType = _DWMSBT_AUTO;
                apis.pDwmSetWindowAttribute(hwnd, _DWMWA_SYSTEMBACKDROP_TYPE, &backdropType,
                                            sizeof(backdropType));
                restoreMargins();
            }
            effectBugWorkaround();
            return true;
        }

        if (key == QStringLiteral("acrylic-material")) {
            if (!isWin11OrGreater()) {
                return false;
            }
            if (attribute.toBool()) {
                extendMargins();

                const _DWM_SYSTEMBACKDROP_TYPE backdropType = _DWMSBT_TRANSIENTWINDOW;
                apis.pDwmSetWindowAttribute(hwnd, _DWMWA_SYSTEMBACKDROP_TYPE, &backdropType,
                                            sizeof(backdropType));

                
                
                
                
                
                
                
                
                
                
                
                
                
                
            } else {
                const _DWM_SYSTEMBACKDROP_TYPE backdropType = _DWMSBT_AUTO;
                apis.pDwmSetWindowAttribute(hwnd, _DWMWA_SYSTEMBACKDROP_TYPE, &backdropType,
                                            sizeof(backdropType));

                
                
                
                
                
                
                
                
                

                restoreMargins();
            }
            effectBugWorkaround();
            return true;
        }

        if (key == QStringLiteral("dwm-blur")) {
            
            restoreMargins();
            if (attribute.toBool()) {
                if (isWin8OrGreater()) {
                    ACCENT_POLICY policy{};
                    policy.dwAccentState = ACCENT_ENABLE_BLURBEHIND;
                    policy.dwAccentFlags = ACCENT_NONE;
                    WINDOWCOMPOSITIONATTRIBDATA wcad{};
                    wcad.Attrib = WCA_ACCENT_POLICY;
                    wcad.pvData = &policy;
                    wcad.cbData = sizeof(policy);
                    apis.pSetWindowCompositionAttribute(hwnd, &wcad);
                } else {
                    DWM_BLURBEHIND bb{};
                    bb.fEnable = TRUE;
                    bb.dwFlags = DWM_BB_ENABLE;
                    apis.pDwmEnableBlurBehindWindow(hwnd, &bb);
                }
            } else {
                if (isWin8OrGreater()) {
                    ACCENT_POLICY policy{};
                    policy.dwAccentState = ACCENT_DISABLED;
                    policy.dwAccentFlags = ACCENT_NONE;
                    WINDOWCOMPOSITIONATTRIBDATA wcad{};
                    wcad.Attrib = WCA_ACCENT_POLICY;
                    wcad.pvData = &policy;
                    wcad.cbData = sizeof(policy);
                    apis.pSetWindowCompositionAttribute(hwnd, &wcad);
                } else {
                    DWM_BLURBEHIND bb{};
                    bb.fEnable = FALSE;
                    bb.dwFlags = DWM_BB_ENABLE;
                    apis.pDwmEnableBlurBehindWindow(hwnd, &bb);
                }
            }
            effectBugWorkaround();
            return true;
        }
        return false;
    }

    QWK_USED static constexpr const struct {
        const WPARAM wParam = MAKEWPARAM(44500, 61897);
        const LPARAM lParam = MAKELPARAM(62662, 44982); 
    } kMessageTag;

    static inline quint64 getKeyState() {
        quint64 result = 0;
        const auto &get = [](const int virtualKey) -> bool {
            return (::GetAsyncKeyState(virtualKey) < 0);
        };
        const bool buttonSwapped = ::GetSystemMetrics(SM_SWAPBUTTON);
        if (get(VK_LBUTTON)) {
            result |= (buttonSwapped ? MK_RBUTTON : MK_LBUTTON);
        }
        if (get(VK_RBUTTON)) {
            result |= (buttonSwapped ? MK_LBUTTON : MK_RBUTTON);
        }
        if (get(VK_SHIFT)) {
            result |= MK_SHIFT;
        }
        if (get(VK_CONTROL)) {
            result |= MK_CONTROL;
        }
        if (get(VK_MBUTTON)) {
            result |= MK_MBUTTON;
        }
        if (get(VK_XBUTTON1)) {
            result |= MK_XBUTTON1;
        }
        if (get(VK_XBUTTON2)) {
            result |= MK_XBUTTON2;
        }
        return result;
    }

    static void emulateClientAreaMessage(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam,
                                         const std::optional<int> &overrideMessage = std::nullopt) {
        const int myMsg = overrideMessage.value_or(message);
        const auto wParamNew = [myMsg, wParam]() -> WPARAM {
            if (myMsg == WM_NCMOUSELEAVE) {
                
                
                
                return kMessageTag.wParam;
            }
            const quint64 keyState = getKeyState();
            if ((myMsg >= WM_NCXBUTTONDOWN) && (myMsg <= WM_NCXBUTTONDBLCLK)) {
                const auto xButtonMask = GET_XBUTTON_WPARAM(wParam);
                return MAKEWPARAM(keyState, xButtonMask);
            }
            return keyState;
        }();
        const auto lParamNew = [myMsg, lParam, hWnd]() -> LPARAM {
            if (myMsg == WM_NCMOUSELEAVE) {
                
                return 0;
            }
            const auto screenPos = POINT{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            POINT clientPos = screenPos;
            ::ScreenToClient(hWnd, &clientPos);
            return MAKELPARAM(clientPos.x, clientPos.y);
        }();
#if 0
#  define SEND_MESSAGE ::SendMessageW
#else
#  define SEND_MESSAGE ::PostMessageW
#endif
        switch (myMsg) {
            case WM_NCHITTEST: 
            case WM_NCMOUSEMOVE:
                SEND_MESSAGE(hWnd, WM_MOUSEMOVE, wParamNew, lParamNew);
                break;
            case WM_NCLBUTTONDOWN:
                SEND_MESSAGE(hWnd, WM_LBUTTONDOWN, wParamNew, lParamNew);
                break;
            case WM_NCLBUTTONUP:
                SEND_MESSAGE(hWnd, WM_LBUTTONUP, wParamNew, lParamNew);
                break;
            case WM_NCLBUTTONDBLCLK:
                SEND_MESSAGE(hWnd, WM_LBUTTONDBLCLK, wParamNew, lParamNew);
                break;
            case WM_NCRBUTTONDOWN:
                SEND_MESSAGE(hWnd, WM_RBUTTONDOWN, wParamNew, lParamNew);
                break;
            case WM_NCRBUTTONUP:
                SEND_MESSAGE(hWnd, WM_RBUTTONUP, wParamNew, lParamNew);
                break;
            case WM_NCRBUTTONDBLCLK:
                SEND_MESSAGE(hWnd, WM_RBUTTONDBLCLK, wParamNew, lParamNew);
                break;
            case WM_NCMBUTTONDOWN:
                SEND_MESSAGE(hWnd, WM_MBUTTONDOWN, wParamNew, lParamNew);
                break;
            case WM_NCMBUTTONUP:
                SEND_MESSAGE(hWnd, WM_MBUTTONUP, wParamNew, lParamNew);
                break;
            case WM_NCMBUTTONDBLCLK:
                SEND_MESSAGE(hWnd, WM_MBUTTONDBLCLK, wParamNew, lParamNew);
                break;
            case WM_NCXBUTTONDOWN:
                SEND_MESSAGE(hWnd, WM_XBUTTONDOWN, wParamNew, lParamNew);
                break;
            case WM_NCXBUTTONUP:
                SEND_MESSAGE(hWnd, WM_XBUTTONUP, wParamNew, lParamNew);
                break;
            case WM_NCXBUTTONDBLCLK:
                SEND_MESSAGE(hWnd, WM_XBUTTONDBLCLK, wParamNew, lParamNew);
                break;
#if 0 
        case WM_NCPOINTERUPDATE:
        case WM_NCPOINTERDOWN:
        case WM_NCPOINTERUP:
            break;
#endif
            case WM_NCMOUSEHOVER:
                SEND_MESSAGE(hWnd, WM_MOUSEHOVER, wParamNew, lParamNew);
                break;
            case WM_NCMOUSELEAVE:
                SEND_MESSAGE(hWnd, WM_MOUSELEAVE, wParamNew, lParamNew);
                break;
            default:
                
                break;
        }

#undef SEND_MESSAGE
    }

    static inline void requestForMouseLeaveMessage(HWND hWnd, bool nonClient) {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        if (nonClient) {
            tme.dwFlags |= TME_NONCLIENT;
        }
        tme.hwndTrack = hWnd;
        tme.dwHoverTime = HOVER_DEFAULT;
        ::TrackMouseEvent(&tme);
    }

    bool Win32WindowContext::snapLayoutHandler(HWND hWnd, UINT message, WPARAM wParam,
                                               LPARAM lParam, LRESULT *result) {
        switch (message) {
            case WM_MOUSELEAVE: {
                if (wParam != kMessageTag.wParam) {
                    
                    
                    
                    
                    
                    
                    
                    
                    DWORD dwScreenPos = ::GetMessagePos();
                    POINT screenPoint{GET_X_LPARAM(dwScreenPos), GET_Y_LPARAM(dwScreenPos)};
                    ::ScreenToClient(hWnd, &screenPoint);
                    QPoint qtScenePos = QHighDpi::fromNativeLocalPosition(point2qpoint(screenPoint),
                                                                          m_windowHandle.data());
                    auto dummy = WindowAgentBase::Unknown;
                    if (isInSystemButtons(qtScenePos, &dummy)) {
                        
                        
                        
                        mouseLeaveBlocked = true;
                        *result = FALSE;
                        return true;
                    }
                }
                mouseLeaveBlocked = false;
                break;
            }

            case WM_MOUSEMOVE: {
                
                
                
                if (lastHitTestResult != WindowPart::ChromeButton && mouseLeaveBlocked) {
                    mouseLeaveBlocked = false;
                    requestForMouseLeaveMessage(hWnd, false);
                }
                break;
            }

            case WM_NCMOUSEMOVE:
            case WM_NCLBUTTONDOWN:
            case WM_NCLBUTTONUP:
            case WM_NCLBUTTONDBLCLK:
            case WM_NCRBUTTONDOWN:
            case WM_NCRBUTTONUP:
            case WM_NCRBUTTONDBLCLK:
            case WM_NCMBUTTONDOWN:
            case WM_NCMBUTTONUP:
            case WM_NCMBUTTONDBLCLK:
            case WM_NCXBUTTONDOWN:
            case WM_NCXBUTTONUP:
            case WM_NCXBUTTONDBLCLK:
#if 0 
    case WM_NCPOINTERUPDATE:
    case WM_NCPOINTERDOWN:
    case WM_NCPOINTERUP:
#endif
            case WM_NCMOUSEHOVER: {
                if (message == WM_NCMOUSEMOVE) {
                    if (lastHitTestResult != WindowPart::ChromeButton) {
                        
                        
                        
                        
                        
                        

                        
                        
                        
                        
                        

                        
                        
                        

                        
                        
                        

                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        
                        

                        
                        
                        
                        
                        
                        
                        

                        
                        
                        
                        

                        m_delegate->resetQtGrabbedControl(m_host);

                        
                        
                        if (mouseLeaveBlocked) {
                            emulateClientAreaMessage(hWnd, message, wParam, lParam,
                                                     WM_NCMOUSELEAVE);
                        }
                    }
                }

                if (lastHitTestResult == WindowPart::ChromeButton) {
                    if (message == WM_NCMOUSEMOVE) {
                        
                        
                        
                        
                        *result = ::DefWindowProcW(hWnd, WM_NCMOUSEMOVE, wParam, lParam);
                        emulateClientAreaMessage(hWnd, message, wParam, lParam);
                        return true;
                    }

                    if (lastHitTestResultRaw == HTSYSMENU) {
                        switch (message) {
                            case WM_NCLBUTTONDOWN:
                                if (iconButtonClickLevel == 0) {
                                    
                                    
                                    
                                    
                                    iconButtonClickTime = ::GetTickCount64();
                                    *result = ::DefWindowProcW(hWnd, message, wParam, lParam);
                                    iconButtonClickTime = 0;
                                    if (iconButtonClickLevel & IconButtonTriggersClose) {
                                        ::PostMessageW(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
                                    }
                                    if (iconButtonClickLevel & IconButtonDoubleClicked) {
                                        iconButtonClickLevel = 0;
                                    }
                                    
                                    
                                    
                                } else {
                                    iconButtonClickLevel = 0;
                                }
                                break;
                            case WM_NCLBUTTONDBLCLK:
                                
                                *result = ::DefWindowProcW(hWnd, message, wParam, lParam);
                                break;
                            default:
                                *result = FALSE;
                                emulateClientAreaMessage(hWnd, message, wParam, lParam);
                                break;
                        }
                    } else {
                        
                        
                        
                        *result =
                            (((message >= WM_NCXBUTTONDOWN) && (message <= WM_NCXBUTTONDBLCLK))
                                 ? TRUE
                                 : FALSE);
                        emulateClientAreaMessage(hWnd, message, wParam, lParam);
                    }
                    return true;
                }
                break;
            }

            case WM_NCMOUSELEAVE: {
                const WindowPart currentWindowPart = lastHitTestResult;
                if (currentWindowPart == WindowPart::ChromeButton) {
                    
                    
                    
                    
                    
                    if (mouseLeaveBlocked) {
                        mouseLeaveBlocked = false;
                        requestForMouseLeaveMessage(hWnd, false);
                    }
                } else {
                    if (mouseLeaveBlocked) {
                        
                        
                        emulateClientAreaMessage(hWnd, message, wParam, lParam, WM_NCMOUSELEAVE);
                    }

                    if (currentWindowPart == WindowPart::Outside) {
                        
                        
                        
                        

                        
                        m_delegate->resetQtGrabbedControl(m_host);
                    }
                }
                break;
            }

            default:
                break;
        }
        return false;
    }

    bool Win32WindowContext::customWindowHandler(HWND hWnd, UINT message, WPARAM wParam,
                                                 LPARAM lParam, LRESULT *result) {
        switch (message) {
            case WM_NCHITTEST: {
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                

                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                
                [[maybe_unused]] const auto &hitTestRecorder = qScopeGuard([this, result]() {
                    lastHitTestResultRaw = int(*result);
                    lastHitTestResult = getHitWindowPart(lastHitTestResultRaw);
                });

                POINT nativeGlobalPos{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
                POINT nativeLocalPos = nativeGlobalPos;
                ::ScreenToClient(hWnd, &nativeLocalPos);

                RECT clientRect{0, 0, 0, 0};
                ::GetClientRect(hWnd, &clientRect);
                auto clientWidth = RECT_WIDTH(clientRect);
                auto clientHeight = RECT_HEIGHT(clientRect);

                QPoint qtScenePos = QHighDpi::fromNativeLocalPosition(point2qpoint(nativeLocalPos),
                                                                      m_windowHandle.data());

                int frameSize = getResizeBorderThickness(hWnd);

                bool isFixedWidth = isHostWidthFixed();
                bool isFixedHeight = isHostHeightFixed();
                bool isFixedSize = isHostSizeFixed();
                bool isInLeftBorder = nativeLocalPos.x <= frameSize;
                bool isInTopBorder = nativeLocalPos.y <= frameSize;
                bool isInRightBorder = nativeLocalPos.x > clientWidth - frameSize;
                bool isInBottomBorder = nativeLocalPos.y > clientHeight - frameSize;
                bool isInTitleBar = isInTitleBarDraggableArea(qtScenePos);
                WindowAgentBase::SystemButton sysButtonType = WindowAgentBase::Unknown;
                bool isInCaptionButtons = isInSystemButtons(qtScenePos, &sysButtonType);
                static constexpr bool dontOverrideCursor = false; 

                if (isInCaptionButtons) {
                    
                    
                    *result = HTNOWHERE;
                    
                    
                    
                    
                    if (isWindowNoState(hWnd)) {
                        static constexpr const quint8 kBorderSize = 2;
                        bool isTop = nativeLocalPos.y <= kBorderSize;
                        bool isLeft = nativeLocalPos.x <= kBorderSize;
                        bool isRight = nativeLocalPos.x > clientWidth - kBorderSize;
                        if (isTop || isLeft || isRight) {
                            if (isFixedSize || dontOverrideCursor) {
                                
                                
                                
                                *result = isInTitleBar ? HTCAPTION : HTCLIENT;
                            } else {
                                if (isTop) {
                                    if (isLeft) {
                                        if (isFixedWidth) {
                                            *result = HTTOP;
                                        } else if (isFixedHeight) {
                                            *result = HTLEFT;
                                        } else {
                                            *result = HTTOPLEFT;
                                        }
                                    } else if (isRight) {
                                        if (isFixedWidth) {
                                            *result = HTTOP;
                                        } else if (isFixedHeight) {
                                            *result = HTRIGHT;
                                        } else {
                                            *result = HTTOPRIGHT;
                                        }
                                    } else {
                                        *result = isFixedHeight ? HTBORDER : HTTOP;
                                    }
                                } else {
                                    if (isFixedWidth) {
                                        *result = HTBORDER;
                                    } else {
                                        *result = isLeft ? HTLEFT : HTRIGHT;
                                    }
                                }
                            }
                        }
                    }
                    if (*result == HTNOWHERE) {
                        
                        
                        
                        switch (sysButtonType) {
                            case WindowAgentBase::WindowIcon:
                                *result = HTSYSMENU;
                                break;
                            case WindowAgentBase::Help:
                                *result = HTHELP;
                                break;
                            case WindowAgentBase::Minimize:
                                *result = HTREDUCE;
                                break;
                            case WindowAgentBase::Maximize:
                                *result = HTZOOM;
                                break;
                            case WindowAgentBase::Close:
                                *result = HTCLOSE;
                                break;
                            default:
                                
                                break;
                        }
                    }
                    if (*result == HTNOWHERE) {
                        
                        
                        
                        *result = HTCLIENT;
                    }
                    return true;
                }
                
                

                bool max = isMaximized(hWnd);
                bool full = isFullScreen(hWnd);

                if (isSystemBorderEnabled()) {
                    
                    
                    LRESULT originalHitTestResult = ::DefWindowProcW(hWnd, WM_NCHITTEST, 0, lParam);
                    if (originalHitTestResult != HTCLIENT) {
                        
                        
                        
                        
                        
                        *result = HTNOWHERE; 
                                             
                        if (originalHitTestResult == HTCAPTION) {
                        } else if (isFixedSize || dontOverrideCursor) {
                            *result = HTBORDER;
                        } else if (isFixedWidth || isFixedHeight) {
                            if (originalHitTestResult == HTTOPLEFT) {
                                if (isFixedWidth) {
                                    *result = HTTOP;
                                } else {
                                    *result = HTLEFT;
                                }
                            } else if (originalHitTestResult == HTTOPRIGHT) {
                                if (isFixedWidth) {
                                    *result = HTTOP;
                                } else {
                                    *result = HTRIGHT;
                                }
                            } else if (originalHitTestResult == HTBOTTOMRIGHT) {
                                if (isFixedWidth) {
                                    *result = HTBOTTOM;
                                } else {
                                    *result = HTRIGHT;
                                }
                            } else if (originalHitTestResult == HTBOTTOMLEFT) {
                                if (isFixedWidth) {
                                    *result = HTBOTTOM;
                                } else {
                                    *result = HTLEFT;
                                }
                            } else if (originalHitTestResult == HTLEFT ||
                                       originalHitTestResult == HTRIGHT) {
                                if (isFixedWidth) {
                                    *result = HTBORDER;
                                }
                            } else if (originalHitTestResult == HTTOP ||
                                       originalHitTestResult == HTBOTTOM) {
                                if (isFixedHeight) {
                                    *result = HTBORDER;
                                }
                            }
                        }
                        if (*result == HTNOWHERE) {
                            *result = originalHitTestResult;
                        }
                        return true;
                    }
                    if (full) {
                        *result = HTCLIENT;
                        return true;
                    }
                    if (max) {
                        *result = isInTitleBar ? HTCAPTION : HTCLIENT;
                        return true;
                    }
                    
                    
                    
                    
                    
                    if (isInTopBorder) {
                        
                        
                        
                        *result = [&]() {
                            if (isFixedSize || isFixedHeight || dontOverrideCursor ||
                                (isFixedWidth && (isInLeftBorder || isInRightBorder))) {
                                if (isInTitleBar) {
                                    return HTCAPTION;
                                } else {
                                    return HTCLIENT;
                                }
                            } else {
                                return HTTOP;
                            }
                        }();
                        return true;
                    }
                    if (isInTitleBar) {
                        *result = HTCAPTION;
                        return true;
                    }
                    *result = HTCLIENT;
                    return true;
                } else {
                    if (full) {
                        *result = HTCLIENT;
                        return true;
                    }
                    if (max || isFixedSize || dontOverrideCursor) {
                        *result = isInTitleBar ? HTCAPTION : HTCLIENT;
                        return true;
                    }
                    if (isFixedWidth || isFixedHeight) {
                        if (isInLeftBorder && isInTopBorder) {
                            if (isFixedWidth) {
                                *result = HTTOP;
                            } else {
                                *result = HTLEFT;
                            }
                        } else if (isInRightBorder && isInTopBorder) {
                            if (isFixedWidth) {
                                *result = HTTOP;
                            } else {
                                *result = HTRIGHT;
                            }
                        } else if (isInRightBorder && isInBottomBorder) {
                            if (isFixedWidth) {
                                *result = HTBOTTOM;
                            } else {
                                *result = HTRIGHT;
                            }
                        } else if (isInLeftBorder && isInBottomBorder) {
                            if (isFixedWidth) {
                                *result = HTBOTTOM;
                            } else {
                                *result = HTLEFT;
                            }
                        } else if (isInLeftBorder || isInRightBorder) {
                            if (isFixedWidth) {
                                *result = HTCLIENT;
                            } else {
                                *result = isInLeftBorder ? HTLEFT : HTRIGHT;
                            }
                        } else if (isInTopBorder || isInBottomBorder) {
                            if (isFixedHeight) {
                                *result = HTCLIENT;
                            } else {
                                *result = isInTopBorder ? HTTOP : HTBOTTOM;
                            }
                        } else {
                            *result = HTCLIENT;
                        }
                        return true;
                    } else {
                        if (isInTopBorder) {
                            if (isInLeftBorder) {
                                *result = HTTOPLEFT;
                                return true;
                            }
                            if (isInRightBorder) {
                                *result = HTTOPRIGHT;
                                return true;
                            }
                            *result = HTTOP;
                            return true;
                        }
                        if (isInBottomBorder) {
                            if (isInLeftBorder) {
                                *result = HTBOTTOMLEFT;
                                return true;
                            }
                            if (isInRightBorder) {
                                *result = HTBOTTOMRIGHT;
                                return true;
                            }
                            *result = HTBOTTOM;
                            return true;
                        }
                        if (isInLeftBorder) {
                            *result = HTLEFT;
                            return true;
                        }
                        if (isInRightBorder) {
                            *result = HTRIGHT;
                            return true;
                        }
                    }
                    if (isInTitleBar) {
                        *result = HTCAPTION;
                        return true;
                    }
                    *result = HTCLIENT;
                    return true;
                }
            }

            case WM_WINDOWPOSCHANGING: {
                
                
                
                
                
                
                
                static constexpr const auto kBadWindowPosFlag =
                    SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED;
                const auto windowPos = reinterpret_cast<LPWINDOWPOS>(lParam);
                if (windowPos->flags == kBadWindowPosFlag) {
                    windowPos->flags |= SWP_NOCOPYBITS;
                }
                break;
            }

            case WM_SHOWWINDOW: {
                if (!wParam || !isWindowNoState(hWnd) || isFullScreen(hWnd)) {
                    break;
                }
                RECT windowRect{};
                ::GetWindowRect(hWnd, &windowRect);
                static constexpr const auto swpFlags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE |
                                                       SWP_FRAMECHANGED | SWP_NOOWNERZORDER;
                ::SetWindowPos(hWnd, nullptr, 0, 0, RECT_WIDTH(windowRect) + 1,
                               RECT_HEIGHT(windowRect) + 1, swpFlags);
                ::SetWindowPos(hWnd, nullptr, 0, 0, RECT_WIDTH(windowRect), RECT_HEIGHT(windowRect),
                               swpFlags);
                break;
            }

            default:
                break;
        }

        if (!isSystemBorderEnabled()) {
            switch (message) {
                case WM_NCUAHDRAWCAPTION:
                case WM_NCUAHDRAWFRAME: {
                    
                    
                    
                    *result = FALSE;
                    return true;
                }
                case WM_NCPAINT: {
                    

                    if (!isDwmCompositionEnabled()) {
                        
                        
                        
                        *result = FALSE;
                        return true;
                    } else {
                        break;
                    }
                }
                case WM_NCACTIVATE: {
                    if (isDwmCompositionEnabled()) {
                        
                        
                        
                        
                        
                        *result = ::DefWindowProcW(hWnd, WM_NCACTIVATE, wParam, -1);
                    } else {
                        *result = TRUE;
                    }
                    return true;
                }
                case WM_SETICON:
                case WM_SETTEXT: {
                    
                    
                    const auto oldStyle = static_cast<DWORD>(::GetWindowLongPtrW(hWnd, GWL_STYLE));
                    
                    
                    const DWORD newStyle = (oldStyle & ~WS_VISIBLE);
                    ::SetWindowLongPtrW(hWnd, GWL_STYLE, static_cast<LONG_PTR>(newStyle));
                    triggerFrameChange(hWnd);
                    const LRESULT originalResult = ::DefWindowProcW(hWnd, message, wParam, lParam);
                    ::SetWindowLongPtrW(hWnd, GWL_STYLE, static_cast<LONG_PTR>(oldStyle));
                    triggerFrameChange(hWnd);
                    *result = originalResult;
                    return true;
                }
                default:
                    break;
            }
        }
        return false;
    }

    bool Win32WindowContext::nonClientCalcSizeHandler(HWND hWnd, UINT message, WPARAM wParam,
                                                      LPARAM lParam, LRESULT *result) {
        Q_UNUSED(message)
        Q_UNUSED(this)

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        

        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        

        const auto clientRect = wParam ? &(reinterpret_cast<LPNCCALCSIZE_PARAMS>(lParam))->rgrc[0]
                                       : reinterpret_cast<LPRECT>(lParam);
        [[maybe_unused]] const auto &flickerReducer = qScopeGuard([this]() {
            
            
            
            
            
            const auto &isTargetSurface = [](const QSurface::SurfaceType st) {
                return st != QSurface::RasterSurface && st != QSurface::OpenGLSurface &&
                       st != QSurface::RasterGLSurface && st != QSurface::OpenVGSurface;
            };
            if (m_windowHandle && isTargetSurface(m_windowHandle->surfaceType()) &&
                isDwmCompositionEnabled() && DynamicApis::instance().pDwmFlush) {
                DynamicApis::instance().pDwmFlush();
            }
        });
        if (isSystemBorderEnabled()) {
            
            
            const LONG originalTop = clientRect->top;
            
            
            
            
            
            
            
            
            const LRESULT originalResult = ::DefWindowProcW(hWnd, WM_NCCALCSIZE, wParam, lParam);
            if (originalResult != 0) {
                *result = originalResult;
                return true;
            }
            
            
            
            
            
            
            
            
            clientRect->top = originalTop;
        }

        const bool max = isMaximized(hWnd);
        const bool full = isFullScreen(hWnd);
        
        
        
        if (max && !full) {
            
            
            
            
            
            
            const quint32 frameSize = getResizeBorderThickness(hWnd);
            clientRect->top += frameSize;
            if (!isSystemBorderEnabled()) {
                clientRect->bottom -= frameSize;
                clientRect->left += frameSize;
                clientRect->right -= frameSize;
            }
        }
        
        
        
        
        
        
        if (max || full) {
            APPBARDATA abd{};
            abd.cbSize = sizeof(abd);
            const UINT taskbarState = ::SHAppBarMessage(ABM_GETSTATE, &abd);
            
            if (taskbarState & ABS_AUTOHIDE) {
                bool top = false, bottom = false, left = false, right = false;
                
                
                
                if (isWin8Point1OrGreater()) {
                    const RECT monitorRect = getMonitorForWindow(hWnd).rcMonitor;
                    
                    
                    
                    const auto hasAutohideTaskbar = [monitorRect](const UINT edge) -> bool {
                        APPBARDATA abd2{};
                        abd2.cbSize = sizeof(abd2);
                        abd2.uEdge = edge;
                        abd2.rc = monitorRect;
                        const auto hTaskbar =
                            reinterpret_cast<HWND>(::SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &abd2));
                        return (hTaskbar != nullptr);
                    };
                    top = hasAutohideTaskbar(ABE_TOP);
                    bottom = hasAutohideTaskbar(ABE_BOTTOM);
                    left = hasAutohideTaskbar(ABE_LEFT);
                    right = hasAutohideTaskbar(ABE_RIGHT);
                } else {
                    int edge = -1;
                    APPBARDATA abd2{};
                    abd2.cbSize = sizeof(abd2);
                    abd2.hWnd = ::FindWindowW(L"Shell_TrayWnd", nullptr);
                    HMONITOR windowMonitor = ::MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
                    HMONITOR taskbarMonitor =
                        ::MonitorFromWindow(abd2.hWnd, MONITOR_DEFAULTTOPRIMARY);
                    if (taskbarMonitor == windowMonitor) {
                        ::SHAppBarMessage(ABM_GETTASKBARPOS, &abd2);
                        edge = int(abd2.uEdge);
                    }
                    top = (edge == ABE_TOP);
                    bottom = (edge == ABE_BOTTOM);
                    left = (edge == ABE_LEFT);
                    right = (edge == ABE_RIGHT);
                }
                
                
                
                
                
                
                
                
                
                
                if (top) {
                    
                    clientRect->top += kAutoHideTaskBarThickness;
                } else if (bottom) {
                    clientRect->bottom -= kAutoHideTaskBarThickness;
                } else if (left) {
                    clientRect->left += kAutoHideTaskBarThickness;
                } else if (right) {
                    clientRect->right -= kAutoHideTaskBarThickness;
                }
            }
        }
        
        
        
        
        
        
        
        
        
        

        
        
        *result = FALSE;
        return true;
    }

    bool Win32WindowContext::systemMenuHandler(HWND hWnd, UINT message, WPARAM wParam,
                                               LPARAM lParam, LRESULT *result) {
        const auto getNativePosFromMouse = [lParam]() -> POINT {
            return {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        };
        const auto getNativeGlobalPosFromKeyboard = [hWnd]() -> POINT {
            const bool maxOrFull = isMaximized(hWnd) || isFullScreen(hWnd);
            const quint32 frameSize = getResizeBorderThickness(hWnd);
            const quint32 horizontalOffset =
                ((maxOrFull || !isSystemBorderEnabled()) ? 0 : frameSize);
            const auto verticalOffset = [hWnd, maxOrFull, frameSize]() -> quint32 {
                const quint32 titleBarHeight = getTitleBarHeight(hWnd);
                if (!isSystemBorderEnabled()) {
                    return titleBarHeight;
                }
                if (isWin11OrGreater()) {
                    if (maxOrFull) {
                        return (titleBarHeight + frameSize);
                    }
                    return titleBarHeight;
                }
                if (maxOrFull) {
                    return titleBarHeight;
                }
                return titleBarHeight - frameSize;
            }();
            RECT windowPos{};
            ::GetWindowRect(hWnd, &windowPos);
            return {static_cast<LONG>(windowPos.left + horizontalOffset),
                    static_cast<LONG>(windowPos.top + verticalOffset)};
        };
        bool shouldShowSystemMenu = false;
        bool broughtByKeyboard = false;
        POINT nativeGlobalPos{};

        switch (message) {
            case WM_RBUTTONUP: {
                const POINT nativeLocalPos = getNativePosFromMouse();
                const QPoint qtScenePos = QHighDpi::fromNativeLocalPosition(
                    point2qpoint(nativeLocalPos), m_windowHandle.data());
                WindowAgentBase::SystemButton sysButtonType = WindowAgentBase::Unknown;
                if (isInTitleBarDraggableArea(qtScenePos) ||
                    (isInSystemButtons(qtScenePos, &sysButtonType) &&
                     sysButtonType == WindowAgentBase::WindowIcon)) {
                    shouldShowSystemMenu = true;
                    nativeGlobalPos = nativeLocalPos;
                    ::ClientToScreen(hWnd, &nativeGlobalPos);
                }
                break;
            }
            case WM_NCRBUTTONUP: {
                if (wParam == HTCAPTION) {
                    shouldShowSystemMenu = true;
                    nativeGlobalPos = getNativePosFromMouse();
                }
                break;
            }
            case WM_SYSCOMMAND: {
                const WPARAM filteredWParam = (wParam & 0xFFF0);
                switch (filteredWParam) {
                    case SC_MOUSEMENU:
                        shouldShowSystemMenu = true;
                        nativeGlobalPos = getNativeGlobalPosFromKeyboard();
                        break;
                    case SC_KEYMENU:
                        if (lParam == VK_SPACE) {
                            shouldShowSystemMenu = true;
                            broughtByKeyboard = true;
                            nativeGlobalPos = getNativeGlobalPosFromKeyboard();
                        }
                        break;
                    default:
                        break;
                }
                break;
            }
            case WM_KEYDOWN:
            case WM_SYSKEYDOWN: {
                const bool altPressed = ((wParam == VK_MENU) || (::GetKeyState(VK_MENU) < 0));
                const bool spacePressed = ((wParam == VK_SPACE) || (::GetKeyState(VK_SPACE) < 0));
                if (altPressed && spacePressed) {
                    shouldShowSystemMenu = true;
                    broughtByKeyboard = true;
                    nativeGlobalPos = getNativeGlobalPosFromKeyboard();
                }
                break;
            }
            default:
                break;
        }
        if (shouldShowSystemMenu) {
            static HHOOK mouseHook = nullptr;
            static std::optional<POINT> mouseClickPos;
            static bool mouseDoubleClicked = false;
            bool mouseHookedLocal = false;

            
            if (iconButtonClickTime > 0) {
                POINT menuPos{0, static_cast<LONG>(getTitleBarHeight(hWnd))};
                if (const auto tb = titleBar()) {
                    auto titleBarHeight = qreal(m_delegate->mapGeometryToScene(tb).height());
                    titleBarHeight *= m_windowHandle->devicePixelRatio();
                    menuPos.y = qRound(titleBarHeight);
                }
                ::ClientToScreen(hWnd, &menuPos);
                nativeGlobalPos = menuPos;

                
                if (!mouseHook) {
                    mouseHook = ::SetWindowsHookExW(
                        WH_MOUSE,
                        [](int nCode, WPARAM wParam, LPARAM lParam) -> LRESULT {
                            if (nCode >= 0) {
                                switch (wParam) {
                                    case WM_LBUTTONDBLCLK:
                                        mouseDoubleClicked = true;
                                        Q_FALLTHROUGH();

                                        

                                    case WM_LBUTTONDOWN: {
                                        auto pMouseStruct =
                                            reinterpret_cast<MOUSEHOOKSTRUCT *>(lParam);
                                        if (pMouseStruct) {
                                            mouseClickPos = pMouseStruct->pt;
                                        }
                                        break;
                                    }
                                    default:
                                        break;
                                }
                            }
                            return ::CallNextHookEx(nullptr, nCode, wParam, lParam);
                        },
                        nullptr, ::GetCurrentThreadId());
                    mouseHookedLocal = true;
                }
            }

            bool res =
                showSystemMenu_sys(hWnd, nativeGlobalPos, broughtByKeyboard, isHostSizeFixed());

            
            if (mouseHookedLocal) {
                ::UnhookWindowsHookEx(mouseHook);

                
                if (!res && mouseClickPos.has_value()) {
                    POINT nativeLocalPos = mouseClickPos.value();
                    ::ScreenToClient(hWnd, &nativeLocalPos);
                    QPoint qtScenePos = QHighDpi::fromNativeLocalPosition(
                        point2qpoint(nativeLocalPos), m_windowHandle.data());
                    WindowAgentBase::SystemButton sysButtonType = WindowAgentBase::Unknown;
                    if (isInSystemButtons(qtScenePos, &sysButtonType) &&
                        sysButtonType == WindowAgentBase::WindowIcon) {
                        iconButtonClickLevel |= IconButtonClicked;
                        if (::GetTickCount64() - iconButtonClickTime <= ::GetDoubleClickTime()) {
                            iconButtonClickLevel |= IconButtonTriggersClose;
                        }
                    }
                }

                if (mouseDoubleClicked) {
                    iconButtonClickLevel |= IconButtonDoubleClicked;
                }

                mouseHook = nullptr;
                mouseClickPos.reset();
                mouseDoubleClicked = false;
            }

            
            
            
            
            *result = FALSE;
            return true;
        }
        return false;
    }

}
