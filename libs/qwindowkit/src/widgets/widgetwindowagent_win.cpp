// Copyright (C) 2023-2024 Stdware Collections (https://www.github.com/stdware)
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#include "widgetwindowagent_p.h"

#include <QtCore/QDebug>
#include <QtCore/QDateTime>
#include <QtGui/QPainter>

#include <QtCore/private/qcoreapplication_p.h>

#include <QWKCore/qwindowkit_windows.h>
#include <QWKCore/private/qwkglobal_p.h>
#include <QWKCore/private/windows10borderhandler_p.h>

namespace QWK {

#if QWINDOWKIT_CONFIG(ENABLE_WINDOWS_SYSTEM_BORDERS)
    
    
    
    

    
    
    
    
    
    
    
    
    
    
    
    

    
    
    
    

    class WidgetBorderHandler : public QObject, public Windows10BorderHandler {
    public:
        explicit WidgetBorderHandler(QWidget *widget, AbstractWindowContext *ctx,
                                     QObject *parent = nullptr)
            : QObject(parent), Windows10BorderHandler(ctx), widget(widget) {
            widget->installEventFilter(this);

            
            if (ctx->windowId()) {
                setupNecessaryAttributes();
            }
            WidgetBorderHandler::updateGeometry();
        }

        void updateGeometry() override {
            
            
            
            
            
            
            
            
            
            
        }

        bool isWindowActive() const override {
            return widget->isActiveWindow();
        }

        inline void forwardEventToWidgetAndDraw(QWidget *w, QEvent *event) {
            
            
            if (!forwardObjectEventFilters(this, w, event)) {
                
                std::ignore = static_cast<QObject *>(w)->event(event);
                QCoreApplicationPrivate::setEventSpontaneous(event, false);
            }

            
            
            
            drawBorderNative();
        }

        inline void forwardEventToWindowAndDraw(QWindow *window, QEvent *event) {
            
            
            if (!forwardObjectEventFilters(ctx, window, event)) {
                
                std::ignore = static_cast<QObject *>(window)->event(event);
                QCoreApplicationPrivate::setEventSpontaneous(event, false);
            }

            
            
            drawBorderNative();
        }

    protected:
        bool sharedEventFilter(QObject *obj, QEvent *event) override {
            Q_UNUSED(obj)

            switch (event->type()) {
                case QEvent::Expose: {
                    
                    
                    
                    

                    
                    
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
                    struct ExposeEvent : public QExposeEvent {
                        inline const QRegion &getRegion() const { return m_region; }
                    };
                    auto ee = static_cast<ExposeEvent *>(event);
                    bool exposeRegionValid = !ee->getRegion().isNull();
#else
                    auto ee = static_cast<QExposeEvent *>(event);
                    bool exposeRegionValid = !ee->region().isNull();
#endif
                    auto window = widget->windowHandle();
                    if (window->isExposed() && isNormalWindow() && exposeRegionValid) {
                        forwardEventToWindowAndDraw(window, event);
                        return true;
                    }
                    break;
                }
                default:
                    break;
            }
            return Windows10BorderHandler::sharedEventFilter(obj, event);
        }

        bool eventFilter(QObject *obj, QEvent *event) override {
            Q_UNUSED(obj)

            switch (event->type()) {
                case QEvent::UpdateRequest: {
                    if (!isNormalWindow())
                        break;
                    forwardEventToWidgetAndDraw(widget, event);
                    return true;
                }

                case QEvent::WindowStateChange: {
                    updateGeometry();
                    break;
                }

                case QEvent::WindowActivate:
                case QEvent::WindowDeactivate: {
                    widget->update();
                    break;
                }

                default:
                    break;
            }
            return false;
        }

        QWidget *widget;
    };

    void WidgetWindowAgentPrivate::setupWindows10BorderWorkaround() {
        
        auto ctx = context.get();
        if (ctx->windowAttribute(QStringLiteral("win10-border-needed")).toBool()) {
            borderHandler = std::make_unique<WidgetBorderHandler>(hostWidget, ctx);
        }
    }
#endif

}
