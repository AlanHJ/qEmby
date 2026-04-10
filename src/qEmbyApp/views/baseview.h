#ifndef BASEVIEW_H
#define BASEVIEW_H

#include <QWidget>
#include <QString>
#include <QVariant>
#include <QPoint>
#include <QPointer> 
#include <qcorotask.h>
#include "../components/cardcontextmenurequest.h"
#include <models/media/mediaitem.h> 

class QEmbyCore;

class BaseView : public QWidget
{
    Q_OBJECT
public:
    explicit BaseView(QEmbyCore* core, QWidget *parent = nullptr);
    virtual ~BaseView() = default;

    
    virtual bool handleBackNavigation() { return false; }

    
    virtual void prepareForStackLeave() {}

public Q_SLOTS:
    virtual void scrollToTop() {}

    
    
    
    virtual void handlePlayRequested(const MediaItem& item);
    virtual void handleFavoriteRequested(const MediaItem& item);
    virtual void handleMoreMenuRequested(const MediaItem& item, const QPoint& globalPos);
    virtual void handleAddToPlaylistRequested(const MediaItem& item);
    virtual void handleRemoveFromPlaylistRequested(const MediaItem& item);
    virtual void handleDeletePlaylistRequested(const MediaItem& item);
    virtual void handleEditMetadataRequested(const MediaItem& item);
    virtual void handleEditImagesRequested(const MediaItem& item);
    virtual void handleIdentifyRequested(const MediaItem& item);
    virtual void handleRefreshMetadataRequested(const MediaItem& item);
    virtual void handleDownloadRequested(const MediaItem& item);
    virtual void handleRemoveMediaRequested(const MediaItem& item);

    
    virtual void handleMarkPlayedRequested(const MediaItem& item);
    virtual void handleMarkUnplayedRequested(const MediaItem& item);
    virtual void handleRemoveFromResumeRequested(const MediaItem& item);

Q_SIGNALS:
    void navigateToDetail(const QString& itemId, const QString& itemName);
    void navigateToFolder(const QString& folderId, const QString& folderName);
    void navigateToPerson(const QString& personId, const QString& personName);
    void triggerSearch(const QString& query);
    void navigateToFilteredView(const QString& filterType, const QString& filterValue);
    void navigateBack();
    void navigateToCategory(const QString& categoryId, const QString& title);

    void navigateToPlayer(const QString& mediaId, const QString& title, const QString& streamUrl = QString(), long long startPositionTicks = 0, const QVariant& extraData = QVariant());
    
    
    
    
    void navigateToSeason(const QString& seriesId, const QString& seasonId, const QString& seasonName);

protected:
    
    virtual CardContextMenuRequest showCardContextMenu(const MediaItem& item,
                                                       const QPoint& globalPos);
    virtual void dispatchCardContextMenuRequest(
        const MediaItem& item, const CardContextMenuRequest& request);

    
    
    
    
    
    virtual void onMediaItemUpdated(const MediaItem& item) {}

    
    virtual void onMediaItemRemoved(const QString& itemId) {}

    
    QCoro::Task<void> refreshAndBroadcastItem(const QString& itemId);

    QEmbyCore* m_core;

private:
    void launchTask(QCoro::Task<void>&& task);

    QCoro::Task<void> executeToggleFavorite(MediaItem item);
    QCoro::Task<void> executePlay(MediaItem item);
    QCoro::Task<void> executeExternalPlay(MediaItem item, QString playerPath);
    QCoro::Task<void> executeMarkPlayed(MediaItem item);
    QCoro::Task<void> executeMarkUnplayed(MediaItem item);
    QCoro::Task<void> executeRemoveFromResume(MediaItem item);
    QCoro::Task<void> executeAddToPlaylist(MediaItem item, QString playlistId,
                                           QString playlistName);
    QCoro::Task<void> executeRemoveFromPlaylist(MediaItem item);
    QCoro::Task<void> executeDeletePlaylist(MediaItem item);
    QCoro::Task<void> executeEditMetadata(MediaItem item);
    QCoro::Task<void> executeServerRefreshAndReloadItem(
        QString itemId, QString itemName, bool triggerServerRefresh,
        bool replaceAllMetadata = false, bool replaceAllImages = false);
    QCoro::Task<void> executeRemoveMedia(MediaItem item);
    QCoro::Task<MediaItem> resolvePlaybackItem(MediaItem item);
    bool isJellyfinServer() const;
};

#endif 
