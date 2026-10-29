#pragma once
#include "../utils/artworkpolicyutils.h"
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QDateTime>
#include <QTimer>
#include <models/media/mediaitem.h>
#include <qcorotask.h>
class QEmbyCore;
class LibraryArtworkManager : public QObject {
    Q_OBJECT
public:
    static LibraryArtworkManager* instance();
    void init(QEmbyCore* core);
    bool enabled() const;
    QString defaultLayoutMode() const;
    QString fallbackViewMode() const;
    QString endpointFingerprint() const;
    bool scanAuthorized() const;
    bool landscape(QString libraryId) const;
    ArtworkPolicyUtils::Layout layout(QString libraryId) const;
    bool hasManualOverride(QString libraryId) const;
    QString libraryForItem(const MediaItem& item) const;
    void observeItems(QList<MediaItem> items);
    void requestEvidence(QList<MediaItem> items);
    void observeImage(MediaItem item, QString type, QString tag, QSize size);
    void invalidate(QString itemId);
    void followDefault(QString libraryId);
    QCoro::Task<QString> resolveLibrary(MediaItem item);
signals:
    void configurationChanged();
    void policyChanged(const QString& libraryId);
    void scanProgress(const QString& libraryId, int checked, int total);
    void scanAuthorizationRequired();
private:
    struct Library {
        QHash<QString, ArtworkPolicyUtils::Evidence> evidence;
        ArtworkPolicyUtils::State state = ArtworkPolicyUtils::State::Unknown;
        bool complete = false;
        bool scanning = false;
        int nextOffset = 0;
        QDateTime verifiedAt;
        QSet<QString> coveredItems;
        int expectedTotal = -1;
        bool rangeComplete = false;
    };
    LibraryArtworkManager();
    void updatePolicy(QString id);
    void configure();
    void saveCache();
    void loadCache();
    QString cachePath() const;
    QCoro::Task<void> discover(quint64 generation);
    QCoro::Task<void> scan(QString libraryId, quint64 generation);
    QString serverId() const;
    QPointer<QEmbyCore> m_core;
    QHash<QString, Library> m_libraries;
    QHash<QString, QString> m_owners;
    QSet<QString> m_libraryIds;
    QHash<QString, QHash<QString, Library>> m_sessionLibraries;
    QHash<QString, QHash<QString, QString>> m_sessionOwners;
    QString m_identity;
    QTimer m_saveTimer;
    quint64 m_generation = 0;
    QObject* m_scanContext = nullptr;
};
