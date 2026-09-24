#include "mpvwidget.h"
#include <config/config_keys.h>
#include <config/configstore.h>
#include "mpvhttpstreamrelay.h"
#include "nativedanmakuoverlay.h"
#include "../utils/logredactionutils.h"
#include "../utils/mpvrendertimingutils.h"
#include <QOpenGLContext>
#include <QMetaObject>
#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QUrl>
#include <QVariantMap>
#include <QOpenGLFramebufferObject>
#include <QOpenGLTextureBlitter>
#include <QPainter>
#include <algorithm>
#include "api/proxymanager.h"
#include "api/useragentmanager.h"

MpvWidget::MpvWidget(QWidget *parent)
    : QOpenGLWidget(parent), m_mpv_gl(nullptr) {

    
    
    
    QSurfaceFormat format = this->format(); 
    
    format.setSwapInterval(1); 
    this->setFormat(format);

    m_videoDueTimer.setSingleShot(true);
    m_videoDueTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_videoDueTimer, &QTimer::timeout, this, [this]() {
        if (m_mpv_gl && m_nativeDanmakuOverlay && m_nativeDanmakuOverlay->isActive()) {
            update();
        }
    });
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
        if (m_videoFrameForSwap) {
            m_videoFrameForSwap = false;
            m_videoPresented = true;
            qInfo() << "[MpvWidget] Video presentation ready | firstFrameSwapped:" << true;
            Q_EMIT videoPresentationChanged(true);
        }
        const bool wasWaiting = m_videoAwaitingSwap;
        m_videoAwaitingSwap = false;
        if (wasWaiting && m_nativeCompositeActive && m_videoFramePending &&
            !m_videoDueTimer.isActive()) {
            update();
        }
    }, Qt::DirectConnection);

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
    connect(m_controller, &MpvController::errorOccurred, this, &MpvWidget::errorOccurred);

    
    

    if (m_controller->init()) {
        m_defaultUserAgent =
            m_controller->getProperty(QStringLiteral("user-agent")).toString();
        qDebug() << "[MpvWidget] captured default User-Agent"
                 << "| hasValue:" << !m_defaultUserAgent.isEmpty();
    }
}

MpvWidget::~MpvWidget() {
    
    
    
    if (QOpenGLContext *ctx = context()) {
        disconnect(ctx, &QOpenGLContext::aboutToBeDestroyed,
                   this, &MpvWidget::cleanupGL);
    }

    
    
    
    shutdown();
    if (m_controller) {
        m_controller->deleteLater();
        m_controller = nullptr;
    }
}


void MpvWidget::shutdown() {
    if (!m_controller) return;

    
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

    cleanupGL();

    m_controller->forceCleanup();
}

void MpvWidget::cleanupGL() {
    resetVideoPresentation(false);
    m_videoDueTimer.stop();
    if (m_nativeDanmakuOverlay) {
        m_nativeDanmakuOverlay->setGraphicsAvailable(false);
    }
    if (m_mpv_gl) {
        mpv_render_context_set_update_callback(m_mpv_gl, nullptr, nullptr);

        
        
        
        if (isValid()) {
            makeCurrent();
            releaseVideoCache();
            mpv_render_context_free(m_mpv_gl);
            doneCurrent();
        } else {
            releaseVideoCache();
            qWarning() << "[MpvWidget] OpenGL surface is invalid; freeing MPV render context without makeCurrent";
            mpv_render_context_free(m_mpv_gl);
        }
        m_mpv_gl = nullptr;
        qInfo() << "[MpvWidget] MPV render context released with OpenGL context";
    }
}

void *MpvWidget::getProcAddress(void *ctx, const char *name) {
    Q_UNUSED(ctx);
    QOpenGLContext *glctx = QOpenGLContext::currentContext();
    return glctx ? reinterpret_cast<void *>(glctx->getProcAddress(QByteArray(name))) : nullptr;
}

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

    
    
    if (m_mpv_gl) {
        mpv_render_context_set_update_callback(m_mpv_gl, nullptr, nullptr);
        mpv_render_context_free(m_mpv_gl);
        m_mpv_gl = nullptr;
    }

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
        return;
    }

    mpv_render_context_set_update_callback(m_mpv_gl, onMpvRenderUpdate, this);

    qInfo() << "[MpvWidget] MPV render context initialized"
            << "| resumePending:" << m_resumeWhenRenderReady;

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
    const bool shouldRelay =
        isHttpStream && proxy.type() != QNetworkProxy::NoProxy;

    QString playbackUrl = url;
    bool usingRelay = false;
    if (shouldRelay && m_streamRelay) {
        const QUrl localUrl = m_streamRelay->prepare(loadQUrl, serverId, proxy);
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
            << "| playbackUrl:" << (usingRelay
                                       ? playbackUrl
                                       : QStringLiteral("<direct>"))
            << "| forceSeekable:" << forceSeekable;

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
    if (!m_mpv_gl || !context() || !context()->isValid()) {
        m_resumeWhenRenderReady = true;
        qInfo() << "[MpvWidget] Resume deferred until OpenGL context is ready";
        update();
        return;
    }

    m_resumeWhenRenderReady = false;
    m_controller->setProperty("pause", false);
    update();
}

void MpvWidget::pause() {
    m_controller->setProperty("pause", true);
}

void MpvWidget::stop() {
    resetVideoPresentation(true);
    m_controller->command(QVariantList{"stop"});
    m_currentUrl.clear();
    m_currentServerId.clear();
}

void MpvWidget::seek(double positionInSeconds) {
    m_controller->command(QVariantList{"seek", positionInSeconds, "absolute"});
}
