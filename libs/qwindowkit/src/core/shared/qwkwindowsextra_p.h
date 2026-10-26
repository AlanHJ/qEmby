// Copyright (C) 2023-2024 Stdware Collections (https://www.github.com/stdware)
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#ifndef QWKWINDOWSEXTRA_P_H
#define QWKWINDOWSEXTRA_P_H










#include <QWKCore/qwindowkit_windows.h>

#include <QtCore/QtMath>
#include <QtGui/QGuiApplication>
#include <QtGui/QStyleHints>
#include <QtGui/QPalette>

#include <QtCore/private/qsystemlibrary_p.h>



typedef struct _MARGINS
{
    int cxLeftWidth;
    int cxRightWidth;
    int cyTopHeight;
    int cyBottomHeight;
} MARGINS, *PMARGINS;

typedef enum MONITOR_DPI_TYPE {
    MDT_EFFECTIVE_DPI = 0,
    MDT_ANGULAR_DPI = 1,
    MDT_RAW_DPI = 2,
    MDT_DEFAULT = MDT_EFFECTIVE_DPI
} MONITOR_DPI_TYPE;

typedef struct _DWM_BLURBEHIND
{
    DWORD dwFlags;
    BOOL fEnable;
    HRGN hRgnBlur;
    BOOL fTransitionOnMaximized;
} DWM_BLURBEHIND, *PDWM_BLURBEHIND;

extern "C" {
    UINT    WINAPI GetDpiForWindow(HWND);
    int     WINAPI GetSystemMetricsForDpi(int, UINT);
    BOOL    WINAPI AdjustWindowRectExForDpi(LPRECT, DWORD, BOOL, DWORD, UINT);
    HRESULT WINAPI GetDpiForMonitor(HMONITOR, MONITOR_DPI_TYPE, UINT *, UINT *);
    HRESULT WINAPI DwmFlush();
    HRESULT WINAPI DwmIsCompositionEnabled(BOOL*);
    HRESULT WINAPI DwmGetWindowAttribute(HWND, DWORD, PVOID, DWORD);
    HRESULT WINAPI DwmSetWindowAttribute(HWND, DWORD, LPCVOID, DWORD);
    HRESULT WINAPI DwmExtendFrameIntoClientArea(HWND, const MARGINS*);
    HRESULT WINAPI DwmEnableBlurBehindWindow(HWND, const DWM_BLURBEHIND*);
} 

namespace QWK {

    enum _DWMWINDOWATTRIBUTE {
        
        _DWMWA_USE_HOSTBACKDROPBRUSH = 17,

        
        
        _DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 = 19,

        
        
        _DWMWA_USE_IMMERSIVE_DARK_MODE = 20,

        
        _DWMWA_WINDOW_CORNER_PREFERENCE = 33,

        
        _DWMWA_VISIBLE_FRAME_BORDER_THICKNESS = 37,

        
        
        _DWMWA_SYSTEMBACKDROP_TYPE = 38,

        
        
        _DWMWA_MICA_EFFECT = 1029
    };

    
    enum _DWM_SYSTEMBACKDROP_TYPE {
        _DWMSBT_AUTO, 
                      
        _DWMSBT_NONE, 
        _DWMSBT_MAINWINDOW,      
                                 
        _DWMSBT_TRANSIENTWINDOW, 
                                 
        _DWMSBT_TABBEDWINDOW,    
                                 
    };

