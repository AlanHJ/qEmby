#include "danmakuclock.h"

#include <algorithm>
#include <cmath>

qreal DanmakuClock::positionMs(qreal presentationOffsetMs) const
{
    if (!m_running || !m_elapsed.isValid()) {
        return m_positionMs;
    }
    
    
    const qreal elapsed = m_elapsed.nsecsElapsed() / 1000000.0 + presentationOffsetMs;
    
    return std::max<qreal>(0.0, m_positionMs + elapsed * m_speed +
           m_correction * std::clamp(elapsed / 1000.0, 0.0, 1.0));
}

void DanmakuClock::reset(qreal positionMs)
{
    m_positionMs = std::max<qreal>(0.0, positionMs);
    m_correction = 0.0;
    m_hasSample = true;
    m_elapsed.start();
}

bool DanmakuClock::synchronize(qreal sampleMs, bool discontinuity)
{
    if (!std::isfinite(sampleMs)) {
        return false;
    }
    const qreal now = positionMs();
    const qreal error = sampleMs - now;
    if (!m_hasSample || discontinuity || !m_running || std::abs(error) > 500.0) {
        reset(sampleMs);
        return true;
    }
    
    m_positionMs = now;
    m_elapsed.restart();
    m_correction = std::abs(error) < 20.0 ? 0.0
        : std::clamp(error, -30.0 * m_speed, 30.0 * m_speed);
    return false;
}

void DanmakuClock::setRunning(bool running)
{
    if (m_running == running) {
        return;
    }
    m_positionMs = positionMs();
    m_correction = 0.0;
    m_elapsed.start();
    m_running = running;
}

void DanmakuClock::setSpeed(qreal speed)
{
    if (!std::isfinite(speed)) {
        return;
    }
    m_positionMs = positionMs();
    m_correction = 0.0;
    m_elapsed.start();
    m_speed = std::clamp(speed, 0.1, 16.0);
}
