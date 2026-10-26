#include "mediapartutils.h"
#include "mediaitemutils.h"

#include <QSet>
#include <QDebug>
#include <QHash>
#include <QRegularExpression>
#include <algorithm>

namespace {

int chapterNumberFromText(const QString &text)
{
    static const QRegularExpression marker(
        QStringLiteral("(?:^|[\\s._\\-\\[\\]()])(?:cd|disc|disk|part|pt)[\\s._-]*(\\d+)(?=$|[\\s._\\-\\[\\]()])"),
        QRegularExpression::CaseInsensitiveOption);
    auto matches = marker.globalMatch(text);
    int number = -1;
    while (matches.hasNext()) {
        bool ok = false;
        const int candidate = matches.next().captured(1).toInt(&ok);
        if (ok && candidate > 0) number = candidate;
    }
    return number;
}

int chapterNumber(const MediaItem &item)
{
    
    
    QStringList paths{item.path};
    for (const MediaSourceInfo &source : item.mediaSources) paths.append(source.path);
    for (QString path : paths) {
        if (path.contains(QStringLiteral("://"))) continue;
        path.replace(QLatin1Char('\\'), QLatin1Char('/'));
        const int number = chapterNumberFromText(path.section(QLatin1Char('/'), -1));
        if (number > 0) return number;
    }
    return chapterNumberFromText(item.name);
}

} 

QList<MediaItem> MediaPartUtils::sequence(const MediaItem &primary,
                                         const QList<MediaItem> &additionalParts)
{
    if (primary.id.isEmpty()) return {};

    QList<MediaItem> parts{primary};
    QSet<QString> ids{primary.id};
    for (const MediaItem &part : additionalParts) {
        if (part.id.isEmpty() || ids.contains(part.id)) continue;
        ids.insert(part.id);
        parts.append(part);
    }
    if (parts.size() < 2) return {};

    QHash<QString, int> numbers;
    bool allNumbered = true;
    for (const MediaItem &part : parts) {
        const int number = chapterNumber(part);
        numbers.insert(part.id, number);
        allNumbered = allNumbered && number > 0;
    }
    if (allNumbered) {
        std::stable_sort(parts.begin(), parts.end(), [&numbers](const MediaItem &left, const MediaItem &right) {
            return numbers.value(left.id) < numbers.value(right.id);
        });
    }
    QStringList orderedIds;
    for (const MediaItem &part : parts) orderedIds.append(part.id);
    qDebug() << "[MediaPartUtils] Chapter sequence resolved"
             << "| ownerId=" << primary.id << "| ownerChapter=" << numbers.value(primary.id)
             << "| order=" << (allNumbered ? "chapter-number" : "server")
             << "| itemIds=" << orderedIds;
    return parts;
}

int MediaPartUtils::indexOf(const QList<MediaItem> &parts, const QString &mediaId)
{
    for (int i = 0; i < parts.size(); ++i) {
        if (parts.at(i).id == mediaId) return i;
    }
    return -1;
}

QString MediaPartUtils::displayTitle(const QList<MediaItem> &parts, int index)
{
    if (index < 0 || index >= parts.size()) return {};
    const QString title = MediaItemUtils::playbackTitle(parts.at(index));
    const QString partLabel = tr("Chapter %1 / %2").arg(index + 1).arg(parts.size());
    return title.isEmpty() ? partLabel : tr("%1 — %2").arg(partLabel, title);
}

long long MediaPartUtils::resumeTicks(const MediaItem &part)
{
    const long long ticks = part.userData.playbackPositionTicks;
    return ticks > 0 && (part.runTimeTicks <= 0 || ticks < part.runTimeTicks) ? ticks : 0;
}
