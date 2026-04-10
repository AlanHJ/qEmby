#include "medialistmodel.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <config/configstore.h>
#include <config/config_keys.h>
#include <QSet>
#include <QVector>
#include <QMutableHashIterator>

namespace {

QString buildImageIdentity(const MediaItem& item)
{
    return QStringLiteral("%1|%2|%3|%4|%5|%6|%7|%8|%9")
        .arg(item.getPrimaryImageId())
        .arg(item.images.primaryTag)
        .arg(item.images.thumbTag)
        .arg(item.images.backdropTag)
        .arg(item.images.logoTag)
        .arg(item.images.parentPrimaryTag)
        .arg(item.images.parentThumbTag)
        .arg(item.images.parentBackdropTag)
        .arg(item.images.parentImageItemId);
}

} 

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
    else if (role == Qt::ToolTipRole) {
        return buildTooltipText(item);
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

QString MediaListModel::buildTooltipText(const MediaItem &item) const {
    QStringList lines;

    if ((item.type == QStringLiteral("Episode") ||
         item.type == QStringLiteral("Season")) &&
        !item.seriesName.isEmpty()) {
        lines << item.seriesName;
    }

    QString title = item.name.trimmed();
    if (item.type == QStringLiteral("Episode") && item.parentIndexNumber >= 0 &&
        item.indexNumber >= 0) {
        title = QStringLiteral("S%1E%2  %3")
                    .arg(item.parentIndexNumber, 2, 10, QChar('0'))
                    .arg(item.indexNumber, 2, 10, QChar('0'))
                    .arg(item.name);
    }
    lines << title;

    QStringList metaParts;
    if (item.type == QStringLiteral("Season") && item.parentIndexNumber >= 0) {
        metaParts << tr("Season %1").arg(item.parentIndexNumber);
    }
    if (item.productionYear > 0) {
        metaParts << QString::number(item.productionYear);
    }
    if (!item.premiereDate.trimmed().isEmpty()) {
        metaParts << item.premiereDate.left(10);
    }
    if (item.runTimeTicks > 0) {
        const long long minutes = item.runTimeTicks / 10000000 / 60;
        if (minutes > 0) {
            metaParts << tr("%1 min").arg(minutes);
        }
    }
    if (item.type == QStringLiteral("Season") && item.recursiveItemCount > 0) {
        metaParts << tr("%1 Episodes").arg(item.recursiveItemCount);
    }
    if (!metaParts.isEmpty()) {
        lines << metaParts.join(QStringLiteral(" • "));
    }

    const QString overview = item.overview.simplified();
    if (!overview.isEmpty()) {
        lines << overview;
    }

    return lines.join(QLatin1Char('\n'));
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
            const bool imageChanged =
                buildImageIdentity(m_items[i]) != buildImageIdentity(updatedItem);

            
            m_items[i] = updatedItem;

            if (imageChanged) {
                m_imageCache.remove(updatedItem.id);
                m_loadingImages.remove(updatedItem.id);
            }
            
            
            QModelIndex idx = index(i, 0);
            QVector<int> roles = {ItemDataRole};
            if (imageChanged) {
                roles.append(PosterPixmapRole);
            }
            Q_EMIT dataChanged(idx, idx, roles);
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
