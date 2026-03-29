#ifndef DASHBOARDVIEW_H
#define DASHBOARDVIEW_H

#include "../baseview.h"
#include <QPropertyAnimation>
#include <qcorotask.h>

class QListView;
class MediaListModel;
class MediaCardDelegate;
class QLabel;
class QVBoxLayout;
class QScrollArea;
class HorizontalListViewGallery;
class MediaSectionWidget;

class DashboardView : public BaseView
{
    Q_OBJECT
public:
    explicit DashboardView(QEmbyCore* core, QWidget *parent = nullptr);
    
    
    QCoro::Task<void> loadDashboardData();

Q_SIGNALS:
    
    void navigateToLibrary(const QString& libraryId, const QString& libraryName);

protected:
    
    void onMediaItemUpdated(const MediaItem& item) override;
    
    
    void onMediaItemRemoved(const QString& itemId) override;

    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void setupUi();

    
    QWidget* createSectionHeader(const QString& title, const QString& type);
    
    QListView* createGridListView(MediaListModel** outModel);
    void adjustLibraryGridHeight();

    QScrollArea* m_mainScrollArea; 

    
    QPropertyAnimation* m_vScrollAnim;
    int m_vScrollTarget;

    
    QWidget* m_resumeHeader;
    HorizontalListViewGallery* m_resumeGallery;

    
    QWidget* m_latestHeader;
    HorizontalListViewGallery* m_latestGallery;

    
    QWidget* m_recommendHeader;
    HorizontalListViewGallery* m_recommendGallery;

    
    QLabel* m_libraryTitle;
    QListView* m_libraryListView;
    MediaListModel* m_libraryModel;

    
    QList<MediaSectionWidget*> m_libraryGalleries;
    QVBoxLayout* m_containerLayout = nullptr;

    
    MediaCardDelegate* m_libraryDelegate;
};

#endif 
