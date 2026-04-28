#include "dashboardview.h"

#include "../../components/horizontallistviewgallery.h"
#include "../../components/libraryactionmenu.h"
#include "../../components/mediaimageeditdialog.h"
#include "../../components/moderntoast.h"
#include "../../components/mediasectionwidget.h"
#include "../../utils/dashboardsectionorderutils.h"
#include "../../utils/textwraputils.h"
#include "../media/mediacarddelegate.h"
#include "../media/medialistmodel.h"
#include <QApplication>
#include <QDebug>
#include <QHBoxLayout>
#include <QLabel>
#include <QListView>
#include <QPointer>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSet>
#include <QShowEvent>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <utility>
#include <vector>
#include <config/config_keys.h>
#include <config/configstore.h>
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>

namespace {

bool isLibraryNavigationItem(const MediaItem& item)
{
    return item.type == "BoxSet" || item.type == "Playlist" ||
           item.type == "Folder";
}

MediaCardDelegate::CardStyle dashboardGalleryStyle()
{
    const bool isTile =
        ConfigStore::instance()->get<QString>(ConfigKeys::DefaultLibraryView,
                                              "poster") == "tile";
    return isTile ? MediaCardDelegate::LibraryTile
                  : MediaCardDelegate::Poster;
}

int dashboardGalleryHeight()
{
    return dashboardGalleryStyle() == MediaCardDelegate::LibraryTile ? 210 : 300;
}

} 

DashboardView::DashboardView(QEmbyCore* core, QWidget* parent)
    : BaseView(core, parent)
{
    setObjectName("dashboard-view");

    m_libraryDelegate =
        new MediaCardDelegate(MediaCardDelegate::LibraryTile, this);
    m_libraryDelegate->setShowMoreButtonForNonPlayableTiles(true);

    connect(m_libraryDelegate, &MediaCardDelegate::playRequested, this,
            &BaseView::handlePlayRequested);
    connect(m_libraryDelegate, &MediaCardDelegate::favoriteRequested, this,
            &BaseView::handleFavoriteRequested);
    connect(m_libraryDelegate, &MediaCardDelegate::moreMenuRequested, this,
            &BaseView::handleMoreMenuRequested);

    setupUi();

    connect(m_libraryListView, &QListView::clicked, this,
            [this](const QModelIndex& index) {
                const MediaItem item = m_libraryModel->getItem(index);
                Q_EMIT navigateToLibrary(item.id, item.name);
            });

    connect(ConfigStore::instance(), &ConfigStore::valueChanged, this,
            [this](const QString& key, const QVariant&) {
                const QString sid = currentServerId();
                const QString customOrderEnabledKey = ConfigKeys::forServer(
                    sid, ConfigKeys::CustomHomeSectionOrderEnabled);
                const QString homeSectionOrderKey =
                    ConfigKeys::forServer(sid, ConfigKeys::HomeSectionOrder);

                if (key == customOrderEnabledKey || key == homeSectionOrderKey) {
                    applyDashboardSectionOrder();
                    if (isVisible()) {
                        launchDashboardTask(loadDashboardData());
                    }
                    return;
                }

                if (key == ConfigKeys::DefaultLibraryView ||
                    key == ConfigKeys::forServer(
                               sid, ConfigKeys::ShowContinueWatching) ||
                    key ==
                        ConfigKeys::forServer(sid, ConfigKeys::ShowLatestAdded) ||
                    key ==
                        ConfigKeys::forServer(sid, ConfigKeys::ShowRecommended) ||
                    key == ConfigKeys::forServer(
                               sid, ConfigKeys::ShowMediaLibraries) ||
                    key ==
                        ConfigKeys::forServer(sid, ConfigKeys::ShowEachLibrary)) {
                    if (isVisible()) {
                        launchDashboardTask(loadDashboardData());
                    }
                }
            });

    applyDashboardSectionOrder();
}

void DashboardView::launchDashboardTask(QCoro::Task<void>&& task)
{
    QCoro::connect(std::move(task), this, []() {});
}