    enum WINDOWCOMPOSITIONATTRIB {
        WCA_UNDEFINED = 0,
        WCA_NCRENDERING_ENABLED = 1,
        WCA_NCRENDERING_POLICY = 2,
        WCA_TRANSITIONS_FORCEDISABLED = 3,
        WCA_ALLOW_NCPAINT = 4,
        WCA_CAPTION_BUTTON_BOUNDS = 5,
        WCA_NONCLIENT_RTL_LAYOUT = 6,
        WCA_FORCE_ICONIC_REPRESENTATION = 7,
        WCA_EXTENDED_FRAME_BOUNDS = 8,
        WCA_HAS_ICONIC_BITMAP = 9,
        WCA_THEME_ATTRIBUTES = 10,
        WCA_NCRENDERING_EXILED = 11,
        WCA_NCADORNMENTINFO = 12,
        WCA_EXCLUDED_FROM_LIVEPREVIEW = 13,
        WCA_VIDEO_OVERLAY_ACTIVE = 14,
        WCA_FORCE_ACTIVEWINDOW_APPEARANCE = 15,
        WCA_DISALLOW_PEEK = 16,
        WCA_CLOAK = 17,
        WCA_CLOAKED = 18,
        WCA_ACCENT_POLICY = 19,
        WCA_FREEZE_REPRESENTATION = 20,
        WCA_EVER_UNCLOAKED = 21,
        WCA_VISUAL_OWNER = 22,
        WCA_HOLOGRAPHIC = 23,
        WCA_EXCLUDED_FROM_DDA = 24,
        WCA_PASSIVEUPDATEMODE = 25,
        WCA_USEDARKMODECOLORS = 26,
        WCA_CORNER_STYLE = 27,
        WCA_PART_COLOR = 28,
        WCA_DISABLE_MOVESIZE_FEEDBACK = 29,
        WCA_LAST = 30
    };

    enum ACCENT_STATE {
        ACCENT_DISABLED = 0,
        ACCENT_ENABLE_GRADIENT = 1,
        ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
        ACCENT_ENABLE_BLURBEHIND = 3,        
        ACCENT_ENABLE_ACRYLICBLURBEHIND = 4, 
        ACCENT_ENABLE_HOST_BACKDROP = 5,     
        ACCENT_INVALID_STATE = 6             
    };

    enum ACCENT_FLAG {
        ACCENT_NONE = 0,
        ACCENT_ENABLE_ACRYLIC = 1,
        ACCENT_ENABLE_ACRYLIC_WITH_LUMINOSITY = 482
    };

    struct ACCENT_POLICY {
        DWORD dwAccentState;
        DWORD dwAccentFlags;
        DWORD dwGradientColor; 
        DWORD dwAnimationId;
    };
    using PACCENT_POLICY = ACCENT_POLICY *;

    struct WINDOWCOMPOSITIONATTRIBDATA {
        WINDOWCOMPOSITIONATTRIB Attrib;
        PVOID pvData;
        SIZE_T cbData;
    };
    using PWINDOWCOMPOSITIONATTRIBDATA = WINDOWCOMPOSITIONATTRIBDATA *;

    enum PREFERRED_APP_MODE {
        PAM_DEFAULT = 0, 
                         
        PAM_AUTO =
            1, 
        PAM_DARK = 2, 
        PAM_LIGHT =
            3, 
        PAM_MAX = 4
    };

    using SetWindowCompositionAttributePtr = BOOL(WINAPI *)(HWND, PWINDOWCOMPOSITIONATTRIBDATA);

    
    using RefreshImmersiveColorPolicyStatePtr = VOID(WINAPI *)(VOID); 
    using AllowDarkModeForWindowPtr = BOOL(WINAPI *)(HWND, BOOL);     
    using AllowDarkModeForAppPtr = BOOL(WINAPI *)(BOOL);              
    using FlushMenuThemesPtr = VOID(WINAPI *)(VOID);                  
    
    using SetPreferredAppModePtr = PREFERRED_APP_MODE(WINAPI *)(PREFERRED_APP_MODE); 

    namespace {

        struct DynamicApis {
            static inline const DynamicApis &instance() {
                static const DynamicApis inst;
                return inst;
            }

#define DYNAMIC_API_DECLARE(NAME) decltype(&::NAME) p##NAME = nullptr

