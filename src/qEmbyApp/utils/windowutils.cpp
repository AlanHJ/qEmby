#include "windowutils.h"

#include <QDebug>
#include <QWidget>
#include <QOpenGLWidget>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

namespace WindowUtils {

void prepareOpenGLComposition(QWidget *window)
{
#if (defined(Q_OS_WIN) || defined(Q_OS_LINUX)) && \
    (QT_VERSION >= QT_VERSION_CHECK(6, 4, 0))
    if (!window || !window->isWindow())
        return;
    const auto anchorName = QStringLiteral("playerCompositionAnchor");
    if (window->findChild<QOpenGLWidget *>(anchorName, Qt::FindDirectChildrenOnly))
        return;
    if (window->testAttribute(Qt::WA_WState_Created)) {
        qWarning() << "[WindowUtils] OpenGL composition preparation was too late"
                   << "| windowClass:" << window->metaObject()->className();
        return;
    }

    
    
    
    auto *anchor = new QOpenGLWidget(window);
    anchor->setObjectName(anchorName);
    anchor->setFixedSize(1, 1);
    anchor->setFocusPolicy(Qt::NoFocus);
    anchor->setAttribute(Qt::WA_TransparentForMouseEvents);
    anchor->hide();
    qInfo() << "[WindowUtils] OpenGL composition prepared before native window creation"
            << "| windowClass:" << window->metaObject()->className()
            << "| nativeWindowCreated:" << window->testAttribute(Qt::WA_WState_Created);
#else
    Q_UNUSED(window);
#endif
}

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
