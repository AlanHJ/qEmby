#ifndef HTTPRANGECACHE_H
#define HTTPRANGECACHE_H

#include <QCache>
#include <QElapsedTimer>
#include <QHash>
#include <QNetworkRequest>
#include <QObject>
#include <QPointer>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;


class HttpRangeCache : public QObject
{
    Q_OBJECT
public:
    struct Block {
        QByteArray data;
        QByteArray contentType;
        QByteArray validator;
        QByteArray cacheControl;
        qint64 first = 0;
        qint64 last = -1;
        qint64 total = -1;
        qint64 expiresAt = 0;
        bool complete = false;
    };
    explicit HttpRangeCache(QNetworkAccessManager *network, QObject *parent = nullptr);
    ~HttpRangeCache() override;
    void acquire(const QString &key, QNetworkRequest request, qint64 first, QObject *consumer, bool preview);
    std::shared_ptr<const Block> block(const QString &key);
    void release(QObject *consumer);
    void clear();

signals:
    void blockChanged(const QString &key);
    void blockFailed(const QString &key, bool fallback, const QString &reason);
    void upstreamBytes(qint64 count);

private:
    struct Job {
        QNetworkRequest request;
        QPointer<QNetworkReply> reply;
        std::shared_ptr<Block> block;
        std::shared_ptr<Block> staleBlock;
        QHash<QObject *, bool> consumers; 
        quint64 sequence = 0;
        bool preview = true;
    };
    void schedule();
    void dispatch();
    void receive(const QString &key, bool finished);
    void fail(const QString &key, bool fallback, const QString &reason);
    void removeJob(const QString &key);

    QNetworkAccessManager *m_network;
    QCache<QString, std::shared_ptr<Block>> m_blocks{128}; 
    QHash<QString, std::shared_ptr<Job>> m_jobs;
    QElapsedTimer m_clock;
    quint64 m_sequence = 0;
    bool m_scheduled = false;
};

#endif
