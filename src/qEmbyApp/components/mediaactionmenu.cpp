#include "mediaactionmenu.h"
#include "../managers/thememanager.h" 
#include "../managers/externalplayerdetector.h" 
#include "../utils/mediaidentifyutils.h"
#include "../utils/mediaitemutils.h"
#include "../utils/playlistutils.h"
#include <qembycore.h>
#include <services/manager/servermanager.h> 
#include <models/profile/serverprofile.h>
#include <QAction>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QVariant>
#include <config/config_keys.h>
#include <config/configstore.h>


MediaActionMenu::MediaActionMenu(const MediaItem& item, QEmbyCore* core,
                                 QWidget *parent,
                                 const QList<CardContextMenuAction> &allowedActions)
    : QMenu(parent), m_item(item), m_core(core), m_allowedActions(allowedActions)
{
    setObjectName("media-action-menu");

    
    
    
    
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);

    
    setAttribute(Qt::WA_TranslucentBackground);

    setupMenu();
}

CardContextMenuRequest MediaActionMenu::execRequest(const QPoint& globalPos)
{
    return requestFromAction(exec(globalPos));
}

QAction* MediaActionMenu::addMenuAction(CardContextMenuAction action,
                                        const QIcon& icon,
                                        const QString& text,
                                        const QString& stringValue)
{
    auto* menuAction = new QAction(icon, text, this);

    CardContextMenuRequest request;
    request.action = action;
    request.stringValue = stringValue;
    menuAction->setData(QVariant::fromValue(request));

    addAction(menuAction);
    return menuAction;
}

CardContextMenuRequest MediaActionMenu::requestFromAction(const QAction* action)
{
    if (!action) {
        return {};
    }

    const QVariant requestData = action->data();
    if (!requestData.canConvert<CardContextMenuRequest>()) {
        return {};
    }

    return requestData.value<CardContextMenuRequest>();
}

