#ifndef BASEVIEW_H
#define BASEVIEW_H

#include <QWidget>
#include <QString>
#include <QVariant>
#include <QPoint>
#include <QPointer> 
#include <qcorotask.h>
#include <models/media/mediaitem.h> 

class QEmbyCore;

class BaseView : public QWidget
{
    Q_OBJECT
public:
    explicit BaseView(QEmbyCore* core, QWidget *parent = nullptr);
    virtual ~BaseView() = default;

public Q_SLOTS:
    virtual void scrollToTop() {}

    
    
    
    virtual void handlePlayRequested(const MediaItem& item);
    virtual void handleFavoriteRequested(const MediaItem& item);
    virtual void handleMoreMenuRequested(const MediaItem& item, const QPoint& globalPos);

    
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

    
    void _internalFavoriteTask(MediaItem item); 
    void _internalPlayTask(MediaItem item); 
    
    
    void _internalMarkPlayedTask(MediaItem item);
    void _internalMarkUnplayedTask(MediaItem item);
    void _internalRemoveFromResumeTask(MediaItem item);
    void _internalExternalPlayTask(MediaItem item, QString playerPath);

protected:
    
    
    
    
    
    virtual void onMediaItemUpdated(const MediaItem& item) {}

    
    virtual void onMediaItemRemoved(const QString& itemId) {}

    
    QCoro::Task<void> refreshAndBroadcastItem(const QString& itemId);

    QEmbyCore* m_core;
};

#endif 
