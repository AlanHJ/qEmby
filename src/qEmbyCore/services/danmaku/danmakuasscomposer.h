#ifndef DANMAKUASSCOMPOSER_H
#define DANMAKUASSCOMPOSER_H

#include "../../models/danmaku/danmakumodels.h"

class DanmakuAssComposer
{
public:
    static QString composeAss(const QList<DanmakuComment> &comments,
                              const DanmakuRenderOptions &options);
};

#endif 
