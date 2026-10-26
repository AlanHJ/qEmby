#include "pagelibrary.h"
#include "../../components/dashboardsectionorderwidget.h"
#include "../../components/modernnumberinput.h"
#include "../../components/moderntoast.h"
#include "../../components/moderncombobox.h"
#include "../../components/modernswitch.h"
#include "../../components/settingscard.h"
#include "../../components/settingssubpanel.h"
#include "../../utils/dashboardsectionorderutils.h"
#include "../../utils/featuredmediautils.h"
#include "../../managers/libraryartworkmanager.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include "fileutils.h"
#include "qembycore.h"
#include "services/manager/servermanager.h"
#include "services/media/mediaservice.h"
#include <QLabel>
#include <QSignalBlocker>
#include <QComboBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

namespace {
const QString kHomeSectionOrderIconPath =
    QStringLiteral(":/svg/dark/home-section-order.svg");
const QString kHomeSectionOrderDragIconPath =
    QStringLiteral(":/svg/dark/home-section-drag.svg");

ModernNumberInput *createRequestLimitInput(const QString &zeroText,
                                           QWidget *parent) {
  auto *input = new ModernNumberInput(parent);
  input->setSpecialValueText(zeroText);
  return input;
}
}

PageLibrary::PageLibrary(QEmbyCore *core, QWidget *parent)
    : SettingsPageBase(core, tr("Library"), parent) {
  
  QString sid = m_core->serverManager()->activeProfile().id;

  auto *carouselCard = new SettingsCard(
      ":/svg/dark/media-carousel.svg", tr("Show Media Carousel"),
      tr("Display the media carousel at the top of the home screen for this server"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowMediaCarousel), this,
      QVariant(true));
  m_mainLayout->addWidget(carouselCard);
  auto *carouselCountInput = new ModernNumberInput(this);
  carouselCountInput->setRange(FeaturedMediaUtils::MinimumSlides,
                               FeaturedMediaUtils::MaximumSlides);
  carouselCountInput->setAccessibleName(tr("Carousel Slide Count"));
  auto *carouselCountCard = new SettingsCard(
      ":/svg/dark/media-carousel.svg", tr("Carousel Slide Count"),
      tr("Number of carousel slides for this server (%1–%2, default %3)")
          .arg(FeaturedMediaUtils::MinimumSlides)
          .arg(FeaturedMediaUtils::MaximumSlides)
          .arg(FeaturedMediaUtils::DefaultSlides),
      carouselCountInput,
      ConfigKeys::forServer(sid, ConfigKeys::MediaCarouselCount), this,
      QVariant(FeaturedMediaUtils::DefaultSlides));
  m_mainLayout->addWidget(carouselCountCard);
  auto *carouselHeightCombo = new ModernComboBox(this);
  carouselHeightCombo->addItem(tr("35%"), QStringLiteral("35"));
  carouselHeightCombo->addItem(tr("50% (Default)"), QStringLiteral("50"));
  carouselHeightCombo->addItem(tr("65%"), QStringLiteral("65"));
  carouselHeightCombo->setAccessibleName(tr("Carousel Height"));
  auto *carouselHeightCard = new SettingsCard(
      ":/svg/dark/media-carousel.svg", tr("Carousel Height"),
      tr("Height relative to the home area below the title bar, capped at two-thirds of the displayed backdrop height. Applies to this server."),
      carouselHeightCombo,
      ConfigKeys::forServer(sid, ConfigKeys::MediaCarouselHeightPercent), this,
      QVariant(QStringLiteral("50")));
  m_mainLayout->addWidget(carouselHeightCard);
  carouselCard->setEnabled(!sid.isEmpty());
  carouselCountCard->setEnabled(!sid.isEmpty());
  carouselHeightCard->setEnabled(!sid.isEmpty());
  connect(m_core->serverManager(), &ServerManager::activeServerChanged,
          this, [carouselCard, carouselCountCard, carouselHeightCard](const ServerProfile &profile) {
            carouselCard->setEnabled(!profile.id.isEmpty());
            carouselCountCard->setEnabled(!profile.id.isEmpty());
            carouselHeightCard->setEnabled(!profile.id.isEmpty());
            if (!profile.id.isEmpty()) {
              carouselCard->setConfigKey(ConfigKeys::forServer(
                  profile.id, ConfigKeys::ShowMediaCarousel));
              carouselCountCard->setConfigKey(ConfigKeys::forServer(
                  profile.id, ConfigKeys::MediaCarouselCount));
              carouselHeightCard->setConfigKey(ConfigKeys::forServer(
                  profile.id, ConfigKeys::MediaCarouselHeightPercent));
            }
          });

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/play-history.svg", tr("Show Continue Watching"),
      tr("Display unfinished media on the home screen"), new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowContinueWatching), this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/latest-added.svg", tr("Show Latest Added"),
      tr("Display recently added media on the home screen"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowLatestAdded), this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/star.svg", tr("Show Recommended"),
      tr("Display recommended media on the home screen"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowRecommended), this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/library.svg", tr("Show Media Libraries"),
      tr("Display media libraries on the home screen"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowMediaLibraries), this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/appearance-library-view.svg", tr("Show Each Library"),
      tr("Display each library section individually on the home screen"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowEachLibrary), this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/completed-watching.svg", tr("Show Completed Watching"),
      tr("Display completed media on the home screen"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowCompletedWatching), this,
      QVariant(false)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/play-history-request-limit.svg",
      tr("Continue Watching Request Limit"),
      tr("Full list request count. 0 means unlimited; home sections use the same value up to 50."),
      createRequestLimitInput(tr("Unlimited"), this),
      ConfigKeys::forServer(sid, ConfigKeys::ContinueWatchingRequestLimit),
      this, QVariant(0)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/latest-added-request-limit.svg",
      tr("Latest Media Request Limit"),
      tr("Full list request count. 0 means unlimited; home sections use the same value up to 50."),
      createRequestLimitInput(tr("Unlimited"), this),
      ConfigKeys::forServer(sid, ConfigKeys::LatestMediaRequestLimit), this,
      QVariant(1000)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/recommended-request-limit.svg",
      tr("Recommended Request Limit"),
      tr("Full list request count. 0 means unlimited; home sections use the same value up to 50."),
      createRequestLimitInput(tr("Unlimited"), this),
      ConfigKeys::forServer(sid, ConfigKeys::RecommendedRequestLimit), this,
      QVariant(1000)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/completed-watching-request-limit.svg",
      tr("Completed Watching Request Limit"),
      tr("Full list request count. 0 means unlimited; home sections use the same value up to 50."),
      createRequestLimitInput(tr("Unlimited"), this),
      ConfigKeys::forServer(sid, ConfigKeys::CompletedWatchingRequestLimit),
      this, QVariant(0)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/library.svg", tr("Show Live TV"), tr("Display available TV channels on the home screen"),
      new ModernSwitch(this), ConfigKeys::forServer(sid, ConfigKeys::ShowLiveTv), this, QVariant(true)));
  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/library.svg", tr("Show Music"), tr("Display available music albums on the home screen"),
      new ModernSwitch(this), ConfigKeys::forServer(sid, ConfigKeys::ShowMusic), this, QVariant(true)));
  auto *viewCombo = new ModernComboBox(this);
  viewCombo->addItem(tr("Automatic detection"), QStringLiteral("auto"));
  viewCombo->addItem(tr("Portrait posters"), QStringLiteral("poster"));
  viewCombo->addItem(tr("Landscape tiles"), QStringLiteral("tile"));
  viewCombo->setCurrentIndex(viewCombo->findData(LibraryArtworkManager::instance()->defaultLayoutMode()));
  auto* layoutCard = new SettingsCard(
      ":/svg/dark/appearance-library-view.svg", tr("Default cover layout"),
      tr("Default layout for this server. Automatic detection uses loaded cover dimensions; manual choices on individual pages take priority."),
      viewCombo, QString(), this);
  layoutCard->setEnabled(!sid.isEmpty());
  m_mainLayout->addWidget(layoutCard);

  auto* scanLayout = new ModernSwitch(this);
  auto* scanCard = new SettingsCard(
      ":/svg/dark/scan-library.svg", tr("Check all library artwork in the background"),
      tr("Automatic detection normally uses loaded items only. A full check reads all artwork information and may increase traffic and server load; use on fast connections."),
      scanLayout, ConfigKeys::forServer(sid, ConfigKeys::ScanLibraryArtwork), this, QVariant(false));
  scanCard->setEnabled(!sid.isEmpty() && viewCombo->currentData() == QStringLiteral("auto"));
  m_mainLayout->addWidget(scanCard);
  connect(viewCombo, &QComboBox::currentIndexChanged, this, [this, viewCombo](int) {
      const QString serverId = m_core->serverManager()->activeProfile().id;
      if (!serverId.isEmpty()) ConfigStore::instance()->set(
          ConfigKeys::forServer(serverId, ConfigKeys::ArtworkLayoutMode), viewCombo->currentData().toString());
  });
  const auto syncLayout = [this, viewCombo, layoutCard, scanCard]() {
      const bool hasServer = m_core->serverManager()->activeProfile().isValid();
      const QSignalBlocker blocker(viewCombo);
      viewCombo->setCurrentIndex(viewCombo->findData(LibraryArtworkManager::instance()->defaultLayoutMode()));
      layoutCard->setEnabled(hasServer);
      scanCard->setEnabled(hasServer && viewCombo->currentData() == QStringLiteral("auto"));
  };
  connect(LibraryArtworkManager::instance(), &LibraryArtworkManager::configurationChanged, this, syncLayout);
  connect(scanLayout, &ModernSwitch::toggled, this, [this](bool enabled) {
      const QString serverId = m_core->serverManager()->activeProfile().id;
      if (enabled && !serverId.isEmpty()) ConfigStore::instance()->set(
          ConfigKeys::forServer(serverId, ConfigKeys::ScanLibraryArtworkEndpoint),
          LibraryArtworkManager::instance()->endpointFingerprint());
  });
  auto* scanProgress = new QLabel(this);
  scanProgress->setObjectName("SettingsCardDesc");
  m_mainLayout->addWidget(scanProgress);
  connect(LibraryArtworkManager::instance(), &LibraryArtworkManager::configurationChanged,
          scanProgress, [scanProgress]() {
      if (!LibraryArtworkManager::instance()->enabled()) scanProgress->clear();
  });
  const auto showScanAuthorization = [scanProgress]() {
      scanProgress->setText(tr("Server address changed. Turn background checking off and on to enable it here."));
  };
  connect(LibraryArtworkManager::instance(), &LibraryArtworkManager::scanAuthorizationRequired,
          scanProgress, showScanAuthorization);
  if (scanLayout->isChecked() && !LibraryArtworkManager::instance()->scanAuthorized()) showScanAuthorization();
  connect(LibraryArtworkManager::instance(), &LibraryArtworkManager::scanProgress,
          scanProgress, [scanProgress](const QString&, int checked, int total) {
      scanProgress->setText(tr("Artwork checked: %1 / %2").arg(checked).arg(total));
  });

  connect(m_core->serverManager(), &ServerManager::activeServerChanged, this,
          [scanCard, scanProgress, scanLayout, syncLayout, showScanAuthorization](const ServerProfile& profile) {
      if (!profile.id.isEmpty()) scanCard->setConfigKey(
          ConfigKeys::forServer(profile.id, ConfigKeys::ScanLibraryArtwork));
      syncLayout();
      scanProgress->clear();
      if (profile.isValid() && scanLayout->isChecked() &&
          !LibraryArtworkManager::instance()->scanAuthorized()) showScanAuthorization();
  });

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/adaptive-image.svg", tr("Use another image when artwork is missing"),
      tr("When artwork such as a primary poster is missing, use another available image. This controls image selection; cover layout is set separately."),
      new ModernSwitch(this),
      ConfigKeys::AdaptiveImages, this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/appearance-library-view.svg", tr("Show Media Tooltips"),
      tr("Show media details in tooltips when hovering over media cards"),
      new ModernSwitch(this),
      ConfigKeys::ShowMediaTooltips, this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/play-history.svg", tr("Show Playback Progress on Media Cards"),
      tr("Display the playback progress at the bottom of media images"),
      new ModernSwitch(this), ConfigKeys::ShowMediaPlaybackProgress, this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/star-filled.svg", tr("Show Cover Ratings"),
      tr("Display media ratings on media card covers"),
      new ModernSwitch(this), ConfigKeys::ShowCoverRating, this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/folder-heart.svg", tr("Show Favorite Folders"),
      tr("Display favorite folders in the favorites page"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowFavoriteFolders), this,
      QVariant(false)));

  auto *qualityCombo = new ModernComboBox(this);
  qualityCombo->addItem(tr("Low"), "low");
  qualityCombo->addItem(tr("Medium"), "medium");
  qualityCombo->addItem(tr("High"), "high");
  qualityCombo->addItem(tr("Original"), "original");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/image-quality.svg", tr("Image Quality"),
                       tr("Set image download quality (higher uses more bandwidth)"),
                       qualityCombo, ConfigKeys::ImageQuality, this));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/image-quality.svg", tr("Shimmer Loading Animation"),
      tr("Show shimmer skeleton animation while loading dashboard content"),
      new ModernSwitch(this),
      ConfigKeys::ShimmerAnimation, this,
      QVariant(false)));

  const QString customHomeSectionOrderEnabledKey =
      ConfigKeys::forServer(sid, ConfigKeys::CustomHomeSectionOrderEnabled);
  const QString homeSectionOrderKey =
      ConfigKeys::forServer(sid, ConfigKeys::HomeSectionOrder);
  auto* configStore = ConfigStore::instance();
  const QStringList storedHomeSectionOrder =
      configStore->get<QStringList>(homeSectionOrderKey, {});
  const QStringList initialHomeSectionOrder =
      storedHomeSectionOrder.isEmpty()
          ? DashboardSectionOrderUtils::defaultSectionOrder()
          : DashboardSectionOrderUtils::normalizeSectionOrder(
                storedHomeSectionOrder);

  auto* customHomeOrderSwitch = new ModernSwitch(this);
  m_mainLayout->addWidget(new SettingsCard(
      kHomeSectionOrderIconPath, tr("Custom Home Section Order"),
      tr("Override the default order of the main home screen sections"),
      customHomeOrderSwitch, customHomeSectionOrderEnabledKey, this,
      QVariant(false)));

  m_homeSectionOrderPanel =
      new SettingsSubPanel(kHomeSectionOrderDragIconPath, this);
  m_homeSectionOrderWidget =
      new DashboardSectionOrderWidget(m_homeSectionOrderPanel);
  m_homeSectionOrderWidget->setSectionOrder(initialHomeSectionOrder);
  configStore->set(ConfigKeys::forServer(sid, ConfigKeys::HomeSectionOrderVersion), 2);
  m_homeSectionOrderPanel->contentLayout()->addWidget(
      m_homeSectionOrderWidget, 1);
  m_mainLayout->addWidget(m_homeSectionOrderPanel);

  if (!storedHomeSectionOrder.isEmpty() &&
      storedHomeSectionOrder != initialHomeSectionOrder) {
    configStore->set(homeSectionOrderKey, initialHomeSectionOrder);
  }

  connect(customHomeOrderSwitch, &ModernSwitch::toggled,
          m_homeSectionOrderPanel, &SettingsSubPanel::setExpanded);
  connect(m_homeSectionOrderWidget,
          &DashboardSectionOrderWidget::sectionOrderChanged, this,
          [configStore, homeSectionOrderKey](const QStringList& order) {
            configStore->set(
                homeSectionOrderKey,
                DashboardSectionOrderUtils::normalizeSectionOrder(order));
          });
  connect(configStore, &ConfigStore::valueChanged, this,
          [this, configStore, customHomeSectionOrderEnabledKey,
           homeSectionOrderKey](const QString& key, const QVariant& value) {
            if (key == customHomeSectionOrderEnabledKey &&
                m_homeSectionOrderPanel) {
              m_homeSectionOrderPanel->setExpanded(value.toBool());
              return;
            }

            if (key != homeSectionOrderKey || !m_homeSectionOrderWidget) {
              return;
            }

            const QStringList normalizedOrder =
                DashboardSectionOrderUtils::normalizeSectionOrder(
                    value.toStringList());
            m_homeSectionOrderWidget->setSectionOrder(normalizedOrder);

            if (normalizedOrder != value.toStringList()) {
              configStore->set(homeSectionOrderKey, normalizedOrder);
            }
          });

  if (configStore->get<bool>(customHomeSectionOrderEnabledKey, false)) {
    m_homeSectionOrderPanel->initExpanded();
  }

  
  
  
  auto *cacheTitle = new QLabel(tr("Cache Management"), this);
  cacheTitle->setObjectName("SettingsSubTitle");
  m_mainLayout->addWidget(cacheTitle);

  
  auto *dataCacheCombo = new ModernComboBox(this);
  dataCacheCombo->addItem(tr("6 Hours"), "6");
  dataCacheCombo->addItem(tr("12 Hours"), "12");
  dataCacheCombo->addItem(tr("1 Day"), "24");
  dataCacheCombo->addItem(tr("3 Days"), "72");
  dataCacheCombo->addItem(tr("7 Days"), "168");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/cache-data.svg", tr("Recommendation Cache Duration"),
                       tr("Set how long to cache recommended content before refreshing"),
                       dataCacheCombo, ConfigKeys::DataCacheDuration, this));

  
  m_dataCacheSizeLabel = new QLabel(this);
  m_dataCacheSizeLabel->setObjectName("SettingsCardDesc");
  auto *clearDataBtn = new QPushButton(tr("Clear"), this);
  clearDataBtn->setObjectName("SettingsCardButton");

  auto *dataClearWidget = new QWidget(this);
  auto *dataClearLayout = new QHBoxLayout(dataClearWidget);
  dataClearLayout->setContentsMargins(0, 0, 0, 0);
  dataClearLayout->setSpacing(12);
  dataClearLayout->addWidget(m_dataCacheSizeLabel);
  dataClearLayout->addWidget(clearDataBtn);

  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/cache-clear.svg", tr("Clear Recommendation Cache"),
                       tr("Clear cached recommendations and refresh on next visit"),
                       dataClearWidget, QString(), this));
  connect(clearDataBtn, &QPushButton::clicked, this, [this]() {
    if (m_core && m_core->mediaService()) {
      m_core->mediaService()->clearRecommendCache();
      updateCacheSizes();
      ModernToast::showMessage(tr("Recommendation cache cleared"), 1500);
    }
  });

  
  auto *imgCacheCombo = new ModernComboBox(this);
  imgCacheCombo->addItem(tr("1 Day"), "1");
  imgCacheCombo->addItem(tr("3 Days"), "3");
  imgCacheCombo->addItem(tr("7 Days"), "7");
  imgCacheCombo->addItem(tr("14 Days"), "14");
  imgCacheCombo->addItem(tr("30 Days"), "30");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/cache-image.svg", tr("Image Cache Duration"),
                       tr("How long to keep downloaded images in local cache"),
                       imgCacheCombo, ConfigKeys::ImageCacheDuration, this));

  
  m_imgCacheSizeLabel = new QLabel(this);
  m_imgCacheSizeLabel->setObjectName("SettingsCardDesc");
  auto *clearImgBtn = new QPushButton(tr("Clear"), this);
  clearImgBtn->setObjectName("SettingsCardButton");

  auto *imgClearWidget = new QWidget(this);
  auto *imgClearLayout = new QHBoxLayout(imgClearWidget);
  imgClearLayout->setContentsMargins(0, 0, 0, 0);
  imgClearLayout->setSpacing(12);
  imgClearLayout->addWidget(m_imgCacheSizeLabel);
  imgClearLayout->addWidget(clearImgBtn);

  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/cache-clear.svg", tr("Clear Image Cache"),
                       tr("Remove all cached images from disk"),
                       imgClearWidget, QString(), this));
  connect(clearImgBtn, &QPushButton::clicked, this, [this]() {
    if (m_core && m_core->mediaService()) {
      m_core->mediaService()->clearImageCaches();
    }
    updateCacheSizes();
    ModernToast::showMessage(tr("Image cache cleared"), 1500);
  });

  m_mainLayout->addStretch();

  
  updateCacheSizes();
}

void PageLibrary::updateCacheSizes() {
  
  QString sid = m_core->serverManager()->activeProfile().id;
  QString dataCachePath =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      QStringLiteral("/qEmby_RecommendCache_%1.json").arg(sid);
  QFileInfo dataInfo(dataCachePath);
  qint64 dataSize = dataInfo.exists() ? dataInfo.size() : 0;
  m_dataCacheSizeLabel->setText(FileUtils::formatSize(dataSize));

  
  QString imgCachePath =
      QStandardPaths::writableLocation(QStandardPaths::CacheLocation) +
      "/qEmby_ImageCache";
  qint64 imgSize = FileUtils::calcDirSize(imgCachePath);
  m_imgCacheSizeLabel->setText(FileUtils::formatSize(imgSize));
}
