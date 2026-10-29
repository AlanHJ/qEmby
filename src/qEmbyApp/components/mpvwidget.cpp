#include "mpvwidget.h"
#include <config/config_keys.h>
#include <config/configstore.h>
#include "mpvhttpstreamrelay.h"
#include "../utils/httprangeutils.h"
#include "nativedanmakuoverlay.h"
#include "../utils/logredactionutils.h"
#include "../utils/mpvrendertimingutils.h"
#include <QMetaObject>
#include <QCoreApplication>
#include <QUrl>
#include <QVariantMap>
#include <QPainter>
#include <algorithm>
// MpvWidget 持有 unique_ptr<QOpenGLFramebufferObject/QOpenGLTextureBlitter>,
// 两个后端分支的析构都需要完整类型, 故无条件包含 (Qt OpenGL 头跨平台可用)。
#include <QOpenGLFramebufferObject>
#include <QOpenGLTextureBlitter>
#include "api/proxymanager.h"
#include "api/useragentmanager.h"

#if defined(QEMBY_MPV_D3D11_RENDER)
// Windows Direct3D 11 渲染通道 (Qt RHI 桥)。仅在 CMake 判定
// WIN32 && Qt >= 6.7 (QEMBY_MPV_D3D11_RENDER) 时编译, 其余平台绝不引入
// Windows 独占头文件。
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstring>
#include <QScreen>
#include <rhi/qrhi.h>
#if defined(__has_include)
#if __has_include(<mpv/render_d3d11.h>)
#include <mpv/render_d3d11.h>
#define QEMBY_HAVE_MPV_RENDER_D3D11_H 1
#endif
#endif
#ifndef QEMBY_HAVE_MPV_RENDER_D3D11_H
// 与 mpv (e471496c56) render_d3d11.h ABI 一致的最小结构定义, 供捆绑 SDK 未
// 附带该头文件时回退 (两处定义互斥, 不会重定义)。
typedef struct mpv_d3d11_init_params {
    void *device;
} mpv_d3d11_init_params;
typedef struct mpv_d3d11_fbo {
    void *tex;
} mpv_d3d11_fbo;
#endif
#else
#include <QOpenGLContext>
#include <QSurfaceFormat>
#endif

namespace {

#if defined(QEMBY_MPV_D3D11_RENDER)
// 全屏三角形 (SV_VertexID, 无顶点缓冲), 采样纹理按预乘 alpha 混合,
// 用于把弹幕 QImage 纹理叠加到 mpv 已渲染的视频帧之上。
const char kQuadVertexShaderHlsl[] = R"(
struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};
VSOut main(uint vid : SV_VertexID) {
    VSOut o;
    float2 uv = float2((vid << 1) & 2, vid & 2);
    o.pos = float4(uv * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
    o.uv = uv;
    return o;
}
)";

const char kQuadPixelShaderHlsl[] = R"(
Texture2D quadTex : register(t0);
SamplerState quadSampler : register(s0);
struct VSOut {
    float4 pos : SV_Position;
    float2 uv  : TEXCOORD0;
};
float4 main(VSOut i) : SV_Target {
    return quadTex.Sample(quadSampler, i.uv);
}
)";

// d3dcompiler 为系统组件 (windeployqt 会随 Qt 一并收编), 运行期动态加载,
// 缺失时仅禁用弹幕叠加而保留视频渲染 (容灾回退)。
using D3DCompileFn = HRESULT(WINAPI *)(LPCVOID, SIZE_T, LPCSTR,
                                       const D3D10_SHADER_MACRO *, LPD3D10INCLUDE,
                                       LPCSTR, LPCSTR, UINT, UINT,
                                       ID3DBlob **, ID3DBlob **);

bool compileQuadShader(const char *source, const char *entry, const char *target,
                       ID3DBlob **blobOut) {
    static D3DCompileFn compile = nullptr;
    static bool resolved = false;
    if (!resolved) {
        resolved = true;
        for (const wchar_t *name : {L"d3dcompiler_47.dll", L"d3dcompiler_43.dll"}) {
            if (HMODULE module = LoadLibraryW(name)) {
                compile = reinterpret_cast<D3DCompileFn>(
                    GetProcAddress(module, "D3DCompile"));
                if (compile) {
                    break;
                }
                FreeLibrary(module);
            }
        }
        qInfo() << "[MpvWidget] D3DCompile resolved:" << (compile != nullptr);
    }
    if (!compile) {
        return false;
    }
    ID3DBlob *errors = nullptr;
    const HRESULT hr = compile(source, strlen(source), nullptr, nullptr, nullptr,
                               entry, target, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
                               blobOut, &errors);
    if (errors) {
        if (FAILED(hr)) {
            qWarning() << "[MpvWidget] HLSL compilation failed"
                       << "| entry:" << entry
                       << "| log:" << QString::fromUtf8(
                              static_cast<const char *>(errors->GetBufferPointer()),
                              static_cast<int>(errors->GetBufferSize()));
        }
        errors->Release();
    }
    return SUCCEEDED(hr);
}
#endif

} // namespace

