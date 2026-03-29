#include "mediaactionmenu.h"
#include "../managers/thememanager.h" 
#include "../managers/externalplayerdetector.h" 
#include <qembycore.h>
#include <services/manager/servermanager.h> 
#include <models/profile/serverprofile.h>
#include <QAction>
#include <QIcon>
#include <QFile>
#include <QFileInfo>
#include <config/config_keys.h>
#include <config/configstore.h>


MediaActionMenu::MediaActionMenu(const MediaItem& item, QEmbyCore* core, QWidget *parent)
    : QMenu(parent), m_item(item), m_core(core)
{
    setObjectName("media-action-menu"); 
    
    
    
    
    
    setWindowFlags(windowFlags() | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);
    
    
    setAttribute(Qt::WA_TranslucentBackground);

    setupMenu();
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

    
    bool isPlayable = (m_item.mediaType == "Video" || 
                       m_item.type == "Movie" || 
                       m_item.type == "Episode" || 
                       m_item.type == "MusicVideo");
    
    if (isPlayable) {
        auto* playAction = new QAction(adaptiveIcon("play.svg"), tr("Play"), this);
        connect(playAction, &QAction::triggered, this, &MediaActionMenu::playRequested);
        addAction(playAction);

        
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

                auto* extPlayAction = new QAction(
                    adaptiveIcon("external-player.svg"),
                    tr("Play with %1").arg(playerName), this);
                connect(extPlayAction, &QAction::triggered, this, [this, activePlayerPath]() {
                    emit externalPlayRequested(activePlayerPath);
                });
                addAction(extPlayAction);
            }
        }
    }

    
    auto* detailAction = new QAction(adaptiveIcon("info.svg"), tr("View Details"), this);
    connect(detailAction, &QAction::triggered, this, &MediaActionMenu::detailRequested);
    addAction(detailAction);

    addSeparator();

    
    QString favText = m_item.isFavorite() ? tr("Remove from Favorites") : tr("Add to Favorites");
    QString favIconName = m_item.isFavorite() ? "heart-fill.svg" : "heart-outline.svg";
    
    auto* favAction = new QAction(adaptiveIcon(favIconName), favText, this);
    connect(favAction, &QAction::triggered, this, &MediaActionMenu::favoriteRequested);
    addAction(favAction);

    
    
    
    
    
    QString playedText = m_item.userData.played ? tr("Mark as Unplayed") : tr("Mark as Played");
    QString playedIconName = m_item.userData.played ? "played.svg" : "unplayed.svg";
    
    auto* playedAction = new QAction(adaptiveIcon(playedIconName), playedText, this);
    connect(playedAction, &QAction::triggered, this, [this]() {
        if (m_item.userData.played) {
            emit markUnplayedRequested();
        } else {
            emit markPlayedRequested();
        }
    });
    addAction(playedAction);

    
    bool hasProgress = (m_item.userData.playbackPositionTicks > 0) || (m_item.userData.playedPercentage > 0.0);
    
    
    bool isJellyfin = false;
    if (m_core && m_core->serverManager() && m_core->serverManager()->activeProfile().isValid()) {
        isJellyfin = (m_core->serverManager()->activeProfile().type == ServerProfile::Jellyfin);
    }
    
    
    if (hasProgress && !isJellyfin) {
        auto* removeResumeAction = new QAction(adaptiveIcon("remove.svg"), tr("Remove from Continue Watching"), this);
        connect(removeResumeAction, &QAction::triggered, this, &MediaActionMenu::removeFromResumeRequested);
        addAction(removeResumeAction);
    }
}
