#ifndef DANMAKUFRAMECLOCK_H
#define DANMAKUFRAMECLOCK_H

#include <QElapsedTimer>




class DanmakuFrameClock
{
public:
    void reset();
    
    
    qreal nextFrameOffsetMs(qreal refreshIntervalMs);
    void frameSwapped(qreal refreshIntervalMs);

    qreal phaseErrorMs() const { return m_phaseErrorMs; }
    qreal frameIntervalMs() const { return m_intervalMs; }

private:
    qreal prepare(qreal refreshIntervalMs);
    void reanchor(qreal nowMs, qreal intervalMs);

    QElapsedTimer m_elapsed;
    qreal m_intervalMs = 0.0;
    qreal m_targetMs = 0.0;
    qreal m_lastActivityMs = 0.0;
    qreal m_phaseErrorMs = 0.0;
    bool m_hasFrameTarget = false;
};

#endif
