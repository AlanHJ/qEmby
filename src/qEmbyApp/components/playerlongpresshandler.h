#ifndef PLAYERLONGPRESSHANDLER_H
#define PLAYERLONGPRESSHANDLER_H

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QString>

class QWidget;
class QLabel;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;


class PlayerLongPressHandler : public QObject
{
    Q_OBJECT
public:
    explicit PlayerLongPressHandler(QWidget *parent);

    
    void startKeyLongPress(int direction, bool silent);
    void stopKeyLongPress(bool applyShortPressFallback = true);

    
    void startMouseEdgeLongPress(int direction);
    bool stopMouseEdgeLongPress();
    bool isMouseEdgeLongPressEnabled() const;
    bool isMouseEdgeActive() const;
    int mouseEdgeLongPressDirection() const;

    
    void updateGeometry(int parentWidth, int parentHeight, int topHudHeight);
    void setTeardown(bool teardown);
    void stopAllAnimations();

    
    int activeLongPressDirection() const;
    bool isKeyLongPressTriggered() const;
    int keyLongPressDirection() const;

    
    static QString formatSpeedText(double speed);
    static QString formatLongPressSeekText(double seconds);

signals:
    void seekRequested(double delta, bool silent);
    void toastRequested(const QString &msg);

private:
    
    QString longPressMode() const;
    int longPressTriggerMs() const;
    double nextPlaybackRate(double currentRate) const;
    bool canStartPropertyAnimation(const QPropertyAnimation *animation) const;

    
    void onKeyLongPressTimeout();

    
    void onMouseEdgeLongPressTimeout();

    
    void updateIndicators();
    void updateIndicatorPulse();
    QRect indicatorGeometry(int direction, bool active) const;
    void animateIndicator(QWidget *overlay, int direction, bool active, bool visible);
    void animateIndicatorIcon(QLabel *iconLabel, int direction, bool active, bool immediate = false);
    void setIndicatorState(QWidget *overlay, QLabel *iconLabel, QLabel *textLabel,
                           int direction, bool active, const QString &speedText);
    void stopIndicatorAnimations();

    QWidget *createIndicator(const QString &iconPath, QLabel *&iconLabel, QLabel *&textLabel,
                             QGraphicsOpacityEffect *&opacityEffect,
                             QPropertyAnimation *&opacityAnim,
                             QPropertyAnimation *&geometryAnim,
                             QPropertyAnimation *&iconAnim);

    
    QWidget *m_parentWidget = nullptr;
    int m_topHudHeight = 0;

    
    QTimer *m_keyLongPressTimer = nullptr;
    int m_keyDirection = 0;
    double m_keySeekRate = 2.0;
    double m_keyRestoreSpeed = 1.0;
    double m_keyCurrentSpeed = 1.0;
    int m_keySpeedTick = 0;
    bool m_keyTriggered = false;
    bool m_keySilent = false;
    bool m_keySpeedActive = false;
    QString m_keyActiveMode;

    
    QTimer *m_mouseEdgeTimer = nullptr;
    int m_mouseEdgeDirection = 0;
    int m_mouseEdgeTick = 0;
    double m_mouseEdgeSpeed = 1.0;
    bool m_mouseEdgeActive = false;

    
    QWidget *m_leftIndicator = nullptr;
    QWidget *m_rightIndicator = nullptr;
    QLabel *m_leftIndicatorIcon = nullptr;
    QLabel *m_rightIndicatorIcon = nullptr;
    QLabel *m_leftIndicatorText = nullptr;
    QLabel *m_rightIndicatorText = nullptr;
    QGraphicsOpacityEffect *m_leftIndicatorOpacity = nullptr;
    QGraphicsOpacityEffect *m_rightIndicatorOpacity = nullptr;
    QPropertyAnimation *m_leftIndicatorOpacityAnim = nullptr;
    QPropertyAnimation *m_rightIndicatorOpacityAnim = nullptr;
    QPropertyAnimation *m_leftIndicatorGeometryAnim = nullptr;
    QPropertyAnimation *m_rightIndicatorGeometryAnim = nullptr;
    QPropertyAnimation *m_leftIndicatorIconAnim = nullptr;
    QPropertyAnimation *m_rightIndicatorIconAnim = nullptr;
    QTimer *m_pulseTimer = nullptr;
    bool m_pulseForward = false;
    bool m_teardown = false;
};

#endif 
