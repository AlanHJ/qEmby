#ifndef CATEGORYVIEW_H
#define CATEGORYVIEW_H

#include "../baseview.h" 
#include <models/media/mediaitem.h>
#include <QList>
#include <QString>
#include <qcorotask.h>

class MediaGridWidget;
class ElidedLabel;
class ModernSortButton;
class QPushButton;
class QLabel;
class QHBoxLayout;

class QPropertyAnimation;

class CategoryView : public BaseView {
    Q_OBJECT
public:
    explicit CategoryView(QEmbyCore* core, QWidget *parent = nullptr);
    
    
    QCoro::Task<void> loadCategory(const QString& categoryType, const QString& title);

protected:
    
    void onMediaItemUpdated(const MediaItem& item) override;
    
    
    void onMediaItemRemoved(const QString& itemId) override;

private slots:
    
    QCoro::Task<void> onFilterChanged();

private:
    struct DashboardCategoryQuery {
        QString category;
        QString sortBy;
        QString sortOrder;
        int requestLimit = 0;
        int firstPageSize = 100;
        int pageSize = 300;
    };

    struct DashboardCategoryPage {
        QList<MediaItem> items;
        QString fingerprint;
        int rawItemCount = 0;
        int totalRecordCount = 0;
        bool hasTotalRecordCount = false;
    };

    void setupTopBar(QHBoxLayout* headerLayout);
    bool isCastStyleCategory(const QString& categoryType) const;
    bool isProgressiveDashboardCategory(const QString& categoryType) const;
    QString currentViewPreferenceCategoryId() const;
    void applyViewMode(bool isTile);
    void saveViewPreference();
    void restoreViewPreference();
    int dashboardCategoryRequestLimit(const QString& categoryType) const;
    DashboardCategoryQuery buildDashboardCategoryQuery(const QString& sortBy,
                                                       const QString& sortOrder) const;
    QCoro::Task<void> loadDashboardCategoryProgressively(DashboardCategoryQuery query);
    QCoro::Task<DashboardCategoryPage> fetchDashboardCategoryPage(DashboardCategoryQuery query,
                                                                  int startIndex,
                                                                  int limit);
    void appendUniqueLoadedItems(const QList<MediaItem>& items);
    void setLoadedItems(const QList<MediaItem>& items);

    
    QCoro::Task<void> refreshData();

    QString m_currentCategory;
    
    ElidedLabel* m_titleLabel;
    ModernSortButton* m_sortButton; 
    QPushButton* m_viewSwitchBtn;
    QPushButton* m_refreshBtn;   
    QLabel* m_statsLabel;
    
    MediaGridWidget* m_mediaGrid;
    QList<MediaItem> m_loadedItems;
    int m_requestGeneration = 0;

    
    QPropertyAnimation* m_refreshAnimation = nullptr;
};

#endif 
