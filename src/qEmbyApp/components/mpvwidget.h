#ifndef MPVWIDGET_H
#define MPVWIDGET_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QString>
#include <mpv/render_gl.h>
#include "mpvcontroller.h"

class MpvWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit MpvWidget(QWidget *parent = nullptr);
    ~MpvWidget() override;

    
    void shutdown();

    void loadMedia(const QString &url);
    void play();
    void pause();
    void stop();
    void seek(double positionInSeconds);

    MpvController* controller() const { return m_controller; }

signals:
    
    void positionChanged(double position);
    void durationChanged(double duration);
    void playbackStateChanged(bool isPaused);
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

    MpvController *m_controller;
    mpv_render_context *m_mpv_gl;
    
    
    QString m_pendingUrl; 
};

#endif 
