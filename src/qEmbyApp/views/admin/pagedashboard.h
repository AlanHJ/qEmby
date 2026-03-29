#ifndef PAGEDASHBOARD_H
#define PAGEDASHBOARD_H

#include "managepagebase.h"
#include <qcorotask.h>
#include <QMap>

class QListView;
class QStandardItemModel;

class QLabel;
class QPushButton;
class QVBoxLayout;
class QGridLayout;
class QTimer;
class HorizontalWidgetGallery;
class NowPlayingCard;

class PageDashboard : public ManagePageBase {
    Q_OBJECT
public:
    explicit PageDashboard(QEmbyCore* core, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    QCoro::Task<void> loadData(bool isManual = false);
    QCoro::Task<void> onRestartClicked();
    QCoro::Task<void> onShutdownClicked();

    
    QLabel* m_serverNameValue;
    QLabel* m_versionValue;
    QLabel* m_osValue;
    QLabel* m_addressValue;

    
    HorizontalWidgetGallery* m_nowPlayingGallery;
    QLabel* m_nowPlayingEmptyLabel;
    
    QMap<QString, NowPlayingCard*> m_nowPlayingCards;
    
    QMap<QString, qint64> m_cardLastSeen;

    
    QGridLayout* m_sessionsGrid;
    QLabel* m_sessionsEmptyLabel;

    
    QListView* m_activityListView;
    QStandardItemModel* m_activityModel;
    QLabel* m_activityEmptyLabel;

    
    QPushButton* m_btnRefresh;
    QPushButton* m_btnRestart;
    QPushButton* m_btnShutdown;

    
    QTimer* m_refreshTimer;
    bool m_loadingData = false;
};

#endif 
