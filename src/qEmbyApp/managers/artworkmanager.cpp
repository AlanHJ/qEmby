#include "artworkmanager.h"
#include <QtConcurrent>
#include <qcorofuture.h>
#include <QGuiApplication>
#include <QDebug>
#include <QScopeGuard>
#include <QFutureWatcher>
#include <services/admin/adminservice.h>

ArtworkManager* ArtworkManager::instance() { static ArtworkManager manager; return &manager; }
ArtworkManager::ArtworkManager()
{
    m_clock.start();
    m_timer.setSingleShot(true);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &ArtworkManager::tick);
    connect(qGuiApp, &QGuiApplication::applicationStateChanged, this, [this]() { schedule(); });
}

QCoro::Task<ArtworkManager::Asset> ArtworkManager::fetch(QPointer<MediaService> service, QPointer<AdminService> admin,
    QString itemId, QString type, QString tag, int width, ImageRequestPriority priority,
    QObject* context, ImageFetchPolicy policy)
{
    if (!service) co_return Asset{};
    QPointer<QObject> contextGuard(context);
    const bool hasContext = context != nullptr;
    auto data = co_await service->fetchImageData(itemId, type, tag, width, priority, context, policy);
    if (!service) co_return Asset{};
    if (policy != ImageFetchPolicy::NetworkOnly) {
        if (const auto* entry = m_entries.object(data.identity))
            co_return Asset{data.identity, entry->animation.preview, entry->animation.canvasSize, entry->animation.isAnimated()};
    }
    const QString identity = data.identity;
    
    
    
    if (admin && width > 0 && data.data.startsWith(QByteArray::fromHex("89504e470d0a1a0a")) &&
        !data.data.contains("acTL")) {
        try {
            const auto images = co_await admin->getItemImages(itemId);
            if (!service || (hasContext && !contextGuard)) co_return Asset{};
            for (auto image : images) {
                if (image.imageType != type || image.width <= 0 || image.height <= 0 ||
                    image.size <= 0 || image.size > 8 * 1024 * 1024 ||
                    quint64(image.width) * image.height > ApngDecoder::MaxCanvasPixels) continue;
                const auto original = co_await service->fetchImageData(itemId, type, tag, 0,
                    priority, contextGuard.data(), policy);
                if (!service || (hasContext && !contextGuard)) co_return Asset{};
                if (original.data.contains("acTL")) data = original;
                break;
            }
        } catch (...) { qDebug() << "[ArtworkManager] Original PNG unavailable; using server thumbnail"; }
    }
    data.identity = identity;
    auto job = policy == ImageFetchPolicy::NetworkOnly
        ? QSharedPointer<DecodeJob>() : m_decodeJobs.value(identity);
    if (!job) {
        job = QSharedPointer<DecodeJob>::create();
        job->future = QtConcurrent::run([bytes = data.data, preview = data.preview, decodeBudget = m_decodeBudget]() {
            decodeBudget->acquire();
            const auto releaseBudget = qScopeGuard([decodeBudget]() { decodeBudget->release(); });
            if (!bytes.isEmpty()) return ApngDecoder::decode(bytes);
            ApngDecoder::Animation animation;
            animation.preview = preview; animation.canvasSize = preview.size();
            return animation;
        });
        m_decodeJobs.insert(identity, job);
    }
    auto future = job->future;
    auto decoded = co_await future;
    
    
    if (m_decodeJobs.value(identity) == job) m_decodeJobs.remove(identity);
    if (policy != ImageFetchPolicy::NetworkOnly) {
        if (const auto* existing = m_entries.object(identity))
            co_return Asset{identity, existing->animation.preview, existing->animation.canvasSize, existing->animation.isAnimated()};
    }
    if (decoded.preview.isNull()) decoded.preview = data.preview;
    if (!decoded.canvasSize.isValid()) decoded.canvasSize = decoded.preview.size();
    Asset result{data.identity, decoded.preview, decoded.canvasSize, decoded.isAnimated()};
    if (!decoded.failure.isEmpty())
        qWarning() << "[ArtworkManager] Animation fallback | reason=" << decoded.failure << "| size=" << decoded.canvasSize;
    if (!decoded.preview.isNull()) {
        auto* entry = new Entry;
        if (const auto* previous = m_entries.object(identity)) entry->owners = previous->owners;
        entry->animation = std::move(decoded);
        entry->delayMs = entry->animation.isAnimated() ? entry->animation.frames.first().delayMs : 100;
        entry->streamedFrame = entry->animation.stream ? entry->animation.preview : QImage();
        entry->due = m_clock.elapsed() + entry->delayMs;
        const quint64 storage = entry->animation.stream ? entry->animation.storageBytes :
            quint64(result.size.width()) * result.size.height() * 4 * (entry->animation.frames.size() + 1);
        const int cost = int(storage / 1024);
        m_entries.insert(data.identity, entry, qMax(1, cost));
    }
    co_return result;
}

