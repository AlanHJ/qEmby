#pragma once
#include "../utils/apngdecoder.h"
#include <QCache>
#include <QElapsedTimer>
#include <QSharedPointer>
#include <QSet>
#include <QSemaphore>
#include <QFuture>
#include <QTimer>
#include <services/media/mediaservice.h>
class AdminService;

class ArtworkManager : public QObject {
    Q_OBJECT
public:
    struct Asset { QString key; QImage preview; QSize size; bool animated = false; };
    static ArtworkManager* instance();
    QCoro::Task<Asset> fetch(QPointer<MediaService> service, QPointer<AdminService> admin, QString itemId, QString type,
        QString tag, int width, ImageRequestPriority priority, QObject* context,
        ImageFetchPolicy policy);
    QImage currentFrame(const QString& key) const;
    void subscribe(const QString& key, QObject* owner, bool active);
    void unsubscribe(QObject* owner);
signals:
    void frameChanged(const QString& key);
private:
    struct Entry {
        ApngDecoder::Animation animation;
        int frame = 0;
        quint32 completedLoops = 0;
        qint64 due = 0;
        QSet<QObject*> owners;
        bool finished = false;
        bool decoding = false;
        QImage streamedFrame;
        int delayMs = 100;
    };
    ArtworkManager();
    void tick();
    void decodeNextFrame(QString key);
    void schedule();
    QCache<QString, Entry> m_entries{131072};
    struct DecodeJob {
        QFuture<ApngDecoder::Animation> future;
    };
    QHash<QString, QSharedPointer<DecodeJob>> m_decodeJobs;
    QSharedPointer<QSemaphore> m_decodeBudget = QSharedPointer<QSemaphore>::create(2);
    QSet<QObject*> m_owners;
    QElapsedTimer m_clock;
    QTimer m_timer;
};
