// Copyright (C) 2023-2024 Stdware Collections (https:
// Copyright (C) 2021-2023 wangwenx190 (Yuhang Zhao)
// SPDX-License-Identifier: Apache-2.0

#ifndef STYLEAGENTPRIVATE_H
#define STYLEAGENTPRIVATE_H










#include <QtCore/QHash>

#include <QWKCore/styleagent.h>

namespace QWK {

    class StyleAgentPrivate : public QObject {
        Q_DECLARE_PUBLIC(StyleAgent)
    public:
        StyleAgentPrivate();
        ~StyleAgentPrivate() override;

        void init();

        StyleAgent *q_ptr;

        StyleAgent::SystemTheme systemTheme = StyleAgent::Unknown;

        void setupSystemThemeHook();
        void removeSystemThemeHook();

        void notifyThemeChanged(StyleAgent::SystemTheme theme);
    };

}

#endif 