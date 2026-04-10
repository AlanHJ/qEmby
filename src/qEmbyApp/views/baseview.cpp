#include "baseview.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/media/mediaservice.h>
#include <services/manager/servermanager.h> 
#include <models/profile/serverprofile.h>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <models/media/playerlaunchcontext.h>
#include "../components/addtoplaylistdialog.h"
#include "../components/librarymetadataeditdialog.h"
#include "../components/mediaidentifydialog.h"
#include "../components/mediaimageeditdialog.h"
#include "../components/mediaactionmenu.h"
#include "../components/downloadmanagerdialog.h"
#include "../components/modernmessagebox.h"
#include "../components/moderntoast.h" 
#include "../managers/downloadmanager.h"
#include "../managers/playbackmanager.h" 
#include "../utils/mediaitemutils.h"
#include "../utils/mediasourcepreferenceutils.h"
#include "../utils/mediaidentifyutils.h"
#include "../utils/playerpreferenceutils.h"
#include "../utils/playlistutils.h"
#include <QDialog>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <QPointer> 
#include <QTimer>
#include <utility>

namespace {

int resolvePreferredMediaSourceIndex(const QList<MediaSourceInfo>& mediaSources)
{
    return MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
        mediaSources,
        ConfigStore::instance()
            ->get<QString>(ConfigKeys::PlayerPreferredVersion)
            .trimmed());
}

} 

BaseView::BaseView(QEmbyCore* core, QWidget *parent)
    : QWidget(parent), m_core(core)
{
    setAttribute(Qt::WA_StyledBackground, true);
    setProperty("showGlobalSearch", true);
}

void BaseView::launchTask(QCoro::Task<void>&& task)
{
    QCoro::connect(std::move(task), this, []() {});
}

bool BaseView::isJellyfinServer() const
{
    if (!m_core || !m_core->serverManager()) {
        return false;
    }

    const ServerProfile profile = m_core->serverManager()->activeProfile();
    return profile.isValid() && profile.type == ServerProfile::Jellyfin;
}

QCoro::Task<MediaItem> BaseView::resolvePlaybackItem(MediaItem item)
{
    if (!m_core || !m_core->mediaService()) {
        co_return {};
    }

    QPointer<BaseView> safeThis(this);
    MediaItem detail = item;

    if (detail.type == "Series") {
        QList<MediaItem> nextUpList =
            co_await m_core->mediaService()->getNextUp(detail.id);
        if (!safeThis) {
            co_return {};
        }

        if (!nextUpList.isEmpty()) {
            detail = nextUpList.first();
        } else {
            QList<MediaItem> seasons =
                co_await m_core->mediaService()->getSeasons(detail.id);
            if (!safeThis) {
                co_return {};
            }

            if (!seasons.isEmpty()) {
                QList<MediaItem> episodes =
                    co_await m_core->mediaService()->getEpisodes(
                        detail.id, seasons.first().id);
                if (!safeThis) {
                    co_return {};
                }

                if (!episodes.isEmpty()) {
                    detail = episodes.first();
                } else {
                    qDebug() << "Series has no episodes, cannot play.";
                    co_return {};
                }
            } else {
                qDebug() << "Series has no seasons, cannot play.";
                co_return {};
            }
        }
    }

    if (detail.mediaSources.isEmpty()) {
        detail = co_await m_core->mediaService()->getItemDetail(detail.id);
        if (!safeThis) {
            co_return {};
        }
    }

    co_return detail;
}