QImage ArtworkManager::currentFrame(const QString& key) const
{
    const auto* entry = m_entries.object(key);
    if (entry && entry->animation.stream) return entry->streamedFrame;
    return entry && entry->animation.isAnimated() ? entry->animation.frames[entry->frame].image : QImage();
}
void ArtworkManager::subscribe(const QString& key, QObject* owner, bool active)
{
    if (!owner) return;
    auto* entry = m_entries.object(key);
    if (!entry || !entry->animation.isAnimated()) return;
    if (active) {
        if (entry->owners.isEmpty()) entry->due = m_clock.elapsed() + entry->delayMs;
        entry->owners.insert(owner);
        if (!m_owners.contains(owner)) {
            m_owners.insert(owner);
            connect(owner, &QObject::destroyed, this, [this, owner]() { unsubscribe(owner); m_owners.remove(owner); });
        }
    } else entry->owners.remove(owner);
    schedule();
}
void ArtworkManager::unsubscribe(QObject* owner)
{
    for (const auto& key : m_entries.keys()) m_entries.object(key)->owners.remove(owner);
    schedule();
}
void ArtworkManager::schedule()
{
    m_timer.stop();
    if (qGuiApp->applicationState() != Qt::ApplicationActive) return;
    qint64 due = -1;
    for (const auto& key : m_entries.keys()) {
        const auto* entry = m_entries.object(key);
        if (!entry->owners.isEmpty() && !entry->finished && !entry->decoding) due = due < 0 ? entry->due : qMin(due, entry->due);
    }
    if (due >= 0) m_timer.start(int(qMax<qint64>(1, due - m_clock.elapsed())));
}
void ArtworkManager::tick()
{
    const qint64 now = m_clock.elapsed();
    for (const auto& key : m_entries.keys()) {
        auto* entry = m_entries.object(key);
        if (!entry || entry->owners.isEmpty() || entry->finished || entry->decoding || entry->due > now) continue;
        if (entry->animation.stream) {
            if (entry->frame + 1 >= entry->animation.stream->frameCount() &&
                entry->animation.loops && ++entry->completedLoops >= entry->animation.loops) {
                entry->finished = true;
                continue;
            }
            decodeNextFrame(key);
            continue;
        }
        if (++entry->frame >= entry->animation.frames.size()) {
            if (entry->animation.loops && ++entry->completedLoops >= entry->animation.loops) {
                entry->finished = true; entry->frame = entry->animation.frames.size() - 1;
            } else entry->frame = 0;
        }
        entry->delayMs = entry->animation.frames[entry->frame].delayMs;
        entry->due = now + entry->delayMs;
        Q_EMIT frameChanged(key);
    }
    schedule();
}

void ArtworkManager::decodeNextFrame(QString key)
{
    auto* entry = m_entries.object(key);
    if (!entry || !entry->animation.stream || entry->decoding) return;
    const auto stream = entry->animation.stream;
    entry->decoding = true;
    auto* watcher = new QFutureWatcher<ApngDecoder::Frame>(this);
    connect(watcher, &QFutureWatcher<ApngDecoder::Frame>::finished, this, [this, watcher, key, stream]() {
        ApngDecoder::Frame frame;
        try { frame = watcher->result(); } catch (...) {}
        watcher->deleteLater();
        auto* current = m_entries.object(key);
        if (!current || current->animation.stream != stream) return;
        current->decoding = false;
        if (frame.image.isNull()) {
            current->finished = true;
            qWarning() << "[ArtworkManager] Streaming animation stopped after frame decode failure";
        } else {
            current->frame = (current->frame + 1) % stream->frameCount();
            current->streamedFrame = frame.image;
            current->delayMs = frame.delayMs;
            current->due = qMax(current->due + frame.delayMs, m_clock.elapsed());
            Q_EMIT frameChanged(key);
        }
        schedule();
    });
    watcher->setFuture(QtConcurrent::run([stream, decodeBudget = m_decodeBudget]() {
        decodeBudget->acquire();
        const auto releaseBudget = qScopeGuard([decodeBudget]() { decodeBudget->release(); });
        return stream->nextFrame();
    }));
}
