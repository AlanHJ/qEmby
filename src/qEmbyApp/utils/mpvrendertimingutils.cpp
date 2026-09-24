#include "mpvrendertimingutils.h"

#include <cmath>

namespace MpvRenderTimingUtils {

TargetTime normalizeTargetTime(qint64 targetTime, qint64 nowUs)
{
    if (targetTime <= 0 || nowUs <= 0) {
        return {targetTime, false};
    }
    
    
    
    
    
    const long double microsecondDistance = std::abs(
        static_cast<long double>(targetTime) - nowUs);
    const long double nanosecondDistance = std::abs(
        static_cast<long double>(targetTime) / 1000.0L - nowUs);
    
    
    
    if (microsecondDistance > 10000000.0L && nanosecondDistance <= 1000000.0L) {
        return {targetTime / 1000, true};
    }
    return {targetTime, false};
}

} 
