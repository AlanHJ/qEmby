#ifndef LOADINGOVERLAY_H
#define LOADINGOVERLAY_H

#include <QWidget>
#include <QColor>
#include <QElapsedTimer>

class QPropertyAnimation;
class QPainter;
class QTimer;
class QPushButton;
class QShortcut;
class QGraphicsOpacityEffect;

class LoadingOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
    Q_PROPERTY(qreal angle READ angle WRITE setAngle)
    Q_PROPERTY(qreal completion READ completion WRITE setCompletion)

    
    Q_PROPERTY(QColor overlayBgColor READ overlayBgColor WRITE setOverlayBgColor)
    Q_PROPERTY(QColor hudColorTop READ hudColorTop WRITE setHudColorTop)
    Q_PROPERTY(QColor hudColorMid READ hudColorMid WRITE setHudColorMid)
    Q_PROPERTY(QColor hudColorBottom READ hudColorBottom WRITE setHudColorBottom)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor sweepColor READ sweepColor WRITE setSweepColor)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)
    Q_PROPERTY(QColor glowColor READ glowColor WRITE setGlowColor)

    
    Q_PROPERTY(bool isImmersive READ isImmersive WRITE setImmersive)
    Q_PROPERTY(bool hudPanelVisible READ isHudPanelVisible WRITE setHudPanelVisible)
    Q_PROPERTY(bool subtleOverlay READ isSubtleOverlay WRITE setSubtleOverlay)

public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    void start();
    void stop();
    void forceStop();
    
    void setAmbientMode(bool ambient);
    void setReducedMotion(bool reduced);
    void setStatusText(const QString& text);
    
    void setCancellable(bool cancellable, int buttonDelayMs = 0);
    
    void complete();

    qreal completion() const { return m_completion; }
    void setCompletion(qreal progress);

    qreal opacity() const;
    void setOpacity(qreal op);

    qreal angle() const;
    void setAngle(qreal a);

    bool isImmersive() const;
    void setImmersive(bool immersive);
    bool isHudPanelVisible() const;
    void setHudPanelVisible(bool visible);
    bool isSubtleOverlay() const;
    void setSubtleOverlay(bool subtle);

    
    QColor overlayBgColor() const { return m_overlayBgColor; }
    void setOverlayBgColor(const QColor& c) { m_overlayBgColor = c; update(); }

    QColor hudColorTop() const { return m_hudColorTop; }
    void setHudColorTop(const QColor& c) { m_hudColorTop = c; update(); }

    QColor hudColorMid() const { return m_hudColorMid; }
    void setHudColorMid(const QColor& c) { m_hudColorMid = c; update(); }

    QColor hudColorBottom() const { return m_hudColorBottom; }
    void setHudColorBottom(const QColor& c) { m_hudColorBottom = c; update(); }

    QColor borderColor() const { return m_borderColor; }
    void setBorderColor(const QColor& c) { m_borderColor = c; update(); }

    QColor trackColor() const { return m_trackColor; }
    void setTrackColor(const QColor& c) { m_trackColor = c; update(); }

    QColor sweepColor() const { return m_sweepColor; }
    void setSweepColor(const QColor& c) { m_sweepColor = c; update(); }

    QColor shadowColor() const { return m_shadowColor; }
    void setShadowColor(const QColor& c) { m_shadowColor = c; update(); }

    QColor glowColor() const { return m_glowColor; }
    void setGlowColor(const QColor& c) { m_glowColor = c; update(); }

Q_SIGNALS:
    void completed();
    void cancelRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    enum class State { Idle, Loading, Completing, Complete, FadingOut };
    void reveal();
    void paintAmbient(QPainter& painter);
    void updateCancelControls();
    void positionCancelButton();

    State m_state = State::Idle;
    qreal m_opacity = 0.0;
    qreal m_angle = 0.0;
    qreal m_completion = 0.0;
    bool m_ambientMode = false;
    bool m_reducedMotion = false;
    bool m_cancellable = false;
    int m_cancelButtonDelayMs = 0;
    QElapsedTimer m_loadingElapsed;
    QString m_statusText;
    bool m_isImmersive = false; 
    bool m_hudPanelVisible = true;
    bool m_subtleOverlay = false;

    QPropertyAnimation* m_rotateAnim;
    QPropertyAnimation* m_fadeAnim;
    QPropertyAnimation* m_completionAnim;
    QTimer* m_revealTimer;
    QTimer* m_cancelRevealTimer;
    QPushButton* m_cancelButton;
    QShortcut* m_cancelShortcut;
    QGraphicsOpacityEffect* m_cancelOpacityEffect = nullptr;

    
    QColor m_overlayBgColor = QColor(0, 0, 0, 100);
    QColor m_hudColorTop = QColor(70, 70, 75, 170);
    QColor m_hudColorMid = QColor(40, 40, 45, 170);
    QColor m_hudColorBottom = QColor(20, 20, 20, 170);
    QColor m_borderColor = QColor(255, 255, 255, 55);
    QColor m_trackColor = QColor(255, 255, 255, 12);
    QColor m_sweepColor = QColor(255, 255, 255, 255);
    QColor m_shadowColor = QColor(0, 0, 0, 60);
    QColor m_glowColor = Qt::transparent;
};

#endif 