            DYNAMIC_API_DECLARE(DwmFlush);
            DYNAMIC_API_DECLARE(DwmIsCompositionEnabled);
            DYNAMIC_API_DECLARE(DwmGetWindowAttribute);
            DYNAMIC_API_DECLARE(DwmSetWindowAttribute);
            DYNAMIC_API_DECLARE(DwmExtendFrameIntoClientArea);
            DYNAMIC_API_DECLARE(DwmEnableBlurBehindWindow);
            DYNAMIC_API_DECLARE(GetDpiForWindow);
            DYNAMIC_API_DECLARE(GetSystemMetricsForDpi);
            DYNAMIC_API_DECLARE(AdjustWindowRectExForDpi);
            DYNAMIC_API_DECLARE(GetDpiForMonitor);

#undef DYNAMIC_API_DECLARE

            SetWindowCompositionAttributePtr pSetWindowCompositionAttribute = nullptr;
            RefreshImmersiveColorPolicyStatePtr pRefreshImmersiveColorPolicyState = nullptr;
            AllowDarkModeForWindowPtr pAllowDarkModeForWindow = nullptr;
            AllowDarkModeForAppPtr pAllowDarkModeForApp = nullptr;
            FlushMenuThemesPtr pFlushMenuThemes = nullptr;
            SetPreferredAppModePtr pSetPreferredAppMode = nullptr;

        private:
            inline DynamicApis() {
#define DYNAMIC_API_RESOLVE(DLL, NAME)                                                             \
    p##NAME = reinterpret_cast<decltype(p##NAME)>(DLL.resolve(#NAME))

                QSystemLibrary user32(QStringLiteral("user32"));
                DYNAMIC_API_RESOLVE(user32, GetDpiForWindow);
                DYNAMIC_API_RESOLVE(user32, GetSystemMetricsForDpi);
                DYNAMIC_API_RESOLVE(user32, SetWindowCompositionAttribute);
                DYNAMIC_API_RESOLVE(user32, AdjustWindowRectExForDpi);

                QSystemLibrary shcore(QStringLiteral("shcore"));
                DYNAMIC_API_RESOLVE(shcore, GetDpiForMonitor);

                QSystemLibrary dwmapi(QStringLiteral("dwmapi"));
                DYNAMIC_API_RESOLVE(dwmapi, DwmFlush);
                DYNAMIC_API_RESOLVE(dwmapi, DwmIsCompositionEnabled);
                DYNAMIC_API_RESOLVE(dwmapi, DwmGetWindowAttribute);
                DYNAMIC_API_RESOLVE(dwmapi, DwmSetWindowAttribute);
                DYNAMIC_API_RESOLVE(dwmapi, DwmExtendFrameIntoClientArea);
                DYNAMIC_API_RESOLVE(dwmapi, DwmEnableBlurBehindWindow);

#undef DYNAMIC_API_RESOLVE

#define UNDOC_API_RESOLVE(DLL, NAME, ORDINAL)                                                      \
    p##NAME = reinterpret_cast<decltype(p##NAME)>(DLL.resolve(MAKEINTRESOURCEA(ORDINAL)))

                QSystemLibrary uxtheme(QStringLiteral("uxtheme"));
                UNDOC_API_RESOLVE(uxtheme, RefreshImmersiveColorPolicyState, 104);
                UNDOC_API_RESOLVE(uxtheme, AllowDarkModeForWindow, 133);
                UNDOC_API_RESOLVE(uxtheme, AllowDarkModeForApp, 135);
                UNDOC_API_RESOLVE(uxtheme, FlushMenuThemes, 136);
                UNDOC_API_RESOLVE(uxtheme, SetPreferredAppMode, 135);

#undef UNDOC_API_RESOLVE
            }

            inline ~DynamicApis() = default;

            Q_DISABLE_COPY(DynamicApis)
        };

    }

    inline constexpr bool operator==(const POINT &lhs, const POINT &rhs) noexcept {
        return ((lhs.x == rhs.x) && (lhs.y == rhs.y));
    }

    inline constexpr bool operator!=(const POINT &lhs, const POINT &rhs) noexcept {
        return !operator==(lhs, rhs);
    }

    inline constexpr bool operator==(const SIZE &lhs, const SIZE &rhs) noexcept {
        return ((lhs.cx == rhs.cx) && (lhs.cy == rhs.cy));
    }

    inline constexpr bool operator!=(const SIZE &lhs, const SIZE &rhs) noexcept {
        return !operator==(lhs, rhs);
    }

