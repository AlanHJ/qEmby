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
    return assTrackRendererId();
}

QString normalizeRendererId(QString value)
{
    value = value.trimmed().toLower();
    if (value == nativeSmoothRendererId()) {
        return nativeSmoothRendererId();
    }
    return assTrackRendererId();
}

bool isNativeRenderer(QString value)
{
    return normalizeRendererId(value) == nativeSmoothRendererId();
}

} 
