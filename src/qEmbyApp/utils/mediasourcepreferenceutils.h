#ifndef MEDIASOURCEPREFERENCEUTILS_H
#define MEDIASOURCEPREFERENCEUTILS_H

#include <QList>
#include <QString>
#include <QStringList>
#include <models/media/playbackinfo.h>

namespace MediaSourcePreferenceUtils {

QStringList splitPreferredVersionRules(const QString &rawRules);

int resolvePreferredMediaSourceIndex(const QList<MediaSourceInfo> &mediaSources,
                                     const QString &rawRules);

} 

#endif 
