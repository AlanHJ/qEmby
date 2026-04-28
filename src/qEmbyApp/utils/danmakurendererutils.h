#ifndef DANMAKURENDERERUTILS_H
#define DANMAKURENDERERUTILS_H

#include <QString>

namespace DanmakuRendererUtils {

QString assTrackRendererId();
QString nativeSmoothRendererId();
QString defaultRendererId();
QString normalizeRendererId(QString value);
bool isNativeRenderer(QString value);

} 

#endif 