    inline constexpr bool operator>(const SIZE &lhs, const SIZE &rhs) noexcept {
        return ((lhs.cx * lhs.cy) > (rhs.cx * rhs.cy));
    }

    inline constexpr bool operator>=(const SIZE &lhs, const SIZE &rhs) noexcept {
        return (operator>(lhs, rhs) || operator==(lhs, rhs));
    }

    inline constexpr bool operator<(const SIZE &lhs, const SIZE &rhs) noexcept {
        return (operator!=(lhs, rhs) && !operator>(lhs, rhs));
    }

    inline constexpr bool operator<=(const SIZE &lhs, const SIZE &rhs) noexcept {
        return (operator<(lhs, rhs) || operator==(lhs, rhs));
    }

    inline constexpr bool operator==(const RECT &lhs, const RECT &rhs) noexcept {
        return ((lhs.left == rhs.left) && (lhs.top == rhs.top) && (lhs.right == rhs.right) &&
                (lhs.bottom == rhs.bottom));
    }

    inline constexpr bool operator!=(const RECT &lhs, const RECT &rhs) noexcept {
        return !operator==(lhs, rhs);
    }

    inline constexpr QPoint point2qpoint(const POINT &point) {
        return QPoint{int(point.x), int(point.y)};
    }

    inline constexpr POINT qpoint2point(const QPoint &point) {
        return POINT{LONG(point.x()), LONG(point.y())};
    }

    inline constexpr QSize size2qsize(const SIZE &size) {
        return QSize{int(size.cx), int(size.cy)};
    }

    inline constexpr SIZE qsize2size(const QSize &size) {
        return SIZE{LONG(size.width()), LONG(size.height())};
    }

    inline constexpr QRect rect2qrect(const RECT &rect) {
        return QRect{
            QPoint{int(rect.left),        int(rect.top)         },
            QSize{int(RECT_WIDTH(rect)), int(RECT_HEIGHT(rect))}
        };
    }

    inline constexpr RECT qrect2rect(const QRect &qrect) {
        return RECT{LONG(qrect.left()), LONG(qrect.top()), LONG(qrect.right()),
                    LONG(qrect.bottom())};
    }

    inline constexpr QMargins margins2qmargins(const MARGINS &margins) {
        return {margins.cxLeftWidth, margins.cyTopHeight, margins.cxRightWidth,
                margins.cyBottomHeight};
    }

    inline constexpr MARGINS qmargins2margins(const QMargins &qmargins) {
        return {qmargins.left(), qmargins.right(), qmargins.top(), qmargins.bottom()};
    }

    inline   QString hwnd2str(const WId windowId) {
        
        return QLatin1String("0x") +
               QString::number(windowId, 16).toUpper().rightJustified(8, u'0');
    }

    inline   QString hwnd2str(HWND hwnd) {
        
        return hwnd2str(reinterpret_cast<WId>(hwnd));
    }

    inline bool isDwmCompositionEnabled() {
        if (isWin8OrGreater()) {
            return true;
        }
        const DynamicApis &apis = DynamicApis::instance();
        if (!apis.pDwmIsCompositionEnabled) {
            return false;
        }
        BOOL enabled = FALSE;
        return SUCCEEDED(apis.pDwmIsCompositionEnabled(&enabled)) && enabled;
    }

    inline bool isWindowFrameBorderColorized() {
        WindowsRegistryKey registry(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\DWM)");
        if (!registry.isValid()) {
            return false;
        }
        auto value = registry.dwordValue(L"ColorPrevalence");
        if (!value.second) {
            return false;
        }
        return value.first;
    }

    inline bool isHighContrastModeEnabled() {
        HIGHCONTRASTW hc{};
        hc.cbSize = sizeof(hc);
        ::SystemParametersInfoW(SPI_GETHIGHCONTRAST, sizeof(hc), &hc, FALSE);
        return (hc.dwFlags & HCF_HIGHCONTRASTON);
    }

