#include "windowutils.h"

#include <QDebug>
#include <QWidget>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace WindowUtils {

void syncNativeMaximizeCapability(QWidget *window, bool enabled)
{
#ifdef Q_OS_WIN
    
    
    
    if (!window || !window->isWindow() || window->isFullScreen() || !window->internalWinId())
        return;

    const auto handle = reinterpret_cast<HWND>(window->internalWinId());
    const LONG_PTR oldStyle = GetWindowLongPtrW(handle, GWL_STYLE);
    const bool allowMaximize = enabled &&
        window->windowFlags().testFlag(Qt::WindowMaximizeButtonHint) &&
        !window->windowFlags().testFlag(Qt::MSWindowsFixedSizeDialogHint) &&
        window->minimumWidth() < window->maximumWidth() &&
        window->minimumHeight() < window->maximumHeight();
    const LONG_PTR newStyle = allowMaximize
        ? oldStyle | WS_MAXIMIZEBOX : oldStyle & ~LONG_PTR(WS_MAXIMIZEBOX);
    if (oldStyle == newStyle)
        return;

    
    
    
    SetLastError(ERROR_SUCCESS);
    if (!SetWindowLongPtrW(handle, GWL_STYLE, newStyle) && GetLastError() != ERROR_SUCCESS) {
        qWarning() << "[WindowUtils] Failed to update maximize style | error:" << GetLastError();
        return;
    }
    if (!SetWindowPos(handle, nullptr, 0, 0, 0, 0,
                      SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER |
                      SWP_NOACTIVATE | SWP_FRAMECHANGED)) {
        qWarning() << "[WindowUtils] Failed to refresh window frame | error:" << GetLastError();
    }
    qInfo() << "[WindowUtils] Native maximize capability | requested:" << enabled
            << "| before:" << bool(oldStyle & WS_MAXIMIZEBOX)
            << "| after:" << bool(GetWindowLongPtrW(handle, GWL_STYLE) & WS_MAXIMIZEBOX)
            << "| minimum:" << window->minimumSize()
            << "| maximum:" << window->maximumSize();
#else
    Q_UNUSED(window);
    Q_UNUSED(enabled);
#endif
}

} 
