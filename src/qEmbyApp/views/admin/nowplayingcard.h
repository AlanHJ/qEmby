#ifndef NOWPLAYINGCARD_H
#define NOWPLAYINGCARD_H

#include <QFrame>
#include <QPixmap>
#include <qcorotask.h>


struct SessionInfo;
class QEmbyCore;
class QLabel;
class QProgressBar;

class NowPlayingCard : public QFrame {
    Q_OBJECT
public:
    explicit NowPlayingCard(const SessionInfo& session, QEmbyCore* core, QWidget* parent = nullptr);

    
    void updateSession(const SessionInfo& session);

    QString sessionId() const { return m_sessionId; }

private:
    void setupUi(const SessionInfo& session);
    QCoro::Task<void> loadImage(const SessionInfo& session);

    QEmbyCore* m_core;
    QString m_sessionId;
    QString m_currentImageItemId;

    
    QLabel* m_imageLabel;
    QLabel* m_titleLabel;
    QLabel* m_subtitleLabel;
    QLabel* m_progressText;
    QLabel* m_stateIcon;
    QProgressBar* m_progressBar;
    QLabel* m_userLabel;
    QLabel* m_streamLabel;
};

#endif 
