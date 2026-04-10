#ifndef LIBRARYACTIONMENU_H
#define LIBRARYACTIONMENU_H

#include "librarycontextmenurequest.h"
#include <QMenu>
#include <models/admin/adminmodels.h>

class QAction;
class QIcon;
class QPoint;

class LibraryActionMenu : public QMenu
{
    Q_OBJECT
public:
    explicit LibraryActionMenu(const VirtualFolder& folder, bool scanActive,
                               QWidget* parent = nullptr);
    LibraryContextMenuRequest execRequest(const QPoint& globalPos);

private:
    QAction* addMenuAction(LibraryContextMenuAction action, const QIcon& icon,
                           const QString& text,
                           const QString& stringValue = QString());
    void setupMenu();
    static LibraryContextMenuRequest requestFromAction(const QAction* action);

    VirtualFolder m_folder;
    bool m_scanActive = false;
};

#endif 
