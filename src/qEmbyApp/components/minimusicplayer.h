#pragma once
#include <QWidget>
#include <qcorotask.h>
class ElidedLabel;
class ModernSlider;
class QPushButton;
class QLabel;
class QEmbyCore;
class MiniMusicPlayer : public QWidget {
    Q_OBJECT
public:
    explicit MiniMusicPlayer(QEmbyCore* core, QWidget* parent = nullptr);
    void setSuppressed(bool suppressed) { m_suppressed = suppressed; refresh(); }
private:
    void refresh();
    QCoro::Task<void> loadCover(QString id, QString tag);
    QEmbyCore* m_core;
    QLabel* m_cover;
    ElidedLabel* m_title;
    ElidedLabel* m_artist;
    QPushButton* m_play;
    ModernSlider* m_progress;
    QString m_coverId;
    bool m_suppressed = false;
};
