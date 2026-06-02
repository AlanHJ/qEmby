#ifndef PLAYEROSDLAYER_H
#define PLAYEROSDLAYER_H

#include <QObject>

class QWidget;
class QLabel;
class QProgressBar;
class QGraphicsOpacityEffect;
class QPropertyAnimation;
class QTimer;


class PlayerOsdLayer : public QObject
{
    Q_OBJECT
public:
    explicit PlayerOsdLayer(QWidget *parent);

    
    void showSeek(double position, double duration, const QString &timeText);

    
    void showVolume(int volumePercent, const QString &text, bool muted);

    
    void hide();

    
    void forceHide();

    
    void updateGeometry(int parentWidth, int parentHeight);

    
    void stopAnimations();

    
    bool isVisible() const;

    
    bool isSeekLineVisible() const;

    
    QWidget *container() const;

    
    void updateSeekPosition(int position, const QString &timeText);

private:
    void fadeIn();
    bool canStartAnimation() const;
    void updateSeekLayout();
    void updateVolumeLayout();

    QWidget *m_container = nullptr;
    QProgressBar *m_seekLine = nullptr;
    QLabel *m_seekTimeLabel = nullptr;
    QWidget *m_seekStem = nullptr;
    QWidget *m_seekMarker = nullptr;
    QWidget *m_volumePanel = nullptr;
    QLabel *m_volumeIconLabel = nullptr;
    QProgressBar *m_volumeBar = nullptr;
    QLabel *m_volumeLabel = nullptr;
    QGraphicsOpacityEffect *m_opacity = nullptr;
    QPropertyAnimation *m_fadeAnim = nullptr;
    QTimer *m_hideTimer = nullptr;
    int m_parentWidth = 0;
    int m_parentHeight = 0;
    double m_seekPosition = 0.0;
    double m_seekDuration = 0.0;
    int m_volumePercent = 100;
};

#endif 
