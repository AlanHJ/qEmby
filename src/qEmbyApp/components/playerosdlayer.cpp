#include "playerosdlayer.h"
#include <QAbstractAnimation>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QProgressBar>
#include <QPropertyAnimation>
#include <QTimer>
#include <QWidget>

namespace
{
void stopFadeAnimationSafely(QPropertyAnimation *animation)
{
    if (!animation || !animation->targetObject() ||
        animation->state() == QAbstractAnimation::Stopped)
    {
        return;
    }

    animation->stop();
}
} 

PlayerOsdLayer::PlayerOsdLayer(QWidget *parent)
    : QObject(parent)
{
    
    m_container = new QWidget(parent);
    m_container->setObjectName("osdContainer");
    m_container->setAttribute(Qt::WA_TransparentForMouseEvents); 
    m_container->hide();

    
    m_opacity = new QGraphicsOpacityEffect(m_container);
    m_opacity->setOpacity(0.0);
    m_container->setGraphicsEffect(m_opacity);

    
    m_fadeAnim = new QPropertyAnimation(m_opacity, "opacity", this);
    m_fadeAnim->setDuration(250);

    
    m_hideTimer = new QTimer(this);
    m_hideTimer->setSingleShot(true);
    connect(m_hideTimer, &QTimer::timeout, this, &PlayerOsdLayer::hide);

    
    m_seekLine = new QProgressBar(m_container);
    m_seekLine->setObjectName("osdSeekLine");
    m_seekLine->setTextVisible(false);
    m_seekLine->setFixedHeight(3);

    
    m_seekTimeLabel = new QLabel(m_container);
    m_seekTimeLabel->setObjectName("osdSeekTimeLabel");
    m_seekTimeLabel->setAlignment(Qt::AlignCenter);

    
    m_volumeLabel = new QLabel(m_container);
    m_volumeLabel->setObjectName("osdVolumeLabel");
    m_volumeLabel->setAlignment(Qt::AlignCenter);
}

void PlayerOsdLayer::showSeek(double position, double duration, const QString &timeText)
{
    fadeIn();

    m_seekLine->show();
    m_seekTimeLabel->show();
    m_volumeLabel->hide();

    m_seekLine->setMaximum(static_cast<int>(duration));
    m_seekLine->setValue(static_cast<int>(position));
    m_seekTimeLabel->setText(timeText);

    m_hideTimer->start(1500); 
}

void PlayerOsdLayer::showVolume(const QString &text)
{
    fadeIn();

    m_seekLine->hide();
    m_seekTimeLabel->hide();
    m_volumeLabel->show();

    m_volumeLabel->setText(text);

    m_hideTimer->start(1500);
}

void PlayerOsdLayer::hide()
{
    if (m_opacity->opacity() <= 0.0)
    {
        return;
    }
    stopFadeAnimationSafely(m_fadeAnim);
    m_fadeAnim->setStartValue(m_opacity->opacity());
    m_fadeAnim->setEndValue(0.0);
    if (canStartAnimation())
    {
        m_fadeAnim->start();
    }
    else
    {
        m_opacity->setOpacity(0.0);
        if (m_container)
        {
            m_container->hide();
        }
    }
}

void PlayerOsdLayer::forceHide()
{
    if (m_hideTimer)
    {
        m_hideTimer->stop();
    }
    stopFadeAnimationSafely(m_fadeAnim);
    if (m_opacity)
    {
        m_opacity->setOpacity(0.0);
    }
    if (m_container)
    {
        m_container->hide();
    }
}

void PlayerOsdLayer::updateGeometry(int parentWidth, int parentHeight)
{
    m_container->setGeometry(0, 0, parentWidth, parentHeight);
    m_seekLine->setGeometry(0, parentHeight - 3, parentWidth, 3);
    m_seekTimeLabel->setGeometry((parentWidth - 200) / 2, parentHeight - 45, 200, 36);
    m_volumeLabel->setGeometry((parentWidth - 160) / 2, parentHeight - 45, 160, 36);
}

void PlayerOsdLayer::stopAnimations()
{
    if (m_hideTimer)
    {
        m_hideTimer->stop();
    }
    if (m_fadeAnim)
    {
        stopFadeAnimationSafely(m_fadeAnim);
    }
    if (m_opacity)
    {
        m_opacity->setOpacity(0.0);
    }
    if (m_container)
    {
        m_container->hide();
    }
}

bool PlayerOsdLayer::isVisible() const
{
    return m_container && m_container->isVisible();
}

bool PlayerOsdLayer::isSeekLineVisible() const
{
    return isVisible() && m_seekLine && !m_seekLine->isHidden();
}

QWidget *PlayerOsdLayer::container() const
{
    return m_container;
}

void PlayerOsdLayer::updateSeekPosition(int position, const QString &timeText)
{
    m_seekLine->setValue(position);
    m_seekTimeLabel->setText(timeText);
}

void PlayerOsdLayer::fadeIn()
{
    if (!m_container->isVisible() || m_opacity->opacity() == 0.0)
    {
        m_container->show();
        stopFadeAnimationSafely(m_fadeAnim);
        m_fadeAnim->setStartValue(m_opacity->opacity());
        m_fadeAnim->setEndValue(1.0);
        if (canStartAnimation())
        {
            m_fadeAnim->start();
        }
        else
        {
            m_opacity->setOpacity(1.0);
        }
    }
}

bool PlayerOsdLayer::canStartAnimation() const
{
    return m_fadeAnim && m_fadeAnim->targetObject();
}
