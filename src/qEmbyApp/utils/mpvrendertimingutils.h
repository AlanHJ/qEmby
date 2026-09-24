#ifndef MPVRENDERTIMINGUTILS_H
#define MPVRENDERTIMINGUTILS_H

#include <QtGlobal>

namespace MpvRenderTimingUtils {

struct TargetTime {
    qint64 microseconds = 0;
    bool sourceIsNanoseconds = false;
};



TargetTime normalizeTargetTime(qint64 targetTime, qint64 nowUs);

} 
#endif
