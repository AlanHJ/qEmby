#ifndef PLAYERPREFERENCEUTILS_H
#define PLAYERPREFERENCEUTILS_H

#include <QList>
#include <QString>
#include <QStringList>
#include <models/media/playbackinfo.h>

namespace PlayerPreferenceUtils {

QStringList splitLanguageRules(const QString &rawRules);

bool isAutomaticLanguageRules(const QString &rawRules);
bool isSubtitleDisabled(const QString &rawRules);

int findPreferredStreamIndex(const QList<MediaStreamInfo> &mediaStreams,
                             const QString &streamType,
                             const QString &rawRules);

void applyPreferredStreamRules(MediaSourceInfo &selectedSource,
                               const QString &audioRules,
                               const QString &subtitleRules);

QStringList mpvLanguageCodesForRules(const QString &rawRules);

} 

#endif 
