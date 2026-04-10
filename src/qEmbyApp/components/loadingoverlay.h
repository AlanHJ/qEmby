#ifndef LOADINGOVERLAY_H
#define LOADINGOVERLAY_H

#include <QWidget>
#include <QColor>

class QPropertyAnimation;

class LoadingOverlay : public QWidget {
    Q_OBJECT
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)
    Q_PROPERTY(qreal angle READ angle WRITE setAngle)

    
    Q_PROPERTY(QColor overlayBgColor READ overlayBgColor WRITE setOverlayBgColor)
    Q_PROPERTY(QColor hudColorTop READ hudColorTop WRITE setHudColorTop)
    Q_PROPERTY(QColor hudColorMid READ hudColorMid WRITE setHudColorMid)
    Q_PROPERTY(QColor hudColorBottom READ hudColorBottom WRITE setHudColorBottom)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor sweepColor READ sweepColor WRITE setSweepColor)
    Q_PROPERTY(QColor shadowColor READ shadowColor WRITE setShadowColor)

    
    Q_PROPERTY(bool isImmersive READ isImmersive WRITE setImmersive)
    Q_PROPERTY(bool hudPanelVisible READ isHudPanelVisible WRITE setHudPanelVisible)
    Q_PROPERTY(bool subtleOverlay READ isSubtleOverlay WRITE setSubtleOverlay)

public:
    explicit LoadingOverlay(QWidget* parent = nullptr);

    void start();
    void stop();
    void forceStop();

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

protected:
    void paintEvent(QPaintEvent* event) override;
    
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    qreal m_opacity = 0.0;
    qreal m_angle = 0.0;
    bool m_isImmersive = false; 
    bool m_hudPanelVisible = true;
    bool m_subtleOverlay = false;

    QPropertyAnimation* m_rotateAnim;
    QPropertyAnimation* m_fadeAnim;

    
    QColor m_overlayBgColor = QColor(0, 0, 0, 100);
    QColor m_hudColorTop = QColor(70, 70, 75, 170);
    QColor m_hudColorMid = QColor(40, 40, 45, 170);
    QColor m_hudColorBottom = QColor(20, 20, 20, 170);
    QColor m_borderColor = QColor(255, 255, 255, 55);
    QColor m_trackColor = QColor(255, 255, 255, 12);
    QColor m_sweepColor = QColor(255, 255, 255, 255);
    QColor m_shadowColor = QColor(0, 0, 0, 60);
};

#endif 
