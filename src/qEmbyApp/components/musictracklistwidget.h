#pragma once
#include <QWidget>
#include <models/media/mediaitem.h>
#include <qcorotask.h>
class QEmbyCore;
class QTreeWidget;
class QLabel;
class MusicTrackListWidget : public QWidget {
    Q_OBJECT
public:
    explicit MusicTrackListWidget(QEmbyCore* core, QWidget* parent = nullptr);
    void loadAlbum(QString albumId);
private:
    QCoro::Task<void> fetchTracks(QString albumId, quint64 generation);
    void updatePlayingTrack();
    QEmbyCore* m_core;
    QTreeWidget* m_table;
    QLabel* m_status;
    QList<MediaItem> m_tracks;
    QString m_albumId;
    QObject* m_requestContext = nullptr;
    quint64 m_generation = 0;
    QString m_displayedTrack;
    int m_displayedState = -1;
};
