#include "danmakurendererutils.h"

namespace DanmakuRendererUtils {

QString assTrackRendererId()
{
    return QStringLiteral("ass-track");
}

QString nativeSmoothRendererId()
{
    return QStringLiteral("native-smooth");
}

QString defaultRendererId()
{
    return nativeSmoothRendererId();
}

QString normalizeRendererId(QString value)
{
    value = value.trimmed().toLower();
    if (value == nativeSmoothRendererId()) {
        return nativeSmoothRendererId();
    }
    if (value == assTrackRendererId()) {
        return assTrackRendererId();
    }
    return defaultRendererId();
}

bool isNativeRenderer(QString value)
{
    return normalizeRendererId(value) == nativeSmoothRendererId();
}

} 
