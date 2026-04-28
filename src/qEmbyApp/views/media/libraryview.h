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
    QCoro::Task<void> onLoadMoreRequested();

private:
    struct QueryState {
        ViewMode mode = LibraryMode;
        QString targetId;
        QString sortBy;
        QString sortOrder;
        QString filters;
        QString includeTypes;
        QString genreFilter;
        QString tagFilter;
        QString studioFilter;
        bool recursive = false;
        bool includeChildCount = false;
        bool needsPlaylistEnrichment = false;
    };

    void setupTopBar(class QHBoxLayout* headerLayout);
    
    void updateFavBtnState();
    QCoro::Task<QList<MediaItem>> enrichPlaylistItemsForRemoval(QList<MediaItem> items);
    QueryState buildCurrentQueryState(const QString& sortBy, const QString& sortOrder) const;
    bool isQueryValid(const QueryState& query) const;
    void resetPaginationState();
    void updateStatsLabel();
    QCoro::Task<void> loadInitialItems();
    QString currentPreferenceTargetId() const;
    void applyViewMode(bool isTile);
    
    void saveSortPreference();
    void restoreSortPreference();
    void saveViewPreference();
    void restoreViewPreference();

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
    QueryState m_activeQuery;
    QList<MediaItem> m_loadedItems;
    int m_totalItemCount = 0;
    int m_pageSize = 0;
    int m_requestGeneration = 0;
    bool m_hasMoreItems = false;
    bool m_isLoadingMore = false;
};

#endif 
