#ifndef FAVORITESVIEW_H
#define FAVORITESVIEW_H

#include "../baseview.h"
#include <QPropertyAnimation>
#include <qcorotask.h>

class QListView;
class QLabel;
class QScrollArea;
class HorizontalListViewGallery;

class FavoritesView : public BaseView
{
    Q_OBJECT
public:
    explicit FavoritesView(QEmbyCore* core, QWidget *parent = nullptr);
    
    
    QCoro::Task<void> loadFavoritesData();

protected:
    
    void onMediaItemUpdated(const MediaItem& item) override;
    void onMediaItemRemoved(const QString& itemId) override;

    void showEvent(QShowEvent* event) override;
    
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();

    
    QWidget* createSectionHeader(const QString& title, const QString& itemType = QString());

    QScrollArea* m_mainScrollArea;

    
    QPropertyAnimation* m_vScrollAnim;
    int m_vScrollTarget;

    
    QWidget* m_moviesHeader;
    HorizontalListViewGallery* m_moviesGallery;

    
    QWidget* m_seriesHeader;
    HorizontalListViewGallery* m_seriesGallery;

    
    QWidget* m_collectionsHeader;
    HorizontalListViewGallery* m_collectionsGallery;

    
    QWidget* m_playlistsHeader;
    HorizontalListViewGallery* m_playlistsGallery;

    
    QWidget* m_peopleHeader;
    HorizontalListViewGallery* m_peopleGallery;

    
    QWidget* m_foldersHeader;
    HorizontalListViewGallery* m_foldersGallery;
};

#endif 
