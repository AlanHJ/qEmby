#ifndef HOMEVIEW_H
#define HOMEVIEW_H

#include <QStack>
#include <QWidget>
#include <QHBoxLayout>
#include <QPropertyAnimation>
#include <QPointer>
#include <qcorotask.h>
#include <optional>

class QTimer;
class QEmbyCore;
class SlidingStackedWidget;
class QLabel;
class QPushButton;
class QLineEdit;
class QListWidget;
class QCompleter;
class QStringListModel;
class ElidedLabel;
class QBoxLayout;
class QVBoxLayout;
class DashboardView;
class FavoritesView;
class SettingsView;
class SeasonView; 
class PlaybackManager; 
class PlayerView;      
class ManageView;      


struct RouteInfo {
    QPointer<QWidget> widget; 
    bool isDynamic;           
    QString routeType;        
    QString routeId;          
    QString routeTitle;       
    QString routeExtraId;     
};

class HomeView : public QWidget
{
    Q_OBJECT
public:
    explicit HomeView(QEmbyCore* core, QWidget *parent = nullptr);
    
    
    QCoro::Task<void> refreshProfile();

    
    void triggerSearch(const QString& query);

    
    bool canNavigateBack() const;

    bool canGoHome() const;

    bool canGoFav() const;

    
    PlayerView* activePlayerView() const;

public Q_SLOTS:
    
    void navigateBack();
    void goHome();
    void goFav();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

signals:
    void logoutRequested();
    void canNavigateBackChanged(bool canBack);
    void homeContentSwitched();
    
    
    void immersiveStateChanged(bool isImmersive);

private:
    void scheduleProfileRefresh();
    void setupUi();
    void setupSidebar();

    void showSidebar();
    void hideSidebar();
    void applySidebarPosition();
    void applySidebarPinned(bool pinned);
    bool isCurrentViewImmersive() const;
    int sidebarWidthForMode(bool pinned) const;
    void applySidebarMetrics(bool pinned);
    void syncSidebarVisibility();
    void setupSearchHistory();
    void updateSearchCompleter(const QString &text = QString());
    QString currentSearchServerId() const;

    
    void pushView(QWidget* view);
    void resetToView(QWidget* view);

    QWidget* createDetailView(const QString& itemId, const QString& itemName = "");
    QWidget* createCategoryView(const QString& categoryId, const QString& title = "");
    QWidget* createLibraryView(const QString& libraryId, const QString& title = "");
    QWidget* createPersonView(const QString& personId, const QString& personName = "");
    QWidget* createSearchView(const QString& query);
    QWidget* createFilteredView(const QString& filterType, const QString& filterValue);
    QWidget* createPlayerView(const QString& mediaId, const QString& title, const QString& streamUrl, long long startPositionTicks, const QVariant &extraData);
    QWidget* createSettingsView();
    QWidget* createManageView(); 
    
    
    QWidget* createSeasonView(const QString& seriesId, const QString& seasonId, const QString& seasonName);

    
    void launchPlayer(const QString& mediaId, const QString& title, const QString& streamUrl, long long startPositionTicks, const QVariant& extraData);

    QEmbyCore* m_core;

    SlidingStackedWidget* m_contentSwitcher = nullptr;
    QStack<RouteInfo> m_navStack; 

    DashboardView* m_dashboardView = nullptr;
    FavoritesView* m_favoritesView = nullptr;

    QWidget* m_sidebar = nullptr;
    QWidget* m_edgeTrigger = nullptr;
    QPropertyAnimation* m_sidebarAnim = nullptr;
    QTimer* m_sidebarAutoHideTimer = nullptr;   
    bool m_sidebarOnRight = false;
    bool m_sidebarPinned = false;
    bool m_sidebarPinnedApplied = false;
    QHBoxLayout* m_contentLayout = nullptr;

    QLabel* m_serverIconLabel = nullptr;
    QBoxLayout* m_serverInfoLayout = nullptr;
    QVBoxLayout* m_serverNameLayout = nullptr;
    ElidedLabel* m_serverNameLabel = nullptr;
    ElidedLabel* m_serverAddressLabel = nullptr;
    QLineEdit* m_searchBox = nullptr;
    QCompleter* m_searchCompleter = nullptr;
    QStringListModel* m_searchHistoryModel = nullptr;

    QPushButton* m_btnHome = nullptr;
    QPushButton* m_btnFavorites = nullptr;
    QListWidget* m_libraryList = nullptr;

    QLabel* m_userAvatarLabel = nullptr;
    QHBoxLayout* m_userInfoLayout = nullptr;
    ElidedLabel* m_userNameLabel = nullptr;
    QPushButton* m_btnSettings = nullptr;
    QPushButton* m_btnManage = nullptr;
    QPushButton* m_btnLogout = nullptr;

    std::optional<QCoro::Task<void>> m_pendingProfileRefreshTask;
    QString m_lastRouteType;
};

#endif 