void DashboardView::setupUi()
{
    auto* dashLayout = new QVBoxLayout(this);
    dashLayout->setContentsMargins(0, 0, 0, 0);

    m_mainScrollArea = new QScrollArea(this);
    m_mainScrollArea->setObjectName("dashboard-scroll");
    m_mainScrollArea->setWidgetResizable(true);
    m_mainScrollArea->setFrameShape(QFrame::NoFrame);
    m_mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* container = new QWidget(m_mainScrollArea);
    container->setObjectName("dashboard-container");

    m_containerLayout = new QVBoxLayout(container);
    m_containerLayout->setContentsMargins(20, 20, 20, 20);
    m_containerLayout->setSpacing(0);

    const MediaCardDelegate::CardStyle galleryStyle = dashboardGalleryStyle();
    const int galleryHeight = dashboardGalleryHeight();

    auto connectGallerySignals = [this](HorizontalListViewGallery* gallery) {
        gallery->listView()->setProperty("isHorizontalListView", true);
        gallery->listView()->viewport()->installEventFilter(this);

        connect(gallery, &HorizontalListViewGallery::itemClicked, this,
                [this](const MediaItem& item) {
                    if (isLibraryNavigationItem(item)) {
                        Q_EMIT navigateToLibrary(item.id, item.name);
                    } else {
                        Q_EMIT navigateToDetail(item.id, item.name);
                    }
                });
        connect(gallery, &HorizontalListViewGallery::playRequested, this,
                &BaseView::handlePlayRequested);
        connect(gallery, &HorizontalListViewGallery::favoriteRequested, this,
                &BaseView::handleFavoriteRequested);
        connect(gallery, &HorizontalListViewGallery::moreMenuRequested, this,
                &BaseView::handleMoreMenuRequested);
    };

    auto createGallerySection =
        [this, container, galleryHeight, &connectGallerySignals](
            QWidget* header, HorizontalListViewGallery** outGallery,
            MediaCardDelegate::CardStyle style) {
            auto* section = new QWidget(container);
            auto* layout = new QVBoxLayout(section);
            layout->setContentsMargins(0, 0, 0, 0);
            layout->setSpacing(0);
            layout->addWidget(header);

            *outGallery = new HorizontalListViewGallery(m_core, section);
            (*outGallery)->setFixedHeight(galleryHeight);
            (*outGallery)->setCardStyle(style);
            connectGallerySignals(*outGallery);

            layout->addWidget(*outGallery);
            return section;
        };

    m_resumeHeader = createSectionHeader(tr("Continue Watching"), "resume");
    m_resumeSection =
        createGallerySection(m_resumeHeader, &m_resumeGallery, galleryStyle);

    m_latestHeader = createSectionHeader(tr("Latest Media"), "latest");
    m_latestSection =
        createGallerySection(m_latestHeader, &m_latestGallery, galleryStyle);

    m_recommendHeader = createSectionHeader(tr("Recommended"), "recommended");
    m_recommendSection = createGallerySection(m_recommendHeader,
                                              &m_recommendGallery, galleryStyle);

    m_libraryGridSection = new QWidget(container);
    auto* libraryGridLayout = new QVBoxLayout(m_libraryGridSection);
    libraryGridLayout->setContentsMargins(0, 0, 0, 0);
    libraryGridLayout->setSpacing(0);

    m_libraryTitle = new QLabel(tr("All Libraries"), m_libraryGridSection);
    m_libraryTitle->setObjectName("dashboard-section-title");
    m_libraryListView = createGridListView(&m_libraryModel);

    libraryGridLayout->addWidget(m_libraryTitle);
    libraryGridLayout->addWidget(m_libraryListView);

    m_librarySectionsContainer = new QWidget(container);
    m_librarySectionsLayout = new QVBoxLayout(m_librarySectionsContainer);
    m_librarySectionsLayout->setContentsMargins(0, 0, 0, 0);
    m_librarySectionsLayout->setSpacing(0);

    m_containerLayout->addWidget(m_resumeSection);
    m_containerLayout->addWidget(m_latestSection);
    m_containerLayout->addWidget(m_recommendSection);
    m_containerLayout->addWidget(m_libraryGridSection);
    m_containerLayout->addWidget(m_librarySectionsContainer);
    m_containerLayout->addStretch();

    m_resumeSection->hide();
    m_latestSection->hide();
    m_recommendSection->hide();
    m_libraryGridSection->hide();
    m_librarySectionsContainer->hide();

    m_mainScrollArea->setWidget(container);
    dashLayout->addWidget(m_mainScrollArea);

    m_vScrollAnim = new QPropertyAnimation(m_mainScrollArea->verticalScrollBar(),
                                           "value", this);
    m_vScrollAnim->setEasingCurve(QEasingCurve::OutCubic);
    m_vScrollAnim->setDuration(450);

    m_mainScrollArea->viewport()->installEventFilter(this);
}

void DashboardView::applyDashboardSectionOrder()
{
    if (!m_containerLayout) {
        return;
    }

    const QString sid = currentServerId();
    if (!sid.isEmpty()) {
        auto* store = ConfigStore::instance();
        const QString customOrderEnabledKey = ConfigKeys::forServer(
            sid, ConfigKeys::CustomHomeSectionOrderEnabled);
        const QString homeSectionOrderKey =
            ConfigKeys::forServer(sid, ConfigKeys::HomeSectionOrder);

        if (store->get<bool>(customOrderEnabledKey, false)) {
            const QStringList rawOrder =
                store->get<QStringList>(homeSectionOrderKey, {});
            if (!rawOrder.isEmpty()) {
                const QStringList normalizedOrder =
                    DashboardSectionOrderUtils::normalizeSectionOrder(rawOrder);
                if (normalizedOrder != rawOrder) {
                    store->set(homeSectionOrderKey, normalizedOrder);
                }
            }
        }
    }

    int insertIndex = 0;
    for (const QString& sectionId : dashboardSectionOrder()) {
        QWidget* widget = sectionWidgetForId(sectionId);
        if (widget) {
            m_containerLayout->insertWidget(insertIndex++, widget);
        }
    }
}

