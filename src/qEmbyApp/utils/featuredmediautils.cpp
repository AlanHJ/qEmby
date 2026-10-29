#include "featuredmediautils.h"

#include <QDebug>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QRandomGenerator>
#include <QSet>
#include <QSaveFile>
#include <QStandardPaths>
#include <algorithm>
#include <tuple>
#include <utility>

namespace {

QString selectionPath(const QString& serverId, const QString& userId)
{
    if (serverId.isEmpty() || userId.isEmpty()) return {};
    const QByteArray identity = QJsonDocument(QJsonArray{serverId, userId})
                                    .toJson(QJsonDocument::Compact);
    const QString bucket = QString::fromLatin1(
        QCryptographicHash::hash(identity, QCryptographicHash::Sha256).toHex());
    return QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
        QStringLiteral("/featured-selections/") + bucket + QStringLiteral(".json");
}

bool isSelectable(const MediaItem& item)
{
    return !item.id.isEmpty() && !item.images.backdropTag.isEmpty() &&
        (item.type == QLatin1String("Movie") || item.type == QLatin1String("Series"));
}

QString normalizedText(const QString& value)
{
    return value.normalized(QString::NormalizationForm_KC).simplified().toCaseFolded();
}

QMap<QString, QString> normalizedProviderIds(const QVariantMap& ids)
{
    QMap<QString, QString> result;
    for (auto it = ids.cbegin(); it != ids.cend(); ++it) {
        QString provider = normalizedText(it.key());
        if (provider == QLatin1String("themoviedb")) provider = QStringLiteral("tmdb");
        if (provider == QLatin1String("thetvdb")) provider = QStringLiteral("tvdb");
        const QString id = normalizedText(it.value().toString());
        if (!id.isEmpty() && id != QLatin1String("0")) result.insert(provider, id);
    }
    return result;
}

struct Candidate {
    MediaItem item;
    QSet<QString> itemIds;
    QMap<QString, QString> providerIds;
    QSet<QString> titles;
    QSet<int> productionYears;
    QString family;
    bool played = false;
    bool selected = false;
};

Candidate makeCandidate(const MediaItem& item)
{
    Candidate candidate;
    candidate.item = item;
    candidate.itemIds.insert(item.id);
    candidate.providerIds = normalizedProviderIds(item.providerIds);
    if (item.productionYear > 0) candidate.productionYears.insert(item.productionYear);
    for (const QString& title : {item.name, item.originalTitle}) {
        const QString normalized = normalizedText(title);
        if (!normalized.isEmpty()) candidate.titles.insert(normalized);
    }
    if (item.type == QLatin1String("Movie")) {
        const QString collection = candidate.providerIds.value(QStringLiteral("tmdbcollection"));
        if (!collection.isEmpty()) candidate.family = QStringLiteral("tmdbcollection:") + collection;
        else if (!item.collectionId.isEmpty()) candidate.family = QStringLiteral("collection:") + item.collectionId;
    }
    candidate.played = item.userData.played;
    return candidate;
}

bool sameWork(const Candidate& first, const Candidate& second)
{
    if (first.item.type != second.item.type) return false;
    if (first.itemIds.intersects(second.itemIds)) return true;
    bool sharedProvider = false;
    
    for (const QString& provider : {QStringLiteral("tmdb"), QStringLiteral("imdb"),
                                    QStringLiteral("tvdb")}) {
        if (!first.providerIds.contains(provider) || !second.providerIds.contains(provider)) continue;
        sharedProvider = true;
        if (first.providerIds.value(provider) == second.providerIds.value(provider)) return true;
    }
    
    
    return !sharedProvider && first.productionYears.intersects(second.productionYears) &&
        first.titles.intersects(second.titles);
}

int artworkQuality(const MediaItem& item)
{
    return (!item.images.logoTag.isEmpty() ? 2 : 0) +
           (!item.overview.trimmed().isEmpty() ? 1 : 0);
}

void mergeVersions(Candidate& target, const Candidate& version)
{
    target.itemIds.unite(version.itemIds);
    target.titles.unite(version.titles);
    target.productionYears.unite(version.productionYears);
    for (auto it = version.providerIds.cbegin(); it != version.providerIds.cend(); ++it) {
        if (!target.providerIds.contains(it.key())) target.providerIds.insert(it.key(), it.value());
    }
    
    
    target.played = target.played || version.played;
    if (target.family.isEmpty()) target.family = version.family;
    if (artworkQuality(version.item) > artworkQuality(target.item)) target.item = version.item;
}

} 

