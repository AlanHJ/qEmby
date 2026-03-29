#ifndef MEDIAACTIONMENU_H
#define MEDIAACTIONMENU_H

#include <QMenu>
#include <models/media/mediaitem.h>


class QEmbyCore;

class MediaActionMenu : public QMenu
{
    Q_OBJECT
public:
    
    explicit MediaActionMenu(const MediaItem& item, QEmbyCore* core, QWidget *parent = nullptr);

signals:
    
    void playRequested();
    void externalPlayRequested(const QString &playerPath);
    void favoriteRequested();
    void detailRequested();
    void markPlayedRequested();
    void markUnplayedRequested();
    void removeFromResumeRequested();

private:
    void setupMenu();

    MediaItem m_item;
    QEmbyCore* m_core; 
};

#endif 