MpvWidget::MpvWidget(QWidget *parent)
#if defined(QEMBY_MPV_D3D11_RENDER)
    : QRhiWidget(parent), m_mpv_gl(nullptr) {
    // 显式钉住 Direct3D 11 后端 (Windows 平台 QRhiWidget 的默认值, 此处显式
    // 声明以获得确定性)。QRhiWidget 渲染进纹理并由 Qt 背板合成, 上层兄弟控件
    // (HUD/弹幕容器) 的堆叠与输入分发行为与 QOpenGLWidget 完全一致。
    setApi(QRhiWidget::Api::Direct3D11);
    // 独立 UI 节拍: 以屏幕刷新率驱动重绘, 弹幕/界面动画与视频帧率彻底解耦
    // (24fps 视频不再把整个 UI 渲染循环拖到 24Hz)。
    m_uiRefreshTimer.setTimerType(Qt::PreciseTimer);
    m_uiRefreshTimer.setInterval(uiRefreshIntervalMs());
    connect(&m_uiRefreshTimer, &QTimer::timeout, this, [this]() { update(); });
#else
    : QOpenGLWidget(parent), m_mpv_gl(nullptr) {





    QSurfaceFormat format = this->format();

    // Windows GL 回退路径: 非阻塞交换 (present 不等待 vsync); 其余平台保持
    // vsync 以避免画面撕裂。
#ifdef Q_OS_WIN
    format.setSwapInterval(0);
#else
    format.setSwapInterval(1);
#endif
    this->setFormat(format);
#endif

    m_videoDueTimer.setSingleShot(true);
    m_videoDueTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_videoDueTimer, &QTimer::timeout, this, [this]() {
        if (m_mpv_gl && m_nativeDanmakuOverlay && m_nativeDanmakuOverlay->isActive()) {
            update();
        }
    });
#if defined(QEMBY_MPV_D3D11_RENDER)
    connect(this, &QRhiWidget::frameSubmitted, this, [this]() {
        handleSurfacePresented();
        Q_EMIT surfaceFrameSwapped();
    }, Qt::DirectConnection);
#else
    connect(this, &QOpenGLWidget::aboutToCompose, this, [this]() {
        if (m_nativeCompositeActive) {
            m_compositionClock.start();
        }
    }, Qt::DirectConnection);
    connect(this, &QOpenGLWidget::frameSwapped, this, [this]() {
        if (m_nativeCompositeActive && m_compositionClock.isValid()) {
            m_maxCompositionMs = std::max(m_maxCompositionMs,
                m_compositionClock.nsecsElapsed() / 1000000.0);
            m_compositionClock.invalidate();
        }
        handleSurfacePresented();
        Q_EMIT surfaceFrameSwapped();
        const bool wasWaiting = m_videoAwaitingSwap;
        m_videoAwaitingSwap = false;
        if (wasWaiting && m_nativeCompositeActive && m_videoFramePending &&
            !m_videoDueTimer.isActive()) {
            update();
        }
    }, Qt::DirectConnection);
#endif

    m_controller = new MpvController(this);
    connect(m_controller, &MpvController::playbackStarting, this, [this]() {
        resetVideoPresentation(true);
    });
    connect(m_controller, &MpvController::playbackStopped, this, [this]() {
        resetVideoPresentation(true);
    });
    connect(m_controller, &MpvController::fileLoaded, this, [this]() {
        m_mediaLoaded = true;
        refreshVideoOutputState();
        qInfo() << "[MpvWidget] Active decoder"
                << "| hwdec-current:"
                << m_controller->getProperty(QStringLiteral("hwdec-current")).toString()
                << "| current-vo:"
                << m_controller->getProperty(QStringLiteral("current-vo")).toString();
    });
    connect(m_controller, &MpvController::videoOutputChanged,
            this, &MpvWidget::refreshVideoOutputState);
    connect(m_controller, &MpvController::propertyRead, this,
            [this](const QString &property, const QVariant &value) {
        if (property != QLatin1String("video-out-params")) {
            return;
        }
        const QVariantMap output = value.toMap();
        m_videoOutputAvailable = output.value(QStringLiteral("w")).toInt() > 0 &&
                                 output.value(QStringLiteral("h")).toInt() > 0;
        qDebug() << "[MpvWidget] Async video output state | available:" << m_videoOutputAvailable
                 << "| mediaLoaded:" << m_mediaLoaded;
        if (!m_videoOutputAvailable) {
            resetVideoPresentation(false);
        }
        update();
    });
    m_streamRelay = new MpvHttpStreamRelay(this);
    connect(UserAgentManager::instance(),
            &UserAgentManager::userAgentChanged, this, [this]() {
                if (m_currentUrl.isEmpty()) {
                    return;
                }
                const UserAgentConfig config =
                    m_currentServerId.isEmpty()
                        ? UserAgentManager::instance()->resolveForUrl(
                              QUrl(m_currentUrl))
                        : UserAgentManager::instance()->resolveForServerId(
                              m_currentServerId);
                applyUserAgentConfig(config);
                publishPreviewSource();
                qInfo() << "[MpvWidget] User-Agent configuration refreshed"
                        << "| serverId:"
                        << (m_currentServerId.isEmpty()
                                ? QStringLiteral("<none>")
                                : m_currentServerId)
                        << "| customEnabled:" << config.isEffective();
            });
    connect(m_streamRelay, &MpvHttpStreamRelay::upstreamSpeedChanged, this,
            &MpvWidget::networkSpeedChanged);




    connect(m_controller, &MpvController::positionChanged, this, &MpvWidget::positionChanged);
    connect(m_controller, &MpvController::durationChanged, this, &MpvWidget::durationChanged);
    connect(m_controller, &MpvController::playbackStateChanged, this, &MpvWidget::playbackStateChanged);
#if defined(QEMBY_MPV_D3D11_RENDER)
    connect(m_controller, &MpvController::playbackStateChanged, this, [this](bool isPaused) {
        m_playbackPaused = isPaused;
        updateUiRefreshTimer();
    });
#endif
    connect(m_controller, &MpvController::errorOccurred, this, &MpvWidget::errorOccurred);




    if (m_controller->init()) {
        m_defaultUserAgent =
            m_controller->getProperty(QStringLiteral("user-agent")).toString();
        qDebug() << "[MpvWidget] captured default User-Agent"
                 << "| hasValue:" << !m_defaultUserAgent.isEmpty();
    }
}

MpvWidget::~MpvWidget() {




#if !defined(QEMBY_MPV_D3D11_RENDER)
    if (QOpenGLContext *ctx = context()) {
        disconnect(ctx, &QOpenGLContext::aboutToBeDestroyed,
                   this, &MpvWidget::cleanupGL);
    }
#endif





    shutdown();
    if (m_controller) {
        m_controller->deleteLater();
        m_controller = nullptr;
    }
}


void MpvWidget::shutdown() {
    if (!m_controller) return;
    m_previewUrl.clear();
    emit previewSourceChanged(QString(), QVariantMap());



    this->blockSignals(true);
    m_controller->blockSignals(true);




    m_controller->command(QVariantList{"stop"});
    if (m_streamRelay) {
        m_streamRelay->stop();
    }
    if (m_usingStreamRelay) {
        m_usingStreamRelay = false;
        Q_EMIT relayActiveChanged(false);
    }

#if defined(QEMBY_MPV_D3D11_RENDER)
    releaseD3D11Resources();
#else
    cleanupGL();
#endif

    m_controller->forceCleanup();
}

void MpvWidget::freeMpvRenderContext() {
    if (!m_mpv_gl) {
        return;
    }
    mpv_render_context_set_update_callback(m_mpv_gl, nullptr, nullptr);
    mpv_render_context_free(m_mpv_gl);
    m_mpv_gl = nullptr;
}

#if !defined(QEMBY_MPV_D3D11_RENDER)
void MpvWidget::cleanupGL() {
    resetVideoPresentation(false);
    m_videoDueTimer.stop();
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->setGraphicsAvailable(false);
    }
    if (m_mpv_gl) {





        if (isValid()) {
            makeCurrent();
            releaseVideoCache();
            freeMpvRenderContext();
            doneCurrent();
        } else {
            releaseVideoCache();
            qWarning() << "[MpvWidget] OpenGL surface is invalid; freeing MPV render context without makeCurrent";
            freeMpvRenderContext();
        }
        qInfo() << "[MpvWidget] MPV render context released with OpenGL context";
    }
}

void *MpvWidget::getProcAddress(void *ctx, const char *name) {
    Q_UNUSED(ctx);
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    return glctx ? reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name))) : nullptr;
}
#endif

void MpvWidget::onMpvRenderUpdate(void *ctx) {

    auto *self = static_cast<MpvWidget *>(ctx);
    QMetaObject::invokeMethod(self, "maybeUpdate", Qt::QueuedConnection);
}

void MpvWidget::maybeUpdate() {
    m_videoFramePending = true;
    update();
}

void MpvWidget::setNativeDanmakuOverlay(NativeDanmakuOverlay *overlay) {
    m_nativeDanmakuOverlay = overlay;
    update();
}

void MpvWidget::resetVideoPresentation(bool resetMedia) {
    m_videoFrameRendered = false;
    m_videoFrameForSwap = false;
    if (resetMedia) {
        m_mediaLoaded = false;
        m_videoOutputAvailable = false;
        m_discardVideoCache = true;
        m_videoCacheDirty = true;
        m_videoFramePending = true;
        m_videoAwaitingSwap = false;
        m_videoDueTimer.stop();
    }
    if (m_videoPresented) {
        m_videoPresented = false;
        qInfo() << "[MpvWidget] Video presentation reset | mediaChanged:" << resetMedia;
        Q_EMIT videoPresentationChanged(false);
    }
#if defined(QEMBY_MPV_D3D11_RENDER)
    updateUiRefreshTimer();
#endif
}

