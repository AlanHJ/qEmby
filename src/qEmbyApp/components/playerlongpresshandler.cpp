#include "playerlongpresshandler.h"
#include <QAbstractAnimation>
#include <QDebug>
#include <QEasingCurve>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QPropertyAnimation>
#include <QStyle>
#include <QTimer>
#include <QWidget>
#include <cmath>
#include <config/config_keys.h>
#include <config/configstore.h>

namespace
{
void stopPropertyAnimationSafely(QPropertyAnimation *animation)
{
    if (!animation || !animation->targetObject() ||
        animation->state() == QAbstractAnimation::Stopped)
    {
        return;
    }

    animation->stop();
}
} 

PlayerLongPressHandler::PlayerLongPressHandler(QWidget *parent)
    : QObject(parent), m_parentWidget(parent)
{
    
    m_keyLongPressTimer = new QTimer(this);
    connect(m_keyLongPressTimer, &QTimer::timeout, this,
            &PlayerLongPressHandler::onKeyLongPressTimeout);

    
    m_mouseEdgeTimer = new QTimer(this);
    connect(m_mouseEdgeTimer, &QTimer::timeout, this,
            &PlayerLongPressHandler::onMouseEdgeLongPressTimeout);

    
    m_pulseTimer = new QTimer(this);
    m_pulseTimer->setInterval(260);
    connect(m_pulseTimer, &QTimer::timeout, this,
            &PlayerLongPressHandler::updateIndicatorPulse);

    
    m_leftIndicator = createIndicator(
        QStringLiteral(":/svg/player/fast-rewind.svg"),
        m_leftIndicatorIcon, m_leftIndicatorText,
        m_leftIndicatorOpacity, m_leftIndicatorOpacityAnim,
        m_leftIndicatorGeometryAnim, m_leftIndicatorIconAnim);
    m_rightIndicator = createIndicator(
        QStringLiteral(":/svg/player/fast-forward.svg"),
        m_rightIndicatorIcon, m_rightIndicatorText,
        m_rightIndicatorOpacity, m_rightIndicatorOpacityAnim,
        m_rightIndicatorGeometryAnim, m_rightIndicatorIconAnim);
}

QWidget *PlayerLongPressHandler::createIndicator(
    const QString &iconPath, QLabel *&iconLabel, QLabel *&textLabel,
    QGraphicsOpacityEffect *&opacityEffect,
    QPropertyAnimation *&opacityAnim,
    QPropertyAnimation *&geometryAnim,
    QPropertyAnimation *&iconAnim)
{
    auto *overlay = new QWidget(m_parentWidget);
    overlay->setObjectName("playerMouseEdgeIndicator");
    overlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    overlay->setAttribute(Qt::WA_TranslucentBackground, true);
    overlay->setProperty("gestureActive", false);
    overlay->setProperty("indicatorVisible", false);
    overlay->hide();

    auto *layout = new QHBoxLayout(overlay);
    layout->setContentsMargins(18, 12, 18, 12);
    layout->setSpacing(12);
    layout->setAlignment(Qt::AlignCenter);

    iconLabel = new QLabel(overlay);
    iconLabel->setObjectName("playerMouseEdgeIndicatorIcon");
    iconLabel->setFixedSize(18, 18);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setPixmap(QIcon(iconPath).pixmap(18, 18));
    layout->addWidget(iconLabel, 0, Qt::AlignCenter);

    textLabel = new QLabel(overlay);
    textLabel->setObjectName("playerMouseEdgeIndicatorText");
    textLabel->setAlignment(Qt::AlignCenter);
    textLabel->hide();
    layout->addWidget(textLabel, 0, Qt::AlignCenter);

    opacityEffect = new QGraphicsOpacityEffect(overlay);
    opacityEffect->setOpacity(0.0);
    overlay->setGraphicsEffect(opacityEffect);

    opacityAnim = new QPropertyAnimation(opacityEffect, "opacity", this);
    opacityAnim->setEasingCurve(QEasingCurve::OutCubic);

    geometryAnim = new QPropertyAnimation(overlay, "geometry", this);
    geometryAnim->setEasingCurve(QEasingCurve::OutCubic);

    iconAnim = new QPropertyAnimation(iconLabel, "pos", this);
    iconAnim->setDuration(220);
    iconAnim->setEasingCurve(QEasingCurve::InOutSine);

    connect(opacityAnim, &QPropertyAnimation::finished, overlay,
            [overlay, opacityEffect]()
            {
                if (!overlay->property("indicatorVisible").toBool() &&
                    opacityEffect && opacityEffect->opacity() <= 0.01)
                {
                    overlay->hide();
                }
            });

    return overlay;
}



