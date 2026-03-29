#ifndef SEARCHHISTORYMANAGER_H
#define SEARCHHISTORYMANAGER_H

#include <QObject>
#include <QHash>
#include <QList>
#include <QSet>
#include <QStringList>

class SearchHistoryManager : public QObject {
    Q_OBJECT
public:
    struct SearchHistoryEntry {
        QString term;
        QString normalizedTerm;
        int searchCount = 0;
        qint64 lastSearchedAtMs = 0;
    };

    static SearchHistoryManager *instance();

    bool isEnabled() const;
    bool isAutocompleteEnabled() const;
    void recordSearch(const QString &serverId, QString query);
    void clearHistory(const QString &serverId);
    void clearAllHistory();
    void removeHistoryTerm(const QString &serverId, QString term);

    QStringList completionSuggestions(const QString &serverId,
                                      const QString &prefix = QString(),
                                      int limit = 8) const;
    QList<SearchHistoryEntry> historyEntries(const QString &serverId,
                                             const QString &prefix = QString()) const;
    QStringList recentSearches(const QString &serverId, int limit = 6,
                               const QString &prefix = QString()) const;
    QStringList frequentSearches(const QString &serverId, int limit = 6,
                                 const QString &prefix = QString(),
                                 const QStringList &excludeTerms = {}) const;

Q_SIGNALS:
    void historyChanged(const QString &serverId);
    void enabledChanged(bool enabled);
    void autocompleteEnabledChanged(bool enabled);

private:
    explicit SearchHistoryManager(QObject *parent = nullptr);

    QString effectiveServerBucket(const QString &serverId) const;
    QString historyConfigKey(const QString &serverId) const;
    QStringList allHistoryBuckets() const;

    void ensureLoaded(const QString &serverId) const;
    void saveEntries(const QString &serverId,
                     const QList<SearchHistoryEntry> &entries);
    QList<SearchHistoryEntry> filteredEntries(const QString &serverId,
                                              const QString &prefix) const;

    static QString normalizeQuery(QString query);

    mutable QHash<QString, QList<SearchHistoryEntry>> m_cache;
    mutable QSet<QString> m_loadedBuckets;
};

#endif 
