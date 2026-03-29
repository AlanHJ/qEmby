#include "medialistmodel.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <QSet>
#include <QMutableHashIterator>

MediaListModel::MediaListModel(int imageMaxWidth, QEmbyCore* core, QObject *parent)
    : QAbstractListModel(parent), m_imageMaxWidth(imageMaxWidth), m_core(core) {}

int MediaListModel::rowCount(const QModelIndex &parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return m_items.size();
}

QVariant MediaListModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() >= m_items.size()) {
        return QVariant();
    }
    
    const MediaItem& item = m_items.at(index.row());

    if (role == ItemDataRole) {
        return QVariant::fromValue(item);
    }
    else if (role == PosterPixmapRole) {
        if (m_imageCache.contains(item.id)) {
            return m_imageCache.value(item.id);
        }

        if (!m_loadingImages.contains(item.id)) {
            m_loadingImages.insert(item.id);
            MediaListModel* nonConstThis = const_cast<MediaListModel*>(this);

            QString imgType = "Primary";
            QString imgTag = item.images.primaryTag;

            if (m_preferThumb) {
                if (ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages, true)) {
                    
                    auto best = item.images.bestThumb();
                    imgTag = best.first;
                    imgType = best.second;
                } else {
                    
                    if (!item.images.thumbTag.isEmpty()) {
                        imgType = "Thumb";
                        imgTag = item.images.thumbTag;
                    } else if (!item.images.backdropTag.isEmpty()) {
                        imgType = "Backdrop";
                        imgTag = item.images.backdropTag;
                    }
                }
            } else if (imgTag.isEmpty() && ConfigStore::instance()->get<bool>(ConfigKeys::AdaptiveImages, true)) {
                
                auto best = item.images.bestPoster();
                imgTag = best.first;
                imgType = best.second;
            }

            
            QString targetImageId = item.getPrimaryImageId();

            
            bool usingParentImage = false;
            if (!imgTag.isEmpty() && item.images.isParentTag(imgTag)
                && !item.images.parentImageItemId.isEmpty()) {
                targetImageId = item.images.parentImageItemId;
                usingParentImage = true;
            }

            if (!imgTag.isEmpty()) {
                
                int fetchWidth = usingParentImage ? m_imageMaxWidth * 2 : m_imageMaxWidth;
                QPointer<MediaListModel> safeThis(nonConstThis);
                
                executeImageFetch(safeThis, item.id, targetImageId, imgType, imgTag, fetchWidth, m_core);
            }
        }
        return QVariant(); 
    }
    return QVariant();
}


void MediaListModel::setItems(const QList<MediaItem>& newItems) {
    if (m_items.isEmpty() || newItems.isEmpty()) {
        beginResetModel();
        m_items = newItems;
        if (newItems.isEmpty()) {
            m_imageCache.clear();
            m_loadingImages.clear();
        }
        endResetModel();
        return;
    }

    
    QSet<QString> newIds;
    for (const auto& item : newItems) {
        newIds.insert(item.id);
    }

    
    
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (!newIds.contains(m_items[i].id)) {
            beginRemoveRows(QModelIndex(), i, i);
            m_items.removeAt(i);
            endRemoveRows();
        }
    }

    
    for (int newIdx = 0; newIdx < newItems.size(); ++newIdx) {
        const MediaItem& newItem = newItems[newIdx];
        
        
        
        
        int oldIdx = -1;
        for (int j = newIdx; j < m_items.size(); ++j) {
            if (m_items[j].id == newItem.id) {
                oldIdx = j;
                break;
            }
        }

        if (oldIdx == -1) {
            
            beginInsertRows(QModelIndex(), newIdx, newIdx);
            m_items.insert(newIdx, newItem);
            endInsertRows();
        } 
        else if (oldIdx == newIdx) {
            
            m_items[newIdx] = newItem;
            Q_EMIT dataChanged(index(newIdx, 0), index(newIdx, 0), {ItemDataRole});
        } 
        else {
            
            
            beginMoveRows(QModelIndex(), oldIdx, oldIdx, QModelIndex(), newIdx);
            m_items.move(oldIdx, newIdx);
            endMoveRows();
            
            
            m_items[newIdx] = newItem;
            Q_EMIT dataChanged(index(newIdx, 0), index(newIdx, 0), {ItemDataRole});
        }
    }

    
    QMutableHashIterator<QString, QPixmap> it(m_imageCache);
    while (it.hasNext()) {
        it.next();
        if (!newIds.contains(it.key())) {
            it.remove();
        }
    }
}

MediaItem MediaListModel::getItem(const QModelIndex& index) const {
    if (!index.isValid() || index.row() >= m_items.size()) {
        return MediaItem();
    }
    return m_items.at(index.row());
}

void MediaListModel::updateItem(const MediaItem& updatedItem) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == updatedItem.id) {
            
            m_items[i] = updatedItem;
            
            
            QModelIndex idx = index(i, 0);
            Q_EMIT dataChanged(idx, idx, {ItemDataRole});
            break; 
        }
    }
}




void MediaListModel::removeItem(const QString& itemId) {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == itemId) {
            
            beginRemoveRows(QModelIndex(), i, i);
            
            
            m_items.removeAt(i);
            
            
            endRemoveRows();

            
            m_imageCache.remove(itemId);
            m_loadingImages.remove(itemId);
            
            break; 
        }
    }
}


QCoro::Task<void> MediaListModel::executeImageFetch(QPointer<MediaListModel> safeThis, QString itemId, QString targetImageId, QString imgType, QString imgTag, int maxWidth, QEmbyCore* core) {
    try {
        
        QPixmap pix = co_await core->mediaService()->fetchImage(targetImageId, imgType, imgTag, maxWidth);
        
        
        if (!safeThis) {
            co_return; 
        }

        
        safeThis->m_imageCache.insert(itemId, pix);
        safeThis->m_loadingImages.remove(itemId);

        
        int currentRow = -1;
        for (int i = 0; i < safeThis->m_items.size(); ++i) {
            if (safeThis->m_items.at(i).id == itemId) {
                currentRow = i;
                break;
            }
        }

        
        if (currentRow >= 0) {
            Q_EMIT safeThis->dataChanged(safeThis->index(currentRow, 0), safeThis->index(currentRow, 0), {PosterPixmapRole});
        }
    } catch (...) {
        
        if (safeThis) {
            safeThis->m_loadingImages.remove(itemId);
        }
    }
}