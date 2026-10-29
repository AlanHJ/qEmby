#ifndef DASHBOARDVIEW_H
#define DASHBOARDVIEW_H

#include "../baseview.h"
#include "../../utils/featuredmediautils.h"
#include <QSet>
#include <QStringList>
#include <qcorotask.h>

class QListView;
class MediaListModel;
class MediaCardDelegate;
class QLabel;
class QWidget;
class QVBoxLayout;
class QScrollArea;
class HorizontalListViewGallery;
class MediaSectionWidget;
class SmoothScrollController;
class MediaCarouselWidget;

class DashboardView : public BaseView
{
    Q_OBJECT
public:
    explicit DashboardView(QEmbyCore* core, QWidget *parent = nullptr);

    enum class CarouselRefreshPolicy { Reuse, Refresh, NewSelection };
    
    
    QCoro::Task<void> loadDashboardData(
        CarouselRefreshPolicy carouselPolicy = CarouselRefreshPolicy::Refresh);

public Q_SLOTS:
    void scrollToTop() override;

Q_SIGNALS:
    
    void navigateToLibrary(const QString& libraryId, const QString& libraryName);

protected:
    
    void onMediaItemUpdated(const MediaItem& item) override;
    
    
    void onMediaItemRemoved(const QString& itemId) override;
    CardContextMenuRequest showCardContextMenu(const MediaItem& item,
                                               const QPoint& globalPos) override;
    void dispatchCardContextMenuRequest(
        const MediaItem& item, const CardContextMenuRequest& request) override;

    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void launchDashboardTask(QCoro::Task<void>&& task);
    void setupUi();
    void applyDashboardSectionOrder();
    void applyCarouselPreference(bool loadIfEnabled,
        CarouselRefreshPolicy policy = CarouselRefreshPolicy::Reuse);
    void updateCarouselHeight();
    QStringList dashboardSectionOrder() const;
    QString currentServerId() const;
    QString currentDashboardContextKey() const;
    QWidget* sectionWidgetForId(const QString& sectionId) const;
    void clearLibraryGallerySections();
    void applyLibraryVisibility(bool refreshItems = false);
    void clearDashboardState(bool resetScrollPositions);
    void resetDashboardScrollPositions();
    void clearDashboardGallery(HorizontalListViewGallery* gallery);
    void resetDashboardGalleryScrollPosition(
        HorizontalListViewGallery* gallery) const;
    void resetListViewScrollPosition(QListView* listView) const;
    bool isManageableDashboardLibraryCard(const MediaItem& item) const;
    void openDashboardLibraryImageEditor(const MediaItem& item);

    
    QWidget* createSectionHeader(const QString& title, const QString& type);
    
    QListView* createGridListView(MediaListModel** outModel);
    void adjustLibraryGridHeight();

    
    QCoro::Task<void> loadFeaturedSection(int carouselGeneration, bool newSelection);
    QCoro::Task<void> loadFeaturedItemMetadata(MediaItem item, int carouselGeneration);
    QCoro::Task<void> loadResumeSection(bool show, int generation);
    QCoro::Task<void> loadLatestSection(bool show, int generation);
    QCoro::Task<void> loadRecommendedSection(bool show, int generation);
    QCoro::Task<void> loadCompletedSection(bool show, int generation);
    QCoro::Task<void> loadResourceSection(bool music, int generation);
    QCoro::Task<void> loadLibrarySections(bool showLibraries, bool showEachLibrary, int generation);
    QCoro::Task<void> executeDashboardLibraryRefresh(
        MediaItem item, bool replaceAllMetadata, bool replaceAllImages,
        bool isMetadataRefresh);

    QScrollArea* m_mainScrollArea = nullptr; 
    MediaCarouselWidget* m_featuredCarousel = nullptr;

    SmoothScrollController* m_vScrollController = nullptr;

    
    QWidget* m_resumeSection = nullptr;
    QWidget* m_resumeHeader = nullptr;
    HorizontalListViewGallery* m_resumeGallery = nullptr;

    
    QWidget* m_latestSection = nullptr;
    QWidget* m_latestHeader = nullptr;
    HorizontalListViewGallery* m_latestGallery = nullptr;

    
    QWidget* m_recommendSection = nullptr;
    QWidget* m_recommendHeader = nullptr;
    HorizontalListViewGallery* m_recommendGallery = nullptr;

    
    QWidget* m_completedSection = nullptr;
    QWidget* m_liveTvSection = nullptr;
    QWidget* m_musicSection = nullptr;
    HorizontalListViewGallery* m_liveTvGallery = nullptr;
    HorizontalListViewGallery* m_musicGallery = nullptr;
    QWidget* m_completedHeader = nullptr;
    HorizontalListViewGallery* m_completedGallery = nullptr;

    
    QWidget* m_libraryGridSection = nullptr;
    QWidget* m_librarySectionsContainer = nullptr;
    QLabel* m_libraryTitle = nullptr;
    QListView* m_libraryListView = nullptr;
    MediaListModel* m_libraryModel = nullptr;
    QList<MediaItem> m_homeLibraries; 

    
    QList<MediaSectionWidget*> m_libraryGalleries;
    QVBoxLayout* m_containerLayout = nullptr;
    QVBoxLayout* m_librarySectionsLayout = nullptr;

    
    MediaCardDelegate* m_libraryDelegate = nullptr;

    
    int m_loadGeneration = 0;
    int m_liveTvGeneration = 0;
    int m_musicGeneration = 0;
    
    int m_featuredLoadGeneration = 0;
    bool m_featuredLoaded = false;
    bool m_featuredLoading = false;
    bool m_featuredNeedsRefresh = false;
    FeaturedMediaUtils::DailySelection m_featuredSelection;
    QDate m_featuredRequestDate;
    QSet<QString> m_featuredUnavailableIds;
    QString m_dashboardContextKey;
};

#endif 
