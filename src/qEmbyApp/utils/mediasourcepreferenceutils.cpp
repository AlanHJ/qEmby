#include "mediasourcepreferenceutils.h"

#include <QDateTime>
#include <QDebug>
#include <QFileInfo>
#include <QHash>
#include <QRegularExpression>
#include <algorithm>

namespace {

enum class SortCriterion {
    BitrateDesc,
    BitrateAsc,
    SizeDesc,
    SizeAsc,
    DateDesc,
    DateAsc,
};

struct ParsedRule {
    bool isSort = false;
    QString keyword;
    SortCriterion sortCriterion = SortCriterion::BitrateDesc;
};

QString normalizeRuleToken(QString token) {
    token = token.trimmed();
    token.replace(QChar(0xFF0C), QChar(','));
    token.replace(QChar(0xFF1B), QChar(','));
    token.replace(QChar(0x3000), QChar(' '));
    token.replace(QChar(0xFF08), QChar('('));
    token.replace(QChar(0xFF09), QChar(')'));
    token.replace(QLatin1Char('_'), QLatin1Char('-'));
    return token.simplified().toLower();
}

const QHash<QString, SortCriterion> &sortRuleAliases() {
    static const QHash<QString, SortCriterion> aliases = {
        {QStringLiteral("bitrate-desc"), SortCriterion::BitrateDesc},
        {QStringLiteral("bitrate-high"), SortCriterion::BitrateDesc},
        {QStringLiteral("bitrate-high-to-low"), SortCriterion::BitrateDesc},
        {QStringLiteral("bitrate-from-high-to-low"),
         SortCriterion::BitrateDesc},
        {QStringLiteral("video-bitrate-desc"), SortCriterion::BitrateDesc},
        {QStringLiteral("码率从高到低"), SortCriterion::BitrateDesc},
        {QStringLiteral("码率高到低"), SortCriterion::BitrateDesc},
        {QStringLiteral("码率降序"), SortCriterion::BitrateDesc},
        {QStringLiteral("码率最高优先"), SortCriterion::BitrateDesc},
        {QStringLiteral("bitrate-asc"), SortCriterion::BitrateAsc},
        {QStringLiteral("bitrate-low"), SortCriterion::BitrateAsc},
        {QStringLiteral("bitrate-low-to-high"), SortCriterion::BitrateAsc},
        {QStringLiteral("bitrate-from-low-to-high"), SortCriterion::BitrateAsc},
        {QStringLiteral("video-bitrate-asc"), SortCriterion::BitrateAsc},
        {QStringLiteral("码率从低到高"), SortCriterion::BitrateAsc},
        {QStringLiteral("码率低到高"), SortCriterion::BitrateAsc},
        {QStringLiteral("码率升序"), SortCriterion::BitrateAsc},
        {QStringLiteral("码率最低优先"), SortCriterion::BitrateAsc},
        {QStringLiteral("size-desc"), SortCriterion::SizeDesc},
        {QStringLiteral("size-large"), SortCriterion::SizeDesc},
        {QStringLiteral("size-large-to-small"), SortCriterion::SizeDesc},
        {QStringLiteral("file-size-desc"), SortCriterion::SizeDesc},
        {QStringLiteral("文件大小从大到小"), SortCriterion::SizeDesc},
        {QStringLiteral("文件大小大到小"), SortCriterion::SizeDesc},
        {QStringLiteral("大小从大到小"), SortCriterion::SizeDesc},
        {QStringLiteral("大小降序"), SortCriterion::SizeDesc},
        {QStringLiteral("size-asc"), SortCriterion::SizeAsc},
        {QStringLiteral("size-small"), SortCriterion::SizeAsc},
        {QStringLiteral("size-small-to-large"), SortCriterion::SizeAsc},
        {QStringLiteral("file-size-asc"), SortCriterion::SizeAsc},
        {QStringLiteral("文件大小从小到大"), SortCriterion::SizeAsc},
        {QStringLiteral("文件大小小到大"), SortCriterion::SizeAsc},
        {QStringLiteral("大小从小到大"), SortCriterion::SizeAsc},
        {QStringLiteral("大小升序"), SortCriterion::SizeAsc},
        {QStringLiteral("date-desc"), SortCriterion::DateDesc},
        {QStringLiteral("date-newest"), SortCriterion::DateDesc},
        {QStringLiteral("date-new-to-old"), SortCriterion::DateDesc},
        {QStringLiteral("updated-desc"), SortCriterion::DateDesc},
        {QStringLiteral("modified-desc"), SortCriterion::DateDesc},
        {QStringLiteral("日期从新到旧"), SortCriterion::DateDesc},
        {QStringLiteral("日期从旧到新"), SortCriterion::DateAsc},
        {QStringLiteral("日期最新优先"), SortCriterion::DateDesc},
        {QStringLiteral("更新日期从新到旧"), SortCriterion::DateDesc},
        {QStringLiteral("更新时间从新到旧"), SortCriterion::DateDesc},
        {QStringLiteral("date-asc"), SortCriterion::DateAsc},
        {QStringLiteral("date-oldest"), SortCriterion::DateAsc},
        {QStringLiteral("date-old-to-new"), SortCriterion::DateAsc},
        {QStringLiteral("updated-asc"), SortCriterion::DateAsc},
        {QStringLiteral("modified-asc"), SortCriterion::DateAsc},
        {QStringLiteral("日期最旧优先"), SortCriterion::DateAsc},
        {QStringLiteral("更新日期从旧到新"), SortCriterion::DateAsc},
        {QStringLiteral("更新时间从旧到新"), SortCriterion::DateAsc},
    };

    return aliases;
}

ParsedRule parseRule(const QString &rawToken) {
    const QString normalizedToken = normalizeRuleToken(rawToken);
    const auto sortIt = sortRuleAliases().constFind(normalizedToken);
    if (sortIt != sortRuleAliases().constEnd()) {
        ParsedRule rule;
        rule.isSort = true;
        rule.sortCriterion = *sortIt;
        return rule;
    }

    ParsedRule rule;
    rule.keyword = normalizedToken;
    return rule;
}

const MediaStreamInfo *findPrimaryVideoStream(const MediaSourceInfo &source) {
    for (const MediaStreamInfo &stream : source.mediaStreams) {
        if (stream.type == QStringLiteral("Video")) {
            return &stream;
        }
    }
    return nullptr;
}

QString buildSourceSearchText(const MediaSourceInfo &source) {
    QStringList parts;
    parts << source.name.trimmed() << source.path.trimmed()
          << QFileInfo(source.path).fileName().trimmed()
          << source.container.trimmed();

    if (const MediaStreamInfo *videoStream = findPrimaryVideoStream(source)) {
        parts << videoStream->displayTitle.trimmed() << videoStream->title.trimmed()
              << videoStream->codec.trimmed() << videoStream->profile.trimmed();
        if (videoStream->width > 0 && videoStream->height > 0) {
            parts << QStringLiteral("%1x%2")
                         .arg(videoStream->width)
                         .arg(videoStream->height);
        }
        if (videoStream->height > 0) {
            parts << QStringLiteral("%1p").arg(videoStream->height);
        }
        if (videoStream->width >= 3840 || videoStream->height >= 2160) {
            parts << QStringLiteral("4k");
        }
    }

    QString searchText = parts.join(QLatin1Char(' '));
    searchText.replace(QChar(0x3000), QChar(' '));
    return searchText.simplified().toLower();
}

bool sourceMatchesKeyword(const MediaSourceInfo &source, const QString &keyword) {
    if (keyword.isEmpty()) {
        return false;
    }

    return buildSourceSearchText(source).contains(keyword);
}

qint64 sourceVideoBitrate(const MediaSourceInfo &source, bool *hasValue) {
    if (const MediaStreamInfo *videoStream = findPrimaryVideoStream(source)) {
        if (videoStream->bitRate > 0) {
            if (hasValue) {
                *hasValue = true;
            }
            return videoStream->bitRate;
        }
    }

    if (hasValue) {
        *hasValue = false;
    }
    return 0;
}

qint64 sourceFileSize(const MediaSourceInfo &source, bool *hasValue) {
    if (source.size > 0) {
        if (hasValue) {
            *hasValue = true;
        }
        return source.size;
    }

    if (hasValue) {
        *hasValue = false;
    }
    return 0;
}

QDateTime sourceModifiedDate(const MediaSourceInfo &source, bool *hasValue) {
    if (source.dateModified.isValid()) {
        if (hasValue) {
            *hasValue = true;
        }
        return source.dateModified;
    }

    if (source.dateCreated.isValid()) {
        if (hasValue) {
            *hasValue = true;
        }
        return source.dateCreated;
    }

    const QFileInfo fileInfo(source.path);
    if (fileInfo.exists()) {
        const QDateTime modified = fileInfo.lastModified();
        if (modified.isValid()) {
            if (hasValue) {
                *hasValue = true;
            }
            return modified;
        }
    }

    if (hasValue) {
        *hasValue = false;
    }
    return {};
}

QString criterionName(SortCriterion criterion) {
    switch (criterion) {
    case SortCriterion::BitrateDesc:
        return QStringLiteral("bitrate-desc");
    case SortCriterion::BitrateAsc:
        return QStringLiteral("bitrate-asc");
    case SortCriterion::SizeDesc:
        return QStringLiteral("size-desc");
    case SortCriterion::SizeAsc:
        return QStringLiteral("size-asc");
    case SortCriterion::DateDesc:
        return QStringLiteral("date-desc");
    case SortCriterion::DateAsc:
        return QStringLiteral("date-asc");
    }

    return QStringLiteral("unknown");
}

int compareByCriterion(const MediaSourceInfo &left, const MediaSourceInfo &right,
                       SortCriterion criterion, bool *criterionWasUseful) {
    auto compareNumbers = [criterion, criterionWasUseful](qint64 leftValue,
                                                          bool leftValid,
                                                          qint64 rightValue,
                                                          bool rightValid) {
        if (!leftValid && !rightValid) {
            return 0;
        }
        if (criterionWasUseful) {
            *criterionWasUseful = true;
        }
        if (leftValid != rightValid) {
            return leftValid ? -1 : 1;
        }
        if (leftValue == rightValue) {
            return 0;
        }

        const bool descending = criterion == SortCriterion::BitrateDesc ||
                                criterion == SortCriterion::SizeDesc;
        if (descending) {
            return leftValue > rightValue ? -1 : 1;
        }
        return leftValue < rightValue ? -1 : 1;
    };

    if (criterion == SortCriterion::BitrateDesc ||
        criterion == SortCriterion::BitrateAsc) {
        bool leftValid = false;
        bool rightValid = false;
        return compareNumbers(sourceVideoBitrate(left, &leftValid), leftValid,
                              sourceVideoBitrate(right, &rightValid), rightValid);
    }

    if (criterion == SortCriterion::SizeDesc ||
        criterion == SortCriterion::SizeAsc) {
        bool leftValid = false;
        bool rightValid = false;
        return compareNumbers(sourceFileSize(left, &leftValid), leftValid,
                              sourceFileSize(right, &rightValid), rightValid);
    }

    bool leftValid = false;
    bool rightValid = false;
    const QDateTime leftDate = sourceModifiedDate(left, &leftValid);
    const QDateTime rightDate = sourceModifiedDate(right, &rightValid);
    if (!leftValid && !rightValid) {
        return 0;
    }
    if (criterionWasUseful) {
        *criterionWasUseful = true;
    }
    if (leftValid != rightValid) {
        return leftValid ? -1 : 1;
    }
    if (leftDate == rightDate) {
        return 0;
    }

    if (criterion == SortCriterion::DateDesc) {
        return leftDate > rightDate ? -1 : 1;
    }
    return leftDate < rightDate ? -1 : 1;
}

QList<int> sortedCandidates(const QList<MediaSourceInfo> &mediaSources,
                            QList<int> candidates,
                            const QList<SortCriterion> &sortCriteria,
                            bool *usedMeaningfulSort) {
    if (usedMeaningfulSort) {
        *usedMeaningfulSort = false;
    }
    if (sortCriteria.isEmpty() || candidates.size() <= 1) {
        return candidates;
    }

    bool meaningfulSortDetected = false;
    for (const SortCriterion criterion : sortCriteria) {
        for (int leftPos = 0; leftPos < candidates.size() && !meaningfulSortDetected;
             ++leftPos) {
            for (int rightPos = leftPos + 1;
                 rightPos < candidates.size() && !meaningfulSortDetected;
                 ++rightPos) {
                bool criterionWasUseful = false;
                const int result = compareByCriterion(
                    mediaSources[candidates[leftPos]],
                    mediaSources[candidates[rightPos]], criterion,
                    &criterionWasUseful);
                if (criterionWasUseful && result != 0) {
                    meaningfulSortDetected = true;
                }
            }
        }
    }

    if (usedMeaningfulSort) {
        *usedMeaningfulSort = meaningfulSortDetected;
    }

    std::stable_sort(candidates.begin(), candidates.end(),
                     [&mediaSources, &sortCriteria](int leftIndex,
                                                    int rightIndex) {
                         const MediaSourceInfo &left = mediaSources[leftIndex];
                         const MediaSourceInfo &right = mediaSources[rightIndex];

                         for (const SortCriterion criterion : sortCriteria) {
                             bool criterionWasUseful = false;
                             const int result = compareByCriterion(
                                 left, right, criterion, &criterionWasUseful);
                             if (result < 0) {
                                 return true;
                             }
                             if (result > 0) {
                                 return false;
                             }
                         }

                         return leftIndex < rightIndex;
                     });

    return candidates;
}

QList<SortCriterion> collectAllSortCriteria(const QList<ParsedRule> &rules) {
    QList<SortCriterion> result;
    for (const ParsedRule &rule : rules) {
        if (rule.isSort) {
            result.append(rule.sortCriterion);
        }
    }
    return result;
}

QList<SortCriterion> collectFollowingSortCriteria(const QList<ParsedRule> &rules,
                                                  int startIndex) {
    QList<SortCriterion> result;
    for (int i = startIndex + 1; i < rules.size(); ++i) {
        if (!rules[i].isSort) {
            break;
        }
        result.append(rules[i].sortCriterion);
    }
    return result;
}

} 

