#ifndef LIBRARYVIEW_H
#define LIBRARYVIEW_H

#include "../baseview.h"
#include <qembycore.h>
#include <models/media/mediaitem.h>
#include <QList>
#include <qcorotask.h>

class QPushButton;
class QButtonGroup;
class MediaGridWidget;
class ModernSortButton;
class ElidedLabel;
class QLabel;

class LibraryView : public BaseView
{
    Q_OBJECT
public:
    enum ViewMode {
        LibraryMode,
        PersonMode,
        FilteredMode
    };

    explicit LibraryView(QEmbyCore* core, QWidget *parent = nullptr);
    
    
    QCoro::Task<void> loadLibrary(const QString& libraryId, const QString& libraryName);
    QCoro::Task<void> loadPerson(const QString& personId, const QString& personName);
    QCoro::Task<void> loadFiltered(const QString& filterType, const QString& filterValue);

protected:
    
    void onMediaItemUpdated(const MediaItem& item) override;
    void onMediaItemRemoved(const QString& itemId) override;

private slots:
    
    QCoro::Task<void> onFilterChanged();

private:
    void setupTopBar(class QHBoxLayout* headerLayout);
    
    void updateFavBtnState();
    QCoro::Task<QList<MediaItem>> enrichPlaylistItemsForRemoval(QList<MediaItem> items);
    
    void saveSortPreference();
    void restoreSortPreference();

    ViewMode m_currentMode;        
    QString m_currentLibraryId;
    QString m_currentPersonId;     
    QString m_filterType;          
    QString m_filterValue;         
    MediaItem m_currentMediaItem;  
    bool m_isFavorite = false;     

    ElidedLabel* m_titleLabel;
    QPushButton* m_favBtn;         
    
    QWidget* m_tabBarWidget;       
    QButtonGroup* m_tabGroup;
    ModernSortButton* m_sortButton; 
    QPushButton* m_viewSwitchBtn;
    QLabel* m_statsLabel;

    MediaGridWidget* m_mediaGrid;
};

#endif 
