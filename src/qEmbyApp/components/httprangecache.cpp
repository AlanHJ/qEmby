#include "httprangecache.h"
#include "../utils/httprangeutils.h"

#include <QDebug>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTimer>

HttpRangeCache::HttpRangeCache(QNetworkAccessManager *network, QObject *parent)
    : QObject(parent), m_network(network)
{
    m_clock.start();
}

HttpRangeCache::~HttpRangeCache()
{
    clear();
}

std::shared_ptr<const HttpRangeCache::Block> HttpRangeCache::block(const QString &key)
{
    const auto job = m_jobs.value(key);
    if (job) return job->block;
    if (auto cached = m_blocks.object(key)) {
        if ((*cached)->expiresAt > m_clock.elapsed()) return *cached;
    }
    return {};
}

void HttpRangeCache::acquire(const QString &key, QNetworkRequest request, qint64 first,
                            QObject *consumer, bool preview)
{
    auto job = m_jobs.value(key);
    if (job) {
        job->consumers.insert(consumer, preview);
        if (!preview) job->preview = false;
        qDebug() << "[HttpRangeCache] Joined download | offset:" << first << "| preview:" << preview;
    } else if (HttpRangeUtils::freshnessSeconds(request.rawHeader("Cache-Control"), {}) > 0 && block(key)) {
        qDebug() << "[HttpRangeCache] Hit | offset:" << first << "| preview:" << preview;
        return;
    } else {
        job = std::make_shared<Job>();
        job->request = request;
        job->preview = preview;
        job->sequence = ++m_sequence;
        job->consumers.insert(consumer, preview);
        job->block = std::make_shared<Block>();
        job->block->first = first;
        job->block->data.reserve(HttpRangeUtils::BlockBytes);
        if (auto cached = m_blocks.object(key)) job->staleBlock = *cached;
        m_jobs.insert(key, job);
    }
    schedule();
}

void HttpRangeCache::schedule()
{
    if (m_scheduled) return;
    m_scheduled = true;
    QTimer::singleShot(0, this, [this]() { m_scheduled = false; dispatch(); });
}

void HttpRangeCache::dispatch()
{
    int active = 0, previews = 0;
    for (const auto &job : m_jobs) {
        if (job->reply) { ++active; if (job->preview) ++previews; }
    }
    
    
    while (active < 3) {
        QString key;
        std::shared_ptr<Job> next;
        for (auto it = m_jobs.cbegin(); it != m_jobs.cend(); ++it) {
            const auto &job = it.value();
            if (job->reply || (job->preview && previews >= 1)) continue;
            if (!next || (next->preview && !job->preview) ||
                (next->preview == job->preview && job->sequence < next->sequence)) {
                next = job;
                key = it.key();
            }
        }
        if (!next) break;
        auto request = next->request;
        request.setRawHeader("Range", "bytes=" + QByteArray::number(next->block->first) + "-" +
                             QByteArray::number(next->block->first + HttpRangeUtils::BlockBytes - 1));
        if (next->staleBlock) {
            request.setRawHeader("If-Range", QByteArray());
            const auto validator = next->staleBlock->validator;
            request.setRawHeader(validator.startsWith('"') ? "If-None-Match" : "If-Modified-Since", validator);
        }
        request.setPriority(next->preview ? QNetworkRequest::LowPriority : QNetworkRequest::HighPriority);
        request.setTransferTimeout(15000);
        
        request.setAttribute(QNetworkRequest::CookieLoadControlAttribute, QNetworkRequest::Manual);
        request.setAttribute(QNetworkRequest::CookieSaveControlAttribute, QNetworkRequest::Manual);
        next->reply = m_network->get(request);
        next->reply->setReadBufferSize(256 * 1024);
        connect(next->reply, &QNetworkReply::metaDataChanged, this, [this, key]() { receive(key, false); });
        connect(next->reply, &QNetworkReply::readyRead, this, [this, key]() { receive(key, false); });
        connect(next->reply, &QNetworkReply::finished, this, [this, key]() { receive(key, true); });
        qDebug() << "[HttpRangeCache] Fetch | offset:" << next->block->first << "| preview:" << next->preview;
        ++active;
        if (next->preview) ++previews;
    }
}