FeaturedMediaUtils::DailySelection FeaturedMediaUtils::loadSelection(
    const QString& serverId, const QString& userId)
{
    const QString path = selectionPath(serverId, userId);
    if (path.isEmpty()) return {};
    QFile file(path);
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly) || file.size() > 64 * 1024) {
        qWarning() << "[FeaturedMedia] Cannot read daily selection cache";
        return {};
    }
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    DailySelection selection;
    selection.date = QDate::fromString(root.value("date").toString(), Qt::ISODate);
    const QJsonArray ids = root.value("ids").toArray();
    if (root.value("version").toInt() != 1 || !selection.date.isValid() ||
        !root.value("ids").isArray() || ids.size() > MaximumSlides) {
        qWarning() << "[FeaturedMedia] Ignoring invalid daily selection cache";
        return {};
    }
    for (const QJsonValue& value : ids) {
        const QString id = value.toString();
        if (id.isEmpty() || selection.itemIds.contains(id)) {
            qWarning() << "[FeaturedMedia] Ignoring invalid cached media IDs";
            return {};
        }
        selection.itemIds.append(id);
    }
    qDebug() << "[FeaturedMedia] Restored selection | date=" << selection.date
             << "| items=" << selection.itemIds.size();
    return selection;
}

bool FeaturedMediaUtils::saveSelection(const QString& serverId, const QString& userId,
                                      const DailySelection& selection)
{
    const QString path = selectionPath(serverId, userId);
    if (path.isEmpty() || !selection.date.isValid()) return false;
    QJsonArray ids;
    for (const QString& id : selection.itemIds.mid(0, MaximumSlides)) ids.append(id);
    const QByteArray data = QJsonDocument(QJsonObject{
        {QStringLiteral("version"), 1},
        {QStringLiteral("date"), selection.date.toString(Qt::ISODate)},
        {QStringLiteral("ids"), ids}}).toJson(QJsonDocument::Compact);
    QSaveFile file(path);
    if (!QDir().mkpath(QFileInfo(path).absolutePath()) ||
        !file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
        qWarning() << "[FeaturedMedia] Failed to persist daily selection";
        return false;
    }
    qDebug() << "[FeaturedMedia] Saved selection | date=" << selection.date
             << "| items=" << ids.size();
    return true;
}

QStringList FeaturedMediaUtils::itemIds(const QList<MediaItem>& items)
{
    QStringList ids;
    for (const MediaItem& item : items) ids.append(item.id);
    return ids;
}

QList<MediaItem> FeaturedMediaUtils::orderedItems(
    const QStringList& ids, const QList<MediaItem>& available)
{
    QHash<QString, MediaItem> byId;
    for (const MediaItem& item : available) {
        if (isSelectable(item)) byId.insert(item.id, item);
    }
    QList<MediaItem> ordered;
    for (const QString& id : ids.mid(0, MaximumSlides)) {
        if (byId.contains(id)) ordered.append(byId.take(id));
    }
    return ordered;
}

