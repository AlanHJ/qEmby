#ifndef SUBTITLESTYLEUTILS_H
#define SUBTITLESTYLEUTILS_H

#include <QString>

class MpvController;

namespace SubtitleStyleUtils {

bool isSubtitleStyleKey(const QString &key);
void applyToController(MpvController *controller,
                       bool protectPrimarySubtitleForDanmaku = false);

} 

#endif 
