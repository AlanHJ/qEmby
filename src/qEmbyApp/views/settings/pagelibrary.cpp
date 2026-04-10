#include "pagelibrary.h"
#include "../../components/dashboardsectionorderwidget.h"
#include "../../components/moderntoast.h"
#include "../../components/moderncombobox.h"
#include "../../components/modernswitch.h"
#include "../../components/settingscard.h"
#include "../../components/settingssubpanel.h"
#include "../../utils/dashboardsectionorderutils.h"
#include "config/config_keys.h"
#include "config/configstore.h"
#include "fileutils.h"
#include "qembycore.h"
#include "services/manager/servermanager.h"
#include "services/media/mediaservice.h"
#include <QLabel>
#include <QPushButton>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>

namespace {
const QString kHomeSectionOrderIconPath =
    QStringLiteral(":/svg/dark/home-section-order.svg");
const QString kHomeSectionOrderDragIconPath =
    QStringLiteral(":/svg/dark/home-section-drag.svg");
}

PageLibrary::PageLibrary(QEmbyCore *core, QWidget *parent)
    : SettingsPageBase(core, tr("Library"), parent) {
  
  QString sid = m_core->serverManager()->activeProfile().id;

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
      ":/svg/dark/adaptive-image.svg", tr("Adaptive Images"),
      tr("When a specific image type is missing, automatically use another available type as fallback"),
      new ModernSwitch(this),
      ConfigKeys::AdaptiveImages, this,
      QVariant(true)));

  m_mainLayout->addWidget(new SettingsCard(
      ":/svg/dark/folder-heart.svg", tr("Show Favorite Folders"),
      tr("Display favorite folders in the favorites page"),
      new ModernSwitch(this),
      ConfigKeys::forServer(sid, ConfigKeys::ShowFavoriteFolders), this,
      QVariant(false)));

  auto *viewCombo = new ModernComboBox(this);
  viewCombo->addItem(tr("Poster Wall"), "poster");
  viewCombo->addItem(tr("List View"), "tile");
  m_mainLayout->addWidget(
      new SettingsCard(":/svg/dark/appearance-library-view.svg",
                       tr("Default Library View"),
                       tr("Choose how media items are presented by default"),
                       viewCombo, ConfigKeys::DefaultLibraryView, this));

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
    
    QString cachePath = QStandardPaths::writableLocation(
                            QStandardPaths::CacheLocation)
                        + "/qEmby_ImageCache";
    QDir cacheDir(cachePath);
    if (cacheDir.exists()) {
      cacheDir.removeRecursively();
      QDir().mkpath(cachePath); 
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
