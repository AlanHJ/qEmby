#ifndef MPVWIDGET_H
#define MPVWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QString>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <memory>
#include <mpv/render_gl.h>
#include "mpvcontroller.h"

class MpvHttpStreamRelay;
class NativeDanmakuOverlay;
class QOpenGLFramebufferObject;
class QOpenGLTextureBlitter;
struct UserAgentConfig;

class MpvWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget *parent = nullptr);
    ~MpvWidget() override;

    
    void shutdown();

    void loadMedia(const QString &url, const QString &serverId = QString());
    void play();
    void resumeAfterContextRestore();
    void pause();
    void stop();
    void seek(double positionInSeconds);

    MpvController* controller() const { return m_controller; }
    void setNativeDanmakuOverlay(NativeDanmakuOverlay *overlay);
    bool hasPresentedVideo() const { return m_videoPresented; }

signals:
    
    void positionChanged(double position);
    void durationChanged(double duration);
    void playbackStateChanged(bool isPaused);
    void videoPresentationChanged(bool presented);
    void networkSpeedChanged(qint64 bytesPerSecond);
    void relayActiveChanged(bool active);
    void errorOccurred(const QString &errorMsg);

protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;

private slots:
    void cleanupGL(); 
    void maybeUpdate(); 

private:
    static void onMpvRenderUpdate(void *ctx);
    static void *getProcAddress(void *ctx, const char *name);
    void loadMediaNow(const QString &url, const QString &serverId, bool wasPending);
    void applyUserAgentConfig(const UserAgentConfig &config);
    bool paintNativeComposite(int width, int height);
    void releaseVideoCache();
    void checkCompositeGlErrors(const char *stage);
    void resetVideoPresentation(bool resetMedia);
    void refreshVideoOutputState();
    void recordVideoFrameRendered(bool framePresent);

    MpvController *m_controller;
    MpvHttpStreamRelay *m_streamRelay = nullptr;
    bool m_usingStreamRelay = false;
    bool m_resumeWhenRenderReady = false;
    mpv_render_context *m_mpv_gl;
    QPointer<NativeDanmakuOverlay> m_nativeDanmakuOverlay;
    std::unique_ptr<QOpenGLFramebufferObject> m_videoFramebuffer;
    std::unique_ptr<QOpenGLTextureBlitter> m_videoBlitter;
    QTimer m_videoDueTimer;
    QElapsedTimer m_compositeStatsClock;
    QElapsedTimer m_compositionClock;
    int m_videoFrames = 0;
    int m_cachedVideoFrames = 0;
    int m_deferredVideoFrames = 0;
    int m_compositeGlErrors = 0;
    qreal m_maxVideoRenderMs = 0.0;
    qreal m_maxCompositeCpuMs = 0.0;
    qreal m_maxCompositionMs = 0.0;
    qreal m_maxTargetDelayMs = 0.0;
    int m_vsyncVideoFrames = 0;
    int m_videoTargetUnit = 0;
    quint64 m_compositeAuditFrames = 0;
    bool m_checkCompositeErrorsThisFrame = true;
    bool m_videoAwaitingSwap = false;
    bool m_videoFramePending = true;
    bool m_videoCacheDirty = true;
    bool m_discardVideoCache = true;
    bool m_nativeCompositeActive = false;
    bool m_nativeCompositeFailed = false;
    bool m_mediaLoaded = false;
    bool m_videoOutputAvailable = false;
    bool m_videoFrameRendered = false;
    bool m_videoFrameForSwap = false;
    bool m_videoPresented = false;

    
    QString m_pendingUrl;
    QString m_pendingServerId;  
    QString m_currentUrl;
    QString m_currentServerId;
    QString m_defaultUserAgent;
    bool m_customUserAgentApplied = false;
};

#endif 
