#ifndef MODERNSLIDER_H
#define MODERNSLIDER_H

#include <QSlider>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QEvent>
#include <QColor>
#include <QtGlobal>

class ModernSlider : public QSlider {
    Q_OBJECT
    
    Q_PROPERTY(QColor themeColor READ themeColor WRITE setThemeColor)
    Q_PROPERTY(QColor trackColor READ trackColor WRITE setTrackColor)
    Q_PROPERTY(QColor bufferColor READ bufferColor WRITE setBufferColor)
    Q_PROPERTY(QColor handleColor READ handleColor WRITE setHandleColor)
    Q_PROPERTY(int normalTrackHeight READ normalTrackHeight WRITE setNormalTrackHeight)
    Q_PROPERTY(int hoverTrackHeight READ hoverTrackHeight WRITE setHoverTrackHeight)
    Q_PROPERTY(int normalHandleRadius READ normalHandleRadius WRITE setNormalHandleRadius)
    Q_PROPERTY(int activeHandleRadius READ activeHandleRadius WRITE setActiveHandleRadius)

public:
    explicit ModernSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

    void setBufferValue(int value);
    int bufferValue() const;

    QColor themeColor() const { return m_themeColor; }
    void setThemeColor(const QColor &c) { m_themeColor = c; update(); }

    QColor trackColor() const { return m_trackColor; }
    void setTrackColor(const QColor &c) { m_trackColor = c; update(); }

    QColor bufferColor() const { return m_bufferColor; }
    void setBufferColor(const QColor &c) { m_bufferColor = c; update(); }

    QColor handleColor() const { return m_handleColor; }
    void setHandleColor(const QColor &c) { m_handleColor = c; update(); }

    int normalTrackHeight() const { return m_normalTrackHeight; }
    void setNormalTrackHeight(int value) {
        m_normalTrackHeight = qMax(1, value);
        update();
    }

    int hoverTrackHeight() const { return m_hoverTrackHeight; }
    void setHoverTrackHeight(int value) {
        m_hoverTrackHeight = qMax(1, value);
        update();
    }

    int normalHandleRadius() const { return m_normalHandleRadius; }
    void setNormalHandleRadius(int value) {
        m_normalHandleRadius = qMax(1, value);
        update();
    }

    int activeHandleRadius() const { return m_activeHandleRadius; }
    void setActiveHandleRadius(int value) {
        m_activeHandleRadius = qMax(1, value);
        update();
    }

protected:
    void mousePressEvent(QMouseEvent *ev) override;
    void mouseReleaseEvent(QMouseEvent *ev) override;
    void mouseMoveEvent(QMouseEvent *ev) override;
    bool event(QEvent *ev) override;
    void paintEvent(QPaintEvent *ev) override;

private:
    int m_bufferValue = 0;
    bool m_isHovered = false;
    bool m_isPressed = false;

    
    QColor m_themeColor = QColor("#3B82F6");
    QColor m_trackColor = QColor(255, 255, 255, 30);
    QColor m_bufferColor = QColor(255, 255, 255, 80);
    QColor m_handleColor = QColor("#FFFFFF");
    int m_normalTrackHeight = 2;
    int m_hoverTrackHeight = 4;
    int m_normalHandleRadius = 4;
    int m_activeHandleRadius = 6;
};

#endif 
