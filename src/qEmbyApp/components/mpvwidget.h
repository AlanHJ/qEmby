#ifndef MPVWIDGET_H
#define MPVWIDGET_H

#include <QString>
#include <QVariantMap>
#include <QMap>
#include <QUrl>
#include <utility>
#include <QPointer>
#include <QTimer>
#include <QElapsedTimer>
#include <QImage>
#include <memory>
#include <mpv/render_gl.h>
#include "mpvcontroller.h"

// 平台抽象 (PAL) — 渲染表面后端选择:
//  - Windows (Qt >= 6.7, 宏 QEMBY_MPV_D3D11_RENDER 由 CMake 定义):
//    QRhiWidget (Qt RHI / Direct3D 11) + MPV_RENDER_API_TYPE_D3D11 渲染通道,
//    mpv 直接渲染进 Qt 的 D3D11 纹理, 解锁 hwdec=d3d11va 原生零拷贝。
//  - 其余平台与旧 Qt (< 6.7) 回退: QOpenGLWidget + MPV_RENDER_API_TYPE_OPENGL,
//    维持既有 GL 合成管线 (Linux VAAPI/NVDEC 零拷贝走 EGL dma-buf/CUDA 互操作)。
// 上层 (PlayerView / NativeDanmakuOverlay / 输入分发) 对后端完全无感。
#if defined(QEMBY_MPV_D3D11_RENDER)
#include <QRhiWidget>
class QRhiCommandBuffer;
struct ID3D11Device;
struct ID3D11DeviceContext;
struct ID3D11Texture2D;
struct ID3D11RenderTargetView;
struct ID3D11ShaderResourceView;
struct ID3D11VertexShader;
struct ID3D11PixelShader;
struct ID3D11BlendState;
struct ID3D11SamplerState;
struct ID3D11RasterizerState;
struct ID3D11DepthStencilState;
#else
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#endif

class MpvHttpStreamRelay;
class NativeDanmakuOverlay;
class QOpenGLFramebufferObject;
class QOpenGLTextureBlitter;
struct UserAgentConfig;

class MpvWidget :
#if defined(QEMBY_MPV_D3D11_RENDER)
    public QRhiWidget
#else
    public QOpenGLWidget, protected QOpenGLFunctions
#endif
{
    Q_OBJECT
public:
    explicit MpvWidget(QWidget *parent = nullptr);
    ~MpvWidget() override;


    void shutdown();

    void loadMedia(const QString &url, const QString &serverId = QString());
    void setHttpHeaders(QMap<QString, QString> headers) { m_httpHeaders = std::move(headers); }
    void setTlsPolicy(QUrl origin, bool ignoreErrors) { m_tlsOrigin = std::move(origin); m_ignoreTlsErrors = ignoreErrors; }
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
    void previewSourceChanged(const QString &url, const QVariantMap &networkOptions);
    void errorOccurred(const QString &errorMsg);
    // 表面呈现节拍 (GL: frameSwapped / D3D11: frameSubmitted), 弹幕帧泵的统一驱动源。
    void surfaceFrameSwapped();

protected:
#if defined(QEMBY_MPV_D3D11_RENDER)
    void initialize(QRhiCommandBuffer *cb) override;
    void render(QRhiCommandBuffer *cb) override;
    void releaseResources() override;
#else
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
#endif

private slots:
#if !defined(QEMBY_MPV_D3D11_RENDER)
    void cleanupGL();
#endif
    void maybeUpdate();

private:
    static void onMpvRenderUpdate(void *ctx);
#if !defined(QEMBY_MPV_D3D11_RENDER)
    static void *getProcAddress(void *ctx, const char *name);
#endif
    void loadMediaNow(const QString &url, const QString &serverId, bool wasPending);
    void applyUserAgentConfig(const UserAgentConfig &config);
    void publishPreviewSource();
    // mpv 渲染上下文生命周期 (双平台共用入口, 内部按后端分支)。
    bool initMpvRenderContext();
    void onRenderContextReady();
    void freeMpvRenderContext();
    void handleSurfacePresented();
    void resetVideoPresentation(bool resetMedia);
    void refreshVideoOutputState();
    void recordVideoFrameRendered(bool framePresent);
#if defined(QEMBY_MPV_D3D11_RENDER)
    // D3D11 渲染管线 (Qt RHI 桥 + 弹幕纹理混合)。
    bool ensureD3D11Resources(const QSize &targetSize);
    void releaseD3D11Resources();
    bool rasterizeDanmaku(const QSize &targetSize);
    void drawDanmakuQuad(const QSize &targetSize);
    ID3D11Device *d3dDevice() const;
    ID3D11DeviceContext *d3dContext() const;
#else
    bool paintNativeComposite(int width, int height);
    void releaseVideoCache();
    void checkCompositeGlErrors(const char *stage);
#endif

    MpvController *m_controller;
    MpvHttpStreamRelay *m_streamRelay = nullptr;
    QMap<QString, QString> m_httpHeaders;
    QUrl m_tlsOrigin;
    bool m_ignoreTlsErrors = false;
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

#if defined(QEMBY_MPV_D3D11_RENDER)
    // 借用 Qt RHI 的设备/上下文/颜色纹理 (不持有所有权的除外)。
    ID3D11Texture2D *m_d3dColorTex = nullptr;          // AddRef
    ID3D11RenderTargetView *m_d3dColorRtv = nullptr;   // owned
    ID3D11Texture2D *m_danmakuTex = nullptr;           // owned
    ID3D11ShaderResourceView *m_danmakuSrv = nullptr;  // owned
    ID3D11VertexShader *m_quadVs = nullptr;            // owned
    ID3D11PixelShader *m_quadPs = nullptr;             // owned
    ID3D11BlendState *m_blendState = nullptr;          // owned
    ID3D11SamplerState *m_sampler = nullptr;           // owned
    ID3D11RasterizerState *m_rasterState = nullptr;    // owned
    ID3D11DepthStencilState *m_depthState = nullptr;   // owned
    QImage m_danmakuBuffer;
    QSize m_danmakuTexSize;
#endif


    QString m_pendingUrl;
    QString m_pendingServerId;
    QString m_currentUrl;
    QString m_previewUrl;
    QString m_currentServerId;
    QString m_defaultUserAgent;
    bool m_customUserAgentApplied = false;
};

#endif
