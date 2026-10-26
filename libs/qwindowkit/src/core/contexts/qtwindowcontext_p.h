// Copyright (C) 2023-2024 Stdware Collections (https:
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#ifndef QTWINDOWCONTEXT_P_H
#define QTWINDOWCONTEXT_P_H










#include <QWKCore/private/abstractwindowcontext_p.h>

namespace QWK {

    class QtWindowContext : public AbstractWindowContext {
        Q_OBJECT
    public:
        QtWindowContext();
        ~QtWindowContext() override;

        QString key() const override;
        void virtual_hook(int id, void *data) override;

    protected:
        void winIdChanged(WId winId, WId oldWinId) override;

    protected:
        std::unique_ptr<SharedEventFilter> qtWindowEventFilter;
    };

}

#endif 
