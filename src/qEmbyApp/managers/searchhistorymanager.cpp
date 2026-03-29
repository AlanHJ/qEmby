#include "searchhistorymanager.h"

#include "../../qEmbyCore/config/config_keys.h"
#include "../../qEmbyCore/config/configstore.h"
#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <algorithm>

namespace
{

constexpr int kMaxStoredSearchEntries = 100;

bool isBetterRecentMatch(const SearchHistoryManager::SearchHistoryEntry &lhs,
                         const SearchHistoryManager::SearchHistoryEntry &rhs, const QString &normalizedPrefix)
{
    const bool lhsStartsWith = lhs.normalizedTerm.startsWith(normalizedPrefix);
    const bool rhsStartsWith = rhs.normalizedTerm.startsWith(normalizedPrefix);
    if (lhsStartsWith != rhsStartsWith)
    {
        return lhsStartsWith;
    }
    if (lhs.lastSearchedAtMs != rhs.lastSearchedAtMs)
    {
        return lhs.lastSearchedAtMs > rhs.lastSearchedAtMs;
    }
    if (lhs.searchCount != rhs.searchCount)
    {
        return lhs.searchCount > rhs.searchCount;
    }
    return lhs.term.localeAwareCompare(rhs.term) < 0;
}

bool isBetterFrequentMatch(const SearchHistoryManager::SearchHistoryEntry &lhs,
                           const SearchHistoryManager::SearchHistoryEntry &rhs, const QString &normalizedPrefix)
{
    const bool lhsStartsWith = lhs.normalizedTerm.startsWith(normalizedPrefix);
    const bool rhsStartsWith = rhs.normalizedTerm.startsWith(normalizedPrefix);
    if (lhsStartsWith != rhsStartsWith)
    {
        return lhsStartsWith;
    }
    if (lhs.searchCount != rhs.searchCount)
    {
        return lhs.searchCount > rhs.searchCount;
    }
    if (lhs.lastSearchedAtMs != rhs.lastSearchedAtMs)
    {
        return lhs.lastSearchedAtMs > rhs.lastSearchedAtMs;
    }
    return lhs.term.localeAwareCompare(rhs.term) < 0;
}

QStringList takeTerms(const QList<SearchHistoryManager::SearchHistoryEntry> &entries, int limit)
{
    QStringList result;
    result.reserve(qMin(limit, entries.size()));

    QSet<QString> seenTerms;
    for (const auto &entry : entries)
    {
        if (entry.term.isEmpty() || seenTerms.contains(entry.normalizedTerm))
        {
            continue;
        }
        result.append(entry.term);
        seenTerms.insert(entry.normalizedTerm);
        if (result.size() >= limit)
        {
            break;
        }
    }
    return result;
}

} 

SearchHistoryManager *SearchHistoryManager::instance()
{
    static SearchHistoryManager *s_instance = new SearchHistoryManager(qApp);
    return s_instance;
}

SearchHistoryManager::SearchHistoryManager(QObject *parent) : QObject(parent)
{
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &value)
            {
                if (key == ConfigKeys::SearchHistoryEnabled)
                {
                    qDebug() << "[SearchHistoryManager] Search history feature toggled | enabled:" << value.toBool();
                    Q_EMIT enabledChanged(value.toBool());
                    Q_EMIT autocompleteEnabledChanged(isAutocompleteEnabled());
                }
                else if (key == ConfigKeys::SearchAutocompleteEnabled)
                {
                    qDebug() << "[SearchHistoryManager] Search autocomplete toggled | requestedEnabled:"
                             << value.toBool() << "| effectiveEnabled:" << isAutocompleteEnabled();
                    Q_EMIT autocompleteEnabledChanged(isAutocompleteEnabled());
                }
            });
}

bool SearchHistoryManager::isEnabled() const
{
    return ConfigStore::instance()->get<bool>(ConfigKeys::SearchHistoryEnabled, false);
}