void MpvWidget::refreshVideoOutputState() {


    m_controller->requestProperty(QStringLiteral("video-out-params"));
}

void MpvWidget::recordVideoFrameRendered(bool framePresent) {
    if (m_videoPresented) {
        return;
    }





    m_videoFrameRendered = m_videoFrameRendered || framePresent;
    m_videoFrameForSwap = m_mediaLoaded && m_videoOutputAvailable && m_videoFrameRendered;
}

void MpvWidget::handleSurfacePresented() {
    if (m_videoFrameForSwap) {
        m_videoFrameForSwap = false;
        m_videoPresented = true;
        qInfo() << "[MpvWidget] Video presentation ready | firstFrameSwapped:" << true;
        Q_EMIT videoPresentationChanged(true);
    }
}

void MpvWidget::onRenderContextReady() {
    if (!m_pendingUrl.isEmpty()) {
        loadMediaNow(m_pendingUrl, m_pendingServerId, true);
        m_pendingUrl.clear();
        m_pendingServerId.clear();
    }

    if (m_resumeWhenRenderReady) {
        m_resumeWhenRenderReady = false;
        m_controller->setProperty("pause", false);
    }

    update();
}

#if defined(QEMBY_MPV_D3D11_RENDER)

// ===================== Direct3D 11 渲染管线 (Qt RHI 桥) =====================
//
// mpv 以 MPV_RENDER_API_TYPE_D3D11 创建渲染上下文, 直接把视频帧渲染进 Qt
// RHI 的颜色纹理 (QRhiWidget::colorTexture); d3d11va 硬解 Surface 在同一
// ID3D11Device 上流转, 全程无 GPU->CPU->GPU 回传 (原生零拷贝)。弹幕由
// NativeDanmakuOverlay 以 QPainter 光栅进 QImage 后作为纹理按预乘 alpha
// 混合叠加。整个渲染在 QRhiCommandBuffer 的 beginExternal 块内以原生 D3D11
// 完成, 与 QRhi 状态跟踪契约兼容。

ID3D11Device *MpvWidget::d3dDevice() const {
    if (!rhi()) {
        return nullptr;
    }
    auto *native = static_cast<const QRhiD3D11NativeHandles *>(rhi()->nativeHandles());
    return native ? static_cast<ID3D11Device *>(native->dev) : nullptr;
}

ID3D11DeviceContext *MpvWidget::d3dContext() const {
    if (!rhi()) {
        return nullptr;
    }
    auto *native = static_cast<const QRhiD3D11NativeHandles *>(rhi()->nativeHandles());
    return native ? static_cast<ID3D11DeviceContext *>(native->context) : nullptr;
}

bool MpvWidget::initMpvRenderContext() {
    freeMpvRenderContext();

    ID3D11Device *device = d3dDevice();
    if (!device) {
        qCritical() << "[MpvWidget] Qt RHI Direct3D 11 device unavailable";
        emit errorOccurred(tr("Direct3D 11 rendering initialization failed."));
        return false;
    }

    mpv_d3d11_init_params d3d11_init{device};
    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_D3D11)},
        {MPV_RENDER_PARAM_D3D11_INIT_PARAMS, &d3d11_init},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    const int createError = mpv_render_context_create(&m_mpv_gl, m_controller->mpv(), params);
    if (createError < 0) {
        qCritical() << "[MpvWidget] MPV D3D11 render context creation failed"
                    << "| error:" << mpv_error_string(createError);
        emit errorOccurred(tr("Direct3D 11 rendering initialization failed."));
        return false;
    }

    mpv_render_context_set_update_callback(m_mpv_gl, onMpvRenderUpdate, this);
    m_mpvDevice = device;

    qInfo() << "[MpvWidget] MPV render context initialized"
            << "| backend:D3D11 | zeroCopyHwdec:d3d11va"
            << "| device:" << device
            << "| resumePending:" << m_resumeWhenRenderReady;

    onRenderContextReady();
    return true;
}

void MpvWidget::initialize(QRhiCommandBuffer *cb) {
    Q_UNUSED(cb);
    ID3D11Device *device = d3dDevice();
    if (m_mpv_gl && device && device == m_mpvDevice) {
        // Qt 在颜色纹理重建 (窗口缩放等) 时会再次回调 initialize()。设备未变时
        // 绝不能重建 mpv 渲染上下文: 重建会销毁 VO ("Video: no video"), 当前
        // mpv 构建的视频链无法恢复, 表现为缩放窗口后永久黑屏 (实测日志)。
        qInfo() << "[MpvWidget] Qt RHI texture re-initialized | device unchanged"
                << "| keeping MPV D3D11 render context";
        if (m_d3dColorRtv) {
            m_d3dColorRtv->Release();
            m_d3dColorRtv = nullptr;
        }
        if (m_d3dColorTex) {
            m_d3dColorTex->Release();
            m_d3dColorTex = nullptr;
        }
        return;
    }

    resetVideoPresentation(false);
    releaseD3D11Resources();
    m_nativeCompositeFailed = false;
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->invalidateGraphicsCache();
    }

    qInfo() << "[MpvWidget] Qt RHI initialized"
            << "| api:" << (api() == QRhiWidget::Api::Direct3D11 ? "d3d11" : "other")
            << "| device:" << device;
    initMpvRenderContext();
}

