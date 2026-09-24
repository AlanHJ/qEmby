#ifndef DANMAKUCLOCK_H
#define DANMAKUCLOCK_H

#include <QElapsedTimer>



class DanmakuClock
{
public:
    
    qreal positionMs(qreal presentationOffsetMs = 0.0) const;
    void reset(qreal positionMs);
    bool synchronize(qreal positionMs, bool discontinuity = false);
    void setRunning(bool running);
    void setSpeed(qreal speed);
    bool isRunning() const { return m_running; }
    qreal speed() const { return m_speed; }

private:
    QElapsedTimer m_elapsed;
    qreal m_positionMs = 0.0;
    qreal m_speed = 1.0;
    qreal m_correction = 0.0;
    bool m_running = false;
    bool m_hasSample = false;
};

#endif
