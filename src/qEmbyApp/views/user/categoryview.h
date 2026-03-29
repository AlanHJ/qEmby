#ifndef CATEGORYVIEW_H
#define CATEGORYVIEW_H

#include "../baseview.h" 
#include <models/media/mediaitem.h>
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
    void setupTopBar(QHBoxLayout* headerLayout);

    
    QCoro::Task<void> refreshData();

    QString m_currentCategory;
    
    ElidedLabel* m_titleLabel;
    ModernSortButton* m_sortButton; 
    QPushButton* m_viewSwitchBtn;
    QPushButton* m_refreshBtn;   
    QLabel* m_statsLabel;
    
    MediaGridWidget* m_mediaGrid;

    
    QPropertyAnimation* m_refreshAnimation = nullptr;
};

#endif 
