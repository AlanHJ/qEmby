#include "libraryactionmenu.h"

#include "../managers/thememanager.h"
#include <QAction>
#include <QFile>
#include <QIcon>
#include <QVariant>

LibraryActionMenu::LibraryActionMenu(const VirtualFolder& folder,
                                     bool scanActive, QWidget* parent)
    : QMenu(parent), m_folder(folder), m_scanActive(scanActive)
{
    
    setObjectName("media-action-menu");
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint |
                   Qt::NoDropShadowWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    setupMenu();
}

LibraryContextMenuRequest LibraryActionMenu::execRequest(const QPoint& globalPos)
{
    return requestFromAction(exec(globalPos));
}

QAction* LibraryActionMenu::addMenuAction(LibraryContextMenuAction action,
                                          const QIcon& icon,
                                          const QString& text,
                                          const QString& stringValue)
{
    auto* menuAction = new QAction(icon, text, this);

    LibraryContextMenuRequest request;
    request.action = action;
    request.stringValue = stringValue;
    menuAction->setData(QVariant::fromValue(request));

    addAction(menuAction);
    return menuAction;
}

LibraryContextMenuRequest
LibraryActionMenu::requestFromAction(const QAction* action)
{
    if (!action) {
        return {};
    }

    const QVariant requestData = action->data();
    if (!requestData.canConvert<LibraryContextMenuRequest>()) {
        return {};
    }

    return requestData.value<LibraryContextMenuRequest>();
}

void LibraryActionMenu::setupMenu()
{
    const QString themeDir =
        ThemeManager::instance()->isDarkMode() ? "dark" : "light";
    auto adaptiveIcon = [&themeDir](const QString& baseName) -> QIcon {
        const QString themePath = QString(":/svg/%1/%2").arg(themeDir, baseName);
        if (QFile::exists(themePath)) {
            return QIcon(themePath);
        }

        return ThemeManager::getAdaptiveIcon(
            QString(":/svg/player/%1").arg(baseName));
    };

    const bool hasItemId = !m_folder.itemId.trimmed().isEmpty();

    QAction* editImagesAction = addMenuAction(LibraryContextMenuAction::EditImages,
                                              adaptiveIcon("edit-images.svg"),
                                              tr("Edit Images"));
    editImagesAction->setEnabled(hasItemId && !m_scanActive);

    QAction* refreshAction = addMenuAction(LibraryContextMenuAction::RefreshMetadata,
                                           adaptiveIcon("refresh.svg"),
                                           tr("Refresh Metadata"));
    refreshAction->setEnabled(hasItemId && !m_scanActive);

    QAction* scanAction = addMenuAction(LibraryContextMenuAction::ScanLibraryFiles,
                                        adaptiveIcon("scan-library.svg"),
                                        tr("Scan Library Files"));
    scanAction->setEnabled(hasItemId && !m_scanActive);
}
