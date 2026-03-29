#ifndef MEDIALISTMODEL_H
#define MEDIALISTMODEL_H

#include <QAbstractListModel>
#include <models/media/mediaitem.h>
#include <QPixmap>
#include <QHash>
#include <QSet>
#include <QPointer>
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

    
    
    
    void removeItem(const QString& itemId);

    
    void clearImageCache() { m_imageCache.clear(); m_loadingImages.clear(); }

private:
    
    static QCoro::Task<void> executeImageFetch(QPointer<MediaListModel> safeThis, QString itemId, QString targetImageId, QString imgType, QString imgTag, int maxWidth, QEmbyCore* core);

    bool m_preferThumb = false;
    int m_imageMaxWidth;
    QEmbyCore* m_core;
    QList<MediaItem> m_items;

    mutable QHash<QString, QPixmap> m_imageCache;
    mutable QSet<QString> m_loadingImages;
};

#endif 