QStringList DashboardView::dashboardSectionOrder() const
{
    const QString sid = currentServerId();
    if (sid.isEmpty()) {
        return DashboardSectionOrderUtils::defaultSectionOrder();
    }

    auto* store = ConfigStore::instance();
    const QString customOrderEnabledKey = ConfigKeys::forServer(
        sid, ConfigKeys::CustomHomeSectionOrderEnabled);
    if (!store->get<bool>(customOrderEnabledKey, false)) {
        return DashboardSectionOrderUtils::defaultSectionOrder();
    }

    const QString homeSectionOrderKey =
        ConfigKeys::forServer(sid, ConfigKeys::HomeSectionOrder);
    return DashboardSectionOrderUtils::normalizeSectionOrder(
        store->get<QStringList>(homeSectionOrderKey,
                                DashboardSectionOrderUtils::defaultSectionOrder()));
}

QString DashboardView::currentServerId() const
{
    if (!m_core || !m_core->serverManager()) {
        return {};
    }

    return m_core->serverManager()->activeProfile().id;
}

QWidget* DashboardView::sectionWidgetForId(const QString& sectionId) const
{
    if (sectionId == QLatin1String(
                         DashboardSectionOrderUtils::ContinueWatchingSectionId)) {
        return m_resumeSection;
    }
    if (sectionId ==
        QLatin1String(DashboardSectionOrderUtils::LatestMediaSectionId)) {
        return m_latestSection;
    }
    if (sectionId ==
        QLatin1String(DashboardSectionOrderUtils::RecommendedSectionId)) {
        return m_recommendSection;
    }
    if (sectionId ==
        QLatin1String(DashboardSectionOrderUtils::AllLibrariesSectionId)) {
        return m_libraryGridSection;
    }
    if (sectionId == QLatin1String(
                         DashboardSectionOrderUtils::
                             EachLibrarySectionsSectionId)) {
        return m_librarySectionsContainer;
    }

    return nullptr;
}

void DashboardView::clearLibraryGallerySections()
{
    if (!m_librarySectionsLayout) {
        return;
    }

    for (MediaSectionWidget* section : std::as_const(m_libraryGalleries)) {
        if (!section) {
            continue;
        }

        m_librarySectionsLayout->removeWidget(section);
        section->deleteLater();
    }

    m_libraryGalleries.clear();
}

QWidget* DashboardView::createSectionHeader(const QString& title,
                                            const QString& type)
{
    auto* w = new QWidget(this);
    auto* l = new QHBoxLayout(w);
    l->setContentsMargins(0, 0, 0, 0);

    auto* label = new QLabel(title, w);
    label->setObjectName("dashboard-section-title");

    auto* btn = new QPushButton(tr("See All >"), w);
    btn->setObjectName("section-more-btn");
    btn->setCursor(Qt::PointingHandCursor);

    connect(btn, &QPushButton::clicked, this,
            [this, type, title]() { Q_EMIT navigateToCategory(type, title); });

    l->addWidget(label);
    l->addStretch();
    l->addWidget(btn);

    return w;
}

QListView* DashboardView::createGridListView(MediaListModel** outModel)
{
    auto* listView = new QListView(this);
    listView->setSelectionMode(QAbstractItemView::NoSelection);
    listView->setViewMode(QListView::IconMode);
    listView->setFlow(QListView::LeftToRight);
    listView->setWrapping(true);
    listView->setSpacing(0);
    listView->setMovement(QListView::Static);
    listView->setResizeMode(QListView::Adjust);
    listView->setUniformItemSizes(true);
    listView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    listView->setFrameShape(QFrame::NoFrame);
    listView->setMouseTracking(true);
    listView->viewport()->setAttribute(Qt::WA_Hover);
    listView->viewport()->installEventFilter(this);
    listView->setStyleSheet(
        "QListView { background: transparent; outline: none; }");

    *outModel = new MediaListModel(500, m_core, this);
    listView->setModel(*outModel);
    listView->setItemDelegate(m_libraryDelegate);
    return listView;
}