bool SearchHistoryManager::isAutocompleteEnabled() const
{
    return isEnabled() && ConfigStore::instance()->get<bool>(
                              ConfigKeys::SearchAutocompleteEnabled, false);
}

void SearchHistoryManager::recordSearch(const QString &serverId, QString query)
{
    if (!isEnabled())
    {
        return;
    }

    query = query.trimmed();
    const QString normalizedQuery = normalizeQuery(query);
    if (normalizedQuery.isEmpty())
    {
        return;
    }

    const QString bucket = effectiveServerBucket(serverId);
    ensureLoaded(bucket);

    auto &entries = m_cache[bucket];
    auto it = std::find_if(entries.begin(), entries.end(), [&normalizedQuery](const SearchHistoryEntry &entry)
                           { return entry.normalizedTerm == normalizedQuery; });

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (it != entries.end())
    {
        it->term = query;
        it->searchCount += 1;
        it->lastSearchedAtMs = nowMs;
    }
    else
    {
        SearchHistoryEntry entry;
        entry.term = query;
        entry.normalizedTerm = normalizedQuery;
        entry.searchCount = 1;
        entry.lastSearchedAtMs = nowMs;
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(),
              [](const SearchHistoryEntry &lhs, const SearchHistoryEntry &rhs)
              {
                  if (lhs.lastSearchedAtMs != rhs.lastSearchedAtMs)
                  {
                      return lhs.lastSearchedAtMs > rhs.lastSearchedAtMs;
                  }
                  if (lhs.searchCount != rhs.searchCount)
                  {
                      return lhs.searchCount > rhs.searchCount;
                  }
                  return lhs.term.localeAwareCompare(rhs.term) < 0;
              });

    if (entries.size() > kMaxStoredSearchEntries)
    {
        entries.erase(entries.begin() + kMaxStoredSearchEntries, entries.end());
    }

    saveEntries(bucket, entries);
    qDebug() << "[SearchHistoryManager] Search recorded | bucket:" << bucket << "| totalEntries:" << entries.size();
    Q_EMIT historyChanged(bucket);
}

void SearchHistoryManager::clearHistory(const QString &serverId)
{
    const QString bucket = effectiveServerBucket(serverId);
    m_loadedBuckets.insert(bucket);
    m_cache.insert(bucket, {});
    saveEntries(bucket, {});
    qDebug() << "[SearchHistoryManager] History cleared | bucket:" << bucket;
    Q_EMIT historyChanged(bucket);
}

void SearchHistoryManager::clearAllHistory()
{
    const QStringList buckets = allHistoryBuckets();
    if (buckets.isEmpty())
    {
        qDebug() << "[SearchHistoryManager] Clear all history requested | no history buckets found";
        return;
    }

    for (const QString &bucket : buckets)
    {
        m_loadedBuckets.insert(bucket);
        m_cache.insert(bucket, {});
        saveEntries(bucket, {});
        Q_EMIT historyChanged(bucket);
    }

    qDebug() << "[SearchHistoryManager] Cleared history for all buckets | count:" << buckets.size();
}

void SearchHistoryManager::removeHistoryTerm(const QString &serverId, QString term)
{
    term = term.trimmed();
    const QString normalizedTerm = normalizeQuery(term);
    if (normalizedTerm.isEmpty())
    {
        return;
    }

    const QString bucket = effectiveServerBucket(serverId);
    ensureLoaded(bucket);

    auto &entries = m_cache[bucket];
    const auto newEnd = std::remove_if(entries.begin(), entries.end(),
                                       [&normalizedTerm](const SearchHistoryEntry &entry) {
                                           return entry.normalizedTerm == normalizedTerm;
                                       });
    if (newEnd == entries.end())
    {
        return;
    }

    entries.erase(newEnd, entries.end());
    saveEntries(bucket, entries);
    qDebug() << "[SearchHistoryManager] History item removed | bucket:" << bucket
             << "| term:" << term << "| remaining:" << entries.size();
    Q_EMIT historyChanged(bucket);
}

