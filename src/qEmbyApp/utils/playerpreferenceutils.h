#ifndef PLAYERPREFERENCEUTILS_H
#define PLAYERPREFERENCEUTILS_H

#include <QList>
#include <QString>
#include <QStringList>
#include <models/media/playbackinfo.h>
#include <optional>

namespace PlayerPreferenceUtils {

struct RememberedStreamSelection {
    std::optional<int> audioIndex;
    std::optional<int> subtitleIndex;
};

QStringList splitLanguageRules(const QString &rawRules);

bool isAutomaticLanguageRules(const QString &rawRules);
bool isSubtitleDisabled(const QString &rawRules);

int findPreferredStreamIndex(const QList<MediaStreamInfo> &mediaStreams,
                             const QString &streamType,
                             const QString &rawRules);

QList<int> preferredStreamOrder(const QList<MediaStreamInfo> &mediaStreams,
                                const QString &streamType,
                                const QString &rawRules);

void applyPreferredStreamRules(MediaSourceInfo &selectedSource,
                               const QString &audioRules,
                               const QString &subtitleRules,
                               const RememberedStreamSelection &remembered = {});

RememberedStreamSelection validatedRememberedStreamSelection(
    const QString &serverId, const QString &mediaId,
    const MediaSourceInfo &mediaSource);
void rememberAudioStreamIndex(const QString &serverId, const QString &mediaId,
                              const QString &mediaSourceId, int streamIndex);
void rememberSubtitleStreamIndex(const QString &serverId,
                                 const QString &mediaId,
                                 const QString &mediaSourceId,
                                 int streamIndex);

void applyRememberedOrPreferredStreamRules(
    MediaSourceInfo &selectedSource, const QString &serverId,
    const QString &mediaId, const QString &audioRules,
    const QString &subtitleRules);

QStringList mpvLanguageCodesForRules(const QString &rawRules);

} 

#endif 
