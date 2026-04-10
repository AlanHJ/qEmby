#ifndef HORIZONTALLISTVIEWGALLERY_H
#define HORIZONTALLISTVIEWGALLERY_H

#include <QWidget>
#include <QPoint>
#include <QList>
#include <QSize>
#include <models/media/mediaitem.h>

class ShimmerWidget;
#include "../views/media/mediacarddelegate.h"

class QEmbyCore;
class QListView;
class QPushButton;
class QPropertyAnimation;
class MediaListModel; 

class HorizontalListViewGallery : public QWidget
{
    Q_OBJECT
public:
    
    explicit HorizontalListViewGallery(QEmbyCore* core, QWidget* parent = nullptr);
    ~HorizontalListViewGallery() override;

    
    QListView* listView() const { return m_listView; }

    
    void setItems(const QList<MediaItem>& items);
    void updateItem(const MediaItem& item);
    
    
    void removeItem(const QString& itemId);
    int itemCount() const;
    void clearImageCache();

    
    void setCardStyle(MediaCardDelegate::CardStyle style);

    
    void setTileSize(const QSize &size);
    void setTextPixelSizes(int titlePx, int subTitlePx);
    void setContentPadding(int padding);

    
    void setHighlightedItemId(const QString &id);

    
    void setLoading(bool loading);

    
    QSize minimumSizeHint() const override {
        return QSize(1, QWidget::minimumSizeHint().height());
    }

Q_SIGNALS:
    
    void itemClicked(const MediaItem& item);
    void playRequested(const MediaItem& item);
    void favoriteRequested(const MediaItem& item);
    void moreMenuRequested(const MediaItem& item, const QPoint& globalPos);

protected:
    void resizeEvent(QResizeEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void updateButtonsVisibility();
    void updateButtonPositions();

    QEmbyCore* m_core;
    QListView* m_listView;
    QPushButton* m_btnLeft;
    QPushButton* m_btnRight;

    
    MediaListModel* m_listModel;
    MediaCardDelegate* m_listDelegate;

    
    QPropertyAnimation* m_hScrollAnim;
    int m_hScrollTarget;

    
    MediaCardDelegate::CardStyle m_cardStyle;

    
    ShimmerWidget* m_shimmer = nullptr;
};

#endif 
