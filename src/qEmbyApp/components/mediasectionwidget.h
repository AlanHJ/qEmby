#ifndef MEDIASECTIONWIDGET_H
#define MEDIASECTIONWIDGET_H

#include <QWidget>
#include <functional>
#include <qcorotask.h>
#include "models/media/mediaitem.h"
#include "../views/media/mediacarddelegate.h"

class QEmbyCore;
class HorizontalListViewGallery;
class QLabel;
class QHBoxLayout;

class MediaSectionWidget : public QWidget {
    Q_OBJECT
public:
    explicit MediaSectionWidget(const QString& title, QEmbyCore* core, QWidget* parent = nullptr);

    
    HorizontalListViewGallery* gallery() const;

    
    void setTitle(const QString& title);
    void setCardStyle(MediaCardDelegate::CardStyle style);
    void setGalleryHeight(int height);
    void setTileSize(const QSize &size);

    
    void setHeaderWidget(QWidget* widget);

    
    void clear();
    void setItems(const QList<MediaItem>& items);
    void updateItem(const MediaItem& item);

    
    using FetchFunction = std::function<QCoro::Task<QList<MediaItem>>()>;
    QCoro::Task<void> loadAsync(FetchFunction fetcher);

signals:
    
    void itemClicked(const MediaItem& item);
    void playRequested(const MediaItem& item);
    void favoriteRequested(const MediaItem& item);
    void moreMenuRequested(const MediaItem& item, const QPoint& pos);

    
    void dataLoaded(int itemCount);

private:
    QEmbyCore* m_core;
    QLabel* m_titleLabel;
    QHBoxLayout* m_headerLayout;
    QWidget* m_headerWidget = nullptr;
    HorizontalListViewGallery* m_gallery;
};

#endif 
