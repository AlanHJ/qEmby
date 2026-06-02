#ifndef MEDIALISTMODEL_H
#define MEDIALISTMODEL_H

#include <QAbstractListModel>
#include <models/media/mediaitem.h>
#include <QPixmap>
#include <QHash>
#include <QSet>
#include <QPointer>
#include <QStringList>
#include <QTimer>
#include <qcorotask.h>

class QEmbyCore;

class MediaListModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum MediaRoles {
        ItemDataRole = Qt::UserRole + 1,
        PosterPixmapRole
    };

    explicit MediaListModel(int imageMaxWidth, QEmbyCore* core, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setItems(const QList<MediaItem>& items);
    MediaItem getItem(const QModelIndex& index) const;

    
    void setPreferThumb(bool prefer) { m_preferThumb = prefer; }

    
    
    void updateItem(const MediaItem& updatedItem);
    void prependOrUpdateItem(const MediaItem& item, int maxItems = 0);

    
    
    
    void removeItem(const QString& itemId);
    void setPriorityRows(const QList<int>& rows);

    
    void clearImageCache()
    {
        ++m_imageRequestGeneration;
        m_imageCache.clear();
        m_loadingImages.clear();
        m_pendingImageRequests.clear();
        m_pendingImageOrder.clear();
        m_priorityImageIds.clear();
        m_pendingImageNotifyIds.clear();
        m_activeImageFetches = 0;
        if (m_imageNotifyTimer) {
            m_imageNotifyTimer->stop();
        }
    }

private:
    struct PendingImageRequest {
        QString targetImageId;
        QString imageType;
        QString imageTag;
        int maxWidth = 0;
    };

    QString buildTooltipText(const MediaItem &item) const;
    void ensureImageRequested(const MediaItem& item);
    void enqueueImageFetch(const QString& itemId,
                           const PendingImageRequest& request);
    void scheduleImageFetches();
    QString takeNextPendingImageId();
    void queueImageDataChanged(const QString& itemId);
    void flushPendingImageDataChanges();

    
    static QCoro::Task<void> executeImageFetch(
        QPointer<MediaListModel> safeThis, QString itemId,
        QString targetImageId, QString imgType, QString imgTag, int maxWidth,
        int generation, QEmbyCore* core);

    bool m_preferThumb = false;
    int m_imageMaxWidth;
    QEmbyCore* m_core;
    QList<MediaItem> m_items;

    mutable QHash<QString, QPixmap> m_imageCache;
    mutable QSet<QString> m_loadingImages;
    QHash<QString, PendingImageRequest> m_pendingImageRequests;
    QStringList m_pendingImageOrder;
    QStringList m_priorityImageIds;
    QSet<QString> m_pendingImageNotifyIds;
    QTimer* m_imageNotifyTimer = nullptr;
    int m_activeImageFetches = 0;
    int m_imageRequestGeneration = 0;
};

#endif 
