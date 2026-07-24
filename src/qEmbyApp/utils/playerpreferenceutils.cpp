#include "playerpreferenceutils.h"

#include <config/config_keys.h>
#include <config/configstore.h>
#include <QDebug>
#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include <limits>

namespace {

struct LanguageRuleDefinition {
    QStringList exactAliases;
    QStringList textAliases;
    QStringList mpvCodes;
};

QString normalizePreferenceText(QString value) {
    value = value.trimmed();
    value.replace(QChar(0xFF0C), QChar(','));  
    value.replace(QChar(0xFF1B), QChar(','));  
    value.replace(QChar(0xFF08), QChar('('));  
    value.replace(QChar(0xFF09), QChar(')'));  
    value.replace(QChar(0x3000), QChar(' '));  
    value.replace(QLatin1Char('_'), QLatin1Char('-'));
    return value.simplified().toLower();
}

QStringList appendUniqueCaseInsensitive(QStringList values,
                                        const QStringList &extras) {
    for (const QString &extra : extras) {
        const QString normalized = normalizePreferenceText(extra);
        if (normalized.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (const QString &value : values) {
            if (value.compare(normalized, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            values.append(normalized);
        }
    }

    return values;
}

const QHash<QString, LanguageRuleDefinition> &languageRuleDefinitions() {
    static const QHash<QString, LanguageRuleDefinition> definitions = {
        {QStringLiteral("chi"),
         {{QStringLiteral("chi"), QStringLiteral("zho"), QStringLiteral("zh"),
           QStringLiteral("zh-cn"), QStringLiteral("zh-hans"),
           QStringLiteral("zh-sg"), QStringLiteral("chs"),
           QStringLiteral("cht"), QStringLiteral("cmn"),
           QStringLiteral("yue"), QStringLiteral("chinese"),
           QStringLiteral("中文")},
          {QStringLiteral("chinese"), QStringLiteral("中文"),
           QStringLiteral("普通话"), QStringLiteral("国语"),
           QStringLiteral("mandarin"), QStringLiteral("cantonese"),
           QStringLiteral("粤语"), QStringLiteral("廣東話"),
           QStringLiteral("简体"), QStringLiteral("繁体"),
           QStringLiteral("简中"), QStringLiteral("繁中"),
           QStringLiteral("traditional chinese"),
           QStringLiteral("hong kong"), QStringLiteral("macau"),
           QStringLiteral("macao"), QStringLiteral("taiwan"),
           QStringLiteral("香港"), QStringLiteral("澳门"),
           QStringLiteral("澳門"), QStringLiteral("台湾"),
           QStringLiteral("台灣")},
          {QStringLiteral("chi"), QStringLiteral("zho"),
           QStringLiteral("zh"), QStringLiteral("zh-cn"),
           QStringLiteral("zh-hans"), QStringLiteral("zh-sg"),
           QStringLiteral("zh-hk"), QStringLiteral("zh-mo"),
           QStringLiteral("zh-tw"), QStringLiteral("zh-hant"),
           QStringLiteral("cmn"), QStringLiteral("yue")}}},
        {QStringLiteral("chi-hk"),
         {{QStringLiteral("chi-hk"), QStringLiteral("zho-hk"),
           QStringLiteral("zh-hk"), QStringLiteral("yue"),
           QStringLiteral("yue-hk"), QStringLiteral("cantonese"),
           QStringLiteral("粤语"), QStringLiteral("廣東話")},
          {QStringLiteral("chinese (hong kong)"),
           QStringLiteral("hong kong chinese"),
           QStringLiteral("hong kong"), QStringLiteral("香港"),
           QStringLiteral("中文(港)"), QStringLiteral("港配"),
           QStringLiteral("港版"), QStringLiteral("cantonese"),
           QStringLiteral("粤语"), QStringLiteral("廣東話")},
          {QStringLiteral("zh-hk"), QStringLiteral("yue"),
           QStringLiteral("yue-hk"), QStringLiteral("chi"),
           QStringLiteral("zho"), QStringLiteral("zh")}}},
        {QStringLiteral("chi-mo"),
         {{QStringLiteral("chi-mo"), QStringLiteral("zho-mo"),
           QStringLiteral("zh-mo")},
          {QStringLiteral("chinese (macau)"),
           QStringLiteral("macau chinese"), QStringLiteral("macau"),
           QStringLiteral("macao"), QStringLiteral("澳门"),
           QStringLiteral("澳門"), QStringLiteral("中文(澳)")},
          {QStringLiteral("zh-mo"), QStringLiteral("chi"),
           QStringLiteral("zho"), QStringLiteral("zh")}}},
        {QStringLiteral("chi-tw"),
         {{QStringLiteral("chi-tw"), QStringLiteral("zho-tw"),
           QStringLiteral("zh-tw"), QStringLiteral("zh-hant"),
           QStringLiteral("zh-hant-tw"),
           QStringLiteral("traditional chinese"),
           QStringLiteral("chinese (traditional)")},
          {QStringLiteral("chinese (taiwan)"),
           QStringLiteral("taiwan chinese"),
           QStringLiteral("chinese (traditional)"),
           QStringLiteral("traditional chinese"),
           QStringLiteral("taiwan"), QStringLiteral("台湾"),
           QStringLiteral("台灣"), QStringLiteral("中文(台)"),
           QStringLiteral("繁体"), QStringLiteral("繁體"),
           QStringLiteral("繁中")},
          {QStringLiteral("zh-tw"), QStringLiteral("zh-hant"),
           QStringLiteral("chi"), QStringLiteral("zho"),
           QStringLiteral("zh")}}},
        {QStringLiteral("eng"),
         {{QStringLiteral("eng"), QStringLiteral("en"),
           QStringLiteral("en-us"), QStringLiteral("en-gb"),
           QStringLiteral("english")},
          {QStringLiteral("english"), QStringLiteral("英语")},
          {QStringLiteral("eng"), QStringLiteral("en"),
           QStringLiteral("en-us"), QStringLiteral("en-gb")}}},
        {QStringLiteral("jpn"),
         {{QStringLiteral("jpn"), QStringLiteral("ja"),
           QStringLiteral("ja-jp"), QStringLiteral("japanese")},
          {QStringLiteral("japanese"), QStringLiteral("日本語"),
           QStringLiteral("日语")},
          {QStringLiteral("jpn"), QStringLiteral("ja"),
           QStringLiteral("ja-jp")}}},
        {QStringLiteral("kor"),
         {{QStringLiteral("kor"), QStringLiteral("ko"),
           QStringLiteral("ko-kr"), QStringLiteral("korean")},
          {QStringLiteral("korean"), QStringLiteral("한국어"),
           QStringLiteral("韩语")},
          {QStringLiteral("kor"), QStringLiteral("ko"),
           QStringLiteral("ko-kr")}}},
        {QStringLiteral("fre"),
         {{QStringLiteral("fre"), QStringLiteral("fra"),
           QStringLiteral("fr"), QStringLiteral("fr-fr"),
           QStringLiteral("french")},
          {QStringLiteral("french"), QStringLiteral("francais"),
           QStringLiteral("français"), QStringLiteral("法语")},
          {QStringLiteral("fre"), QStringLiteral("fra"),
           QStringLiteral("fr"), QStringLiteral("fr-fr")}}},
        {QStringLiteral("ger"),
         {{QStringLiteral("ger"), QStringLiteral("deu"),
           QStringLiteral("de"), QStringLiteral("de-de"),
           QStringLiteral("german")},
          {QStringLiteral("german"), QStringLiteral("deutsch"),
           QStringLiteral("德语")},
          {QStringLiteral("ger"), QStringLiteral("deu"),
           QStringLiteral("de"), QStringLiteral("de-de")}}},
        {QStringLiteral("spa"),
         {{QStringLiteral("spa"), QStringLiteral("es"),
           QStringLiteral("es-es"), QStringLiteral("spanish")},
          {QStringLiteral("spanish"), QStringLiteral("espanol"),
           QStringLiteral("español"), QStringLiteral("西班牙语")},
          {QStringLiteral("spa"), QStringLiteral("es"),
           QStringLiteral("es-es")}}},
        {QStringLiteral("rus"),
         {{QStringLiteral("rus"), QStringLiteral("ru"),
           QStringLiteral("ru-ru"), QStringLiteral("russian")},
          {QStringLiteral("russian"), QStringLiteral("русский"),
           QStringLiteral("俄语")},
          {QStringLiteral("rus"), QStringLiteral("ru"),
           QStringLiteral("ru-ru")}}},
    };

    return definitions;
}

QStringList streamFieldValues(const MediaStreamInfo &stream) {
    QStringList values;
    values = appendUniqueCaseInsensitive(
        values, QStringList{stream.language, stream.displayLanguage,
                            stream.title, stream.displayTitle});
    return values;
}

QString streamSearchText(const MediaStreamInfo &stream) {
    return streamFieldValues(stream).join(QLatin1Char(' '));
}

bool looksLikeLanguageCode(const QString &token) {
    static const QRegularExpression languageCodePattern(
        QStringLiteral("^[a-z]{2,3}(?:-[a-z0-9]{2,8})*$"));
    return languageCodePattern.match(token).hasMatch();
}

bool streamMatchesSingleRule(const MediaStreamInfo &stream,
                             const QString &normalizedRule) {
    if (normalizedRule.isEmpty() || normalizedRule == QStringLiteral("auto") ||
        normalizedRule == QStringLiteral("none")) {
        return false;
    }

    const QStringList fields = streamFieldValues(stream);
    const QString searchText = streamSearchText(stream);
    const auto &definitions = languageRuleDefinitions();

    auto matchesAny = [&fields, &searchText](const QStringList &exactAliases,
                                             const QStringList &textAliases) {
        for (const QString &field : fields) {
            for (const QString &alias : exactAliases) {
                if (field == alias) {
                    return true;
                }
            }
        }

        for (const QString &alias : textAliases) {
            if (!alias.isEmpty() && searchText.contains(alias)) {
                return true;
            }
        }

        return false;
    };

    const auto definitionIt = definitions.constFind(normalizedRule);
    if (definitionIt != definitions.constEnd() &&
        matchesAny(definitionIt->exactAliases, definitionIt->textAliases)) {
        return true;
    }

    return matchesAny(QStringList{normalizedRule},
                      QStringList{normalizedRule});
}

bool containsAnyText(const QString &text, const QStringList &values) {
    for (const QString &value : values) {
        if (text.contains(value)) {
            return true;
        }
    }
    return false;
}

int streamMatchRank(const MediaStreamInfo &stream,
                    const QString &normalizedRule) {
    if (!streamMatchesSingleRule(stream, normalizedRule)) {
        return -1;
    }

    
    
    
    
    if (normalizedRule == QStringLiteral("chi")) {
        const QString searchText = streamSearchText(stream);
        if (containsAnyText(
                searchText,
                {QStringLiteral("cantonese"), QStringLiteral("粤语"),
                 QStringLiteral("廣東話"), QStringLiteral("hong kong"),
                 QStringLiteral("香港"), QStringLiteral("macau"),
                 QStringLiteral("macao"), QStringLiteral("澳门"),
                 QStringLiteral("澳門"), QStringLiteral("taiwan"),
                 QStringLiteral("台湾"), QStringLiteral("台灣"),
                 QStringLiteral("traditional"), QStringLiteral("繁体"),
                 QStringLiteral("繁體"), QStringLiteral("繁中")})) {
            return 2;
        }
        if (containsAnyText(
                searchText,
                {QStringLiteral("mandarin"), QStringLiteral("普通话"),
                 QStringLiteral("国语"), QStringLiteral("simplified"),
                 QStringLiteral("简体"), QStringLiteral("简中"),
                 QStringLiteral("zh-cn"), QStringLiteral("zh-hans"),
                 QStringLiteral("chs"), QStringLiteral("cmn")})) {
            return 0;
        }
        return 1;
    }

    return 0;
}

std::optional<int> readRememberedStreamIndex(const QString &key) {
    if (key.isEmpty()) {
        return std::nullopt;
    }
    const QString stored = ConfigStore::instance()->get<QString>(key).trimmed();
    if (stored.isEmpty()) {
        return std::nullopt;
    }
    bool ok = false;
    const int index = stored.toInt(&ok);
    return ok ? std::optional<int>(index) : std::nullopt;
}

QString streamSelectionKey(const QString &serverId, const QString &mediaId,
                           const QString &mediaSourceId,
                           const char *baseKey) {
    if (serverId.trimmed().isEmpty() || mediaId.trimmed().isEmpty() ||
        mediaSourceId.trimmed().isEmpty()) {
        return {};
    }
    return ConfigKeys::forServerMediaSource(serverId, mediaId, mediaSourceId,
                                            baseKey);
}

bool containsStreamIndex(const MediaSourceInfo &source, const QString &type,
                         int streamIndex) {
    for (const MediaStreamInfo &stream : source.mediaStreams) {
        if (stream.type == type && stream.index == streamIndex) {
            return true;
        }
    }
    return false;
}

} 

namespace PlayerPreferenceUtils {

QStringList splitLanguageRules(const QString &rawRules) {
    QString normalizedRules = rawRules;
    normalizedRules.replace(QChar(0xFF0C), QChar(','));
    normalizedRules.replace(QChar(0xFF1B), QChar(','));

    QStringList rules;
    const QStringList parts =
        normalizedRules.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString normalized = normalizePreferenceText(part);
        if (normalized.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (const QString &rule : rules) {
            if (rule.compare(normalized, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            rules.append(normalized);
        }
    }

    return rules;
}

bool isAutomaticLanguageRules(const QString &rawRules) {
    const QStringList rules = splitLanguageRules(rawRules);
    if (rules.contains(QStringLiteral("none"))) {
        return false;
    }
    return rules.isEmpty() || rules.contains(QStringLiteral("auto"));
}

bool isSubtitleDisabled(const QString &rawRules) {
    return splitLanguageRules(rawRules).contains(QStringLiteral("none"));
}

int findPreferredStreamIndex(const QList<MediaStreamInfo> &mediaStreams,
                             const QString &streamType,
                             const QString &rawRules) {
    const QStringList rules = splitLanguageRules(rawRules);
    if (rules.isEmpty() || rules.contains(QStringLiteral("auto")) ||
        (streamType == QStringLiteral("Subtitle") &&
         rules.contains(QStringLiteral("none")))) {
        return -1;
    }

    for (const QString &rule : rules) {
        int bestPosition = -1;
        int bestRank = std::numeric_limits<int>::max();
        for (int i = 0; i < mediaStreams.size(); ++i) {
            if (mediaStreams[i].type != streamType) {
                continue;
            }
            const int rank = streamMatchRank(mediaStreams[i], rule);
            if (rank >= 0 && rank < bestRank) {
                bestPosition = i;
                bestRank = rank;
            }
        }
        if (bestPosition >= 0) {
            return mediaStreams[bestPosition].index;
        }
    }

    return -1;
}

QList<int> preferredStreamOrder(const QList<MediaStreamInfo> &mediaStreams,
                                const QString &streamType,
                                const QString &rawRules) {
    QList<int> remaining;
    for (int i = 0; i < mediaStreams.size(); ++i) {
        if (mediaStreams[i].type == streamType) {
            remaining.append(i);
        }
    }

    const QStringList rules = splitLanguageRules(rawRules);
    if (remaining.size() <= 1 || rules.isEmpty() ||
        rules.contains(QStringLiteral("auto")) ||
        (streamType == QStringLiteral("Subtitle") &&
         rules.contains(QStringLiteral("none")))) {
        return remaining;
    }

    QList<int> ordered;
    ordered.reserve(remaining.size());
    for (const QString &rule : rules) {
        QList<int> matches;
        for (const int position : remaining) {
            if (streamMatchRank(mediaStreams[position], rule) >= 0) {
                matches.append(position);
            }
        }

        std::stable_sort(
            matches.begin(), matches.end(),
            [&mediaStreams, &rule](int left, int right) {
                return streamMatchRank(mediaStreams[left], rule) <
                       streamMatchRank(mediaStreams[right], rule);
            });
        for (const int position : matches) {
            ordered.append(position);
            remaining.removeOne(position);
        }
    }

    ordered.append(remaining);
    qDebug().noquote()
        << QStringLiteral("[PlayerPreferenceUtils] Ordered media streams")
               + QStringLiteral(" | type=%1").arg(streamType)
               + QStringLiteral(" | rules=%1").arg(rawRules)
               + QStringLiteral(" | streamIndexes=%1")
                     .arg([&mediaStreams, &ordered]() {
                         QStringList indexes;
                         for (const int position : ordered) {
                             indexes.append(
                                 QString::number(mediaStreams[position].index));
                         }
                         return indexes.join(QLatin1Char(','));
                     }());
    return ordered;
}

void applyPreferredStreamRules(MediaSourceInfo &selectedSource,
                               const QString &audioRules,
                               const QString &subtitleRules,
                               const RememberedStreamSelection &remembered) {
    const int bestAudioIdx = findPreferredStreamIndex(
        selectedSource.mediaStreams, QStringLiteral("Audio"), audioRules);
    const int bestSubIdx = findPreferredStreamIndex(
        selectedSource.mediaStreams, QStringLiteral("Subtitle"), subtitleRules);
    const bool subtitleDisabled = isSubtitleDisabled(subtitleRules);

    int firstSubIdx = -1;
    bool hasDefaultSub = false;

    for (const MediaStreamInfo &stream : selectedSource.mediaStreams) {
        if (stream.type != QStringLiteral("Subtitle")) {
            continue;
        }

        if (firstSubIdx < 0) {
            firstSubIdx = stream.index;
        }
        if (stream.isDefault) {
            hasDefaultSub = true;
        }
    }

    const bool rememberedAudioAvailable =
        remembered.audioIndex.has_value() &&
        containsStreamIndex(selectedSource, QStringLiteral("Audio"),
                            *remembered.audioIndex);
    const bool rememberedSubtitleAvailable =
        remembered.subtitleIndex.has_value() &&
        (*remembered.subtitleIndex < 0 ||
         containsStreamIndex(selectedSource, QStringLiteral("Subtitle"),
                             *remembered.subtitleIndex));

    if (rememberedAudioAvailable) {
        for (MediaStreamInfo &stream : selectedSource.mediaStreams) {
            if (stream.type == QStringLiteral("Audio")) {
                stream.isDefault = (stream.index == *remembered.audioIndex);
            }
        }
    } else if (bestAudioIdx >= 0) {
        for (MediaStreamInfo &stream : selectedSource.mediaStreams) {
            if (stream.type == QStringLiteral("Audio")) {
                stream.isDefault = (stream.index == bestAudioIdx);
            }
        }
    }

    QString subtitleDecision = QStringLiteral("keep-server-default");
    if (rememberedSubtitleAvailable) {
        subtitleDecision = *remembered.subtitleIndex < 0
                               ? QStringLiteral("disabled-by-remembered-selection")
                               : QStringLiteral("remembered-selection");
        for (MediaStreamInfo &stream : selectedSource.mediaStreams) {
            if (stream.type == QStringLiteral("Subtitle")) {
                stream.isDefault =
                    *remembered.subtitleIndex >= 0 &&
                    stream.index == *remembered.subtitleIndex;
            }
        }
    } else if (subtitleDisabled) {
        subtitleDecision = QStringLiteral("disabled-by-rule");
        for (MediaStreamInfo &stream : selectedSource.mediaStreams) {
            if (stream.type == QStringLiteral("Subtitle")) {
                stream.isDefault = false;
            }
        }
    } else if (bestSubIdx >= 0) {
        subtitleDecision = QStringLiteral("matched-rule");
        for (MediaStreamInfo &stream : selectedSource.mediaStreams) {
            if (stream.type == QStringLiteral("Subtitle")) {
                stream.isDefault = (stream.index == bestSubIdx);
            }
        }
    } else if (firstSubIdx >= 0 && !hasDefaultSub) {
        subtitleDecision = QStringLiteral("fallback-first-subtitle");
        for (MediaStreamInfo &stream : selectedSource.mediaStreams) {
            if (stream.type == QStringLiteral("Subtitle")) {
                stream.isDefault = (stream.index == firstSubIdx);
            }
        }
    }

    qDebug().noquote()
        << QStringLiteral("[PlayerPreferenceUtils] Applied stream rules")
               + QStringLiteral(" | sourceId=%1").arg(selectedSource.id)
               + QStringLiteral(" | audioRules=%1").arg(audioRules)
               + QStringLiteral(" | selectedAudioIndex=%1")
                     .arg(rememberedAudioAvailable ? *remembered.audioIndex
                                                   : bestAudioIdx)
               + QStringLiteral(" | subtitleRules=%1").arg(subtitleRules)
               + QStringLiteral(" | selectedSubtitleIndex=%1")
                     .arg(rememberedSubtitleAvailable
                              ? *remembered.subtitleIndex
                              : bestSubIdx)
               + QStringLiteral(" | subtitleDecision=%1").arg(subtitleDecision);
}

static RememberedStreamSelection loadRememberedStreamSelection(
    const QString &serverId, const QString &mediaId,
    const QString &mediaSourceId) {
    RememberedStreamSelection result;
    result.audioIndex = readRememberedStreamIndex(streamSelectionKey(
        serverId, mediaId, mediaSourceId, ConfigKeys::PlayerSelectedAudioStream));
    result.subtitleIndex = readRememberedStreamIndex(streamSelectionKey(
        serverId, mediaId, mediaSourceId,
        ConfigKeys::PlayerSelectedSubtitleStream));
    return result;
}

RememberedStreamSelection validatedRememberedStreamSelection(
    const QString &serverId, const QString &mediaId,
    const MediaSourceInfo &mediaSource) {
    RememberedStreamSelection result =
        loadRememberedStreamSelection(serverId, mediaId, mediaSource.id);

    if (result.audioIndex.has_value() &&
        !containsStreamIndex(mediaSource, QStringLiteral("Audio"),
                             *result.audioIndex)) {
        qWarning() << "[PlayerPreferenceUtils] Ignoring unavailable remembered audio stream"
                   << "mediaId=" << mediaId << "sourceId=" << mediaSource.id
                   << "streamIndex=" << *result.audioIndex;
        result.audioIndex.reset();
    }

    if (result.subtitleIndex.has_value() && *result.subtitleIndex >= 0 &&
        !containsStreamIndex(mediaSource, QStringLiteral("Subtitle"),
                             *result.subtitleIndex)) {
        qWarning() << "[PlayerPreferenceUtils] Ignoring unavailable remembered subtitle stream"
                   << "mediaId=" << mediaId << "sourceId=" << mediaSource.id
                   << "streamIndex=" << *result.subtitleIndex;
        result.subtitleIndex.reset();
    }

    return result;
}

void rememberAudioStreamIndex(const QString &serverId, const QString &mediaId,
                              const QString &mediaSourceId, int streamIndex) {
    const QString key = streamSelectionKey(
        serverId, mediaId, mediaSourceId, ConfigKeys::PlayerSelectedAudioStream);
    if (key.isEmpty() || streamIndex < 0) {
        return;
    }
    ConfigStore::instance()->set(key, QString::number(streamIndex));
    qDebug() << "[PlayerPreferenceUtils] Remembered manual audio stream"
             << "mediaId=" << mediaId << "sourceId=" << mediaSourceId
             << "streamIndex=" << streamIndex;
}

void rememberSubtitleStreamIndex(const QString &serverId,
                                 const QString &mediaId,
                                 const QString &mediaSourceId,
                                 int streamIndex) {
    const QString key = streamSelectionKey(
        serverId, mediaId, mediaSourceId,
        ConfigKeys::PlayerSelectedSubtitleStream);
    if (key.isEmpty() || streamIndex < -1) {
        return;
    }
    ConfigStore::instance()->set(key, QString::number(streamIndex));
    qDebug() << "[PlayerPreferenceUtils] Remembered manual subtitle stream"
             << "mediaId=" << mediaId << "sourceId=" << mediaSourceId
             << "streamIndex=" << streamIndex;
}

void applyRememberedOrPreferredStreamRules(
    MediaSourceInfo &selectedSource, const QString &serverId,
    const QString &mediaId, const QString &audioRules,
    const QString &subtitleRules) {
    applyPreferredStreamRules(
        selectedSource, audioRules, subtitleRules,
        validatedRememberedStreamSelection(serverId, mediaId,
                                            selectedSource));
}

QStringList mpvLanguageCodesForRules(const QString &rawRules) {
    if (isAutomaticLanguageRules(rawRules)) {
        return {};
    }

    QStringList codes;
    const QStringList rules = splitLanguageRules(rawRules);
    const auto &definitions = languageRuleDefinitions();

    for (const QString &rule : rules) {
        if (rule == QStringLiteral("auto") || rule == QStringLiteral("none")) {
            continue;
        }

        const auto definitionIt = definitions.constFind(rule);
        if (definitionIt != definitions.constEnd()) {
            codes = appendUniqueCaseInsensitive(codes, definitionIt->mpvCodes);
            continue;
        }

        if (looksLikeLanguageCode(rule)) {
            codes = appendUniqueCaseInsensitive(codes, QStringList{rule});
        }
    }

    return codes;
}

} 