bool MpvWidget::ensureD3D11Resources(const QSize &targetSize) {
    ID3D11Device *device = d3dDevice();
    if (!device || !d3dContext()) {
        return false;
    }

    if (!m_quadVs || !m_quadPs) {
        ID3DBlob *vsBlob = nullptr;
        ID3DBlob *psBlob = nullptr;
        if (!compileQuadShader(kQuadVertexShaderHlsl, "main", "vs_4_0", &vsBlob) ||
            !compileQuadShader(kQuadPixelShaderHlsl, "main", "ps_4_0", &psBlob)) {
            if (vsBlob) vsBlob->Release();
            if (psBlob) psBlob->Release();
            return false;
        }
        HRESULT hr = device->CreateVertexShader(vsBlob->GetBufferPointer(),
                                                vsBlob->GetBufferSize(), nullptr, &m_quadVs);
        if (SUCCEEDED(hr)) {
            hr = device->CreatePixelShader(psBlob->GetBufferPointer(),
                                           psBlob->GetBufferSize(), nullptr, &m_quadPs);
        }
        vsBlob->Release();
        psBlob->Release();
        if (FAILED(hr)) {
            qWarning() << "[MpvWidget] Danmaku quad shader creation failed | hr:" << Qt::hex << hr;
            return false;
        }

        D3D11_BLEND_DESC blend{};
        blend.RenderTarget[0].BlendEnable = TRUE;
        blend.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
        blend.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
        blend.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
        blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
        device->CreateBlendState(&blend, &m_blendState);

        D3D11_SAMPLER_DESC sampler{};
        sampler.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
        sampler.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
        sampler.MaxLOD = D3D11_FLOAT32_MAX;
        device->CreateSamplerState(&sampler, &m_sampler);

        D3D11_RASTERIZER_DESC rasterizer{};
        rasterizer.FillMode = D3D11_FILL_SOLID;
        rasterizer.CullMode = D3D11_CULL_NONE;
        rasterizer.DepthClipEnable = TRUE;
        device->CreateRasterizerState(&rasterizer, &m_rasterState);

        D3D11_DEPTH_STENCIL_DESC depth{};
        depth.DepthEnable = FALSE;
        depth.StencilEnable = FALSE;
        device->CreateDepthStencilState(&depth, &m_depthState);

        if (!m_blendState || !m_sampler || !m_rasterState || !m_depthState) {
            qWarning() << "[MpvWidget] Danmaku pipeline state creation failed";
            return false;
        }
    }

    // Qt RHI 颜色纹理 (可能随尺寸/重建而更换), 弹幕混合需要它的 RTV。
    auto *colorTex = reinterpret_cast<ID3D11Texture2D *>(
        colorTexture() ? colorTexture()->nativeTexture().object : 0);
    if (colorTex != m_d3dColorTex) {
        if (m_d3dColorRtv) {
            m_d3dColorRtv->Release();
            m_d3dColorRtv = nullptr;
        }
        if (m_d3dColorTex) {
            m_d3dColorTex->Release();
            m_d3dColorTex = nullptr;
        }
        if (colorTex &&
            SUCCEEDED(device->CreateRenderTargetView(colorTex, nullptr, &m_d3dColorRtv))) {
            m_d3dColorTex = colorTex;
            m_d3dColorTex->AddRef();
        } else {
            qWarning() << "[MpvWidget] Danmaku overlay RTV creation failed";
            return false;
        }
    }

    if (m_danmakuTex && m_danmakuTexSize != targetSize) {
        m_danmakuSrv->Release();
        m_danmakuSrv = nullptr;
        m_danmakuTex->Release();
        m_danmakuTex = nullptr;
        m_danmakuTexSize = QSize();
    }

    // 视频帧缓存纹理: mpv 只在有新帧时渲染进它, 无新帧时复用其中已呈现的画面。
    if (m_videoTex && m_videoTexSize != targetSize) {
        m_videoSrv->Release();
        m_videoSrv = nullptr;
        m_videoTex->Release();
        m_videoTex = nullptr;
        m_videoTexSize = QSize();
    }
    if (!m_videoTex) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(targetSize.width());
        desc.Height = static_cast<UINT>(targetSize.height());
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &m_videoTex)) ||
            FAILED(device->CreateShaderResourceView(m_videoTex, nullptr, &m_videoSrv))) {
            if (m_videoTex) { m_videoTex->Release(); m_videoTex = nullptr; }
            qWarning() << "[MpvWidget] Video cache texture creation failed | size:" << targetSize;
            return false;
        }
        m_videoTexSize = targetSize;
        m_videoTextureDirty = true;
    }
    return true;
}

void MpvWidget::releaseD3D11Resources() {
    freeMpvRenderContext();
    m_mpvDevice = nullptr;
    m_uiRefreshTimer.stop();
    if (m_videoSrv) { m_videoSrv->Release(); m_videoSrv = nullptr; }
    if (m_videoTex) { m_videoTex->Release(); m_videoTex = nullptr; }
    m_videoTexSize = QSize();
    m_videoTextureDirty = true;
    if (m_danmakuSrv) { m_danmakuSrv->Release(); m_danmakuSrv = nullptr; }
    if (m_danmakuTex) { m_danmakuTex->Release(); m_danmakuTex = nullptr; }
    if (m_d3dColorRtv) { m_d3dColorRtv->Release(); m_d3dColorRtv = nullptr; }
    if (m_d3dColorTex) { m_d3dColorTex->Release(); m_d3dColorTex = nullptr; }
    if (m_quadVs) { m_quadVs->Release(); m_quadVs = nullptr; }
    if (m_quadPs) { m_quadPs->Release(); m_quadPs = nullptr; }
    if (m_blendState) { m_blendState->Release(); m_blendState = nullptr; }
    if (m_sampler) { m_sampler->Release(); m_sampler = nullptr; }
    if (m_rasterState) { m_rasterState->Release(); m_rasterState = nullptr; }
    if (m_depthState) { m_depthState->Release(); m_depthState = nullptr; }
    m_danmakuBuffer = QImage();
    m_danmakuTexSize = QSize();
}

void MpvWidget::releaseResources() {
    // Qt 即将销毁/更换 QRhi (窗口变更)。mpv 渲染上下文持有 ID3D11Device 引用,
    // 设备不会随之销毁 —— 保留上下文以维持视频链 (VO) 存活, 仅丢弃绑定在 Qt
    // 纹理上的 RTV。若后续 initialize() 发现设备已更换, 会整体重建。
    qInfo() << "[MpvWidget] Qt RHI released | keeping MPV render context"
            << "| hasContext:" << (m_mpv_gl != nullptr);
    if (m_d3dColorRtv) {
        m_d3dColorRtv->Release();
        m_d3dColorRtv = nullptr;
    }
    if (m_d3dColorTex) {
        m_d3dColorTex->Release();
        m_d3dColorTex = nullptr;
    }
}

bool MpvWidget::rasterizeDanmaku(const QSize &targetSize) {
    ID3D11Device *device = d3dDevice();
    ID3D11DeviceContext *context = d3dContext();
    if (!device || !context || !m_nativeDanmakuOverlay) {
        return false;
    }

    if (!m_danmakuTex) {
        D3D11_TEXTURE2D_DESC desc{};
        desc.Width = static_cast<UINT>(targetSize.width());
        desc.Height = static_cast<UINT>(targetSize.height());
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.Usage = D3D11_USAGE_DEFAULT;
        desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        if (FAILED(device->CreateTexture2D(&desc, nullptr, &m_danmakuTex)) ||
            FAILED(device->CreateShaderResourceView(m_danmakuTex, nullptr, &m_danmakuSrv))) {
            if (m_danmakuTex) { m_danmakuTex->Release(); m_danmakuTex = nullptr; }
            qWarning() << "[MpvWidget] Danmaku texture creation failed | size:" << targetSize;
            return false;
        }
        m_danmakuTexSize = targetSize;
    }

    if (m_danmakuBuffer.size() != targetSize) {
        m_danmakuBuffer = QImage(targetSize, QImage::Format_RGBA8888_Premultiplied);
    }
    m_danmakuBuffer.fill(Qt::transparent);
    const qreal dpr = devicePixelRatioF();
    QPainter painter(&m_danmakuBuffer);
    // NativeDanmakuOverlay 以逻辑坐标绘制 (与 QOpenGLWidget 的设备变换一致)。
    painter.scale(dpr, dpr);
    const qreal danmakuPosition = m_nativeDanmakuOverlay->beginFrame();
    m_nativeDanmakuOverlay->paint(painter, danmakuPosition);
    painter.end();

    context->UpdateSubresource(m_danmakuTex, 0, nullptr,
                               m_danmakuBuffer.constBits(),
                               static_cast<UINT>(m_danmakuBuffer.bytesPerLine()), 0);
    return true;
}

