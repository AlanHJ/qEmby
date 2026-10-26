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
    void setLibraryArtworkPolicyEnabled(bool enabled);
    void setAutomaticArtworkLayout(bool enabled);

    
    void setLoading(bool loading);

    
    void updateItem(const MediaItem& item);
    void prependOrUpdateItem(const MediaItem& item, int maxItems = 0);
    
    
    void removeItem(const QString& itemId);

    
    int itemCount() const;

    
    int saveScrollPosition() const;
    void restoreScrollPosition(int pos);
    struct ViewAnchor {
        QString itemId;
        QString currentItemId;
        int offset = 0;
        int scrollPosition = 0;
    };
    ViewAnchor captureViewAnchor() const;
    void restoreViewAnchor(const ViewAnchor& anchor);

Q_SIGNALS:
    void itemClicked(const MediaItem& item);
    void loadMoreRequested();
    
    
    
    
    void playRequested(const MediaItem& item);
    void favoriteRequested(const MediaItem& item);
    void moreMenuRequested(const MediaItem& item, const QPoint& globalPos);

protected:
    void hideEvent(QHideEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void adjustGrid();
    void notifyLoadMoreIfNeeded();
    void updateVisibleImagePriority();
    void updateAutomaticArtworkLayout();

    int m_basePadding;
    MediaCardDelegate::CardStyle m_currentStyle;
    bool m_automaticArtworkLayout = false;

    QEmbyCore* m_core;
    QListView* m_listView;
    MediaListModel* m_listModel;
    MediaCardDelegate* m_listDelegate;

    SmoothScrollController* m_vScrollController;

    
    ShimmerWidget* m_shimmer = nullptr;
};

#endif 
