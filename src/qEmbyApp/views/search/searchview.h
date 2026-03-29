#ifndef SEARCHVIEW_H
#define SEARCHVIEW_H

#include "../baseview.h"
#include <models/media/mediaitem.h>
#include <qcorotask.h>

class MediaGridWidget;
class QLabel;
class QButtonGroup;
class QPushButton;

class SearchView : public BaseView
{
    Q_OBJECT
public:
    explicit SearchView(QEmbyCore* core, QWidget *parent = nullptr);

    
    QCoro::Task<void> performSearch(QString query);

protected:
    
    void onMediaItemUpdated(const MediaItem& item) override;

private slots:
    
    QCoro::Task<void> onTabChanged(int tabId);

    
    void onViewModeChanged();

private:
    void setupUi();
    void setupTopBar(class QHBoxLayout* headerLayout);

    QString m_currentQuery;

    
    QList<MediaItem> m_currentItems;

    QLabel* m_titleLabel;
    QLabel* m_statsLabel;

    QButtonGroup* m_tabGroup;
    MediaGridWidget* m_mediaGrid;

    QPushButton* m_btnViewToggle;
};

#endif 