QStringList SearchHistoryManager::completionSuggestions(const QString &serverId, const QString &prefix, int limit) const
{
    if (!isAutocompleteEnabled())
    {
        return {};
    }

    auto matches = filteredEntries(serverId, prefix);
    const QString normalizedPrefix = normalizeQuery(prefix);
    std::sort(matches.begin(), matches.end(),
              [&normalizedPrefix](const SearchHistoryEntry &lhs, const SearchHistoryEntry &rhs)
              { return isBetterRecentMatch(lhs, rhs, normalizedPrefix); });
    return takeTerms(matches, limit);
}

QList<SearchHistoryManager::SearchHistoryEntry>
SearchHistoryManager::historyEntries(const QString &serverId,
                                     const QString &prefix) const
{
    if (!isEnabled())
    {
        return {};
    }

    return filteredEntries(serverId, prefix);
}

QStringList SearchHistoryManager::recentSearches(const QString &serverId, int limit, const QString &prefix) const
{
    if (!isEnabled())
    {
        return {};
    }

    auto matches = filteredEntries(serverId, prefix);
    const QString normalizedPrefix = normalizeQuery(prefix);
    std::sort(matches.begin(), matches.end(),
              [&normalizedPrefix](const SearchHistoryEntry &lhs, const SearchHistoryEntry &rhs)
              { return isBetterRecentMatch(lhs, rhs, normalizedPrefix); });
    return takeTerms(matches, limit);
}

QStringList SearchHistoryManager::frequentSearches(const QString &serverId, int limit, const QString &prefix,
                                                   const QStringList &excludeTerms) const
{
    if (!isEnabled())
    {
        return {};
    }

    auto matches = filteredEntries(serverId, prefix);
    const QString normalizedPrefix = normalizeQuery(prefix);
    QSet<QString> excludedNormalizedTerms;
    for (const QString &term : excludeTerms)
    {
        const QString normalizedTerm = normalizeQuery(term);
        if (!normalizedTerm.isEmpty())
        {
            excludedNormalizedTerms.insert(normalizedTerm);
        }
    }

    matches.erase(std::remove_if(matches.begin(), matches.end(),
                                 [&excludedNormalizedTerms](const SearchHistoryEntry &entry)
                                 { return excludedNormalizedTerms.contains(entry.normalizedTerm); }),
                  matches.end());

    std::sort(matches.begin(), matches.end(),
              [&normalizedPrefix](const SearchHistoryEntry &lhs, const SearchHistoryEntry &rhs)
              { return isBetterFrequentMatch(lhs, rhs, normalizedPrefix); });
    return takeTerms(matches, limit);
}

QString SearchHistoryManager::effectiveServerBucket(const QString &serverId) const
{
    const QString trimmedId = serverId.trimmed();
    return trimmedId.isEmpty() ? QStringLiteral("__global__") : trimmedId;
}

QString SearchHistoryManager::historyConfigKey(const QString &serverId) const
{
    return ConfigKeys::forServer(effectiveServerBucket(serverId), ConfigKeys::SearchHistoryRecords);
}

QStringList SearchHistoryManager::allHistoryBuckets() const
{
    QSet<QString> buckets = m_loadedBuckets;
    for (auto it = m_cache.constBegin(); it != m_cache.constEnd(); ++it)
    {
        buckets.insert(it.key());
    }

    const QString prefix = QStringLiteral("server/");
    const QString suffix =
        QStringLiteral("/") + QString::fromLatin1(ConfigKeys::SearchHistoryRecords);
    const QStringList allKeys = ConfigStore::instance()->allKeys();
    for (const QString &key : allKeys)
    {
        if (!key.startsWith(prefix) || !key.endsWith(suffix))
        {
            continue;
        }

        const int bucketLength = key.size() - prefix.size() - suffix.size();
        if (bucketLength <= 0)
        {
            continue;
        }

        buckets.insert(key.mid(prefix.size(), bucketLength));
    }

    QStringList result = buckets.values();
    std::sort(result.begin(), result.end());
    return result;
}

