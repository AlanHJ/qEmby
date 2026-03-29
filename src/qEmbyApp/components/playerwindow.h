#ifndef PLAYERWINDOW_H
#define PLAYERWINDOW_H

#include <QWidget>
#include <QPointer>

class QEmbyCore;
class PlayerView;




class PlayerWindow : public QWidget
{
    Q_OBJECT
public:
    explicit PlayerWindow(QEmbyCore* core, QWidget *parent = nullptr);

    void playMedia(const QString &mediaId, const QString &title,
                   const QString &streamUrl, long long startPositionTicks = 0,
                   const QVariant& sourceInfoVar = QVariant());

private:
    PlayerView* m_playerView;
};

#endif 