void MediaActionMenu::setupMenu()
{
    
    
    
    QString themeDir = ThemeManager::instance()->isDarkMode() ? "dark" : "light";
    
    auto adaptiveIcon = [&themeDir](const QString& baseName) -> QIcon {
        
        QString themePath = QString(":/svg/%1/%2").arg(themeDir, baseName);
        if (QFile::exists(themePath))
            return QIcon(themePath);
        
        return ThemeManager::getAdaptiveIcon(QString(":/svg/player/%1").arg(baseName));
    };

    
    auto maybeAdd = [this](CardContextMenuAction action, const QIcon &icon,
                           const QString &text,
                           const QString &stringValue = QString()) -> QAction * {
        if (!m_allowedActions.isEmpty() && !m_allowedActions.contains(action))
            return nullptr;
        return addMenuAction(action, icon, text, stringValue);
    };

    const auto activeProfile =
        m_core && m_core->serverManager()
            ? m_core->serverManager()->activeProfile()
            : ServerProfile{};
    const bool canIdentifyMedia =
        activeProfile.isValid() && activeProfile.isAdmin &&
        MediaIdentifyUtils::canIdentify(m_item);
    const bool canEditMetadata =
        activeProfile.isValid() && activeProfile.isAdmin &&
        !m_item.id.trimmed().isEmpty() && m_item.type != "Playlist" &&
        m_item.type != "Person";
    const bool canEditImages =
        activeProfile.isValid() && activeProfile.isAdmin &&
        !m_item.id.trimmed().isEmpty() && m_item.type != "Playlist" &&
        m_item.type != "Person";
    const bool canRefreshMetadata =
        activeProfile.isValid() && activeProfile.isAdmin &&
        !m_item.id.trimmed().isEmpty() && m_item.type != "Playlist" &&
        m_item.type != "Person";
    const bool canRemoveMedia =
        activeProfile.isValid() && activeProfile.isAdmin &&
        !m_item.id.trimmed().isEmpty() && m_item.type != "Playlist" &&
        m_item.type != "Person";
    const bool canDownloadMedia =
        activeProfile.isValid() && activeProfile.canDownloadMedia &&
        !m_item.id.trimmed().isEmpty() && m_item.canDownload;

    if (m_item.type == "Playlist") {
        maybeAdd(CardContextMenuAction::DeletePlaylist,
                      adaptiveIcon("remove.svg"), tr("Delete Playlist"));
        return;
    }

    
    bool isPlayable = (m_item.mediaType == "Video" || 
                       m_item.type == "Movie" || 
                       m_item.type == "Episode" || 
                       m_item.type == "MusicVideo");
    
    if (isPlayable) {
        maybeAdd(CardContextMenuAction::Play, adaptiveIcon("play.svg"),
                      tr("Play"));

        
        bool extEnabled = ConfigStore::instance()->get<bool>(ConfigKeys::ExtPlayerEnable, false);
        if (extEnabled) {
            QString currentPath = ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerPath);
            QList<DetectedPlayer> allPlayers = ExternalPlayerDetector::loadFromConfig();

            
            QSet<QString> knownPaths;
            for (const auto &p : allPlayers)
                knownPaths.insert(p.path);
            if (!currentPath.isEmpty() && currentPath != "custom" &&
                !knownPaths.contains(currentPath) && QFileInfo::exists(currentPath)) {
                allPlayers.prepend({QFileInfo(currentPath).baseName(), currentPath});
            }

            if (!allPlayers.isEmpty()) {
                
                QString activePlayerPath = currentPath;
                if (activePlayerPath.isEmpty() || activePlayerPath == "custom") {
                    activePlayerPath = allPlayers.first().path;
                }

                
                QString playerName;
                for (const auto &p : allPlayers) {
                    if (p.path == activePlayerPath) {
                        playerName = p.name;
                        break;
                    }
                }
                if (playerName.isEmpty())
                    playerName = QFileInfo(activePlayerPath).baseName();

                maybeAdd(CardContextMenuAction::ExternalPlay,
                              adaptiveIcon("external-player.svg"),
                              tr("Play with %1").arg(playerName),
                              activePlayerPath);
            }
        }
    }

    
    maybeAdd(CardContextMenuAction::ViewDetails, adaptiveIcon("info.svg"),
                  tr("View Details"));

    if (canDownloadMedia) {
        maybeAdd(CardContextMenuAction::Download,
                      adaptiveIcon("download-menu.svg"), tr("Download"));
    }

    if (PlaylistUtils::canAddItemToPlaylist(m_item)) {
        maybeAdd(CardContextMenuAction::AddToPlaylist,
                      adaptiveIcon("playlist-add.svg"),
                      tr("Add to Playlist"));
    }

    if (!m_item.playlistId.trimmed().isEmpty() &&
        !m_item.playlistItemId.trimmed().isEmpty()) {
        maybeAdd(CardContextMenuAction::RemoveFromPlaylist,
                      adaptiveIcon("remove.svg"),
                      tr("Remove from Playlist"));
    }

    addSeparator();

    
    QString favText = m_item.isFavorite() ? tr("Remove from Favorites") : tr("Add to Favorites");
    QString favIconName = m_item.isFavorite() ? "heart-fill.svg" : "heart-outline.svg";

    maybeAdd(CardContextMenuAction::ToggleFavorite,
                  adaptiveIcon(favIconName), favText);

    
    
    
    
    
    QString playedText = m_item.userData.played ? tr("Mark as Unplayed") : tr("Mark as Played");
    QString playedIconName = m_item.userData.played ? "played.svg" : "unplayed.svg";

    maybeAdd(m_item.userData.played ? CardContextMenuAction::MarkUnplayed
                                         : CardContextMenuAction::MarkPlayed,
                  adaptiveIcon(playedIconName), playedText);

    
    
    const bool canRemoveFromResume =
        MediaItemUtils::canRemoveFromResume(m_item);
    
    
    bool isJellyfin = false;
    if (m_core && m_core->serverManager() && m_core->serverManager()->activeProfile().isValid()) {
        isJellyfin = (m_core->serverManager()->activeProfile().type == ServerProfile::Jellyfin);
    }
    
    
    if (canRemoveFromResume && !isJellyfin) {
        maybeAdd(CardContextMenuAction::RemoveFromResume,
                      adaptiveIcon("remove.svg"),
                      tr("Remove from Continue Watching"));
    }

    if (canEditMetadata || canEditImages || canRefreshMetadata || canIdentifyMedia ||
        canRemoveMedia) {
        addSeparator();
        if (canEditMetadata) {
            maybeAdd(CardContextMenuAction::EditMetadata,
                          adaptiveIcon("edit.svg"),
                          tr("Edit Metadata"));
        }
        if (canEditImages) {
            maybeAdd(CardContextMenuAction::EditImages,
                          adaptiveIcon("edit-images.svg"),
                          tr("Edit Images"));
        }
        if (canRefreshMetadata) {
            maybeAdd(CardContextMenuAction::RefreshMetadata,
                          adaptiveIcon("refresh.svg"),
                          tr("Refresh Metadata"));
        }
        if (canIdentifyMedia) {
            maybeAdd(CardContextMenuAction::Identify,
                          adaptiveIcon("search.svg"), tr("Identify"));
        }
    }

    if (canRemoveMedia) {
        maybeAdd(CardContextMenuAction::RemoveMedia,
                      adaptiveIcon("remove.svg"), tr("Remove Media"));
    }
}