bool DashboardView::eventFilter(QObject* obj, QEvent* event)
{
    if (m_libraryListView && obj == m_libraryListView->viewport() &&
        event->type() == QEvent::ToolTip) {
        return TextWrapUtils::showWrappedMediaItemToolTip(m_libraryListView,
                                                          event);
    }

    if (event->type() == QEvent::Wheel) {
        const bool isHorizontalViewport =
            obj->parent() &&
            obj->parent()->property("isHorizontalListView").toBool();
        const bool isMainViewport = (obj == m_mainScrollArea->viewport());

        if (isHorizontalViewport || isMainViewport) {
            auto* we = static_cast<QWheelEvent*>(event);
            QScrollBar* vBar = m_mainScrollArea->verticalScrollBar();

            if (vBar) {
                int currentVal = vBar->value();

                if (m_vScrollAnim->state() == QAbstractAnimation::Running) {
                    currentVal = m_vScrollTarget;
                }

                const int step = we->angleDelta().y();
                int newTarget = currentVal - step;
                newTarget = qBound(vBar->minimum(), newTarget, vBar->maximum());

                if (newTarget != vBar->value()) {
                    m_vScrollTarget = newTarget;
                    m_vScrollAnim->stop();
                    m_vScrollAnim->setStartValue(vBar->value());
                    m_vScrollAnim->setEndValue(m_vScrollTarget);
                    m_vScrollAnim->start();
                }
            }
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void DashboardView::adjustLibraryGridHeight()
{
    if (!m_libraryListView || !m_libraryModel) {
        return;
    }

    const int count = m_libraryModel->rowCount();
    if (count == 0) {
        m_libraryListView->setFixedHeight(0);
        return;
    }

    m_libraryListView->setSpacing(0);

    const int padding = 40;
    const int scrollBarWidth =
        qApp->style()->pixelMetric(QStyle::PM_ScrollBarExtent);
    int availableWidth = width() - padding - scrollBarWidth;
    if (availableWidth < 100) {
        availableWidth = 100;
    }

    const int defaultCellWidth = 196;
    const int tolerance = 10;

    int cols = (availableWidth + tolerance) / defaultCellWidth;
    if (cols < 1) {
        cols = 1;
    }

    const int cellWidth = availableWidth / cols;
    const int remainder = availableWidth - (cols * cellWidth);
    const int leftPad = remainder / 2;
    const int rightPad = remainder - leftPad;

    m_libraryListView->setStyleSheet(
        QString("QListView { background: transparent; outline: none; "
                "padding-left: %1px; padding-right: %2px; }")
            .arg(leftPad)
            .arg(rightPad));

    const int imgWidth = cellWidth - 16;
    const int imgHeight = qRound(imgWidth * 9.0 / 16.0);
    const int cellHeight = imgHeight + 16 + 26;

    m_libraryDelegate->setTileSize(QSize(cellWidth, cellHeight));
    m_libraryListView->doItemsLayout();

    const int rows = (count + cols - 1) / cols;
    m_libraryListView->setFixedHeight(rows * cellHeight);
}

void DashboardView::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    adjustLibraryGridHeight();
}

void DashboardView::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    launchDashboardTask(loadDashboardData());
}

bool DashboardView::isManageableDashboardLibraryCard(const MediaItem& item) const
{
    if (!m_libraryModel || item.id.trimmed().isEmpty()) {
        return false;
    }

    const QString normalizedCollectionType = item.collectionType.trimmed().toLower();
    if (normalizedCollectionType == "playlists" ||
        normalizedCollectionType == "boxsets") {
        return false;
    }

    const QString normalizedType = item.type.trimmed().toLower();
    if (normalizedType == "playlist" || normalizedType == "boxset") {
        return false;
    }

    for (int row = 0; row < m_libraryModel->rowCount(); ++row) {
        const QModelIndex index = m_libraryModel->index(row, 0);
        if (!index.isValid()) {
            continue;
        }

        if (m_libraryModel->getItem(index).id == item.id) {
            return true;
        }
    }

    return false;
}

CardContextMenuRequest DashboardView::showCardContextMenu(
    const MediaItem& item, const QPoint& globalPos)
{
    const bool canShowLibraryMenu =
        isManageableDashboardLibraryCard(item) && m_core &&
        m_core->serverManager() &&
        m_core->serverManager()->activeProfile().isValid() &&
        m_core->serverManager()->activeProfile().isAdmin;
    if (!canShowLibraryMenu) {
        return BaseView::showCardContextMenu(item, globalPos);
    }

    VirtualFolder folder;
    folder.itemId = item.id;
    folder.name = item.name;

    LibraryActionMenu menu(folder, false, this);
    const LibraryContextMenuRequest libraryRequest = menu.execRequest(globalPos);

    CardContextMenuRequest request;
    switch (libraryRequest.action) {
    case LibraryContextMenuAction::None:
        return request;
    case LibraryContextMenuAction::EditImages:
        request.action = CardContextMenuAction::EditImages;
        return request;
    case LibraryContextMenuAction::RefreshMetadata:
        request.action = CardContextMenuAction::RefreshMetadata;
        return request;
    case LibraryContextMenuAction::ScanLibraryFiles:
        request.action = CardContextMenuAction::ScanLibraryFiles;
        return request;
    }

    return request;
}

void DashboardView::dispatchCardContextMenuRequest(
    const MediaItem& item, const CardContextMenuRequest& request)
{
    if (!request.isValid()) {
        return;
    }

    if (isManageableDashboardLibraryCard(item)) {
        switch (request.action) {
        case CardContextMenuAction::EditImages:
            openDashboardLibraryImageEditor(item);
            return;
        case CardContextMenuAction::RefreshMetadata:
            launchDashboardTask(
                executeDashboardLibraryRefresh(item, true, true, true));
            return;
        case CardContextMenuAction::ScanLibraryFiles:
            launchDashboardTask(
                executeDashboardLibraryRefresh(item, false, false, false));
            return;
        default:
            break;
        }
    }

    BaseView::dispatchCardContextMenuRequest(item, request);
}

void DashboardView::openDashboardLibraryImageEditor(const MediaItem& item)
{
    if (!m_core || !m_core->serverManager() ||
        !m_core->serverManager()->activeProfile().isValid() ||
        !m_core->serverManager()->activeProfile().isAdmin) {
        ModernToast::showMessage(
            BaseView::tr("This action requires administrator privileges"));
        return;
    }

    MediaImageEditTarget target;
    target.itemId = item.id;
    target.imageItemId =
        item.images.primaryImageItemId.isEmpty() ? item.id
                                                 : item.images.primaryImageItemId;
    target.displayName = item.name;
    target.itemType = item.type;
    target.mediaType = item.mediaType;
    target.collectionType = item.collectionType;
    target.isLibrary = true;

    qDebug() << "[DashboardView] Opening dashboard library image editor"
             << "| itemId=" << item.id
             << "| imageItemId=" << target.imageItemId
             << "| itemType=" << item.type
             << "| collectionType=" << item.collectionType;

    MediaImageEditDialog dialog(m_core, target, this);
    dialog.exec();
    if (!dialog.hasChanges()) {
        return;
    }

    if (m_core->mediaService()) {
        m_core->mediaService()->clearUserViewsCache();
    }
    if (m_libraryModel) {
        m_libraryModel->clearImageCache();
    }
    for (MediaSectionWidget* section : std::as_const(m_libraryGalleries)) {
        if (section && section->gallery()) {
            section->gallery()->clearImageCache();
        }
    }

    qDebug() << "[DashboardView] Library image editor applied changes"
             << "| itemId=" << item.id
             << "| imageItemId=" << target.imageItemId;
    launchDashboardTask(loadDashboardData());
}

QCoro::Task<void> DashboardView::loadDashboardData()
{
    const int generation = ++m_loadGeneration;

    auto* store = ConfigStore::instance();
    const QString sid = currentServerId();
    const bool showResume = store->get<bool>(
        ConfigKeys::forServer(sid, ConfigKeys::ShowContinueWatching), true);
    const bool showLatest = store->get<bool>(
        ConfigKeys::forServer(sid, ConfigKeys::ShowLatestAdded), true);
    const bool showRecommended = store->get<bool>(
        ConfigKeys::forServer(sid, ConfigKeys::ShowRecommended), true);
    const bool showLibraries = store->get<bool>(
        ConfigKeys::forServer(sid, ConfigKeys::ShowMediaLibraries), true);
    const bool showEachLibrary = store->get<bool>(
        ConfigKeys::forServer(sid, ConfigKeys::ShowEachLibrary), true);

    qDebug() << "[DashboardView] loadDashboardData"
             << "| generation=" << generation << "| serverId=" << sid
             << "| showResume=" << showResume
             << "| showLatest=" << showLatest
             << "| showRecommended=" << showRecommended
             << "| showLibraries=" << showLibraries
             << "| showEachLibrary=" << showEachLibrary;

    applyDashboardSectionOrder();

    const MediaCardDelegate::CardStyle style = dashboardGalleryStyle();
    const int galleryHeight = dashboardGalleryHeight();

    if (m_resumeGallery) {
        m_resumeGallery->setCardStyle(style);
        m_resumeGallery->setFixedHeight(galleryHeight);
    }
    if (m_latestGallery) {
        m_latestGallery->setCardStyle(style);
        m_latestGallery->setFixedHeight(galleryHeight);
    }
    if (m_recommendGallery) {
        m_recommendGallery->setCardStyle(style);
        m_recommendGallery->setFixedHeight(galleryHeight);
    }

    
    
    
    
    
    const bool shimmerEnabled =
        store->get<bool>(ConfigKeys::ShimmerAnimation, false);

    if (m_resumeSection) {
        m_resumeSection->setVisible(showResume);
        if (showResume && shimmerEnabled && m_resumeGallery) {
            m_resumeGallery->setLoading(true);
        }
    }
    if (m_latestSection) {
        m_latestSection->setVisible(showLatest);
        if (showLatest && shimmerEnabled && m_latestGallery) {
            m_latestGallery->setLoading(true);
        }
    }
    if (m_recommendSection) {
        m_recommendSection->setVisible(showRecommended);
        if (showRecommended && shimmerEnabled && m_recommendGallery) {
            m_recommendGallery->setLoading(true);
        }
    }
    if (m_libraryGridSection) {
        m_libraryGridSection->setVisible(showLibraries);
    }
    if (m_librarySectionsContainer) {
        m_librarySectionsContainer->setVisible(showEachLibrary);
    }

    loadResumeSection(showResume, generation);
    loadLatestSection(showLatest, generation);
    loadRecommendedSection(showRecommended, generation);
    loadLibrarySections(showLibraries, showEachLibrary, generation);
    co_return;
}

QCoro::Task<void> DashboardView::executeDashboardLibraryRefresh(
    MediaItem item, bool replaceAllMetadata, bool replaceAllImages,
    bool isMetadataRefresh)
{
    const QString itemId = item.id.trimmed();
    const QString itemName =
        item.name.trimmed().isEmpty() ? BaseView::tr("this item")
                                      : item.name.trimmed();
    if (itemId.isEmpty() || !m_core || !m_core->adminService() ||
        !m_core->mediaService()) {
        co_return;
    }

    if (!m_core->serverManager() ||
        !m_core->serverManager()->activeProfile().isValid() ||
        !m_core->serverManager()->activeProfile().isAdmin) {
        ModernToast::showMessage(
            BaseView::tr("This action requires administrator privileges"));
        co_return;
    }

    QPointer<DashboardView> safeThis(this);
    QPointer<AdminService> adminService(m_core->adminService());
    QPointer<MediaService> mediaService(m_core->mediaService());
    const char* actionName =
        isMetadataRefresh ? "RefreshMetadata" : "ScanLibraryFiles";

    try {
        qDebug() << "[DashboardView] Starting dashboard library action"
                 << "| action=" << actionName
                 << "| itemId=" << itemId
                 << "| itemName=" << item.name
                 << "| replaceAllMetadata=" << replaceAllMetadata
                 << "| replaceAllImages=" << replaceAllImages;
        co_await adminService->refreshItemMetadata(itemId, replaceAllMetadata,
                                                   replaceAllImages);
        if (!safeThis) {
            co_return;
        }

        ModernToast::showMessage(
            isMetadataRefresh
                ? BaseView::tr("Refreshing metadata for \"%1\"...")
                      .arg(itemName)
                : tr("Scanning \"%1\"...").arg(itemName),
            2000);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        qWarning() << "[DashboardView] Dashboard library action failed"
                   << "| action=" << actionName
                   << "| itemId=" << itemId
                   << "| itemName=" << item.name
                   << "| error=" << e.what();
        ModernToast::showMessage(
            isMetadataRefresh
                ? BaseView::tr("Metadata refresh failed: %1")
                      .arg(QString::fromUtf8(e.what()))
                : tr("Scan failed: %1").arg(QString::fromUtf8(e.what())),
            3000);
        co_return;
    }

    try {
        if (!mediaService) {
            co_return;
        }

        const MediaItem latestItem = co_await mediaService->getItemDetail(itemId);
        if (safeThis) {
            safeThis->onMediaItemUpdated(latestItem);
        }
    } catch (const std::exception& e) {
        qWarning() << "[DashboardView] Failed to refresh dashboard library card"
                   << "| action=" << actionName
                   << "| itemId=" << itemId
                   << "| error=" << e.what();
    }

    QTimer::singleShot(2000, this, [this, itemId]() {
        if (!m_core || !m_core->mediaService()) {
            return;
        }

        qDebug() << "[DashboardView] Running delayed dashboard library reload"
                 << "| itemId=" << itemId;
        m_core->mediaService()->clearUserViewsCache();
        launchDashboardTask(loadDashboardData());
    });
}

QCoro::Task<void> DashboardView::loadResumeSection(bool show, int generation)
{
    if (!show) {
        if (m_resumeSection) {
            m_resumeSection->setVisible(false);
        }
        co_return;
    }

    QPointer<DashboardView> guard(this);
    auto* mediaService = m_core->mediaService();

    try {
        QList<MediaItem> rawResumeItems =
            co_await mediaService->getResumeItems(50);
        if (!guard || m_loadGeneration != generation) {
            co_return;
        }

        QList<MediaItem> resumeItems;
        QSet<QString> seenSeriesIds;
        QStringList seriesIdsToFetch;
        QList<int> insertIndices;

        for (const MediaItem& item : rawResumeItems) {
            if (item.type == "Episode" && !item.seriesId.isEmpty()) {
                if (seenSeriesIds.contains(item.seriesId)) {
                    continue;
                }

                seenSeriesIds.insert(item.seriesId);
                seriesIdsToFetch.append(item.seriesId);
                insertIndices.append(resumeItems.size());
                resumeItems.append(MediaItem {});
            } else {
                resumeItems.append(item);
            }
        }

        std::vector<QCoro::Task<MediaItem>> detailTasks;
        detailTasks.reserve(seriesIdsToFetch.size());
        for (const QString& seriesId : seriesIdsToFetch) {
            detailTasks.push_back(mediaService->getItemDetail(seriesId));
        }

        for (int i = 0; i < static_cast<int>(detailTasks.size()); ++i) {
            try {
                MediaItem seriesItem = co_await std::move(detailTasks[i]);
                if (!guard || m_loadGeneration != generation) {
                    co_return;
                }

                resumeItems[insertIndices[i]] = seriesItem;
            } catch (...) {
                if (!guard || m_loadGeneration != generation) {
                    co_return;
                }
            }
        }

        for (int i = resumeItems.size() - 1; i >= 0; --i) {
            if (resumeItems[i].id.isEmpty()) {
                resumeItems.removeAt(i);
            }
        }

        m_resumeGallery->setItems(resumeItems);
        if (m_resumeSection) {
            m_resumeSection->setVisible(!resumeItems.isEmpty());
        }
    } catch (const std::exception& e) {
        if (!guard) {
            co_return;
        }

        qDebug() << "Dashboard failed to fetch resume items:" << e.what();
        if (m_resumeGallery) {
            m_resumeGallery->setLoading(false);
        }
        if (m_resumeSection) {
            m_resumeSection->setVisible(false);
        }
    }
}

QCoro::Task<void> DashboardView::loadLatestSection(bool show, int generation)
{
    if (!show) {
        if (m_latestSection) {
            m_latestSection->setVisible(false);
        }
        co_return;
    }

    QPointer<DashboardView> guard(this);
    auto* mediaService = m_core->mediaService();

    try {
        QList<MediaItem> latestItems = co_await mediaService->getLatestItems(50);
        if (!guard || m_loadGeneration != generation) {
            co_return;
        }

        m_latestGallery->setItems(latestItems);
        if (m_latestSection) {
            m_latestSection->setVisible(!latestItems.isEmpty());
        }
    } catch (const std::exception& e) {
        if (!guard) {
            co_return;
        }

        qDebug() << "Dashboard failed to fetch latest items:" << e.what();
        if (m_latestGallery) {
            m_latestGallery->setLoading(false);
        }
        if (m_latestSection) {
            m_latestSection->setVisible(false);
        }
    }
}

QCoro::Task<void> DashboardView::loadRecommendedSection(bool show,
                                                        int generation)
{
    if (!show) {
        if (m_recommendSection) {
            m_recommendSection->setVisible(false);
        }
        co_return;
    }

    QPointer<DashboardView> guard(this);
    auto* mediaService = m_core->mediaService();

    try {
        QList<MediaItem> recommendedItems =
            co_await mediaService->getRecommendedMovies(50);
        if (!guard || m_loadGeneration != generation) {
            co_return;
        }

        m_recommendGallery->setItems(recommendedItems);
        if (m_recommendSection) {
            m_recommendSection->setVisible(!recommendedItems.isEmpty());
        }
    } catch (const std::exception& e) {
        if (!guard) {
            co_return;
        }

        qDebug() << "Dashboard failed to fetch recommended items:" << e.what();
        if (m_recommendGallery) {
            m_recommendGallery->setLoading(false);
        }
        if (m_recommendSection) {
            m_recommendSection->setVisible(false);
        }
    }
}

QCoro::Task<void> DashboardView::loadLibrarySections(bool showLibraries,
                                                     bool showEachLibrary,
                                                     int generation)
{
    if (!showLibraries && !showEachLibrary) {
        if (m_libraryGridSection) {
            m_libraryGridSection->setVisible(false);
        }
        if (m_libraryModel) {
            m_libraryModel->setItems(QList<MediaItem> {});
        }
        clearLibraryGallerySections();
        if (m_librarySectionsContainer) {
            m_librarySectionsContainer->setVisible(false);
        }
        adjustLibraryGridHeight();
        co_return;
    }

    QPointer<DashboardView> guard(this);
    auto* mediaService = m_core->mediaService();
    const QString sid = currentServerId();
    const int previousLibraryCount =
        m_libraryModel ? m_libraryModel->rowCount() : 0;
    const int previousEachLibrarySectionCount = m_libraryGalleries.size();
    bool hasReusableEachLibrarySections =
        previousEachLibrarySectionCount > 0;
    if (hasReusableEachLibrarySections) {
        for (MediaSectionWidget* section : std::as_const(m_libraryGalleries)) {
            if (!section || section->property("serverId").toString() != sid) {
                hasReusableEachLibrarySections = false;
                break;
            }
        }
    }

    try {
        const QList<MediaItem> userViews = co_await mediaService->getUserViews();
        if (!guard || m_loadGeneration != generation) {
            co_return;
        }

        qDebug() << "[DashboardView] loadLibrarySections fetched"
                 << "| generation=" << generation
                 << "| showLibraries=" << showLibraries
                 << "| showEachLibrary=" << showEachLibrary
                 << "| previousLibraryCount=" << previousLibraryCount
                 << "| previousEachLibrarySectionCount="
                 << previousEachLibrarySectionCount
                 << "| userViewCount=" << userViews.size();

        if (!showLibraries) {
            if (m_libraryGridSection) {
                m_libraryGridSection->setVisible(false);
            }
            if (m_libraryModel) {
                m_libraryModel->setItems(QList<MediaItem> {});
            }
            adjustLibraryGridHeight();
        } else {
            if (!userViews.isEmpty() || previousLibraryCount == 0) {
                m_libraryModel->setItems(userViews);
            } else {
                qDebug()
                    << "[DashboardView] keeping previous library grid items "
                       "because fetched views are unexpectedly empty";
            }
            if (m_libraryGridSection) {
                m_libraryGridSection->setVisible(!userViews.isEmpty() ||
                                                previousLibraryCount > 0);
            }
            QTimer::singleShot(0, this, &DashboardView::adjustLibraryGridHeight);
        }

        if (!showEachLibrary) {
            clearLibraryGallerySections();
            if (m_librarySectionsContainer) {
                m_librarySectionsContainer->setVisible(false);
            }
            co_return;
        }

        if (userViews.isEmpty()) {
            if (hasReusableEachLibrarySections) {
                qDebug()
                    << "[DashboardView] keeping previous each-library sections "
                       "because fetched views are unexpectedly empty";
                if (m_librarySectionsContainer) {
                    m_librarySectionsContainer->setVisible(true);
                }
            } else {
                clearLibraryGallerySections();
                if (m_librarySectionsContainer) {
                    m_librarySectionsContainer->setVisible(false);
                }
            }
            co_return;
        }

        const MediaCardDelegate::CardStyle libGalleryStyle =
            dashboardGalleryStyle();
        const int libGalleryHeight = dashboardGalleryHeight();

        bool canReuse = (m_libraryGalleries.size() == userViews.size());
        if (canReuse) {
            for (int i = 0; i < userViews.size(); ++i) {
                if (!m_libraryGalleries[i] ||
                    m_libraryGalleries[i]->property("libraryId").toString() !=
                        userViews[i].id ||
                    m_libraryGalleries[i]->property("libraryName").toString() !=
                        userViews[i].name) {
                    canReuse = false;
                    break;
                }
            }
        }

        if (!canReuse) {
            clearLibraryGallerySections();

            for (const MediaItem& view : userViews) {
                auto* section = new MediaSectionWidget(view.name, m_core,
                                                       m_librarySectionsContainer);
                section->setProperty("serverId", sid);
                section->setProperty("libraryId", view.id);
                section->setProperty("libraryName", view.name);

                if (section->layout()) {
                    section->layout()->setContentsMargins(0, 20, 0, 0);
                }

                auto* seeAllBtn = new QPushButton(tr("See All >"), section);
                seeAllBtn->setObjectName("section-more-btn");
                seeAllBtn->setCursor(Qt::PointingHandCursor);

                const QString libraryId = view.id;
                const QString libraryName = view.name;
                connect(seeAllBtn, &QPushButton::clicked, this,
                        [this, libraryId, libraryName]() {
                            Q_EMIT navigateToLibrary(libraryId, libraryName);
                        });

                QWidget* headerContainer =
                    section->findChild<QWidget*>("section-header");
                if (headerContainer && headerContainer->layout()) {
                    headerContainer->layout()->addWidget(seeAllBtn);
                }

                if (section->gallery() && section->gallery()->listView()) {
                    section->gallery()->listView()->setProperty(
                        "isHorizontalListView", true);
                    section->gallery()->listView()->viewport()->installEventFilter(
                        this);
                }

                connect(section, &MediaSectionWidget::itemClicked, this,
                        [this](const MediaItem& item) {
                            if (isLibraryNavigationItem(item)) {
                                Q_EMIT navigateToLibrary(item.id, item.name);
                            } else {
                                Q_EMIT navigateToDetail(item.id, item.name);
                            }
                        });
                connect(section, &MediaSectionWidget::playRequested, this,
                        &BaseView::handlePlayRequested);
                connect(section, &MediaSectionWidget::favoriteRequested, this,
                        &BaseView::handleFavoriteRequested);
                connect(section, &MediaSectionWidget::moreMenuRequested, this,
                        &BaseView::handleMoreMenuRequested);

                m_librarySectionsLayout->addWidget(section);
                m_libraryGalleries.append(section);
            }
        }

        for (int i = 0; i < userViews.size(); ++i) {
            MediaSectionWidget* section = m_libraryGalleries.value(i, nullptr);
            if (!section) {
                continue;
            }

            const MediaItem& view = userViews[i];
            section->setProperty("serverId", sid);
            section->setProperty("libraryId", view.id);
            section->setProperty("libraryName", view.name);
            section->setTitle(view.name);
            section->setCardStyle(libGalleryStyle);
            section->setGalleryHeight(libGalleryHeight);

            const QString libraryId = view.id;
            section->loadAsync(
                [mediaService, libraryId]() -> QCoro::Task<QList<MediaItem>> {
                    co_return co_await mediaService->getLibraryItems(
                        libraryId, "DateCreated", "Descending", "",
                        "Movie,Series", 0, 20, true);
                });
        }

        if (m_librarySectionsContainer) {
            m_librarySectionsContainer->setVisible(true);
        }
    } catch (const std::exception& e) {
        if (!guard) {
            co_return;
        }

        qDebug() << "[DashboardView] loadLibrarySections failed"
                 << "| generation=" << generation
                 << "| showLibraries=" << showLibraries
                 << "| showEachLibrary=" << showEachLibrary
                 << "| previousLibraryCount=" << previousLibraryCount
                 << "| previousEachLibrarySectionCount="
                 << previousEachLibrarySectionCount
                 << "| error=" << e.what();

        if (showLibraries && previousLibraryCount > 0 && m_libraryGridSection) {
            m_libraryGridSection->setVisible(true);
            QTimer::singleShot(0, this, &DashboardView::adjustLibraryGridHeight);
        }
        if (showEachLibrary && hasReusableEachLibrarySections &&
            m_librarySectionsContainer) {
            m_librarySectionsContainer->setVisible(true);
        }
    }
}

void DashboardView::onMediaItemUpdated(const MediaItem& item)
{
    if (m_resumeGallery) {
        const bool hasProgress = (item.userData.playbackPositionTicks > 0) ||
                                 (item.userData.playedPercentage > 0.0 &&
                                  item.userData.playedPercentage < 100.0);

        if (item.userData.played || !hasProgress) {
            m_resumeGallery->removeItem(item.id);
        } else {
            m_resumeGallery->updateItem(item);
        }
    }

    if (m_latestGallery) {
        m_latestGallery->updateItem(item);
    }
    if (m_recommendGallery) {
        m_recommendGallery->updateItem(item);
    }
    if (m_libraryModel) {
        m_libraryModel->updateItem(item);
    }

    for (MediaSectionWidget* section : std::as_const(m_libraryGalleries)) {
        if (section) {
            section->updateItem(item);
        }
    }
}

void DashboardView::onMediaItemRemoved(const QString& itemId)
{
    if (m_resumeGallery) {
        m_resumeGallery->removeItem(itemId);
    }
    if (m_latestGallery) {
        m_latestGallery->removeItem(itemId);
    }
    if (m_recommendGallery) {
        m_recommendGallery->removeItem(itemId);
    }
    if (m_libraryModel) {
        m_libraryModel->removeItem(itemId);
    }

    for (MediaSectionWidget* section : std::as_const(m_libraryGalleries)) {
        if (section && section->gallery()) {
            section->gallery()->removeItem(itemId);
        }
    }

    if (m_recommendSection && m_recommendGallery) {
        m_recommendSection->setVisible(m_recommendGallery->itemCount() > 0);
    }
}