    inline bool isDarkThemeActive() {
        if (!isWin101809OrGreater()) {
            return false;
        }
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
        return QGuiApplication::styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
        WindowsRegistryKey registry(
            HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\CurrentVersion\Themes\Personalize)");
        if (!registry.isValid()) {
            return false;
        }
        auto value = registry.dwordValue(L"AppsUseLightTheme");
        if (!value.second) {
            return false;
        }
        return !value.first;
#endif
    }

    inline bool isDarkWindowFrameEnabled(HWND hwnd) {
        if (!isWin101809OrGreater()) {
            return false;
        }
        BOOL enabled = FALSE;
        const DynamicApis &apis = DynamicApis::instance();
        const auto attr = isWin1020H1OrGreater() ? _DWMWA_USE_IMMERSIVE_DARK_MODE
                                                 : _DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1;
        return SUCCEEDED(apis.pDwmGetWindowAttribute(hwnd, attr, &enabled, sizeof(enabled))) &&
               enabled;
    }

    inline QColor getAccentColor() {
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
        return QGuiApplication::palette().color(QPalette::Accent);
#else
        WindowsRegistryKey registry(HKEY_CURRENT_USER, LR"(Software\Microsoft\Windows\DWM)");
        if (!registry.isValid()) {
            return {};
        }
        auto value = registry.dwordValue(L"AccentColor");
        if (!value.second) {
            return {};
        }
        
        
        QColor color = QColor::fromRgba(value.first);
        if (!color.isValid()) {
            return {};
        }
        return QColor::fromRgb(color.blue(), color.green(), color.red(), color.alpha());
#endif
    }

    inline quint32 getDpiForWindow(HWND hwnd) {
        const DynamicApis &apis = DynamicApis::instance();
        if (apis.pGetDpiForWindow) { 
            return apis.pGetDpiForWindow(hwnd);
        } else if (apis.pGetDpiForMonitor) { 
            HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            UINT dpiX{0};
            UINT dpiY{0};
            apis.pGetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY);
            return dpiX;
        } else { 
            HDC hdc = ::GetDC(nullptr);
            const int dpiX = ::GetDeviceCaps(hdc, LOGPIXELSX);
            
            ::ReleaseDC(nullptr, hdc);
            return quint32(dpiX);
        }
    }

    inline quint32 getSystemMetricsForDpi(int index, quint32 dpi) {
        const DynamicApis &apis = DynamicApis::instance();
        if (apis.pGetSystemMetricsForDpi) {
            return apis.pGetSystemMetricsForDpi(index, dpi);
        }
        const int result = ::GetSystemMetrics(index);
        
        if (dpi != USER_DEFAULT_SCREEN_DPI) {
            return result;
        }
        const qreal dpr = qreal(dpi) / qreal(USER_DEFAULT_SCREEN_DPI);
        
        return qFloor(qreal(result) / dpr);
    }

    inline quint32 getWindowFrameBorderThickness(HWND hwnd) {
        const DynamicApis &apis = DynamicApis::instance();
        if (isWin11OrGreater()) {
            UINT result = 0;
            if (SUCCEEDED(apis.pDwmGetWindowAttribute(hwnd, _DWMWA_VISIBLE_FRAME_BORDER_THICKNESS,
                                                      &result, sizeof(result)))) {
                return result;
            }
        }
        if (isWin10OrGreater()) {
            const quint32 dpi = getDpiForWindow(hwnd);
            
            return getSystemMetricsForDpi(SM_CXBORDER, dpi);
        }
        
        return 0;
    }

    inline quint32 getResizeBorderThickness(HWND hwnd) {
        const quint32 dpi = getDpiForWindow(hwnd);
        
        
        
        return getSystemMetricsForDpi(SM_CXSIZEFRAME, dpi) +
               getSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    }

    inline quint32 getTitleBarHeight(HWND hwnd) {
        const quint32 dpi = getDpiForWindow(hwnd);
        
        
        
        return getSystemMetricsForDpi(SM_CYCAPTION, dpi) +
               getSystemMetricsForDpi(SM_CYSIZEFRAME, dpi) +
               getSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    }

}

#endif 
