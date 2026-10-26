// Copyright (C) 2023-2024 Stdware Collections (https:
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#ifndef COCOAWINDOWCONTEXT_P_H
#define COCOAWINDOWCONTEXT_P_H










#include <QWKCore/private/abstractwindowcontext_p.h>

namespace QWK {

    class CocoaWindowContext : public AbstractWindowContext {
        Q_OBJECT
    public:
        CocoaWindowContext();
        ~CocoaWindowContext() override;

        QString key() const override;
        void virtual_hook(int id, void *data) override;

        QVariant windowAttribute(const QString &key) const override;

    protected:
        void winIdChanged(WId winId, WId oldWinId) override;
        bool windowAttributeChanged(const QString &key, const QVariant &attribute,
                                    const QVariant &oldAttribute) override;

    protected:
        std::unique_ptr<SharedEventFilter> cocoaWindowEventFilter;
    };

}

#endif 