void HttpRangeCache::receive(const QString &key, bool finished)
{
    const auto job = m_jobs.value(key);
    if (!job || !job->reply) return;
    auto *reply = job->reply.data();
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (status == 0 && !finished) return;
    if (status == 0) { fail(key, false, QStringLiteral("network-error")); return; }
    
    
    if (status >= 400) { fail(key, true, QStringLiteral("request-rejected")); return; }
    if (status == 304 && job->staleBlock) {
        if (!finished) return;
        auto cached = job->staleBlock;
        const auto returnedValidator = cached->validator.startsWith('"') ?
            reply->rawHeader("ETag") : reply->rawHeader("Last-Modified");
        if (reply->error() != QNetworkReply::NoError ||
            (!returnedValidator.isEmpty() && returnedValidator != cached->validator) ||
            HttpRangeUtils::forbidsCaching(reply->rawHeader("Cache-Control")) ||
            reply->rawHeader("Vary").contains('*') || reply->hasRawHeader("Set-Cookie")) {
            fail(key, true, QStringLiteral("invalid-revalidation"));
            return;
        }
        if (reply->hasRawHeader("Cache-Control")) cached->cacheControl = reply->rawHeader("Cache-Control");
        cached->expiresAt = m_clock.elapsed() + 1000 * HttpRangeUtils::freshnessSeconds(
            cached->cacheControl, reply->rawHeader("Age"));
        
        
        *job->block = *cached;
        cached = job->block;
        m_blocks.insert(key, new std::shared_ptr<Block>(cached));
        qDebug() << "[HttpRangeCache] Revalidated | offset:" << cached->first;
        emit blockChanged(key);
        removeJob(key);
        schedule();
        return;
    }
    if (status >= 300 && status < 400 && !finished) return; 
    auto &data = *job->block;
    if (data.total < 0 && data.first == 0) {
        if (reply->bytesAvailable() < 7 && !finished) return;
        if (reply->peek(64).trimmed().startsWith("#EXTM3U")) {
            fail(key, true, QStringLiteral("playlist"));
            return;
        }
    }
    if (data.total < 0) {
        const auto type = reply->rawHeader("Content-Type");
        if (HttpRangeUtils::isPlaylist(reply->url(), type)) { fail(key, true, QStringLiteral("playlist")); return; }
        const auto range = HttpRangeUtils::contentRange(reply->rawHeader("Content-Range"));
        const auto encoding = reply->rawHeader("Content-Encoding").toLower();
        const auto validator = HttpRangeUtils::strongValidator(reply->rawHeader("ETag"),
            reply->rawHeader("Last-Modified"), reply->rawHeader("Date"));
        const int freshness = HttpRangeUtils::freshnessSeconds(reply->rawHeader("Cache-Control"), reply->rawHeader("Age"));
        if (status != 206 || !range || range->first != data.first ||
            range->last != qMin(data.first + HttpRangeUtils::BlockBytes - 1, range->total - 1) ||
            (!encoding.isEmpty() && encoding != "identity") || validator.isEmpty() ||
            HttpRangeUtils::forbidsCaching(reply->rawHeader("Cache-Control")) ||
            reply->rawHeader("Vary").contains('*') || reply->hasRawHeader("Set-Cookie") ||
            type.toLower().startsWith("text/") || type.toLower().contains("json")) {
            qInfo() << "[HttpRangeCache] Bypass | status:" << status << "| rangeValid:" << bool(range)
                    << "| validatorAvailable:" << !validator.isEmpty();
            fail(key, true, QStringLiteral("unsupported-response"));
            return;
        }
        data.total = range->total;
        data.last = range->last;
        data.validator = validator;
        data.contentType = type;
        data.cacheControl = reply->rawHeader("Cache-Control");
        data.expiresAt = m_clock.elapsed() + freshness * 1000;
    }
    const auto chunk = reply->readAll();
    if (!chunk.isEmpty()) {
        emit upstreamBytes(chunk.size());
        if (data.data.size() + chunk.size() > data.last - data.first + 1) {
            fail(key, true, QStringLiteral("invalid-body-length"));
            return;
        }
        data.data.append(chunk);
    }
    if (finished) {
        if (reply->error() != QNetworkReply::NoError || data.data.size() != data.last - data.first + 1) {
            qWarning() << "[HttpRangeCache] Incomplete block | offset:" << data.first << "| error:" << reply->error();
            fail(key, false, QStringLiteral("incomplete-block"));
            return;
        }
        data.complete = true;
        m_blocks.insert(key, new std::shared_ptr<Block>(job->block));
        
        
        emit blockChanged(key);
        removeJob(key);
        schedule();
        return;
    }
    emit blockChanged(key);
}

void HttpRangeCache::removeJob(const QString &key)
{
    const auto job = m_jobs.take(key);
    if (!job || !job->reply) return;
    auto *reply = job->reply.data();
    disconnect(reply, nullptr, this, nullptr);
    if (!reply->isFinished()) reply->abort();
    reply->deleteLater();
}

void HttpRangeCache::fail(const QString &key, bool fallback, const QString &reason)
{
    removeJob(key);
    schedule();
    emit blockFailed(key, fallback, reason);
}

void HttpRangeCache::release(QObject *consumer)
{
    const auto keys = m_jobs.keys();
    for (const auto &key : keys) {
        const auto job = m_jobs.value(key);
        job->consumers.remove(consumer);
        if (job->consumers.isEmpty()) removeJob(key);
        else job->preview = !job->consumers.values().contains(false);
    }
    schedule();
}

void HttpRangeCache::clear()
{
    const auto keys = m_jobs.keys();
    for (const auto &key : keys) removeJob(key);
    m_blocks.clear();
}