void MpvWidget::drawOverlayQuad(ID3D11ShaderResourceView *srv, bool alphaBlend,
                               const QSize &targetSize) {
    ID3D11DeviceContext *context = d3dContext();
    if (!context || !m_d3dColorRtv || !srv) {
        return;
    }
    D3D11_VIEWPORT viewport{};
    viewport.Width = static_cast<FLOAT>(targetSize.width());
    viewport.Height = static_cast<FLOAT>(targetSize.height());
    viewport.MaxDepth = 1.0f;
    context->RSSetViewports(1, &viewport);
    context->RSSetState(m_rasterState);
    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->VSSetShader(m_quadVs, nullptr, 0);
    context->PSSetShader(m_quadPs, nullptr, 0);
    context->PSSetShaderResources(0, 1, &srv);
    context->PSSetSamplers(0, 1, &m_sampler);
    context->OMSetBlendState(alphaBlend ? m_blendState : nullptr, nullptr, 0xffffffffu);
    context->OMSetDepthStencilState(m_depthState, 0);
    context->OMSetRenderTargets(1, &m_d3dColorRtv, nullptr);
    context->Draw(3, 0);
    ID3D11ShaderResourceView *nullSrv = nullptr;
    context->PSSetShaderResources(0, 1, &nullSrv);
}

int MpvWidget::uiRefreshIntervalMs() const {
    qreal refreshHz = 60.0;
    if (const QScreen *scr = screen()) {
        const qreal rate = scr->refreshRate();
        if (rate >= 24.0 && rate <= 1000.0) {
            refreshHz = rate;
        }
    }
    return std::max(4, qRound(1000.0 / refreshHz));
}

void MpvWidget::updateUiRefreshTimer() {
    const bool danmakuActive = m_nativeDanmakuOverlay && m_nativeDanmakuOverlay->isActive();
    // UI 节拍与视频帧率解耦: 有媒体在播 (或弹幕活动) 时按屏幕刷新率驱动重绘,
    // 弹幕位移由引擎按物理时钟外推, 保证满刷新率平滑度。
    const bool needed = m_mediaLoaded && (danmakuActive || !m_playbackPaused);
    if (needed) {
        if (!m_uiRefreshTimer.isActive()) {
            m_uiRefreshTimer.start();
        }
    } else if (m_uiRefreshTimer.isActive()) {
        m_uiRefreshTimer.stop();
    }
}

void MpvWidget::render(QRhiCommandBuffer *cb) {
    if (!m_mpv_gl) {
        return;
    }
    const QSize targetSize = colorTexture() ? colorTexture()->pixelSize() : QSize();
    if (targetSize.isEmpty()) {
        return;
    }
    updateUiRefreshTimer();

    auto *colorTex = reinterpret_cast<ID3D11Texture2D *>(
        colorTexture()->nativeTexture().object);
    // 管线不可用 (如 d3dcompiler 缺失) 时回退: mpv 直接渲染进 Qt 颜色纹理,
    // 无覆盖层, 视频播放不受影响。
    const bool pipelineReady = colorTex && ensureD3D11Resources(targetSize);
    if (!colorTex) {
        return;
    }

    // 按需纹理复用: 仅当有新视频帧或视频缓存纹理失效 (新建/换尺寸) 时执行
    // 完整 mpv 渲染, 否则复用缓存纹理中已呈现的画面 —— 重绘节拍与视频帧率
    // 彻底解耦, 无新帧的 tick 只更新覆盖层。
    const bool newVideoFrame =
        (mpv_render_context_update(m_mpv_gl) & MPV_RENDER_UPDATE_FRAME) != 0;
    const bool renderVideo = !pipelineReady || newVideoFrame || m_videoTextureDirty;

    // 呈现记账 (与 GL 路径的 paintGL 一致)。
    mpv_render_frame_info info{};
    if (renderVideo && !m_videoPresented) {
        mpv_render_context_get_info(m_mpv_gl, {MPV_RENDER_PARAM_NEXT_FRAME_INFO, &info});
    }

    bool danmakuActive = pipelineReady && m_nativeDanmakuOverlay &&
                         m_nativeDanmakuOverlay->isActive() && !m_nativeCompositeFailed;
    if (danmakuActive && !rasterizeDanmaku(targetSize)) {
        qWarning() << "[Danmaku][Composite] Disabled after danmaku raster failure; preserving video";
        m_nativeCompositeFailed = true;
        m_nativeDanmakuOverlay->setGraphicsAvailable(false);
        danmakuActive = false;
    }

    cb->beginPass(renderTarget(), Qt::black, {1.0f, 0}, nullptr,
                  QRhiCommandBuffer::ExternalContent);
    cb->beginExternal();

    if (renderVideo) {
        m_videoTextureDirty = false;
        // mpv 渲染进视频缓存纹理 (同设备 d3d11va Surface, 零拷贝直通);
        // mpv 使用即时上下文且不恢复状态, 其后我们显式重建自身状态。
        mpv_d3d11_fbo fbo{pipelineReady ? m_videoTex : colorTex};
        mpv_render_param params[] = {
            {MPV_RENDER_PARAM_D3D11_FBO, &fbo},
            {MPV_RENDER_PARAM_INVALID, nullptr}
        };
        const int result = mpv_render_context_render(m_mpv_gl, params);
        // 有限诊断: 首几帧打印渲染结果与帧信息标志, 便于定位呈现链路断点。
        static int diagnosticFrames = 0;
        static int diagnosticFailures = 0;
        if (diagnosticFrames < 5) {
            ++diagnosticFrames;
            qInfo() << "[MpvWidget][D3D11][Diag] frame:" << diagnosticFrames
                    << "| renderResult:" << result
                    << "| infoFlags:" << info.flags
                    << "| videoTex:" << (pipelineReady ? m_videoTex : colorTex)
                    << "| targetSize:" << targetSize;
        }
        if (result >= 0) {
            const bool framePresent = (info.flags & MPV_RENDER_FRAME_INFO_PRESENT) &&
                (!(info.flags & MPV_RENDER_FRAME_INFO_REDRAW) ||
                 (m_mediaLoaded && m_videoOutputAvailable));
            recordVideoFrameRendered(framePresent);
        } else if (diagnosticFailures < 16) {
            ++diagnosticFailures;
            qWarning() << "[MpvWidget] D3D11 video render failed | error:" << result;
        }
    }

    if (pipelineReady) {
        drawOverlayQuad(m_videoSrv, false, targetSize);
        if (danmakuActive) {
            drawOverlayQuad(m_danmakuSrv, true, targetSize);
        }
    }

    cb->endExternal();
    cb->endPass();
}

#else

// ===================== OpenGL 渲染管线 (既有 GL 合成路径) =====================

