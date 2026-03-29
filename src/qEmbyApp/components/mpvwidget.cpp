#include "mpvwidget.h"
#include <QOpenGLContext>
#include <QMetaObject>
#include <QCoreApplication>
#include <QSurfaceFormat>

MpvWidget::MpvWidget(QWidget *parent)
    : QOpenGLWidget(parent), m_mpv_gl(nullptr) {

    
    
    
    QSurfaceFormat format = this->format(); 
    
    format.setSwapInterval(1); 
    this->setFormat(format);

    m_controller = new MpvController(this);

    
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
        m_controller->command(QVariantList{"loadfile", m_pendingUrl});
        m_pendingUrl.clear();
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



void MpvWidget::loadMedia(const QString &url) {
    
    if (!m_mpv_gl) {
        m_pendingUrl = url;
        return;
    }
    m_controller->command(QVariantList{"loadfile", url});
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
