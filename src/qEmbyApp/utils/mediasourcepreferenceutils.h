#ifndef MEDIASOURCEPREFERENCEUTILS_H
#define MEDIASOURCEPREFERENCEUTILS_H

#include <QList>
#include <QString>
#include <QStringList>
#include <models/media/playbackinfo.h>

namespace MediaSourcePreferenceUtils {

QStringList splitPreferredVersionRules(const QString &rawRules);

QList<int> preferredMediaSourceOrder(const QList<MediaSourceInfo> &mediaSources,
                                     const QString &rawRules);

int resolvePreferredMediaSourceIndex(const QList<MediaSourceInfo> &mediaSources,
                                     const QString &rawRules,
                                     const QString &rememberedSourceId = QString());

QString rememberedMediaSourceId(const QString &serverId, const QString &mediaId);
void rememberMediaSourceId(const QString &serverId, const QString &mediaId,
                           const QString &mediaSourceId);

} 

#endif 
