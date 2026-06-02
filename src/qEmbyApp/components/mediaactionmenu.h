#ifndef MEDIAACTIONMENU_H
#define MEDIAACTIONMENU_H

#include "cardcontextmenurequest.h"
#include <QList>
#include <QMenu>
#include <models/media/mediaitem.h>

class QAction;
class QIcon;
class QPoint;


class QEmbyCore;

class MediaActionMenu : public QMenu
{
    Q_OBJECT
public:
    
    explicit MediaActionMenu(const MediaItem& item, QEmbyCore* core,
                             QWidget *parent = nullptr,
                             const QList<CardContextMenuAction> &allowedActions = {});
    CardContextMenuRequest execRequest(const QPoint& globalPos);

private:
    QAction* addMenuAction(CardContextMenuAction action, const QIcon& icon,
                           const QString& text,
                           const QString& stringValue = QString());
    void setupMenu();
    static CardContextMenuRequest requestFromAction(const QAction* action);

    MediaItem m_item;
    QEmbyCore* m_core; 
    QList<CardContextMenuAction> m_allowedActions;
};

#endif 