void SearchHistoryManager::ensureLoaded(const QString &serverId) const
{
    const QString bucket = effectiveServerBucket(serverId);
    if (m_loadedBuckets.contains(bucket))
    {
        return;
    }

    m_loadedBuckets.insert(bucket);

    const QString jsonStr = ConfigStore::instance()->get<QString>(historyConfigKey(bucket));
    if (jsonStr.trimmed().isEmpty())
    {
        m_cache.insert(bucket, {});
        return;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8());
    if (!doc.isArray())
    {
        qDebug() << "[SearchHistoryManager] Invalid history cache payload | bucket:" << bucket;
        m_cache.insert(bucket, {});
        return;
    }

    QList<SearchHistoryEntry> entries;
    const QJsonArray items = doc.array();
    entries.reserve(items.size());

    for (const auto &item : items)
    {
        if (!item.isObject())
        {
            continue;
        }
        const QJsonObject obj = item.toObject();
        SearchHistoryEntry entry;
        entry.term = obj.value(QStringLiteral("term")).toString().trimmed();
        entry.normalizedTerm = normalizeQuery(entry.term);
        entry.searchCount = qMax(1, obj.value(QStringLiteral("count")).toInt(1));
        entry.lastSearchedAtMs = obj.value(QStringLiteral("last")).toVariant().toLongLong();
        if (entry.normalizedTerm.isEmpty())
        {
            continue;
        }
        entries.append(entry);
    }

    std::sort(entries.begin(), entries.end(),
              [](const SearchHistoryEntry &lhs, const SearchHistoryEntry &rhs)
              {
                  if (lhs.lastSearchedAtMs != rhs.lastSearchedAtMs)
                  {
                      return lhs.lastSearchedAtMs > rhs.lastSearchedAtMs;
                  }
                  if (lhs.searchCount != rhs.searchCount)
                  {
                      return lhs.searchCount > rhs.searchCount;
                  }
                  return lhs.term.localeAwareCompare(rhs.term) < 0;
              });

    m_cache.insert(bucket, entries);
    qDebug() << "[SearchHistoryManager] History cache loaded | bucket:" << bucket << "| entryCount:" << entries.size();
}

void SearchHistoryManager::saveEntries(const QString &serverId, const QList<SearchHistoryEntry> &entries)
{
    QJsonArray array;
    for (const auto &entry : entries)
    {
        QJsonObject obj;
        obj[QStringLiteral("term")] = entry.term;
        obj[QStringLiteral("count")] = entry.searchCount;
        obj[QStringLiteral("last")] = QString::number(entry.lastSearchedAtMs);
        array.append(obj);
    }

    ConfigStore::instance()->set(
        historyConfigKey(serverId),
        QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

QList<SearchHistoryManager::SearchHistoryEntry> SearchHistoryManager::filteredEntries(const QString &serverId,
                                                                                      const QString &prefix) const
{
    const QString bucket = effectiveServerBucket(serverId);
    ensureLoaded(bucket);

    const QString normalizedPrefix = normalizeQuery(prefix);
    const auto entries = m_cache.value(bucket);
    if (normalizedPrefix.isEmpty())
    {
        return entries;
    }

    QList<SearchHistoryEntry> result;
    result.reserve(entries.size());
    for (const auto &entry : entries)
    {
        if (entry.normalizedTerm.contains(normalizedPrefix))
        {
            result.append(entry);
        }
    }
    return result;
}

QString SearchHistoryManager::normalizeQuery(QString query)
{
    query = query.simplified();
    return query.toCaseFolded();
}
