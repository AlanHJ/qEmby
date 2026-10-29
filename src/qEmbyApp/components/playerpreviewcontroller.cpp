#include "playerpreviewcontroller.h"
#include "playerpreviewworker.h"

#include <config/config_keys.h>
#include <config/configstore.h>
#include <QDebug>
#include <QThread>
#include <cmath>

PlayerPreviewController::PlayerPreviewController(QObject *parent)
    : QObject(parent)
{
    m_clock.start();
    qRegisterMetaType<MpvPreviewUtils::Request>();
    qRegisterMetaType<MpvPreviewUtils::Result>();
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_debounce, &QTimer::timeout, this, &PlayerPreviewController::startRequest);
    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString &key, const QVariant &) {
                if (key == QLatin1String(ConfigKeys::PlayerHoverPreviewMode)) applyMode();
            });
    applyMode();
}

PlayerPreviewController::~PlayerPreviewController()
{
    
    
    if (m_cancelled) m_cancelled->store(true);
    if (m_previewThread) emit shutdownRequested();
}

void PlayerPreviewController::ensureWorker()
{
    if (m_previewThread) return;
    m_previewThread = new QThread;
    m_previewThread->setObjectName(QStringLiteral("PlayerPreview"));
    auto *worker = new PlayerPreviewWorker;
    worker->moveToThread(m_previewThread);
    connect(this, &PlayerPreviewController::decodeRequested,
            worker, &PlayerPreviewWorker::decode, Qt::QueuedConnection);
    connect(this, &PlayerPreviewController::sessionResetRequested,
            worker, &PlayerPreviewWorker::reset, Qt::QueuedConnection);
    connect(this, &PlayerPreviewController::shutdownRequested,
            worker, &PlayerPreviewWorker::shutdown, Qt::QueuedConnection);
    connect(worker, &PlayerPreviewWorker::completed,
            this, &PlayerPreviewController::onFrameCompleted, Qt::QueuedConnection);
    
    connect(worker, &PlayerPreviewWorker::stopped,
            m_previewThread, &QThread::quit, Qt::DirectConnection);
    connect(m_previewThread, &QThread::finished, worker, &QObject::deleteLater);
    connect(m_previewThread, &QThread::finished, m_previewThread, &QObject::deleteLater);
    m_previewThread->start();
}

void PlayerPreviewController::onFrameCompleted(MpvPreviewUtils::Result result)
{
    m_requestActive = false;
    const bool valid = m_enabled && m_activeGeneration == m_generation &&
                       m_cancelled && !m_cancelled->load();
    const bool current = valid && m_activePosition == m_requestedPosition;
    if (valid) {
        if (result.image.isNull()) {
            m_failedRequests.insert(m_activePosition, new qint64(m_clock.elapsed() + 2000));
        } else {
            
            
            m_cache.insert(m_activePosition, new QImage(result.image));
            m_failedRequests.remove(m_activePosition);
        }
        qDebug() << "[PlayerPreview] Frame completed | position:" << m_activePosition
                 << "| generation:" << m_activeGeneration
                 << "| available:" << !result.image.isNull() << "| error:" << result.error
                 << "| stage:" << result.stage << "| reusedSession:" << result.reusedSession
                 << "| displayed:" << current << "| elapsedMs:" << m_requestClock.elapsed();
        if (current) emit frameChanged(result.image);
    } else {
        qDebug() << "[PlayerPreview] Discarded obsolete frame | position:" << m_activePosition
                 << "| generation:" << m_activeGeneration
                 << "| elapsedMs:" << m_requestClock.elapsed();
    }
    if (!current && !m_debounce.isActive() && m_requestedPosition >= 0 &&
        !m_cache.contains(m_requestedPosition)) startRequest();
}

void PlayerPreviewController::applyMode()
{
    const bool enabled = ConfigStore::instance()->get<QString>(
        ConfigKeys::PlayerHoverPreviewMode, QStringLiteral("time")) == QStringLiteral("preview");
    if (m_enabled == enabled) return;
    m_enabled = enabled;
    cancel();
    if (!enabled) {
        m_cache.clear();
        m_failedRequests.clear();
        emit sessionResetRequested();
    }
    qDebug() << "[PlayerPreview] Mode changed | imagePreview:" << enabled;
    emit modeChanged();
}

void PlayerPreviewController::setSource(const QString &url, const QVariantMap &networkOptions)
{
    cancel();
    ++m_generation;
    m_cache.clear();
    m_failedRequests.clear();
    emit sessionResetRequested();
    m_url = url;
    m_networkOptions = networkOptions;
    qDebug() << "[PlayerPreview] Source reset | generation:" << m_generation
             << "| available:" << !m_url.isEmpty();
}

void PlayerPreviewController::cancel()
{
    m_debounce.stop();
    m_requestedPosition = -1;
    if (m_cancelled) m_cancelled->store(true);
    emit frameChanged(QImage());
}

void PlayerPreviewController::requestFrame(int position, double duration)
{
    if (!m_enabled || m_url.isEmpty() || !std::isfinite(duration) || duration <= 0.0) return;
    position = qMax(0, position);
    if (position == m_requestedPosition) return;
    m_requestedPosition = position;
    m_targetSeconds = qBound(0.0, static_cast<double>(position), qMax(0.0, duration - 0.1));
    emit frameChanged(QImage());
    if (const auto *cached = m_cache.object(position)) {
        m_debounce.stop();
        qDebug() << "[PlayerPreview] Cache hit | position:" << position;
        emit frameChanged(*cached);
    } else if (!isFailureCoolingDown(position)) {
        m_debounce.start();
    } else {
        m_debounce.stop();
    }
}

bool PlayerPreviewController::isFailureCoolingDown(int position) const
{
    const qint64 *retryAfter = m_failedRequests.object(position);
    return retryAfter && m_clock.elapsed() < *retryAfter;
}

void PlayerPreviewController::startRequest()
{
    if (!m_enabled || m_url.isEmpty() || m_requestedPosition < 0 ||
        m_requestActive || m_cache.contains(m_requestedPosition) ||
        isFailureCoolingDown(m_requestedPosition)) return;
    m_requestActive = true;
    ensureWorker();
    m_requestClock.start();
    m_activePosition = m_requestedPosition;
    m_activeGeneration = m_generation;
    m_cancelled = std::make_shared<std::atomic_bool>(false);
    const MpvPreviewUtils::Request request{m_url, m_networkOptions, m_targetSeconds, m_cancelled};
    qDebug() << "[PlayerPreview] Request frame | generation:" << m_generation
             << "| position:" << m_activePosition;
    emit decodeRequested(request);
}