QString PlayerLongPressHandler::longPressMode() const
{
    const QString mode =
        ConfigStore::instance()->get<QString>(ConfigKeys::PlayerLongPressMode, "seek");
    return mode == QStringLiteral("speed") ? mode : QStringLiteral("seek");
}

int PlayerLongPressHandler::longPressTriggerMs() const
{
    const int triggerMs =
        ConfigStore::instance()->get<QString>(ConfigKeys::PlayerLongPressTriggerMs, "500").toInt();
    return triggerMs > 0 ? triggerMs : 500;
}

double PlayerLongPressHandler::nextPlaybackRate(double currentRate) const
{
    static const QList<double> kSpeedSteps = {2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    for (double rate : kSpeedSteps)
    {
        if (rate > currentRate + 0.001)
        {
            return rate;
        }
    }
    return kSpeedSteps.constLast();
}

bool PlayerLongPressHandler::canStartPropertyAnimation(const QPropertyAnimation *animation) const
{
    return animation && animation->targetObject();
}

bool PlayerLongPressHandler::isMouseEdgeLongPressEnabled() const
{
    return ConfigStore::instance()->get<bool>(ConfigKeys::PlayerMouseEdgeLongPress, true);
}



QString PlayerLongPressHandler::formatSpeedText(double speed)
{
    QString text = QString::number(speed, 'f', 2);
    while (text.contains('.') && text.endsWith('0'))
    {
        text.chop(1);
    }
    if (text.endsWith('.'))
    {
        text.append('0');
    }
    return text + "X";
}

QString PlayerLongPressHandler::formatLongPressSeekText(double seconds)
{
    QString text = QString::number(seconds, 'f', seconds >= 10.0 ? 0 : 1);
    while (text.contains('.') && text.endsWith('0'))
    {
        text.chop(1);
    }
    if (text.endsWith('.'))
    {
        text.chop(1);
    }
    return text + "s";
}



void PlayerLongPressHandler::startKeyLongPress(int direction, bool silent)
{
    m_teardown = false;
    stopKeyLongPress(false);

    m_keyDirection = direction;
    m_keySeekRate = 2.0;
    m_keyRestoreSpeed = 1.0;
    m_keyCurrentSpeed = 1.0;
    m_keySpeedTick = 0;
    m_keyTriggered = false;
    m_keySilent = silent;
    m_keySpeedActive = false;
    m_keyActiveMode = longPressMode();

    const double step =
        ConfigStore::instance()->get<QString>(ConfigKeys::PlayerSeekStep, "10").toDouble();
    const bool longPressEnabled =
        ConfigStore::instance()->get<bool>(ConfigKeys::PlayerLongPressSeek, true);

    if (!longPressEnabled)
    {
        Q_EMIT seekRequested(direction * step, silent);
        m_keyDirection = 0;
        m_keySilent = false;
        m_keyActiveMode.clear();
        return;
    }

    if (m_keyActiveMode == QStringLiteral("seek"))
    {
        Q_EMIT seekRequested(direction * step, silent);
    }

    qDebug() << "[PlayerLongPressHandler] Direction key pressed"
             << "| direction:" << direction
             << "| mode:" << m_keyActiveMode
             << "| triggerMs:" << longPressTriggerMs();

    m_keyLongPressTimer->setInterval(longPressTriggerMs());
    m_keyLongPressTimer->start();
}

void PlayerLongPressHandler::stopKeyLongPress(bool applyShortPressFallback)
{
    if (m_keyLongPressTimer)
    {
        m_keyLongPressTimer->stop();
    }

    if (m_keyDirection == 0)
    {
        return;
    }

    if (m_keyActiveMode == QStringLiteral("speed"))
    {
        if (!m_keySpeedActive && applyShortPressFallback)
        {
            const double step = ConfigStore::instance()
                                    ->get<QString>(ConfigKeys::PlayerSeekStep, "10")
                                    .toDouble();
            Q_EMIT seekRequested(m_keyDirection * step, m_keySilent);
        }
    }

    m_keyDirection = 0;
    m_keySeekRate = 2.0;
    m_keyRestoreSpeed = 1.0;
    m_keyCurrentSpeed = 1.0;
    m_keySpeedTick = 0;
    m_keyTriggered = false;
    m_keySilent = false;
    m_keySpeedActive = false;
    m_keyActiveMode.clear();
    updateIndicators();
}

void PlayerLongPressHandler::onKeyLongPressTimeout()
{
    if (m_keyDirection == 0 || m_keyActiveMode.isEmpty())
    {
        return;
    }

    if (m_keyActiveMode == QStringLiteral("speed"))
    {
        if (!m_keyTriggered)
        {
            m_keyTriggered = true;
            m_keySpeedActive = true;
            m_keyCurrentSpeed = nextPlaybackRate(1.0);
            m_keySpeedTick = 0;

            const QString speedText = formatSpeedText(m_keyCurrentSpeed);
            updateIndicators();
            Q_EMIT toastRequested(tr("Speed: %1X").arg(speedText.chopped(1)));

            qDebug() << "[PlayerLongPressHandler] Long press speed activated"
                     << "| direction:" << m_keyDirection
                     << "| targetSpeed:" << m_keyCurrentSpeed;
        }
        else if (m_keySpeedTick > 0 && m_keySpeedTick % 2 == 0)
        {
            const double nextSpeed = nextPlaybackRate(m_keyCurrentSpeed);
            if (std::abs(nextSpeed - m_keyCurrentSpeed) > 0.001)
            {
                m_keyCurrentSpeed = nextSpeed;
                const QString speedText = formatSpeedText(m_keyCurrentSpeed);
                updateIndicators();
                Q_EMIT toastRequested(tr("Speed: %1X").arg(speedText.chopped(1)));
                qDebug() << "[PlayerLongPressHandler] Long press speed increased"
                         << "| direction:" << m_keyDirection
                         << "| targetSpeed:" << m_keyCurrentSpeed;
            }
        }

        const double deltaSeconds = m_keyDirection * m_keyCurrentSpeed * 0.25;
        Q_EMIT seekRequested(deltaSeconds, true);
        ++m_keySpeedTick;
        m_keyLongPressTimer->setInterval(250);
        return;
    }

    const bool wasTriggered = m_keyTriggered;
    m_keyTriggered = true;
    m_keyLongPressTimer->setInterval(100);
    Q_EMIT seekRequested(m_keyDirection * m_keySeekRate, m_keySilent);
    updateIndicators();

    if (!wasTriggered)
    {
        qDebug() << "[PlayerLongPressHandler] Long press seek activated"
                 << "| direction:" << m_keyDirection;
    }

    m_keySeekRate *= 1.15;
    if (m_keySeekRate > 30.0)
    {
        m_keySeekRate = 30.0;
    }
}



void PlayerLongPressHandler::startMouseEdgeLongPress(int direction)
{
    m_teardown = false;
    stopMouseEdgeLongPress();

    if (!isMouseEdgeLongPressEnabled())
    {
        return;
    }

    m_mouseEdgeDirection = direction;
    m_mouseEdgeTick = 0;
    m_mouseEdgeSpeed = 1.0;
    m_mouseEdgeActive = false;

    qDebug() << "[PlayerLongPressHandler] Mouse edge long press armed"
             << "| direction:" << direction
             << "| triggerMs:" << longPressTriggerMs();

    if (m_mouseEdgeTimer)
    {
        m_mouseEdgeTimer->setInterval(longPressTriggerMs());
        m_mouseEdgeTimer->start();
    }
}

bool PlayerLongPressHandler::stopMouseEdgeLongPress()
{
    if (m_mouseEdgeTimer)
    {
        m_mouseEdgeTimer->stop();
    }

    const bool wasActive = m_mouseEdgeActive;
    if (wasActive)
    {
        qDebug() << "[PlayerLongPressHandler] Mouse edge long press stopped"
                 << "| direction:" << m_mouseEdgeDirection
                 << "| speed:" << m_mouseEdgeSpeed;
    }

    m_mouseEdgeDirection = 0;
    m_mouseEdgeTick = 0;
    m_mouseEdgeSpeed = 1.0;
    m_mouseEdgeActive = false;
    updateIndicators();
    return wasActive;
}

void PlayerLongPressHandler::onMouseEdgeLongPressTimeout()
{
    if (m_mouseEdgeDirection == 0)
    {
        return;
    }

    if (!m_mouseEdgeActive)
    {
        m_mouseEdgeActive = true;
        m_mouseEdgeSpeed = nextPlaybackRate(1.0);
        updateIndicators();
        qDebug() << "[PlayerLongPressHandler] Mouse edge long press activated"
                 << "| direction:" << m_mouseEdgeDirection
                 << "| speed:" << m_mouseEdgeSpeed;
    }
    else if (m_mouseEdgeTick > 0 && m_mouseEdgeTick % 2 == 0)
    {
        const double nextSpeed = nextPlaybackRate(m_mouseEdgeSpeed);
        if (std::abs(nextSpeed - m_mouseEdgeSpeed) > 0.001)
        {
            m_mouseEdgeSpeed = nextSpeed;
            updateIndicators();
            qDebug() << "[PlayerLongPressHandler] Mouse edge long press accelerated"
                     << "| direction:" << m_mouseEdgeDirection
                     << "| speed:" << m_mouseEdgeSpeed;
        }
    }

    const double deltaSeconds = m_mouseEdgeDirection * m_mouseEdgeSpeed * 0.25;
    Q_EMIT seekRequested(deltaSeconds, true);
    ++m_mouseEdgeTick;

    if (m_mouseEdgeTimer)
    {
        m_mouseEdgeTimer->setInterval(250);
    }
}



bool PlayerLongPressHandler::isMouseEdgeActive() const
{
    return m_mouseEdgeActive;
}

int PlayerLongPressHandler::mouseEdgeLongPressDirection() const
{
    return m_mouseEdgeDirection;
}

bool PlayerLongPressHandler::isKeyLongPressTriggered() const
{
    return m_keyTriggered;
}

int PlayerLongPressHandler::keyLongPressDirection() const
{
    return m_keyDirection;
}

int PlayerLongPressHandler::activeLongPressDirection() const
{
    if (m_mouseEdgeActive && m_mouseEdgeDirection != 0)
    {
        return m_mouseEdgeDirection;
    }
    if (m_keyTriggered && m_keyDirection != 0)
    {
        return m_keyDirection;
    }
    return 0;
}

void PlayerLongPressHandler::setTeardown(bool teardown)
{
    m_teardown = teardown;
    if (!m_teardown)
    {
        return;
    }

    if (m_keyLongPressTimer)
    {
        m_keyLongPressTimer->stop();
    }
    if (m_mouseEdgeTimer)
    {
        m_mouseEdgeTimer->stop();
    }
    stopIndicatorAnimations();
}



QRect PlayerLongPressHandler::indicatorGeometry(int direction, bool active) const
{
    const int w = m_parentWidget ? m_parentWidget->width() : 800;
    const int h = m_parentWidget ? m_parentWidget->height() : 600;
    const int indicatorWidth = active ? 128 : 60;
    const int indicatorHeight = 54;
    const int indicatorMargin = 28;
    const int indicatorY = qMax(m_topHudHeight + 24, (h - indicatorHeight) / 2);
    const int indicatorX = direction < 0
                               ? indicatorMargin
                               : w - indicatorWidth - indicatorMargin;
    return QRect(indicatorX, indicatorY, indicatorWidth, indicatorHeight);
}

void PlayerLongPressHandler::animateIndicator(
    QWidget *overlay, int direction, bool active, bool visible)
{
    if (!overlay)
    {
        return;
    }

    QGraphicsOpacityEffect *opacityEffect =
        direction < 0 ? m_leftIndicatorOpacity : m_rightIndicatorOpacity;
    QPropertyAnimation *opacityAnim =
        direction < 0 ? m_leftIndicatorOpacityAnim : m_rightIndicatorOpacityAnim;
    QPropertyAnimation *geometryAnim =
        direction < 0 ? m_leftIndicatorGeometryAnim : m_rightIndicatorGeometryAnim;

    const QRect targetGeometry = indicatorGeometry(direction, active);
    if (!opacityEffect || !opacityAnim || !geometryAnim)
    {
        overlay->setGeometry(targetGeometry);
        overlay->setVisible(visible);
        return;
    }

    if (!opacityAnim->targetObject() || !geometryAnim->targetObject() || m_teardown)
    {
        overlay->setProperty("indicatorVisible", visible);
        overlay->setGeometry(targetGeometry);
        opacityEffect->setOpacity(visible ? (active ? 1.0 : 0.62) : 0.0);
        overlay->setVisible(visible);
        return;
    }

    stopPropertyAnimationSafely(opacityAnim);
    stopPropertyAnimationSafely(geometryAnim);

    if (visible)
    {
        const bool wasVisible = overlay->isVisible();
        const QRect startGeometry = wasVisible
                                        ? overlay->geometry()
                                        : targetGeometry.translated(
                                              direction < 0 ? -14 : 14, 0);
        const qreal startOpacity = wasVisible ? opacityEffect->opacity() : 0.0;
        const qreal targetOpacity = active ? 1.0 : 0.62;

        overlay->setProperty("indicatorVisible", true);
        overlay->setGeometry(startGeometry);
        overlay->show();
        overlay->raise();
        opacityEffect->setOpacity(startOpacity);

        opacityAnim->setDuration(wasVisible ? 120 : 180);
        opacityAnim->setStartValue(startOpacity);
        opacityAnim->setEndValue(targetOpacity);

        geometryAnim->setDuration(wasVisible ? 140 : 200);
        geometryAnim->setStartValue(startGeometry);
        geometryAnim->setEndValue(targetGeometry);
    }
    else
    {
        if (!overlay->isVisible())
        {
            opacityEffect->setOpacity(0.0);
            return;
        }

        const QRect currentGeometry = overlay->geometry();
        overlay->setProperty("indicatorVisible", false);

        opacityAnim->setDuration(120);
        opacityAnim->setStartValue(opacityEffect->opacity());
        opacityAnim->setEndValue(0.0);

        geometryAnim->setDuration(120);
        geometryAnim->setStartValue(currentGeometry);
        geometryAnim->setEndValue(
            currentGeometry.translated(direction < 0 ? -10 : 10, 0));
    }

    if (canStartPropertyAnimation(opacityAnim))
    {
        opacityAnim->start();
    }
    else if (opacityEffect)
    {
        opacityEffect->setOpacity(visible ? (active ? 1.0 : 0.62) : 0.0);
    }

    if (canStartPropertyAnimation(geometryAnim))
    {
        geometryAnim->start();
    }
    else
    {
        overlay->setGeometry(visible ? targetGeometry
                                     : overlay->geometry().translated(
                                           direction < 0 ? -10 : 10, 0));
    }
}

void PlayerLongPressHandler::animateIndicatorIcon(
    QLabel *iconLabel, int direction, bool active, bool immediate)
{
    if (!iconLabel)
    {
        return;
    }

    QPropertyAnimation *iconAnim =
        direction < 0 ? m_leftIndicatorIconAnim : m_rightIndicatorIconAnim;
    if (!iconAnim)
    {
        return;
    }

    const QPoint basePos = iconLabel->property("basePos").toPoint();
    const int offset = (active && m_pulseForward)
                           ? (direction < 0 ? -4 : 4)
                           : 0;
    const QPoint targetPos = basePos + QPoint(offset, 0);

    if (immediate || !iconAnim->targetObject() || m_teardown)
    {
        iconLabel->move(targetPos);
        return;
    }

    stopPropertyAnimationSafely(iconAnim);
    iconAnim->setStartValue(iconLabel->pos());
    iconAnim->setEndValue(targetPos);
    if (canStartPropertyAnimation(iconAnim))
    {
        iconAnim->start();
    }
    else
    {
        iconLabel->move(targetPos);
    }
}

void PlayerLongPressHandler::setIndicatorState(
    QWidget *overlay, QLabel *iconLabel, QLabel *textLabel, int direction,
    bool active, const QString &speedText)
{
    if (!overlay || !iconLabel || !textLabel)
    {
        return;
    }

    overlay->setProperty("gestureActive", active);
    overlay->style()->unpolish(overlay);
    overlay->style()->polish(overlay);
    overlay->update();

    if (speedText.isEmpty())
    {
        textLabel->clear();
        textLabel->hide();
    }
    else
    {
        textLabel->setText(speedText);
        textLabel->show();
    }

    if (overlay->layout())
    {
        overlay->layout()->activate();
    }

    iconLabel->setProperty("basePos", iconLabel->pos());
    animateIndicatorIcon(iconLabel, direction, active, true);
    animateIndicator(overlay, direction, active, true);
}

void PlayerLongPressHandler::updateIndicatorPulse()
{
    if (m_teardown)
    {
        stopIndicatorAnimations();
        return;
    }

    const int activeDirection = activeLongPressDirection();
    if (activeDirection == 0)
    {
        if (m_pulseTimer)
        {
            m_pulseTimer->stop();
        }
        m_pulseForward = false;
        animateIndicatorIcon(m_leftIndicatorIcon, -1, false, true);
        animateIndicatorIcon(m_rightIndicatorIcon, 1, false, true);
        return;
    }

    m_pulseForward = !m_pulseForward;
    animateIndicatorIcon(m_leftIndicatorIcon, -1, activeDirection < 0);
    animateIndicatorIcon(m_rightIndicatorIcon, 1, activeDirection > 0);
}

void PlayerLongPressHandler::updateIndicators()
{
    if (m_teardown)
    {
        stopIndicatorAnimations();

        if (m_leftIndicator)
        {
            m_leftIndicator->hide();
        }
        if (m_rightIndicator)
        {
            m_rightIndicator->hide();
        }
        animateIndicatorIcon(m_leftIndicatorIcon, -1, false, true);
        animateIndicatorIcon(m_rightIndicatorIcon, 1, false, true);
        return;
    }

    const int activeDirection = activeLongPressDirection();
    if (activeDirection == 0)
    {
        if (m_leftIndicator)
        {
            m_leftIndicator->setProperty("gestureActive", false);
            m_leftIndicator->style()->unpolish(m_leftIndicator);
            m_leftIndicator->style()->polish(m_leftIndicator);
            animateIndicator(m_leftIndicator, -1, false, false);
        }
        if (m_rightIndicator)
        {
            m_rightIndicator->setProperty("gestureActive", false);
            m_rightIndicator->style()->unpolish(m_rightIndicator);
            m_rightIndicator->style()->polish(m_rightIndicator);
            animateIndicator(m_rightIndicator, 1, false, false);
        }
        updateIndicatorPulse();
        return;
    }

    const bool fromMouse = m_mouseEdgeActive && m_mouseEdgeDirection != 0;
    const QString indicatorText =
        fromMouse
            ? formatSpeedText(m_mouseEdgeSpeed)
            : (m_keyActiveMode == QStringLiteral("speed")
                   ? formatSpeedText(m_keyCurrentSpeed)
                   : formatLongPressSeekText(m_keySeekRate));
    const bool leftActive = activeDirection < 0;
    const bool rightActive = activeDirection > 0;

    setIndicatorState(
        m_leftIndicator, m_leftIndicatorIcon,
        m_leftIndicatorText, -1, leftActive,
        leftActive ? indicatorText : QString());
    setIndicatorState(
        m_rightIndicator, m_rightIndicatorIcon,
        m_rightIndicatorText, 1, rightActive,
        rightActive ? indicatorText : QString());

    if (m_pulseTimer && !m_pulseTimer->isActive())
    {
        m_pulseForward = false;
        updateIndicatorPulse();
        m_pulseTimer->start();
    }
}

void PlayerLongPressHandler::stopIndicatorAnimations()
{
    if (m_pulseTimer)
    {
        m_pulseTimer->stop();
    }

    const QList<QPropertyAnimation *> animations = {
        m_leftIndicatorOpacityAnim, m_rightIndicatorOpacityAnim,
        m_leftIndicatorGeometryAnim, m_rightIndicatorGeometryAnim,
        m_leftIndicatorIconAnim, m_rightIndicatorIconAnim};
    for (QPropertyAnimation *animation : animations)
    {
        stopPropertyAnimationSafely(animation);
    }
}

void PlayerLongPressHandler::stopAllAnimations()
{
    stopIndicatorAnimations();
}

void PlayerLongPressHandler::updateGeometry(int parentWidth, int parentHeight, int topHudHeight)
{
    Q_UNUSED(parentHeight);
    m_topHudHeight = topHudHeight;

    auto syncGeometry = [this](QWidget *overlay, QPropertyAnimation *animation, int direction)
    {
        if (!overlay)
        {
            return;
        }

        const QRect targetGeometry = indicatorGeometry(
            direction, overlay->property("gestureActive").toBool());
        if (animation && animation->state() == QAbstractAnimation::Running)
        {
            animation->setEndValue(targetGeometry);
            return;
        }

        overlay->setGeometry(targetGeometry);
    };

    syncGeometry(m_leftIndicator, m_leftIndicatorGeometryAnim, -1);
    syncGeometry(m_rightIndicator, m_rightIndicatorGeometryAnim, 1);
}
