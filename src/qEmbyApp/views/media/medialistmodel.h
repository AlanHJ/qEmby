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
    ~MediaListModel() override;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

    void setItems(const QList<MediaItem>& items);
    MediaItem getItem(const QModelIndex& index) const;
    QList<MediaItem> items() const { return m_items; }

    
    void setPreferThumb(bool prefer);
    void setImageMaxWidth(int maxWidth);
    void setForceNetworkImages(bool forceNetwork);

    
    
    void updateItem(const MediaItem& updatedItem);
    void prependOrUpdateItem(const MediaItem& item, int maxItems = 0);

    
    
    
    void removeItem(const QString& itemId);
    void setPriorityRows(const QList<int>& rows);
    void suspendImageRequests();
    void resumeImageRequests();

    
    void clearImageCache();

    
    
    
    
    
    void clearFailedImageItems();

private:
    struct ImageCandidate {
        QString targetImageId;
        QString imageType;
        QString imageTag;
        int maxWidth = 0;
    };

    struct PendingImageRequest {
        QList<ImageCandidate> candidates;
        int candidateIndex = 0;
        int transientRetryCount = 0;
        bool highPriority = false;
        QString imageIdentity;
    };

    QString buildTooltipText(const MediaItem &item) const;
    void ensureImageRequested(const MediaItem& item,
                              bool highPriority = false);
    void enqueueImageFetch(const QString& itemId,
                           const PendingImageRequest& request);
    void enqueueImageRetry(const QString& itemId,
                           const PendingImageRequest& request);
    void scheduleImageFetches();
    QString takeNextPendingImageId();
    void queueImageDataChanged(const QString& itemId);
    void flushPendingImageDataChanges();
    void retryNextImageCandidate(const QString& itemId,
                                 PendingImageRequest request, int generation);
    bool isImageRequestCurrent(const QString& itemId,
                               const PendingImageRequest& request) const;
    void invalidateItemImageRequest(const QString& itemId);

    
    static QCoro::Task<void> executeImageFetch(
        QPointer<MediaListModel> safeThis, QString itemId,
        PendingImageRequest request, int generation, QEmbyCore* core);

    bool m_preferThumb = false;
    bool m_forceNetworkImages = false;
    int m_imageMaxWidth;
    QEmbyCore* m_core;
    QList<MediaItem> m_items;

    mutable QHash<QString, QPixmap> m_imageCache;
    mutable QSet<QString> m_loadingImages;
    mutable QSet<QString> m_failedImageItems;
    QHash<QString, PendingImageRequest> m_pendingImageRequests;
    QStringList m_pendingImageOrder;
    QStringList m_priorityImageIds;
    QSet<QString> m_pendingImageNotifyIds;
    QTimer* m_imageNotifyTimer = nullptr;
    int m_activeImageFetches = 0;
    int m_imageRequestGeneration = 0;
    QObject* m_imageRequestContext = nullptr;
    bool m_imageRequestsSuspended = false;
};

#endif 
