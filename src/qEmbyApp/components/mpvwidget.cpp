#include "mpvwidget.h"
#include "mpvhttpstreamrelay.h"
#include "../utils/logredactionutils.h"
#include <QOpenGLContext>
#include <QMetaObject>
#include <QCoreApplication>
#include <QSurfaceFormat>
#include <QUrl>
#include <QVariantMap>
#include "api/proxymanager.h"

MpvWidget::MpvWidget(QWidget *parent)
    : QOpenGLWidget(parent), m_mpv_gl(nullptr) {

    
    
    
    QSurfaceFormat format = this->format(); 
    
    format.setSwapInterval(1); 
    this->setFormat(format);

    m_controller = new MpvController(this);
    m_streamRelay = new MpvHttpStreamRelay(this);
    connect(m_streamRelay, &MpvHttpStreamRelay::upstreamSpeedChanged, this,
            &MpvWidget::networkSpeedChanged);

    
    connect(m_controller, &MpvController::positionChanged, this, &MpvWidget::positionChanged);
    connect(m_controller, &MpvController::durationChanged, this, &MpvWidget::durationChanged);
    connect(m_controller, &MpvController::playbackStateChanged, this, &MpvWidget::playbackStateChanged);
    connect(m_controller, &MpvController::errorOccurred, this, &MpvWidget::errorOccurred);

    
    

    m_controller->init();
}

MpvWidget::~MpvWidget() {
    
    
    
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
    if (m_mpv_gl) {
        
        
        if (isValid()) {
            makeCurrent();
            mpv_render_context_set_update_callback(m_mpv_gl, nullptr, nullptr);
            mpv_render_context_free(m_mpv_gl);
            doneCurrent();
        } else {
            qWarning() << "[MpvWidget] OpenGL context is not valid. Bypassing render context free.";
        }
        m_mpv_gl = nullptr;
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
    update();
}

void MpvWidget::initializeGL() {
    initializeOpenGLFunctions();

    QOpenGLContext *ctx = QOpenGLContext::currentContext();
    if (ctx && ctx->format().majorVersion() < 2) {
        qCritical() << "Fatal Error: OpenGL version is too low.";
        emit errorOccurred(tr("当前环境缺乏必需的 OpenGL 硬件加速支持，视频渲染已禁用。"));
        return;
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

    if (mpv_render_context_create(&m_mpv_gl, m_controller->mpv(), params) < 0) {
        emit errorOccurred(tr("OpenGL rendering initialization failed."));
        return;
    }

    mpv_render_context_set_update_callback(m_mpv_gl, onMpvRenderUpdate, this);

    if (!m_pendingUrl.isEmpty()) {
        loadMediaNow(m_pendingUrl, m_pendingServerId, true);
        m_pendingUrl.clear();
        m_pendingServerId.clear();
    }
}

void MpvWidget::paintGL() {
    if (!m_mpv_gl) return;

    int w = static_cast<int>(width() * devicePixelRatio());
    int h = static_cast<int>(height() * devicePixelRatio());
    if (w == 0 || h == 0) return; 

    int fbo = defaultFramebufferObject();
    int flip_y = 1;

    mpv_opengl_fbo mpfbo{fbo, w, h, 0};

    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &mpfbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };

    mpv_render_context_render(m_mpv_gl, params);
}

void MpvWidget::resizeGL(int w, int h) {
    Q_UNUSED(w);
    Q_UNUSED(h);
}



void MpvWidget::loadMediaNow(const QString &url, const QString &serverId, bool wasPending) {
    
    
    
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
    const bool forceSeekable =
        !usingRelay && !mpvProxyValue.isEmpty() && scheme == QStringLiteral("http");

    m_controller->setProperty(QStringLiteral("http-proxy"), mpvProxyValue);
    m_controller->setProperty(QStringLiteral("force-seekable"), forceSeekable);

    QVariantMap loadOptions;
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

void MpvWidget::loadMedia(const QString &url, const QString &serverId) {
    
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

void MpvWidget::pause() {
    m_controller->setProperty("pause", true);
}

void MpvWidget::stop() {
    m_controller->command(QVariantList{"stop"});
}

void MpvWidget::seek(double positionInSeconds) {
    m_controller->command(QVariantList{"seek", positionInSeconds, "absolute"});
}