QList<MediaItem> FeaturedMediaUtils::fillVacancies(
    const QStringList& ids, const QList<MediaItem>& available,
    const QList<MediaItem>& candidates, int limit)
{
    const int slideLimit = qBound(0, limit, MaximumSlides);
    const QStringList retainedIds = ids.mid(0, slideLimit);
    const QList<MediaItem> retained = orderedItems(retainedIds, available);
    const QList<MediaItem> selected = selectItems(candidates, slideLimit, retained);
    QList<MediaItem> replacements = selected.mid(retained.size());
    QHash<QString, MediaItem> byId;
    for (const MediaItem& item : retained) byId.insert(item.id, item);
    QList<MediaItem> result;
    for (const QString& id : retainedIds) {
        if (byId.contains(id)) result.append(byId.take(id));
        else if (!replacements.isEmpty()) result.append(replacements.takeFirst());
    }
    result.append(replacements);
    return result;
}

QList<MediaItem> FeaturedMediaUtils::selectItems(
    const QList<MediaItem>& candidates, int limit, const QList<MediaItem>& retained,
    const QStringList& previousIds)
{
    const int slideLimit = qBound(0, limit, MaximumSlides);
    if (slideLimit == 0) return {};
    QList<Candidate> unique;
    int usable = 0;
    for (const MediaItem& item : candidates) {
        if (!isSelectable(item)) continue;
        ++usable;
        Candidate candidate = makeCandidate(item);
        
        
        for (int i = 0; i < unique.size();) {
            if (!sameWork(candidate, unique.at(i))) {
                ++i;
                continue;
            }
            mergeVersions(candidate, unique.at(i));
            unique.removeAt(i);
            
            i = 0;
        }
        unique.append(std::move(candidate));
    }
    std::shuffle(unique.begin(), unique.end(), *QRandomGenerator::global());

    QList<MediaItem> selected;
    QHash<QString, int> familyCounts;
    QHash<QString, int> typeCounts;
    int selectedUnplayed = 0;
    for (const MediaItem& item : retained) {
        if (!isSelectable(item) || selected.size() == slideLimit) continue;
        const Candidate kept = makeCandidate(item);
        selected.append(item);
        ++typeCounts[item.type];
        if (!kept.family.isEmpty()) ++familyCounts[kept.family];
        if (!kept.played) ++selectedUnplayed;
        for (Candidate& candidate : unique) {
            if (sameWork(candidate, kept)) candidate.selected = true;
        }
    }
    const QSet<QString> previous(previousIds.cbegin(), previousIds.cend());
    while (selected.size() < slideLimit) {
        int best = -1;
        std::tuple<bool, bool, int, int> bestPriority;
        for (int i = 0; i < unique.size(); ++i) {
            const Candidate& candidate = unique.at(i);
            if (candidate.selected) continue;
            
            
            
            const auto priority = std::make_tuple(candidate.played, candidate.itemIds.intersects(previous),
                candidate.family.isEmpty() ? 0 : familyCounts.value(candidate.family),
                typeCounts.value(candidate.item.type));
            if (best < 0 || priority < bestPriority) {
                best = i;
                bestPriority = priority;
            }
        }
        if (best < 0) break;
        Candidate& candidate = unique[best];
        candidate.selected = true;
        selected.append(candidate.item);
        ++typeCounts[candidate.item.type];
        if (!candidate.family.isEmpty()) ++familyCounts[candidate.family];
        if (!candidate.played) ++selectedUnplayed;
        qDebug() << "[FeaturedMedia] Selected candidate | itemId=" << candidate.item.id
                 << "| type=" << candidate.item.type << "| workPlayed=" << candidate.played
                 << "| versions=" << candidate.itemIds.size()
                 << "| hasFamily=" << !candidate.family.isEmpty();
    }
    qDebug() << "[FeaturedMedia] Selection complete | candidates=" << candidates.size()
             << "| usable=" << usable << "| uniqueWorks=" << unique.size()
             << "| duplicates=" << usable - unique.size() << "| selected=" << selected.size()
             << "| unplayed=" << selectedUnplayed
             << "| movies=" << typeCounts.value(QStringLiteral("Movie"))
             << "| series=" << typeCounts.value(QStringLiteral("Series"));
    return selected;
}
