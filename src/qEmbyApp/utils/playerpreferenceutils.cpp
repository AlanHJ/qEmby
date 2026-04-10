#include "playerpreferenceutils.h"

#include <QDebug>
#include <QHash>
#include <QRegularExpression>

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
        for (const MediaStreamInfo &stream : mediaStreams) {
            if (stream.type != streamType) {
                continue;
            }

            if (streamMatchesSingleRule(stream, rule)) {
                return stream.index;
            }
        }
    }

    return -1;
}

void applyPreferredStreamRules(MediaSourceInfo &selectedSource,
                               const QString &audioRules,
                               const QString &subtitleRules) {
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

    if (bestAudioIdx >= 0) {
        for (MediaStreamInfo &stream : selectedSource.mediaStreams) {
            if (stream.type == QStringLiteral("Audio")) {
                stream.isDefault = (stream.index == bestAudioIdx);
            }
        }
    }

    QString subtitleDecision = QStringLiteral("keep-server-default");
    if (subtitleDisabled) {
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
               + QStringLiteral(" | selectedAudioIndex=%1").arg(bestAudioIdx)
               + QStringLiteral(" | subtitleRules=%1").arg(subtitleRules)
               + QStringLiteral(" | selectedSubtitleIndex=%1").arg(bestSubIdx)
               + QStringLiteral(" | subtitleDecision=%1").arg(subtitleDecision);
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