namespace MediaSourcePreferenceUtils {

QStringList splitPreferredVersionRules(const QString &rawRules) {
    QString normalizedRules = rawRules;
    normalizedRules.replace(QChar(0xFF0C), QChar(','));
    normalizedRules.replace(QChar(0xFF1B), QChar(','));

    QStringList result;
    const QStringList parts =
        normalizedRules.split(QLatin1Char(','), Qt::SkipEmptyParts);
    for (const QString &part : parts) {
        const QString normalized = normalizeRuleToken(part);
        if (normalized.isEmpty()) {
            continue;
        }

        bool exists = false;
        for (const QString &existing : result) {
            if (existing.compare(normalized, Qt::CaseInsensitive) == 0) {
                exists = true;
                break;
            }
        }

        if (!exists) {
            result.append(normalized);
        }
    }
    return result;
}

int resolvePreferredMediaSourceIndex(const QList<MediaSourceInfo> &mediaSources,
                                     const QString &rawRules) {
    if (mediaSources.size() <= 1) {
        return 0;
    }

    const QStringList tokens = splitPreferredVersionRules(rawRules);
    if (tokens.isEmpty()) {
        return 0;
    }

    QList<ParsedRule> rules;
    for (const QString &token : tokens) {
        rules.append(parseRule(token));
    }

    const QList<SortCriterion> globalSortCriteria = collectAllSortCriteria(rules);

    for (int i = 0; i < rules.size(); ++i) {
        const ParsedRule &rule = rules[i];
        if (!rule.isSort) {
            QList<int> matches;
            for (int sourceIndex = 0; sourceIndex < mediaSources.size();
                 ++sourceIndex) {
                if (sourceMatchesKeyword(mediaSources[sourceIndex], rule.keyword)) {
                    matches.append(sourceIndex);
                }
            }

            if (matches.isEmpty()) {
                continue;
            }

            QList<SortCriterion> sortCriteria =
                collectFollowingSortCriteria(rules, i);
            if (sortCriteria.isEmpty()) {
                sortCriteria = globalSortCriteria;
            }

            bool usedMeaningfulSort = false;
            const QList<int> orderedMatches = sortedCandidates(
                mediaSources, matches, sortCriteria, &usedMeaningfulSort);
            const int chosenIndex =
                orderedMatches.isEmpty() ? matches.first() : orderedMatches.first();

            QStringList sortNames;
            for (const SortCriterion criterion : sortCriteria) {
                sortNames.append(criterionName(criterion));
            }
            qDebug().noquote()
                << QStringLiteral("[MediaSourcePreferenceUtils] Preferred source matched by keyword")
                       + QStringLiteral(" | rules=%1").arg(rawRules)
                       + QStringLiteral(" | keyword=%1").arg(rule.keyword)
                       + QStringLiteral(" | matchedCount=%1").arg(matches.size())
                       + QStringLiteral(" | sortCriteria=%1").arg(sortNames.join(','))
                       + QStringLiteral(" | selectedIndex=%1").arg(chosenIndex)
                       + QStringLiteral(" | selectedName=%1")
                             .arg(mediaSources[chosenIndex].name);
            return chosenIndex;
        }

        QList<SortCriterion> sortCriteria = collectFollowingSortCriteria(rules, i);
        sortCriteria.prepend(rule.sortCriterion);

        bool usedMeaningfulSort = false;
        QList<int> candidates;
        for (int sourceIndex = 0; sourceIndex < mediaSources.size(); ++sourceIndex) {
            candidates.append(sourceIndex);
        }

        const QList<int> orderedCandidates = sortedCandidates(
            mediaSources, candidates, sortCriteria, &usedMeaningfulSort);
        if (orderedCandidates.isEmpty()) {
            continue;
        }

        if (!usedMeaningfulSort && mediaSources.size() > 1) {
            qDebug().noquote()
                << QStringLiteral("[MediaSourcePreferenceUtils] Sort rule skipped because metadata is unavailable")
                       + QStringLiteral(" | rules=%1").arg(rawRules)
                       + QStringLiteral(" | sort=%1")
                             .arg(criterionName(rule.sortCriterion));
            continue;
        }

        QStringList sortNames;
        for (const SortCriterion criterion : sortCriteria) {
            sortNames.append(criterionName(criterion));
        }
        qDebug().noquote()
            << QStringLiteral("[MediaSourcePreferenceUtils] Preferred source matched by sort")
                   + QStringLiteral(" | rules=%1").arg(rawRules)
                   + QStringLiteral(" | sortCriteria=%1").arg(sortNames.join(','))
                   + QStringLiteral(" | selectedIndex=%1")
                         .arg(orderedCandidates.first())
                   + QStringLiteral(" | selectedName=%1")
                         .arg(mediaSources[orderedCandidates.first()].name);
        return orderedCandidates.first();
    }

    return 0;
}

} 
