// Copyright (C) 2023-2024 Stdware Collections (https:
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#ifndef WINDOWS10BORDERHANDLER_P_H
#define WINDOWS10BORDERHANDLER_P_H










#include <QtGui/QWindow>
#include <QtGui/QMouseEvent>

#include <QWKCore/qwindowkit_windows.h>
#include <QWKCore/private/qwkglobal_p.h>
#include <QWKCore/private/abstractwindowcontext_p.h>

namespace QWK {

    class Windows10BorderHandler : public NativeEventFilter, public SharedEventFilter {
    public:
        inline Windows10BorderHandler(AbstractWindowContext *ctx) : ctx(ctx) {
            ctx->installNativeEventFilter(this);
            ctx->installSharedEventFilter(this);
        }

        inline void setupNecessaryAttributes() {
            if (!isWin11OrGreater()) {
                
                
                static QVariant defaultMargins = QVariant::fromValue(QMargins(0, 1, 0, 0));
                ctx->setWindowAttribute(QStringLiteral("extra-margins"), defaultMargins);
            }

            
            ctx->setWindowAttribute(QStringLiteral("dark-mode"), true);
        }

        inline bool isNormalWindow() const {
            return !(ctx->window()->windowStates() &
                     (Qt::WindowMinimized | Qt::WindowMaximized | Qt::WindowFullScreen));
        }

        inline void drawBorderEmulated(QPainter *painter, const QRect &rect) {
            QRegion region(rect);
            void *args[] = {
                painter,
                const_cast<QRect *>(&rect),
                &region,
            };
            ctx->virtual_hook(AbstractWindowContext::DrawWindows10BorderHook_Emulated, args);
        }

        inline void drawBorderNative() {
            ctx->virtual_hook(AbstractWindowContext::DrawWindows10BorderHook_Native, nullptr);
        }

        inline int borderThickness() const {
            return ctx->windowAttribute(QStringLiteral("border-thickness")).toInt();
        }

        inline void updateExtraMargins(bool windowActive) {
            if (isWin11OrGreater()) {
                return;
            }

            
            if (windowActive) {
                
                static QVariant defaultMargins = QVariant::fromValue(QMargins(0, 1, 0, 0));
                ctx->setWindowAttribute(QStringLiteral("extra-margins"), defaultMargins);
                return;
            }

            
            
            
            
            QRect frame = ctx->windowAttribute(QStringLiteral("window-rect")).toRect();
            QMargins margins{0, -frame.top(), 0, 0};
            ctx->setWindowAttribute(QStringLiteral("extra-margins"), QVariant::fromValue(margins));
        }

        virtual void updateGeometry() = 0;

        virtual bool isWindowActive() const {
            return ctx->window()->isActive();
        }

    protected:
        bool nativeEventFilter(const QByteArray &eventType, void *message,
                               QT_NATIVE_EVENT_RESULT_TYPE *result) override {
            Q_UNUSED(eventType)

            const auto msg = static_cast<const MSG *>(message);
            switch (msg->message) {
                case WM_DPICHANGED: {
                    updateGeometry();
                    updateExtraMargins(isWindowActive());
                    break;
                }

                case WM_ACTIVATE: {
                    updateExtraMargins(LOWORD(msg->wParam) != WA_INACTIVE);
                    break;
                }

                case WM_THEMECHANGED:
                case WM_SYSCOLORCHANGE:
                case WM_DWMCOLORIZATIONCOLORCHANGED: {
                    
                    
                    
                    
                    
                    if (ctx->windowAttribute(QStringLiteral("dark-mode")).toBool()) {
                        ctx->setWindowAttribute(QStringLiteral("dark-mode"), true);
                    }
                    break;
                }

                default:
                    break;
            }
            return false;
        }

        bool sharedEventFilter(QObject *obj, QEvent *event) override {
            Q_UNUSED(obj)

            if (event->type() == QEvent::WinIdChange) {
                if (ctx->windowId()) {
                    setupNecessaryAttributes();
                    updateGeometry();
                }
            }
            return false;
        }

    protected:
        AbstractWindowContext *ctx;
    };

}

#endif 
