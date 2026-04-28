#ifndef POWERINHIBITUTILS_H
#define POWERINHIBITUTILS_H

#include <QString>

namespace PowerInhibitUtils {

bool acquirePlaybackInhibition(const QString &reason = QString());
void releasePlaybackInhibition();
bool isPlaybackInhibited();

} 

#endif 
