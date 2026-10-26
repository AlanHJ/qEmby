#ifndef PLAYERPREVIEWCONTROLLER_H
#define PLAYERPREVIEWCONTROLLER_H

#include "../utils/mpvpreviewutils.h"
#include <QCache>
#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

class QThread;

class PlayerPreviewController : public QObject
{
    Q_OBJECT
public:
    explicit PlayerPreviewController(QObject *parent = nullptr);
    ~PlayerPreviewController() override;
    bool isEnabled() const { return m_enabled; }
    void setSource(const QString &url, const QVariantMap &networkOptions);
    void requestFrame(int position, double duration);
    void cancel();

signals:
    void frameChanged(const QImage &image);
    void modeChanged();
    void decodeRequested(MpvPreviewUtils::Request request);
    void sessionResetRequested();
    void shutdownRequested();

private:
    void applyMode();
    void startRequest();
    void ensureWorker();
    void onFrameCompleted(MpvPreviewUtils::Result result);
    bool isFailureCoolingDown(int position) const;
    QTimer m_debounce;
    QThread *m_previewThread = nullptr;
    QCache<int, QImage> m_cache{96};
    QCache<int, qint64> m_failedRequests{32};
    QElapsedTimer m_clock;
    QElapsedTimer m_requestClock;
    QString m_url;
    QVariantMap m_networkOptions;
    std::shared_ptr<std::atomic_bool> m_cancelled;
    quint64 m_generation = 0;
    quint64 m_activeGeneration = 0;
    int m_requestedPosition = -1;
    int m_activePosition = -1;
    double m_targetSeconds = 0.0;
    bool m_enabled = false;
    bool m_requestActive = false;
};

#endif