bool MpvWidget::initMpvRenderContext() {
    freeMpvRenderContext();

    mpv_opengl_init_params gl_init_params{
        getProcAddress,
        nullptr
    };

    mpv_render_param params[]{
        {MPV_RENDER_PARAM_API_TYPE, const_cast<char *>(MPV_RENDER_API_TYPE_OPENGL)},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    const int createError = mpv_render_context_create(&m_mpv_gl, m_controller->mpv(), params);
    if (createError < 0) {
        qCritical() << "[MpvWidget] MPV render context creation failed"
                    << "| error:" << mpv_error_string(createError);
        emit errorOccurred(tr("OpenGL rendering initialization failed."));
        return false;
    }

    mpv_render_context_set_update_callback(m_mpv_gl, onMpvRenderUpdate, this);

    qInfo() << "[MpvWidget] MPV render context initialized"
            << "| backend:OpenGL | resumePending:" << m_resumeWhenRenderReady;

    onRenderContextReady();
    return true;
}

void MpvWidget::initializeGL() {
    initializeOpenGLFunctions();
    resetVideoPresentation(false);
    releaseVideoCache();
    m_nativeCompositeFailed = false;
    m_compositeGlErrors = 0;
    m_compositeAuditFrames = 0;
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->invalidateGraphicsCache();
    }

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx && ctx->format().majorVersion() < 2) {
        qCritical() << "Fatal Error: OpenGL version is too low.";
        emit errorOccurred(tr("当前环境缺乏必需的 OpenGL 硬件加速支持，视频渲染已禁用。"));
        return;
    }





    if (ctx) {
        connect(ctx, &QOpenGLContext::aboutToBeDestroyed,
                this, &MpvWidget::cleanupGL, Qt::DirectConnection);
    }

    initMpvRenderContext();
}

void MpvWidget::paintGL() {
    if (!m_mpv_gl) return;

    int w = static_cast<int>(width() * devicePixelRatio());
    int h = static_cast<int>(height() * devicePixelRatio());
    if (w == 0 || h == 0) return;

    if (m_nativeDanmakuOverlay && m_nativeDanmakuOverlay->isActive() &&
        !m_nativeCompositeFailed) {
        if (paintNativeComposite(w, h)) {
            return;
        }



        m_nativeCompositeFailed = true;
        m_nativeDanmakuOverlay->setGraphicsAvailable(false);
        qWarning() << "[Danmaku][Composite] Disabled after GL resource failure; preserving video";
    }
    if (m_nativeCompositeActive) {
        releaseVideoCache();
        qInfo() << "[Danmaku][Composite] Original video path restored";
    }

    int fbo = defaultFramebufferObject();
    int flip_y = 1;

    mpv_opengl_fbo mpfbo{fbo, w, h, 0};

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_frame_info info{};
    if (!m_videoPresented) {
        mpv_render_context_get_info(m_mpv_gl, {MPV_RENDER_PARAM_NEXT_FRAME_INFO, &info});
    }
    if (mpv_render_context_render(m_mpv_gl, params) >= 0) {
        const bool framePresent = (info.flags & MPV_RENDER_FRAME_INFO_PRESENT) &&
            (!(info.flags & MPV_RENDER_FRAME_INFO_REDRAW) ||
             (m_mediaLoaded && m_videoOutputAvailable));
        recordVideoFrameRendered(framePresent);
    }
}

void MpvWidget::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
    m_videoCacheDirty = true;
}

void MpvWidget::releaseVideoCache() {
    m_videoDueTimer.stop();
    m_videoBlitter.reset();
    m_videoFramebuffer.reset();
    m_videoFramePending = true;
    m_videoCacheDirty = true;
    m_videoAwaitingSwap = false;
    m_nativeCompositeActive = false;
    m_discardVideoCache = true;
    m_compositeStatsClock.invalidate();
    m_videoFrames = m_cachedVideoFrames = m_deferredVideoFrames = 0;
    m_maxVideoRenderMs = 0.0;
    m_maxCompositeCpuMs = m_maxCompositionMs = m_maxTargetDelayMs = 0.0;
    m_vsyncVideoFrames = 0;
    m_videoTargetUnit = 0;
    m_compositionClock.invalidate();
}

void MpvWidget::checkCompositeGlErrors(const char *stage) {
    if (!m_checkCompositeErrorsThisFrame) {
        return;
    }


    for (int i = 0; i < 8; ++i) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        if (m_compositeGlErrors++ < 16) {
            qWarning() << "[Danmaku][Composite] OpenGL error | stage:" << stage
                       << "| code:" << Qt::hex << error;
        }
    }
}

