#ifndef SERVERWHEELVIEW_H
#define SERVERWHEELVIEW_H

#include <QWidget>
#include <QList>
#include <QPoint>

class QVariantAnimation;

class ServerWheelView : public QWidget {
    Q_OBJECT
public:
    explicit ServerWheelView(QWidget* parent = nullptr);
    ~ServerWheelView() override;

    void addCard(QWidget* card);
    void clear();
    int count() const;

    
    void setCurrentIndex(int index, bool animated = false);
    int currentIndex() const;

    
    void setTransitionMode(bool active);

Q_SIGNALS:
    
    void currentIndexChanged(int index);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void installFilterRecursively(QWidget* w);
    void scrollTo(qreal target);
    void updateLayout();

    void handleMousePress(QPoint globalPos);
    void handleMouseMove(QPoint globalPos);
    bool handleMouseRelease();

    QList<QWidget*> m_items;
    qreal m_scrollOffset = 0.0;
    int m_currentIndex = 0; 
    QVariantAnimation* m_scrollAnim;
    
    bool m_isTransitioning = false; 
    bool m_isDragging = false;
    bool m_dragStarted = false;
    QPoint m_lastPos;
    qint64 m_lastTime = 0;
    qreal m_dragVelocity = 0.0;
};

#endif 
