#ifndef MEDIAGRIDWIDGET_H
#define MEDIAGRIDWIDGET_H

#include <QWidget>
#include <QPoint> 
#include <models/media/mediaitem.h>
#include "../views/media/mediacarddelegate.h" 

class QEmbyCore;
class QListView;
class MediaListModel;
class ShimmerWidget;
class SmoothScrollController;

class MediaGridWidget : public QWidget {
    Q_OBJECT
public:
    explicit MediaGridWidget(QEmbyCore* core, QWidget* parent = nullptr);
    void setItems(const QList<MediaItem>& items);
    void setBasePadding(int padding);

    
    void setCardStyle(MediaCardDelegate::CardStyle style);

    
    void setLoading(bool loading);

    
    void updateItem(const MediaItem& item);
    void prependOrUpdateItem(const MediaItem& item, int maxItems = 0);
    
    
    void removeItem(const QString& itemId);

    
    int itemCount() const;

    
    int saveScrollPosition() const;
    void restoreScrollPosition(int pos);

Q_SIGNALS:
    void itemClicked(const MediaItem& item);
    void loadMoreRequested();
    
    
    
    
    void playRequested(const MediaItem& item);
    void favoriteRequested(const MediaItem& item);
    void moreMenuRequested(const MediaItem& item, const QPoint& globalPos);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void adjustGrid();
    void notifyLoadMoreIfNeeded();
    void updateVisibleImagePriority();

    int m_basePadding;
    MediaCardDelegate::CardStyle m_currentStyle;

    QEmbyCore* m_core;
    QListView* m_listView;
    MediaListModel* m_listModel;
    MediaCardDelegate* m_listDelegate;

    SmoothScrollController* m_vScrollController;

    
    ShimmerWidget* m_shimmer = nullptr;
};

#endif 