bool MpvWidget::paintNativeComposite(int w, int h) {
    QElapsedTimer compositeTime;
    compositeTime.start();
    const qreal danmakuPosition = m_nativeDanmakuOverlay->beginFrame();


    m_checkCompositeErrorsThisFrame = m_compositeAuditFrames < 60 ||
                                     m_compositeAuditFrames % 120 == 0;
    ++m_compositeAuditFrames;
    if (!m_nativeCompositeActive) {
        m_nativeCompositeActive = true;
        m_videoFramePending = true;
        m_videoCacheDirty = true;
        qInfo() << "[Danmaku][Composite] Enabled | surface:" << QSize(w, h)
                << "| singleContext:" << true << "| timedVideoCache:" << true
                << "| GL:" << reinterpret_cast<const char *>(glGetString(GL_VERSION))
                << "| renderer:" << reinterpret_cast<const char *>(glGetString(GL_RENDERER));
    }

    checkCompositeGlErrors("before-qt-painter");
    QPainter painter(this);


    painter.beginNativePainting();
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glActiveTexture(GL_TEXTURE0);
    glUseProgram(0);
    checkCompositeGlErrors("qt-native-boundary");
    if (m_discardVideoCache) {
        m_videoFramebuffer.reset();
        m_videoCacheDirty = true;
        m_videoAwaitingSwap = false;
        m_discardVideoCache = false;
    }

    m_videoFramePending = (mpv_render_context_update(m_mpv_gl) & MPV_RENDER_UPDATE_FRAME) ||
                          m_videoFramePending;
    bool renderVideo = !m_videoAwaitingSwap &&
        (m_videoFramePending || m_videoCacheDirty || !m_videoFramebuffer);
    bool presentVideoFrame = false;
    bool framePresent = false;
    if (renderVideo) {
        mpv_render_frame_info info{};
        const int infoResult = mpv_render_context_get_info(m_mpv_gl,
            {MPV_RENDER_PARAM_NEXT_FRAME_INFO, &info});
        const bool hasFrame = infoResult >= 0 && (info.flags & MPV_RENDER_FRAME_INFO_PRESENT);
        const bool redraw = hasFrame && (info.flags & MPV_RENDER_FRAME_INFO_REDRAW);

        framePresent = hasFrame && (!redraw || (m_mediaLoaded && m_videoOutputAvailable));
        const bool vsyncFrame = hasFrame && (info.flags & MPV_RENDER_FRAME_INFO_BLOCK_VSYNC);
        presentVideoFrame = hasFrame && !redraw;
        if (vsyncFrame) {


            ++m_vsyncVideoFrames;
            m_videoDueTimer.stop();
        } else if (hasFrame && !redraw && info.target_time > 0) {
            const qint64 nowUs = mpv_get_time_us(m_controller->mpv());
            const auto target = MpvRenderTimingUtils::normalizeTargetTime(info.target_time, nowUs);
            const int unit = target.sourceIsNanoseconds ? 1000 : 1;
            if (unit != m_videoTargetUnit) {
                m_videoTargetUnit = unit;
                qDebug() << "[Danmaku][Composite] Target clock | sourceUnit:"
                         << (target.sourceIsNanoseconds ? "ns" : "us")
                         << "| normalizedDelayMs:" << (target.microseconds - nowUs) / 1000.0;
            }
            const qint64 remainingUs = target.microseconds - nowUs;
            m_maxTargetDelayMs = std::max(m_maxTargetDelayMs, remainingUs / 1000.0);
            if (remainingUs > 0) {




                renderVideo = false;
                ++m_deferredVideoFrames;
                if (!m_videoDueTimer.isActive()) {
                    m_videoDueTimer.start(static_cast<int>(std::clamp<int64_t>(
                        (remainingUs + 999) / 1000, 1, 1000)));
                }
            }
        }
    }

    bool resourcesReady = true;
    if (renderVideo) {
        if (!m_videoFramebuffer || m_videoFramebuffer->size() != QSize(w, h)) {
            QOpenGLFramebufferObjectFormat framebufferFormat;
            framebufferFormat.setAttachment(QOpenGLFramebufferObject::NoAttachment);
            framebufferFormat.setInternalTextureFormat(context()->isOpenGLES() ? GL_RGBA : GL_RGBA8);
            m_videoFramebuffer = std::make_unique<QOpenGLFramebufferObject>(QSize(w, h), framebufferFormat);
            resourcesReady = m_videoFramebuffer->isValid();
        }
        if (resourcesReady) {
            m_videoFramebuffer->bind();
            glViewport(0, 0, w, h);
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindTexture(GL_TEXTURE_2D, 0);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glPixelStorei(GL_PACK_ALIGNMENT, 4);
            checkCompositeGlErrors("video-cache-setup");
            mpv_opengl_fbo fbo{static_cast<int>(m_videoFramebuffer->handle()), w, h, 0};
            int flipY = 1;
            int blockForTarget = 0;
            mpv_render_param params[] = {
                {MPV_RENDER_PARAM_OPENGL_FBO, &fbo},
                {MPV_RENDER_PARAM_FLIP_Y, &flipY},
                {MPV_RENDER_PARAM_BLOCK_FOR_TARGET_TIME, &blockForTarget},
                {MPV_RENDER_PARAM_INVALID, nullptr}
            };
            QElapsedTimer renderTime;
            renderTime.start();
            const int result = mpv_render_context_render(m_mpv_gl, params);
            m_maxVideoRenderMs = std::max(m_maxVideoRenderMs, renderTime.nsecsElapsed() / 1000000.0);
            checkCompositeGlErrors("mpv-video-render");
            if (result < 0) {
                qWarning() << "[Danmaku][Composite] Video render failed | error:" << result;
                resourcesReady = false;
            } else {
                recordVideoFrameRendered(framePresent);
            }
            m_videoFramePending = false;
            m_videoAwaitingSwap = presentVideoFrame;
            m_videoCacheDirty = false;
            m_videoDueTimer.stop();
            ++m_videoFrames;
        }
    } else {
        ++m_cachedVideoFrames;
    }


    glBindFramebuffer(GL_FRAMEBUFFER, defaultFramebufferObject());
    glViewport(0, 0, w, h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    if (resourcesReady && m_videoFramebuffer) {
        if (!m_videoBlitter) {
            m_videoBlitter = std::make_unique<QOpenGLTextureBlitter>();
            resourcesReady = m_videoBlitter->create();
        }
        if (resourcesReady) {
            m_videoBlitter->bind();
            const QMatrix4x4 transform = QOpenGLTextureBlitter::targetTransform(
                QRectF(0, 0, w, h), QRect(0, 0, w, h));
            m_videoBlitter->blit(m_videoFramebuffer->texture(), transform,
                                QOpenGLTextureBlitter::OriginBottomLeft);
            m_videoBlitter->release();


            recordVideoFrameRendered(false);
        }
    }
    checkCompositeGlErrors("video-cache-blit");
    painter.endNativePainting();
    if (resourcesReady && m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->paint(painter, danmakuPosition);
    }
    if (!resourcesReady) {

        m_videoFrameForSwap = false;
    }
    painter.end();
    checkCompositeGlErrors("danmaku-paint");


    glUseProgram(0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_BLEND);
    glDisable(GL_SCISSOR_TEST);

    m_maxCompositeCpuMs = std::max(m_maxCompositeCpuMs, compositeTime.nsecsElapsed() / 1000000.0);

    if (!m_compositeStatsClock.isValid()) {
        m_compositeStatsClock.start();
    } else if (m_compositeStatsClock.elapsed() >= 2000) {
        qDebug() << "[Danmaku][Composite] Cadence | elapsedMs:" << m_compositeStatsClock.elapsed()
                 << "| videoRenders:" << m_videoFrames << "| cachedFrames:" << m_cachedVideoFrames
                 << "| deferredFrames:" << m_deferredVideoFrames
                 << "| maxVideoRenderMs:" << m_maxVideoRenderMs
                 << "| maxCompositeCpuMs:" << m_maxCompositeCpuMs
                 << "| maxCompositionMs:" << m_maxCompositionMs
                 << "| maxTargetDelayMs:" << m_maxTargetDelayMs
                 << "| vsyncVideoFrames:" << m_vsyncVideoFrames;
        m_compositeStatsClock.restart();
        m_videoFrames = m_cachedVideoFrames = m_deferredVideoFrames = 0;
        m_maxVideoRenderMs = 0.0;
        m_maxCompositeCpuMs = m_maxCompositionMs = m_maxTargetDelayMs = 0.0;
        m_vsyncVideoFrames = 0;
    }
    return resourcesReady;
}

#endif



void MpvWidget::loadMediaNow(const QString &url, const QString &serverId, bool wasPending) {
    m_discardVideoCache = true;
    m_videoFramePending = true;
    m_videoCacheDirty = true;
    m_currentUrl = url;
    m_currentServerId = serverId;




    const QUrl loadQUrl(url);
    const QNetworkProxy proxy =
        serverId.isEmpty()
            ? ProxyManager::instance()->resolveForUrl(loadQUrl)
            : ProxyManager::instance()->resolveForServerId(serverId);
    const QString scheme = loadQUrl.scheme().toLower();
    const bool isHttpStream =
        scheme == QStringLiteral("http") || scheme == QStringLiteral("https");




    const QVariant tlsVerify = m_controller->getProperty(QStringLiteral("tls-verify"));
    const bool needsNativeNetwork = m_controller->getProperty(QStringLiteral("cookies")).toBool() ||
        (tlsVerify.isValid() && !tlsVerify.toBool()) ||
        !m_controller->getProperty(QStringLiteral("tls-ca-file")).toString().isEmpty() ||
        !m_controller->getProperty(QStringLiteral("tls-cert-file")).toString().isEmpty() ||
        !m_controller->getProperty(QStringLiteral("tls-key-file")).toString().isEmpty();




    const bool shouldRelay = isHttpStream &&
        (!m_httpHeaders.isEmpty() || proxy.type() != QNetworkProxy::NoProxy ||
         (!needsNativeNetwork && !HttpRangeUtils::isPlaylist(loadQUrl)));

    QString playbackUrl = url;
    bool usingRelay = false;
    if (shouldRelay && m_streamRelay) {
        const QUrl localUrl = m_streamRelay->prepare(loadQUrl, serverId, proxy, m_httpHeaders, m_tlsOrigin, m_ignoreTlsErrors);
        if (localUrl.isValid()) {
            playbackUrl = localUrl.toString(QUrl::FullyEncoded);
            usingRelay = true;
        }
    } else if (m_streamRelay) {
        m_streamRelay->stop();
    }
    if (m_usingStreamRelay != usingRelay) {
        m_usingStreamRelay = usingRelay;
        Q_EMIT relayActiveChanged(usingRelay);
    }

    const QString mpvProxyValue =
        usingRelay ? QString() : ProxyManager::toMpvHttpProxy(proxy);
    const UserAgentConfig userAgent =
        serverId.isEmpty()
            ? UserAgentManager::instance()->resolveForUrl(loadQUrl)
            : UserAgentManager::instance()->resolveForServerId(serverId);
    const bool forceSeekable =
        !usingRelay && !mpvProxyValue.isEmpty() && scheme == QStringLiteral("http");

    m_controller->setProperty(QStringLiteral("http-proxy"), mpvProxyValue);
    applyUserAgentConfig(userAgent);
    m_controller->setProperty(QStringLiteral("force-seekable"), forceSeekable);

    QVariantMap loadOptions;
    if (ConfigStore::instance()->get<bool>(ConfigKeys::PlayerHideSubtitles, false)) {

        m_controller->setProperty(QStringLiteral("sid"), QStringLiteral("no"));
        m_controller->setProperty(QStringLiteral("secondary-sid"), QStringLiteral("no"));
        loadOptions.insert(QStringLiteral("sid"), QStringLiteral("no"));
        loadOptions.insert(QStringLiteral("secondary-sid"), QStringLiteral("no"));
        qDebug() << "[Subtitle][Player] Loading media with subtitles hidden";
    }
    if (!mpvProxyValue.isEmpty()) {
        loadOptions.insert(QStringLiteral("http-proxy"), mpvProxyValue);
    }
    if (forceSeekable) {
        loadOptions.insert(QStringLiteral("force-seekable"), QStringLiteral("yes"));
    }

    if (!usingRelay && !mpvProxyValue.isEmpty() && scheme == QStringLiteral("https")) {
        qWarning() << "[MpvWidget] MPV http-proxy is not applied to HTTPS streams by libmpv"
                   << "| url:" << LogRedactionUtils::url(loadQUrl)
                   << "| serverId:" << (serverId.isEmpty()
                                           ? QStringLiteral("<none>")
                                           : serverId);
    }

    qInfo() << (wasPending
                    ? QStringLiteral("[MpvWidget] http-proxy applied (pending)")
                    : QStringLiteral("[MpvWidget] http-proxy applied"))
            << "| url:" << LogRedactionUtils::url(loadQUrl)
            << "| serverId:" << (serverId.isEmpty()
                                     ? QStringLiteral("<none>")
                                     : serverId)
            << "| mpvProxyValue:" << LogRedactionUtils::proxy(mpvProxyValue)
            << "| relay:" << usingRelay
            << "| playbackSource:" << (usingRelay ? QStringLiteral("<local-relay>") : QStringLiteral("<direct>"))
            << "| forceSeekable:" << forceSeekable;

    m_previewUrl = usingRelay ? m_streamRelay->previewUrl().toString(QUrl::FullyEncoded) : playbackUrl;
    publishPreviewSource();

    QVariantList loadCommand;
    if (loadOptions.isEmpty()) {
        loadCommand = QVariantList{QStringLiteral("loadfile"), playbackUrl};
    } else {
        loadCommand = QVariantList{QStringLiteral("loadfile"), playbackUrl,
                                   QStringLiteral("replace"), -1,
                                   loadOptions};
    }

    const int err = m_controller->command(loadCommand, nullptr);
    if (err < 0 && !loadOptions.isEmpty()) {
        qWarning() << "[MpvWidget] loadfile with indexed per-file network options failed, retrying legacy form"
                   << "| error:" << mpv_error_string(err);
        const int legacyErr = m_controller->command(
            QVariantList{QStringLiteral("loadfile"), playbackUrl,
                         QStringLiteral("replace"), loadOptions},
            nullptr);
        if (legacyErr < 0) {
            qWarning() << "[MpvWidget] legacy loadfile per-file options failed, retrying plain loadfile"
                       << "| error:" << mpv_error_string(legacyErr);
            m_controller->command(QVariantList{QStringLiteral("loadfile"), playbackUrl}, nullptr);
        }
    }
}

void MpvWidget::publishPreviewSource() {
    if (!m_controller || m_previewUrl.isEmpty()) return;
    QVariantMap previewOptions;
    for (const QString &name : {QStringLiteral("user-agent"), QStringLiteral("referrer"),
                                QStringLiteral("http-header-fields"), QStringLiteral("http-proxy"),
                                QStringLiteral("tls-verify"), QStringLiteral("tls-ca-file"),
                                QStringLiteral("cookies"), QStringLiteral("cookies-file"),
                                QStringLiteral("force-seekable")}) {
        const QVariant value = m_controller->getProperty(name);
        if (value.isValid()) previewOptions.insert(name, value);
    }
    emit previewSourceChanged(m_previewUrl, previewOptions);
}

void MpvWidget::applyUserAgentConfig(const UserAgentConfig &config) {
    if (config.isEffective()) {
        m_controller->setProperty(QStringLiteral("user-agent"),
                                  config.value.trimmed());
        m_customUserAgentApplied = true;
        return;
    }



    if (m_customUserAgentApplied) {
        m_controller->setProperty(QStringLiteral("user-agent"),
                                  m_defaultUserAgent);
        m_customUserAgentApplied = false;
    }
}

void MpvWidget::loadMedia(const QString &url, const QString &serverId) {
    m_previewUrl.clear();
    emit previewSourceChanged(QString(), QVariantMap());
    resetVideoPresentation(true);

    if (!m_mpv_gl) {
        m_pendingUrl = url;
        m_pendingServerId = serverId;
        return;
    }
    loadMediaNow(url, serverId, false);
}

void MpvWidget::play() {
    m_controller->setProperty("pause", false);
}

void MpvWidget::resumeAfterContextRestore() {
#if defined(QEMBY_MPV_D3D11_RENDER)
    if (!m_mpv_gl) {
        m_resumeWhenRenderReady = true;
        qInfo() << "[MpvWidget] Resume deferred until Direct3D 11 render context is ready";
        update();
        return;
    }

    m_resumeWhenRenderReady = false;
    m_controller->setProperty("pause", false);
    update();
#else
    if (!m_mpv_gl || !context() || !context()->isValid()) {
        m_resumeWhenRenderReady = true;
        qInfo() << "[MpvWidget] Resume deferred until OpenGL context is ready";
        update();
        return;
    }

    m_resumeWhenRenderReady = false;
    m_controller->setProperty("pause", false);
    update();
#endif
}

void MpvWidget::pause() {
    m_controller->setProperty("pause", true);
}

void MpvWidget::stop() {
    m_previewUrl.clear();
    emit previewSourceChanged(QString(), QVariantMap());
    resetVideoPresentation(true);
    m_controller->command(QVariantList{"stop"});
    if (m_streamRelay) m_streamRelay->stop();
    if (m_usingStreamRelay) {
        m_usingStreamRelay = false;
        Q_EMIT relayActiveChanged(false);
    }
    m_currentUrl.clear();
    m_currentServerId.clear();
}

void MpvWidget::seek(double positionInSeconds) {
    m_controller->command(QVariantList{"seek", positionInSeconds, "absolute"});
}