QCoro::Task<void> BaseView::executeToggleFavorite(MediaItem item)
{
    if (!m_core || !m_core->mediaService()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    try {
        const bool targetState = !item.isFavorite();
        const bool actualState =
            co_await m_core->mediaService()->toggleFavorite(item.id, targetState);

        if (!safeThis) {
            co_return;
        }

        item.userData.isFavorite = actualState;
        safeThis->onMediaItemUpdated(item);
        ModernToast::showMessage(actualState ? tr("Added to Favorites")
                                             : tr("Removed from Favorites"));
    } catch (const std::exception& e) {
        qDebug() << "BaseView toggle favorite failed:" << e.what();
        ModernToast::showMessage(tr("Failed to update favorite status"));
    }
}

QCoro::Task<void> BaseView::executePlay(MediaItem item)
{
    if (!m_core || !m_core->mediaService()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    try {
        MediaItem detail = co_await resolvePlaybackItem(item);
        if (!safeThis || detail.id.isEmpty()) {
            co_return;
        }

        const QString playTitle = MediaItemUtils::playbackTitle(detail);
        QString mediaSourceId = detail.id;
        QVariant sourceInfoVar;
        QString streamUrl;

        if (!detail.mediaSources.isEmpty()) {
            const int bestSourceIdx =
                resolvePreferredMediaSourceIndex(detail.mediaSources);
            MediaSourceInfo selectedSource = detail.mediaSources[bestSourceIdx];
            PlayerPreferenceUtils::applyPreferredStreamRules(
                selectedSource,
                ConfigStore::instance()->get<QString>(
                    ConfigKeys::PlayerAudioLang, "auto"),
                ConfigStore::instance()->get<QString>(
                    ConfigKeys::PlayerSubLang, "auto"));

            mediaSourceId = selectedSource.id;
            PlayerLaunchContext launchContext;
            launchContext.mediaItem = detail;
            launchContext.selectedSource = selectedSource;
            sourceInfoVar = QVariant::fromValue(launchContext);
            streamUrl =
                m_core->mediaService()->getStreamUrl(detail.id, selectedSource);
        } else {
            streamUrl =
                m_core->mediaService()->getStreamUrl(detail.id, mediaSourceId);
        }

        const long long ticks = detail.userData.playbackPositionTicks;
        Q_EMIT navigateToPlayer(detail.id, playTitle, streamUrl, ticks,
                                sourceInfoVar);
    } catch (const std::exception& e) {
        qDebug() << "BaseView play task failed:" << e.what();
    }
}

QCoro::Task<void> BaseView::executeExternalPlay(MediaItem item,
                                                QString playerPath)
{
    if (!m_core || !m_core->mediaService() || playerPath.trimmed().isEmpty()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    try {
        MediaItem detail = co_await resolvePlaybackItem(item);
        if (!safeThis || detail.id.isEmpty() || detail.mediaSources.isEmpty()) {
            co_return;
        }

        const int bestSourceIdx =
            resolvePreferredMediaSourceIndex(detail.mediaSources);
        MediaSourceInfo selectedSource = detail.mediaSources[bestSourceIdx];
        PlayerPreferenceUtils::applyPreferredStreamRules(
            selectedSource,
            ConfigStore::instance()->get<QString>(
                ConfigKeys::PlayerAudioLang, "auto"),
            ConfigStore::instance()->get<QString>(
                ConfigKeys::PlayerSubLang, "auto"));
        const QString streamUrl =
            m_core->mediaService()->getStreamUrl(detail.id, selectedSource);
        const long long ticks = detail.userData.playbackPositionTicks;

        PlaybackManager::instance()->startExternalPlayback(
            playerPath, detail.id, MediaItemUtils::playbackTitle(detail), streamUrl, ticks,
            QVariant::fromValue(selectedSource));
    } catch (const std::exception& e) {
        qDebug() << "BaseView external play task failed:" << e.what();
    }
}

QCoro::Task<void> BaseView::executeMarkPlayed(MediaItem item)
{
    if (!m_core || !m_core->mediaService()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    try {
        co_await m_core->mediaService()->markAsPlayed(item.id);

        if (!isJellyfinServer()) {
            co_await m_core->mediaService()->removeFromResume(item.id);
        }

        if (!safeThis) {
            co_return;
        }

        item.userData.played = true;
        item.userData.playbackPositionTicks = 0;
        item.userData.playedPercentage = 100.0;
        safeThis->onMediaItemUpdated(item);

        co_await safeThis->refreshAndBroadcastItem(item.id);
        ModernToast::showMessage(tr("Marked as Played"));
    } catch (const std::exception& e) {
        qDebug() << "BaseView mark as played failed:" << e.what();
        ModernToast::showMessage(tr("Operation failed"));
    }
}

QCoro::Task<void> BaseView::executeMarkUnplayed(MediaItem item)
{
    if (!m_core || !m_core->mediaService()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    try {
        co_await m_core->mediaService()->markAsUnplayed(item.id);
        if (!safeThis) {
            co_return;
        }

        item.userData.played = false;
        item.userData.playbackPositionTicks = 0;
        item.userData.playedPercentage = 0.0;
        safeThis->onMediaItemUpdated(item);

        co_await safeThis->refreshAndBroadcastItem(item.id);
        ModernToast::showMessage(tr("Marked as Unplayed"));
    } catch (const std::exception& e) {
        qDebug() << "BaseView mark as unplayed failed:" << e.what();
        ModernToast::showMessage(tr("Operation failed"));
    }
}

QCoro::Task<void> BaseView::executeRemoveFromResume(MediaItem item)
{
    if (!m_core || !m_core->mediaService()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    try {
        co_await m_core->mediaService()->removeFromResume(item.id);
        if (!safeThis) {
            co_return;
        }

        item.userData.playbackPositionTicks = 0;
        item.userData.playedPercentage = 0.0;
        safeThis->onMediaItemUpdated(item);

        co_await safeThis->refreshAndBroadcastItem(item.id);
        ModernToast::showMessage(tr("Removed from Continue Watching"));
    } catch (const std::exception& e) {
        qDebug() << "BaseView remove from resume failed:" << e.what();
        ModernToast::showMessage(tr("Operation failed"));
    }
}

QCoro::Task<void> BaseView::executeAddToPlaylist(MediaItem item,
                                                 QString playlistId,
                                                 QString playlistName)
{
    if (!m_core || !m_core->adminService() || item.id.trimmed().isEmpty() ||
        playlistId.trimmed().isEmpty()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    qDebug() << "[BaseView] Adding item to playlist"
             << "| itemId=" << item.id
             << "| itemName=" << item.name
             << "| sourcePlaylistId=" << item.playlistId
             << "| playlistId=" << playlistId
             << "| playlistName=" << playlistName;

    try {
        bool isDuplicateAdd =
            !item.playlistId.trimmed().isEmpty() &&
            item.playlistId.compare(playlistId, Qt::CaseInsensitive) == 0;

        if (!isDuplicateAdd) {
            try {
                const QJsonObject playlistItemsResponse =
                    co_await m_core->adminService()->getPlaylistItems(playlistId);
                if (!safeThis) {
                    co_return;
                }

                const QJsonArray playlistItems =
                    playlistItemsResponse.value("Items").toArray();
                for (const QJsonValue& value : playlistItems) {
                    const QString existingItemId =
                        value.toObject().value("Id").toString().trimmed();
                    if (existingItemId.compare(item.id, Qt::CaseInsensitive) ==
                        0) {
                        isDuplicateAdd = true;
                        break;
                    }
                }

                qDebug() << "[BaseView] Playlist duplicate check completed"
                         << "| itemId=" << item.id
                         << "| playlistId=" << playlistId
                         << "| duplicate=" << isDuplicateAdd
                         << "| playlistItemCount=" << playlistItems.size();
            } catch (const std::exception& e) {
                if (!safeThis) {
                    co_return;
                }

                qWarning() << "[BaseView] Playlist duplicate check failed"
                           << "| itemId=" << item.id
                           << "| playlistId=" << playlistId
                           << "| error=" << e.what();
            }
        }

        if (isDuplicateAdd) {
            const QString displayPlaylistName =
                playlistName.isEmpty() ? tr("current playlist") : playlistName;
            const bool confirmed = ModernMessageBox::question(
                safeThis.data(), tr("Duplicate Add"),
                tr("%1 is already in playlist %2.\n\nAdd it again?")
                    .arg(item.name, displayPlaylistName),
                tr("Add Again"), ModernMessageBox::tr("Cancel"),
                ModernMessageBox::Primary, ModernMessageBox::Question);
            if (!safeThis) {
                co_return;
            }
            if (!confirmed) {
                qDebug() << "[BaseView] Duplicate add-to-playlist cancelled"
                         << "| itemId=" << item.id
                         << "| itemName=" << item.name
                         << "| playlistId=" << playlistId
                         << "| playlistName=" << playlistName;
                co_return;
            }
        }

        co_await m_core->adminService()->addToPlaylist(playlistId, {item.id});
        if (!safeThis) {
            co_return;
        }

        qDebug() << "[BaseView] Added item to playlist"
                 << "| itemId=" << item.id
                 << "| itemName=" << item.name
                 << "| playlistId=" << playlistId
                 << "| playlistName=" << playlistName;
        ModernToast::showMessage(tr("Added to playlist \"%1\"").arg(playlistName),
                                 2000);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[BaseView] Failed to add item to playlist"
                   << "| itemId=" << item.id
                   << "| itemName=" << item.name
                   << "| playlistId=" << playlistId
                   << "| playlistName=" << playlistName
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to add to playlist: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

QCoro::Task<void> BaseView::executeRemoveFromPlaylist(MediaItem item)
{
    if (!m_core || !m_core->adminService() || item.playlistId.trimmed().isEmpty() ||
        item.playlistItemId.trimmed().isEmpty()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);

    qDebug() << "[BaseView] Removing item from playlist"
             << "| itemId=" << item.id
             << "| itemName=" << item.name
             << "| playlistId=" << item.playlistId
             << "| playlistItemId=" << item.playlistItemId;

    try {
        co_await m_core->adminService()->removeFromPlaylist(
            item.playlistId, {item.playlistItemId});
        if (!safeThis) {
            co_return;
        }

        qDebug() << "[BaseView] Removed item from playlist"
                 << "| itemId=" << item.id
                 << "| itemName=" << item.name
                 << "| playlistId=" << item.playlistId
                 << "| playlistItemId=" << item.playlistItemId;
        safeThis->onMediaItemRemoved(item.id);
        ModernToast::showMessage(tr("Removed from current playlist"), 2000);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[BaseView] Failed to remove item from playlist"
                   << "| itemId=" << item.id
                   << "| itemName=" << item.name
                   << "| playlistId=" << item.playlistId
                   << "| playlistItemId=" << item.playlistItemId
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to remove from playlist: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
    }
}

QCoro::Task<void> BaseView::executeDeletePlaylist(MediaItem item)
{
    if (!m_core || !m_core->adminService() || item.type != "Playlist" ||
        item.id.trimmed().isEmpty()) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);
    const bool confirmed = ModernMessageBox::question(
        safeThis.data(), tr("Delete Playlist"),
        tr("Are you sure you want to delete playlist \"%1\"?\n\nThis action "
           "cannot be undone.")
            .arg(item.name),
        tr("Delete"), tr("Cancel"), ModernMessageBox::Danger,
        ModernMessageBox::Warning);
    if (!safeThis || !confirmed) {
        co_return;
    }

    qDebug() << "[BaseView] Deleting playlist"
             << "| itemId=" << item.id
             << "| itemName=" << item.name;

    try {
        co_await m_core->adminService()->deleteItem(item.id);
        if (!safeThis) {
            co_return;
        }

        qDebug() << "[BaseView] Deleted playlist"
                 << "| itemId=" << item.id
                 << "| itemName=" << item.name;
        safeThis->onMediaItemRemoved(item.id);
        ModernToast::showMessage(tr("\"%1\" deleted").arg(item.name), 2000);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[BaseView] Failed to delete playlist"
                   << "| itemId=" << item.id
                   << "| itemName=" << item.name
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Delete failed: %1").arg(QString::fromUtf8(e.what())), 3000);
    }
}

QCoro::Task<void> BaseView::executeEditMetadata(MediaItem item)
{
    if (!m_core || !m_core->adminService()) {
        co_return;
    }

    if (!m_core->serverManager() ||
        !m_core->serverManager()->activeProfile().isValid() ||
        !m_core->serverManager()->activeProfile().isAdmin) {
        ModernToast::showMessage(
            tr("This action requires administrator privileges"));
        co_return;
    }

    const QString itemId = item.id.trimmed();
    if (itemId.isEmpty() || item.type == "Playlist" || item.type == "Person") {
        qDebug() << "[BaseView] Skip edit-metadata for unsupported item"
                 << "| itemId=" << item.id
                 << "| itemType=" << item.type;
        co_return;
    }

    QPointer<BaseView> safeThis(this);
    QPointer<AdminService> adminService(m_core->adminService());

    qDebug() << "[BaseView] Starting media metadata edit"
             << "| itemId=" << itemId
             << "| itemName=" << item.name
             << "| itemType=" << item.type;

    try {
        const QJsonObject itemData = co_await adminService->getItemMetadata(itemId);
        if (!safeThis) {
            co_return;
        }

        LibraryMetadataEditDialog dialog(itemData, safeThis.data());
        if (dialog.exec() != QDialog::Accepted) {
            co_return;
        }

        const QJsonObject updatedItemData = dialog.updatedItemData();
        qDebug() << "[BaseView] Saving media metadata"
                 << "| itemId=" << itemId
                 << "| originalName=" << item.name
                 << "| updatedName="
                 << updatedItemData.value(QStringLiteral("Name")).toString();
        co_await adminService->updateItemMetadata(itemId, updatedItemData);
        if (!safeThis) {
            co_return;
        }

        ModernToast::showMessage(tr("Metadata saved"), 2000);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[BaseView] Failed to edit media metadata"
                   << "| itemId=" << itemId
                   << "| itemName=" << item.name
                   << "| itemType=" << item.type
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to update metadata: %1")
                .arg(QString::fromUtf8(e.what())),
            3000);
        co_return;
    }

    if (!safeThis) {
        co_return;
    }

    co_await safeThis->refreshAndBroadcastItem(itemId);
}

QCoro::Task<void> BaseView::executeServerRefreshAndReloadItem(
    QString itemId, QString itemName, bool triggerServerRefresh,
    bool replaceAllMetadata, bool replaceAllImages)
{
    itemId = itemId.trimmed();
    itemName = itemName.trimmed();
    if (itemId.isEmpty() || !m_core) {
        co_return;
    }

    QPointer<BaseView> safeThis(this);
    QPointer<AdminService> adminService(m_core->adminService());
    QPointer<MediaService> mediaService(m_core->mediaService());

    if (triggerServerRefresh && adminService) {
        try {
            qDebug() << "[BaseView] Trigger server refresh for item"
                     << "| itemId=" << itemId
                     << "| itemName=" << itemName
                     << "| replaceAllMetadata=" << replaceAllMetadata
                     << "| replaceAllImages=" << replaceAllImages;
            co_await adminService->refreshItemMetadata(
                itemId, replaceAllMetadata, replaceAllImages);
        } catch (const std::exception& e) {
            qWarning() << "[BaseView] Server refresh failed"
                       << "| itemId=" << itemId
                       << "| error=" << e.what();
            if (safeThis) {
                ModernToast::showMessage(
                    tr("Metadata refresh failed: %1")
                        .arg(QString::fromUtf8(e.what())),
                    3000);
            }
        }
    }

    if (!safeThis || !mediaService) {
        co_return;
    }

    co_await safeThis->refreshAndBroadcastItem(itemId);
}

QCoro::Task<void> BaseView::executeRemoveMedia(MediaItem item)
{
    if (!m_core || !m_core->adminService() || item.id.trimmed().isEmpty() ||
        item.type == "Playlist") {
        co_return;
    }

    if (!m_core->serverManager() ||
        !m_core->serverManager()->activeProfile().isValid() ||
        !m_core->serverManager()->activeProfile().isAdmin) {
        ModernToast::showMessage(
            tr("This action requires administrator privileges"));
        co_return;
    }

    QPointer<BaseView> safeThis(this);
    const QString itemName =
        item.name.trimmed().isEmpty() ? tr("this item") : item.name.trimmed();
    const bool confirmed = ModernMessageBox::question(
        safeThis.data(), tr("Remove Media"),
        tr("Are you sure you want to remove \"%1\" from the server?\n\nThis "
           "action cannot be undone.")
            .arg(itemName),
        tr("Remove"), tr("Cancel"), ModernMessageBox::Danger,
        ModernMessageBox::Warning);
    if (!safeThis || !confirmed) {
        co_return;
    }

    qDebug() << "[BaseView] Removing media from server"
             << "| itemId=" << item.id
             << "| itemName=" << item.name
             << "| itemType=" << item.type
             << "| mediaType=" << item.mediaType;

    try {
        co_await m_core->adminService()->deleteItem(item.id);
        if (!safeThis) {
            co_return;
        }

        qDebug() << "[BaseView] Removed media from server"
                 << "| itemId=" << item.id
                 << "| itemName=" << item.name
                 << "| itemType=" << item.type;
        if (m_core->mediaService()) {
            m_core->mediaService()->removeRecommendCacheItem(item.id);
        }
        safeThis->onMediaItemRemoved(item.id);
        ModernToast::showMessage(tr("\"%1\" removed").arg(itemName), 2000);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[BaseView] Failed to remove media from server"
                   << "| itemId=" << item.id
                   << "| itemName=" << item.name
                   << "| itemType=" << item.type
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to remove media: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

void BaseView::handlePlayRequested(const MediaItem& item)
{
    launchTask(executePlay(item));
}

void BaseView::handleFavoriteRequested(const MediaItem& item)
{
    launchTask(executeToggleFavorite(item));
}

void BaseView::handleAddToPlaylistRequested(const MediaItem& item)
{
    if (!PlaylistUtils::canAddItemToPlaylist(item)) {
        qDebug() << "[BaseView] Skip add-to-playlist for unsupported item"
                 << "| itemId=" << item.id
                 << "| itemType=" << item.type
                 << "| mediaType=" << item.mediaType;
        return;
    }

    if (!m_core) {
        return;
    }

    AddToPlaylistDialog dialog(m_core,
                               PlaylistUtils::inferPlaylistMediaType(item),
                               this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString playlistId = dialog.selectedPlaylistId().trimmed();
    const QString playlistName = dialog.selectedPlaylistName().trimmed();
    if (playlistId.isEmpty()) {
        return;
    }

    qDebug() << "[BaseView] Add-to-playlist confirmed"
             << "| itemId=" << item.id
             << "| itemName=" << item.name
             << "| playlistId=" << playlistId
             << "| playlistName=" << playlistName;
    launchTask(executeAddToPlaylist(item, playlistId, playlistName));
}


void BaseView::handleMarkPlayedRequested(const MediaItem& item)
{
    launchTask(executeMarkPlayed(item));
}

void BaseView::handleRemoveFromPlaylistRequested(const MediaItem& item)
{
    launchTask(executeRemoveFromPlaylist(item));
}

void BaseView::handleDeletePlaylistRequested(const MediaItem& item)
{
    if (item.type != "Playlist") {
        qDebug() << "[BaseView] Skip delete-playlist for non-playlist item"
                 << "| itemId=" << item.id
                 << "| itemType=" << item.type;
        return;
    }

    launchTask(executeDeletePlaylist(item));
}

void BaseView::handleEditMetadataRequested(const MediaItem& item)
{
    launchTask(executeEditMetadata(item));
}

void BaseView::handleEditImagesRequested(const MediaItem& item)
{
    if (!m_core || !m_core->adminService()) {
        return;
    }

    if (!m_core->serverManager() ||
        !m_core->serverManager()->activeProfile().isValid() ||
        !m_core->serverManager()->activeProfile().isAdmin) {
        ModernToast::showMessage(
            tr("This action requires administrator privileges"));
        return;
    }

    const QString itemId = item.id.trimmed();
    if (itemId.isEmpty() || item.type == "Playlist" || item.type == "Person") {
        qDebug() << "[BaseView] Skip edit-images for unsupported item"
                 << "| itemId=" << item.id
                 << "| itemType=" << item.type;
        return;
    }

    MediaImageEditTarget target;
    target.itemId = itemId;
    target.imageItemId =
        item.images.primaryImageItemId.isEmpty() ? itemId
                                                 : item.images.primaryImageItemId;
    target.displayName = item.name;
    target.itemType = item.type;
    target.mediaType = item.mediaType;
    target.collectionType = item.collectionType;

    MediaImageEditDialog dialog(m_core, target, this);
    dialog.exec();
    if (!dialog.hasChanges()) {
        return;
    }

    launchTask(executeServerRefreshAndReloadItem(itemId, item.name, false));
    QTimer::singleShot(1500, this, [this, itemId]() {
        if (itemId.isEmpty()) {
            return;
        }

        qDebug() << "[BaseView] Running delayed image refresh reload"
                 << "| itemId=" << itemId;
        launchTask(executeServerRefreshAndReloadItem(itemId, QString(), false));
    });
}

void BaseView::handleIdentifyRequested(const MediaItem& item)
{
    if (!m_core || !m_core->adminService()) {
        return;
    }

    if (!m_core->serverManager() ||
        !m_core->serverManager()->activeProfile().isValid() ||
        !m_core->serverManager()->activeProfile().isAdmin) {
        ModernToast::showMessage(
            tr("This action requires administrator privileges"));
        return;
    }

    if (!MediaIdentifyUtils::canIdentify(item)) {
        qDebug() << "[BaseView] Skip identify for unsupported item"
                 << "| itemId=" << item.id
                 << "| itemType=" << item.type;
        ModernToast::showMessage(
            tr("Identify is not supported for this item type"));
        return;
    }

    MediaIdentifyDialog dialog(m_core, item, this);
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    const QString itemId = item.id.trimmed();
    if (itemId.isEmpty()) {
        return;
    }

    launchTask(executeServerRefreshAndReloadItem(itemId, item.name, true, true,
                                                 true));

    QTimer::singleShot(2000, this, [this, itemId]() {
        if (itemId.isEmpty()) {
            return;
        }

        qDebug() << "[BaseView] Running delayed identify refresh"
                 << "| itemId=" << itemId;
        launchTask(executeServerRefreshAndReloadItem(itemId, QString(), false));
    });
}

void BaseView::handleRefreshMetadataRequested(const MediaItem& item)
{
    if (!m_core || !m_core->adminService()) {
        return;
    }

    if (!m_core->serverManager() ||
        !m_core->serverManager()->activeProfile().isValid() ||
        !m_core->serverManager()->activeProfile().isAdmin) {
        ModernToast::showMessage(
            tr("This action requires administrator privileges"));
        return;
    }

    const QString itemId = item.id.trimmed();
    if (itemId.isEmpty()) {
        qDebug() << "[BaseView] Skip refresh-metadata for empty item id"
                 << "| itemName=" << item.name
                 << "| itemType=" << item.type;
        return;
    }

    launchTask(executeServerRefreshAndReloadItem(itemId, item.name, true, true,
                                                 true));
    ModernToast::showMessage(
        tr("Refreshing metadata for \"%1\"...")
            .arg(item.name.trimmed().isEmpty() ? tr("this item") : item.name));

    QTimer::singleShot(2000, this, [this, itemId]() {
        if (itemId.isEmpty()) {
            return;
        }

        qDebug() << "[BaseView] Running delayed metadata refresh reload"
                 << "| itemId=" << itemId;
        launchTask(executeServerRefreshAndReloadItem(itemId, QString(), false));
    });
}

void BaseView::handleDownloadRequested(const MediaItem& item)
{
    if (!m_core) {
        return;
    }

    DownloadManager::instance()->init(m_core);
    DownloadManager::instance()->enqueueDownload(item);
    DownloadManagerDialog::showManager(m_core, this);
}

void BaseView::handleRemoveMediaRequested(const MediaItem& item)
{
    if (item.type == "Playlist") {
        qDebug() << "[BaseView] Skip remove-media for playlist item"
                 << "| itemId=" << item.id
                 << "| itemType=" << item.type;
        return;
    }

    launchTask(executeRemoveMedia(item));
}

void BaseView::handleMarkUnplayedRequested(const MediaItem& item)
{
    launchTask(executeMarkUnplayed(item));
}

void BaseView::handleRemoveFromResumeRequested(const MediaItem& item)
{
    launchTask(executeRemoveFromResume(item));
}

void BaseView::handleMoreMenuRequested(const MediaItem& item, const QPoint& globalPos)
{
    const CardContextMenuRequest request = showCardContextMenu(item, globalPos);
    dispatchCardContextMenuRequest(item, request);
}

CardContextMenuRequest BaseView::showCardContextMenu(const MediaItem& item,
                                                     const QPoint& globalPos)
{
    MediaActionMenu menu(item, m_core, this);
    return menu.execRequest(globalPos);
}

void BaseView::dispatchCardContextMenuRequest(
    const MediaItem& item, const CardContextMenuRequest& request)
{
    if (!request.isValid()) {
        return;
    }

    switch (request.action) {
    case CardContextMenuAction::None:
        return;
    case CardContextMenuAction::Play:
        handlePlayRequested(item);
        return;
    case CardContextMenuAction::ExternalPlay:
        if (request.stringValue.trimmed().isEmpty()) {
            qDebug() << "[BaseView] Skip external-play request without player path"
                     << "| itemId=" << item.id
                     << "| itemName=" << item.name;
            return;
        }
        launchTask(executeExternalPlay(item, request.stringValue));
        return;
    case CardContextMenuAction::ViewDetails:
        Q_EMIT navigateToDetail(item.id, item.name);
        return;
    case CardContextMenuAction::Download:
        handleDownloadRequested(item);
        return;
    case CardContextMenuAction::EditMetadata:
        handleEditMetadataRequested(item);
        return;
    case CardContextMenuAction::EditImages:
        handleEditImagesRequested(item);
        return;
    case CardContextMenuAction::Identify:
        handleIdentifyRequested(item);
        return;
    case CardContextMenuAction::RefreshMetadata:
        handleRefreshMetadataRequested(item);
        return;
    case CardContextMenuAction::ScanLibraryFiles:
        return;
    case CardContextMenuAction::ToggleFavorite:
        handleFavoriteRequested(item);
        return;
    case CardContextMenuAction::AddToPlaylist:
        handleAddToPlaylistRequested(item);
        return;
    case CardContextMenuAction::RemoveFromPlaylist:
        handleRemoveFromPlaylistRequested(item);
        return;
    case CardContextMenuAction::DeletePlaylist:
        handleDeletePlaylistRequested(item);
        return;
    case CardContextMenuAction::RemoveMedia:
        handleRemoveMediaRequested(item);
        return;
    case CardContextMenuAction::MarkPlayed:
        handleMarkPlayedRequested(item);
        return;
    case CardContextMenuAction::MarkUnplayed:
        handleMarkUnplayedRequested(item);
        return;
    case CardContextMenuAction::RemoveFromResume:
        handleRemoveFromResumeRequested(item);
        return;
    }
}




QCoro::Task<void> BaseView::refreshAndBroadcastItem(const QString& itemId)
{
    if (itemId.isEmpty() || !m_core || !m_core->mediaService()) {
        co_return;
    }

    
    QPointer<BaseView> safeThis(this);

    try {
        
        MediaItem latestItem = co_await m_core->mediaService()->getItemDetail(itemId);
        
        
        if (safeThis) {
            safeThis->onMediaItemUpdated(latestItem);
        }
    } catch (const std::exception& e) {
        qDebug() << "BaseView::refreshAndBroadcastItem failed for ID:" << itemId << "Error:" << e.what();
    }
}
