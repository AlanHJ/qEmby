#include "danmakuframeclock.h"

#include <algorithm>
#include <cmath>

void DanmakuFrameClock::reset()
{
    m_elapsed.invalidate();
    m_intervalMs = 0.0;
    m_targetMs = 0.0;
    m_lastActivityMs = 0.0;
    m_phaseErrorMs = 0.0;
    m_hasFrameTarget = false;
}

void DanmakuFrameClock::reanchor(qreal nowMs, qreal intervalMs)
{
    m_intervalMs = intervalMs;
    m_targetMs = nowMs + intervalMs;
    m_lastActivityMs = nowMs;
    m_phaseErrorMs = 0.0;
    m_hasFrameTarget = false;
}

qreal DanmakuFrameClock::prepare(qreal refreshIntervalMs)
{
    
    
    const qreal interval = std::isfinite(refreshIntervalMs) && refreshIntervalMs > 0.0
        ? std::clamp(refreshIntervalMs, 1.0, 1000.0) : 1000.0 / 60.0;
    if (!m_elapsed.isValid()) {
        m_elapsed.start();
        reanchor(0.0, interval);
        return 0.0;
    }

    const qreal now = m_elapsed.nsecsElapsed() / 1000000.0;
    const qreal stallLimit = std::max(250.0, interval * 8.0);
    if (std::abs(interval - m_intervalMs) > interval * 0.0001 ||
        now - m_lastActivityMs > stallLimit || now - m_targetMs > stallLimit) {
        
        
        reanchor(now, interval);
    } else {
        m_intervalMs = interval;
        m_lastActivityMs = now;
    }
    return now;
}

qreal DanmakuFrameClock::nextFrameOffsetMs(qreal refreshIntervalMs)
{
    const qreal now = prepare(refreshIntervalMs);
    if (now >= m_targetMs + m_intervalMs * 0.5) {
        
        
        
        const qreal missedPeriods = std::floor((now - m_targetMs) / m_intervalMs + 0.5);
        m_targetMs += missedPeriods * m_intervalMs;
    }
    m_hasFrameTarget = true;
    return m_targetMs - now;
}

void DanmakuFrameClock::frameSwapped(qreal refreshIntervalMs)
{
    const qreal now = prepare(refreshIntervalMs);
    if (!m_hasFrameTarget || now < m_targetMs - m_intervalMs * 0.5) {
        
        
        
        return;
    }

    
    
    const qreal missedPeriods = std::max(0.0,
        std::floor((now - m_targetMs) / m_intervalMs + 0.5));
    const qreal observedTarget = m_targetMs + missedPeriods * m_intervalMs;
    m_phaseErrorMs = now - observedTarget;
    
    
    const qreal correction = std::clamp(m_phaseErrorMs / 16.0,
        -m_intervalMs * 0.03, m_intervalMs * 0.03);
    m_targetMs = observedTarget + m_intervalMs + correction;
    m_hasFrameTarget = false;
}
