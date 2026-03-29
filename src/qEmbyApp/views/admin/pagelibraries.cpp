#include "pagelibraries.h"
#include "../../components/librarycreatedlg.h"
#include "../../components/libraryeditdlg.h"
#include "../../components/loadingoverlay.h"
#include "../../components/modernmessagebox.h"
#include "../../components/moderntoast.h"
#include "../../managers/languagemanager.h"
#include "../../managers/thememanager.h"
#include <qembycore.h>
#include <api/embywebsocket.h>
#include <services/admin/adminservice.h>
#include <services/manager/servermanager.h>
#include <services/media/mediaservice.h>

#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPointer>
#include <QPushButton>
#include <QHideEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QJsonObject>
#include <QShowEvent>
#include <QTimer>
#include <QTransform>
#include <QVariant>
#include <QVBoxLayout>
#include <QVariantAnimation>




namespace {

bool isChineseUiLanguage() {
  const QString currentLanguage = LanguageManager::instance()->currentLanguage();
  if (currentLanguage.startsWith("zh", Qt::CaseInsensitive)) {
    return true;
  }

  if (currentLanguage == "system" || currentLanguage.isEmpty()) {
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &localeName : uiLanguages) {
      if (QLocale(localeName).name().startsWith("zh", Qt::CaseInsensitive)) {
        return true;
      }
    }
  }

  return false;
}

QString localizedDeleteSelectedTitle() {
  const QString text = PageLibraries::tr("Delete Selected");
  if (isChineseUiLanguage() && text == QStringLiteral("Delete Selected")) {
    return PageLibraries::tr("删除已选");
  }
  return text;
}

QString localizedDeleteSelectedPrompt(int selectedCount) {
  const QString text = PageLibraries::tr(
      "Are you sure you want to delete %n selected library(s)?\n\nThis will "
      "remove the libraries from the server, but will not delete the actual "
      "media files.",
      "", selectedCount);
  if (isChineseUiLanguage() &&
      text.startsWith(QStringLiteral("Are you sure you want to delete "))) {
    return PageLibraries::tr(
               "确定要删除 %1 个已选媒体库吗？\n\n"
               "此操作会从服务器移除媒体库，但不会删除实际媒体文件。")
        .arg(selectedCount);
  }
  return text;
}

} 




PageLibraries::PageLibraries(QEmbyCore *core, QWidget *parent)
    : ManagePageBase(core, tr("Libraries"), parent) {
  
  m_mainLayout->setContentsMargins(16, 30, 0, 0);

  
  auto *titleRow = new QHBoxLayout();
  titleRow->setContentsMargins(0, 0, LibraryGrid::RightPadding + 12, 0);

  m_btnRefresh = new QPushButton(this);
  m_btnRefresh->setObjectName("ManageRefreshBtn");
  m_btnRefresh->setCursor(Qt::PointingHandCursor);
  m_btnRefresh->setFixedSize(32, 32);
  m_btnRefresh->setToolTip(tr("Refresh library list"));
  titleRow->addWidget(m_btnRefresh);

  titleRow->addSpacing(6);

  m_btnScanAll = new QPushButton(this);
  m_btnScanAll->setObjectName("ManageLibScanAllBtn");
  m_btnScanAll->setCursor(Qt::PointingHandCursor);
  m_btnScanAll->setFixedSize(32, 32);
  m_btnScanAll->setToolTip(tr("Scan all libraries"));
  titleRow->addWidget(m_btnScanAll);

  titleRow->addSpacing(6);

  m_btnCreate = new QPushButton(this);
  m_btnCreate->setObjectName("ManageLibCreateBtn");
  m_btnCreate->setCursor(Qt::PointingHandCursor);
  m_btnCreate->setFixedSize(32, 32);
  m_btnCreate->setToolTip(tr("Create new library"));
  titleRow->addWidget(m_btnCreate);

  titleRow->addSpacing(6);

  m_btnSelectAll = new QPushButton(this);
  m_btnSelectAll->setObjectName("ManageListSelectBtn");
  m_btnSelectAll->setCursor(Qt::PointingHandCursor);
  m_btnSelectAll->setFixedSize(32, 32);
  m_btnSelectAll->setToolTip(tr("Select all libraries"));
  m_btnSelectAll->setEnabled(false);
  titleRow->addWidget(m_btnSelectAll);

  titleRow->addSpacing(6);

  m_btnDeleteSelected = new QPushButton(this);
  m_btnDeleteSelected->setObjectName("ManageListDeleteBtn");
  m_btnDeleteSelected->setCursor(Qt::PointingHandCursor);
  m_btnDeleteSelected->setFixedSize(32, 32);
  m_btnDeleteSelected->setToolTip(tr("Delete selected libraries"));
  m_btnDeleteSelected->setEnabled(false);
  titleRow->addWidget(m_btnDeleteSelected);

  titleRow->addSpacing(12);

  m_scanProgress = new QProgressBar(this);
  m_scanProgress->setObjectName("ManageLibScanProgress");
  m_scanProgress->setTextVisible(false);
  m_scanProgress->setRange(0, 100);
  m_scanProgress->setValue(0);
  m_scanProgress->hide();
  titleRow->addWidget(m_scanProgress, 1);

  m_scanPercentLabel = new QLabel(this);
  m_scanPercentLabel->setObjectName("ManageInfoValue");
  m_scanPercentLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_scanPercentLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  m_scanPercentLabel->setMinimumWidth(40);
  m_scanPercentLabel->hide();
  titleRow->addWidget(m_scanPercentLabel);

  m_btnCancelScan = new QPushButton(this);
  m_btnCancelScan->setObjectName("ManageLibCancelScanBtn");
  m_btnCancelScan->setCursor(Qt::PointingHandCursor);
  m_btnCancelScan->setFixedSize(20, 20);
  m_btnCancelScan->setToolTip(tr("Cancel scan"));
  m_btnCancelScan->hide();
  titleRow->addWidget(m_btnCancelScan);

  titleRow->addStretch(2);

  m_statsLabel = new QLabel(this);
  m_statsLabel->setObjectName("ManageInfoValue");
  m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_statsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  titleRow->addWidget(m_statsLabel);

  m_mainLayout->addLayout(titleRow);

  
  m_scanTimer = new QTimer(this);
  m_scanTimer->setInterval(2000);
  connect(m_scanTimer, &QTimer::timeout, this,
          [this]() { m_pendingTask = pollScanProgress(); });

  
  auto *scrollContent = new QWidget(this);
  scrollContent->setAttribute(Qt::WA_StyledBackground, true);
  auto *scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 0, 40);
  scrollLayout->setSpacing(16);

  m_grid = new LibraryGrid(scrollContent);
  scrollLayout->addWidget(m_grid, 1);

  
  m_emptyStateContainer = new QWidget(scrollContent);
  m_emptyStateContainer->setSizePolicy(QSizePolicy::Preferred,
                                       QSizePolicy::Expanding);
  auto *emptyLayout = new QVBoxLayout(m_emptyStateContainer);
  emptyLayout->setContentsMargins(0, 0, 0, 0);
  emptyLayout->setSpacing(0);
  emptyLayout->addStretch(1);

  m_emptyLabel = new QLabel(tr("No media libraries found"), m_emptyStateContainer);
  m_emptyLabel->setObjectName("ManageEmptyLabel");
  m_emptyLabel->setAlignment(Qt::AlignCenter);
  m_emptyLabel->hide();
  emptyLayout->addWidget(m_emptyLabel);
  emptyLayout->addStretch(1);

  m_emptyStateContainer->hide();
  scrollLayout->addWidget(m_emptyStateContainer, 1);

  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setObjectName("SettingsScrollArea");
  m_scrollArea->setWidget(scrollContent);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->viewport()->setAutoFillBackground(false);

  m_mainLayout->addWidget(m_scrollArea, 1);

  m_loadingOverlay = new LoadingOverlay(m_scrollArea->viewport());
  m_loadingOverlay->setHudPanelVisible(false);
  m_loadingOverlay->setSubtleOverlay(true);
  updateLoadingOverlayGeometry();

  
  connect(m_btnRefresh, &QPushButton::clicked, this,
          [this]() { m_pendingTask = loadData(true); });
  connect(m_btnScanAll, &QPushButton::clicked, this,
          [this]() { m_pendingTask = onScanAllClicked(); });
  connect(m_btnCreate, &QPushButton::clicked, this,
          [this]() { m_pendingTask = onCreateClicked(); });
  connect(m_btnSelectAll, &QPushButton::clicked, this,
          [this]() { toggleSelectAll(); });
  connect(m_btnDeleteSelected, &QPushButton::clicked, this,
          [this]() { m_pendingTask = onDeleteSelectedClicked(); });
  connect(m_btnCancelScan, &QPushButton::clicked, this,
          [this]() { m_pendingTask = onCancelScanClicked(); });

  connect(
      m_grid, &LibraryGrid::orderChanged, this,
      [this](const QStringList &ids) { m_pendingTask = onOrderChanged(ids); });
}

void PageLibraries::showEvent(QShowEvent *event) {
  ManagePageBase::showEvent(event);
  updateLoadingOverlayGeometry();
  if (!m_loaded) {
    m_pendingTask = loadData();
    m_loaded = true;
  }
  
  m_scanTimer->start();
  
  m_pendingTask = pollScanProgress();
  
  connectWebSocket();
}

void PageLibraries::hideEvent(QHideEvent *event) {
  ManagePageBase::hideEvent(event);
  m_scanTimer->stop();
  disconnectWebSocket();
}

void PageLibraries::resizeEvent(QResizeEvent *event) {
  ManagePageBase::resizeEvent(event);
  updateLoadingOverlayGeometry();
}

bool PageLibraries::hasActiveScan() const {
  return m_isScanAll || !m_scanningItemIds.isEmpty() ||
         !m_runningTaskId.isEmpty();
}

void PageLibraries::setTotalScanProgress(int progress) {
  const int boundedProgress = qBound(0, progress, 100);
  m_scanProgress->setValue(boundedProgress);
  m_scanPercentLabel->setText(QString("%1%").arg(boundedProgress));
  m_scanProgress->show();
  m_scanPercentLabel->show();
  m_btnCancelScan->show();
  m_btnScanAll->setEnabled(false);
}

void PageLibraries::resetTotalScanUi() {
  m_scanProgress->hide();
  m_scanPercentLabel->hide();
  m_btnCancelScan->hide();
  m_scanProgress->setValue(0);
  m_scanPercentLabel->setText("0%");
}

void PageLibraries::setCardScanProgress(const QString &itemId, int progress) {
  for (auto *card : m_grid->cards()) {
    if (card->itemId() == itemId) {
      card->setScanProgress(progress);
      return;
    }
  }
}

void PageLibraries::clearCardScanProgress(const QString &itemId) {
  for (auto *card : m_grid->cards()) {
    if (card->itemId() == itemId) {
      card->clearScanProgress();
      return;
    }
  }
}

void PageLibraries::clearAllCardScanProgress() {
  for (auto *card : m_grid->cards()) {
    if (card->isScanActive()) {
      card->clearScanProgress();
    }
  }
}

void PageLibraries::updateTrackedCardProgressFromTask(int progress) {
  if (m_isScanAll || m_scanningItemIds.size() != 1) {
    return;
  }

  const QString itemId = *m_scanningItemIds.constBegin();
  if (progress <= 0) {
    clearCardScanProgress(itemId);
    return;
  }

  if (progress >= 100) {
    clearCardScanProgress(itemId);
    return;
  }

  setCardScanProgress(itemId, progress);
}

void PageLibraries::requestLibraryReload(bool deferIfScanActive) {
  if (deferIfScanActive && hasActiveScan()) {
    m_reloadDeferredUntilScanEnd = true;
    qDebug() << "[PageLibraries] Deferring library reload until scan ends";
    return;
  }

  qDebug() << "[PageLibraries] Reloading libraries";
  m_pendingTask = loadData(false);
}

void PageLibraries::updateContentState() {
  const bool isEmpty = m_folders.isEmpty();
  m_grid->setVisible(!isEmpty);
  m_emptyStateContainer->setVisible(isEmpty);
  m_emptyLabel->setVisible(isEmpty);
  updateSelectionState();
}

void PageLibraries::updateStats() {
  if (m_isLoading) {
    m_statsLabel->setText(tr("Loading..."));
    return;
  }

  const QString libCountText =
      m_folders.isEmpty() ? tr("0 libraries")
                          : tr("%n library(s)", "", m_folders.size());
  if (m_selectedLibraryIds.isEmpty()) {
    m_statsLabel->setText(libCountText);
    return;
  }

  m_statsLabel->setText(
      tr("%1  ·  %2")
          .arg(libCountText,
               tr("%n selected", "", m_selectedLibraryIds.size())));
}

void PageLibraries::updateSelectionState() {
  m_btnRefresh->setEnabled(!m_isLoading);
  m_btnCreate->setEnabled(!m_isLoading);
  if (!m_isLoading && !hasActiveScan()) {
    m_btnScanAll->setEnabled(true);
  } else if (m_isLoading) {
    m_btnScanAll->setEnabled(false);
  }
  m_btnSelectAll->setEnabled(!m_isLoading && !m_folders.isEmpty());
  m_btnDeleteSelected->setEnabled(!m_isLoading &&
                                  !m_selectedLibraryIds.isEmpty());
  m_btnSelectAll->setToolTip(areAllLibrariesSelected()
                                 ? tr("Clear library selection")
                                 : tr("Select all libraries"));
  updateStats();
}

void PageLibraries::syncCardSelection() {
  for (auto *card : m_grid->cards()) {
    card->setSelected(m_selectedLibraryIds.contains(card->itemId()));
  }
}

void PageLibraries::onCardSelectionChanged(const QString &itemId,
                                           bool selected) {
  if (selected) {
    m_selectedLibraryIds.insert(itemId);
  } else {
    m_selectedLibraryIds.remove(itemId);
  }

  qDebug() << "[PageLibraries] Library selection changed"
           << "| itemId=" << itemId
           << "| selected=" << selected
           << "| selectedCount=" << m_selectedLibraryIds.size();

  updateSelectionState();
}

void PageLibraries::toggleSelectAll() {
  if (areAllLibrariesSelected()) {
    m_selectedLibraryIds.clear();
  } else {
    m_selectedLibraryIds.clear();
    for (const VirtualFolder &folder : m_folders) {
      m_selectedLibraryIds.insert(folder.itemId);
    }
  }

  syncCardSelection();
  updateSelectionState();
}

bool PageLibraries::areAllLibrariesSelected() const {
  return !m_folders.isEmpty() &&
         m_selectedLibraryIds.size() == m_folders.size();
}

void PageLibraries::setLoadingState(bool loading) {
  if (m_isLoading == loading) {
    return;
  }

  m_isLoading = loading;
  qDebug() << "[PageLibraries] Loading state changed"
           << "| loading=" << loading;

  updateContentState();

  if (!m_loadingOverlay) {
    return;
  }

  updateLoadingOverlayGeometry();
  if (loading) {
    m_loadingOverlay->start();
  } else {
    m_loadingOverlay->stop();
  }
}

void PageLibraries::updateLoadingOverlayGeometry() {
  if (!m_loadingOverlay || !m_scrollArea) {
    return;
  }

  m_loadingOverlay->setGeometry(m_scrollArea->viewport()->rect());
}

void PageLibraries::finalizeScan(bool reloadLibraries) {
  qDebug() << "[PageLibraries] Finalizing scan"
           << "reloadLibraries=" << reloadLibraries
           << "deferredReload=" << m_reloadDeferredUntilScanEnd;

  const bool showTotalUi = m_isScanAll || m_scanProgress->isVisible();
  clearAllCardScanProgress();
  if (showTotalUi) {
    setTotalScanProgress(100);
  } else {
    resetTotalScanUi();
    m_btnScanAll->setEnabled(true);
  }

  m_isScanAll = false;
  m_scanningItemIds.clear();
  m_runningTaskId.clear();

  if (showTotalUi) {
    QTimer::singleShot(1500, this, [this]() {
      if (hasActiveScan()) {
        return;
      }
      resetTotalScanUi();
      m_btnScanAll->setEnabled(true);
    });
  }

  const bool shouldReload = reloadLibraries || m_reloadDeferredUntilScanEnd;
  m_reloadDeferredUntilScanEnd = false;
  if (shouldReload) {
    requestLibraryReload(false);
  }
}

QCoro::Task<void> PageLibraries::loadData(bool isManual) {
  QPointer<PageLibraries> safeThis(this);
  setLoadingState(true);
  if (isManual) {
    m_btnRefresh->setEnabled(false);
    QIcon originalIcon = m_btnRefresh->icon();
    QSize iconSz = m_btnRefresh->iconSize();
    QPixmap basePx = originalIcon.pixmap(iconSz);

    auto *rotAnim = new QVariantAnimation(this);
    rotAnim->setStartValue(0.0);
    rotAnim->setEndValue(360.0);
    rotAnim->setDuration(800);
    rotAnim->setEasingCurve(QEasingCurve::InOutCubic);

    connect(rotAnim, &QVariantAnimation::valueChanged, this,
            [this, basePx, iconSz](const QVariant &val) {
              qreal angle = val.toReal();
              QTransform t;
              t.translate(iconSz.width() / 2.0, iconSz.height() / 2.0);
              t.rotate(angle);
              t.translate(-iconSz.width() / 2.0, -iconSz.height() / 2.0);
              QPixmap rotated = basePx.transformed(t, Qt::SmoothTransformation);
              int xOff = (rotated.width() - iconSz.width()) / 2;
              int yOff = (rotated.height() - iconSz.height()) / 2;
              QPixmap cropped =
                  rotated.copy(xOff, yOff, iconSz.width(), iconSz.height());
              m_btnRefresh->setIcon(QIcon(cropped));
            });
    connect(rotAnim, &QVariantAnimation::finished, this,
            [this, originalIcon]() { m_btnRefresh->setIcon(originalIcon); });
    rotAnim->start(QAbstractAnimation::DeleteWhenStopped);
  }

  try {
    const QList<VirtualFolder> fetchedFolders =
        co_await m_core->adminService()->getVirtualFolders();
    if (!safeThis)
      co_return;
    m_folders = fetchedFolders;

    
    QStringList orderIds = co_await m_core->adminService()->getLibraryOrder();
    if (!safeThis)
      co_return;
    if (!orderIds.isEmpty()) {
      QList<VirtualFolder> sorted;
      QList<VirtualFolder> remaining = m_folders;
      for (const auto &orderId : orderIds) {
        for (int i = 0; i < remaining.size(); ++i) {
          if (remaining[i].itemId == orderId) {
            sorted.append(remaining.takeAt(i));
            break;
          }
        }
      }
      sorted.append(remaining);
      m_folders = sorted;
    }

    if (m_folders.isEmpty()) {
      ++m_cardImageLoadGeneration;
      m_grid->setCards({});
      m_selectedLibraryIds.clear();
    } else {
      QSet<QString> retainedSelectionIds;
      QList<LibraryCard *> cards;
      for (int i = 0; i < m_folders.size(); ++i) {
        if (m_selectedLibraryIds.contains(m_folders[i].itemId)) {
          retainedSelectionIds.insert(m_folders[i].itemId);
        }

        auto *card = new LibraryCard(m_folders[i], i, m_grid);
        card->setSelected(retainedSelectionIds.contains(m_folders[i].itemId));
        connect(card, &LibraryCard::editClicked, this,
                [this](int idx) { m_pendingTask = onEditClicked(idx); });
        connect(card, &LibraryCard::scanClicked, this,
                [this](int idx) { m_pendingTask = onScanClicked(idx); });
        connect(card, &LibraryCard::deleteClicked, this,
                [this](int idx) { m_pendingTask = onDeleteClicked(idx); });
        connect(card, &LibraryCard::selectionChanged, this,
                [this](const QString &itemId, bool selected) {
                  onCardSelectionChanged(itemId, selected);
                });
        cards.append(card);
      }

      m_selectedLibraryIds = retainedSelectionIds;
      const quint64 imageLoadGeneration = ++m_cardImageLoadGeneration;
      m_grid->setCards(cards);
      syncCardSelection();

      
      for (int i = 0; i < cards.size(); ++i) {
        loadCardImage(cards[i], m_folders[i], imageLoadGeneration);
      }
    }

    updateContentState();

  } catch (const std::exception &e) {
    if (!safeThis)
      co_return;
    ModernToast::showMessage(
        tr("Failed to load libraries: %1").arg(QString::fromUtf8(e.what())),
        3000);
  }

  if (!safeThis)
    co_return;
  setLoadingState(false);
}

QCoro::Task<void> PageLibraries::loadCardImage(LibraryCard *card,
                                               VirtualFolder folder,
                                               quint64 imageLoadGeneration) {
  QPointer<PageLibraries> safeThis(this);
  QPointer<LibraryCard> safeCard(card);
  const QString folderItemId = folder.itemId;
  const QString imageItemId = folder.primaryImageItemId.isEmpty()
                                  ? folder.itemId
                                  : folder.primaryImageItemId;

  try {
    if (imageItemId.isEmpty())
      co_return;

    qDebug() << "[PageLibraries] Loading library card image"
             << "folderItemId=" << folderItemId
             << "imageItemId=" << imageItemId
             << "generation=" << imageLoadGeneration;

    QPixmap pixmap = co_await m_core->mediaService()->fetchImage(
        imageItemId, "Primary", QString(), LibraryGrid::CardWidth * 2);

    if (!safeThis) {
      qDebug() << "[PageLibraries] Skip library card image apply:"
               << "page already destroyed"
               << "folderItemId=" << folderItemId;
      co_return;
    }

    if (imageLoadGeneration != safeThis->m_cardImageLoadGeneration) {
      qDebug() << "[PageLibraries] Skip stale library card image:"
               << "folderItemId=" << folderItemId
               << "requestGeneration=" << imageLoadGeneration
               << "currentGeneration=" << safeThis->m_cardImageLoadGeneration;
      co_return;
    }

    if (!safeCard) {
      qDebug() << "[PageLibraries] Skip library card image apply:"
               << "card already destroyed"
               << "folderItemId=" << folderItemId;
      co_return;
    }

    if (safeCard->itemId() != folderItemId) {
      qDebug() << "[PageLibraries] Skip mismatched library card image:"
               << "folderItemId=" << folderItemId
               << "cardItemId=" << safeCard->itemId();
      co_return;
    }

    if (!pixmap.isNull()) {
      safeCard->setImage(pixmap);
    }
  } catch (...) {
    
    qDebug() << "[PageLibraries] Failed to load library card image"
             << "folderItemId=" << folderItemId
             << "imageItemId=" << imageItemId;
  }
}

QCoro::Task<void> PageLibraries::onCreateClicked() {
  
  auto providers = co_await m_core->adminService()->getAvailableProviders();
  auto localization = co_await m_core->adminService()->getLocalizationOptions();

  LibraryCreateDialog dlg(m_core, providers.metadataFetchers,
                          providers.imageFetchers,
                          providers.subtitleDownloaders,
                          providers.lyricsFetchers,
                          localization.cultures, localization.countries, this);
  if (dlg.exec() == QDialog::Accepted) {
    try {
      co_await m_core->adminService()->addVirtualFolder(
          dlg.libraryName(), dlg.collectionType(), dlg.paths(),
          dlg.libraryOptions());
      ModernToast::showMessage(tr("Library created successfully"), 2000);
      co_await loadData(false);
    } catch (const std::exception &e) {
      ModernToast::showMessage(
          tr("Failed to create library: %1").arg(QString::fromUtf8(e.what())),
          3000);
    }
  }
}

QCoro::Task<void> PageLibraries::onEditClicked(int row) {
  if (row < 0 || row >= m_folders.size())
    co_return;
  const VirtualFolder folder = m_folders[row];
  const QString virtualFolderId = folder.virtualFolderId();
  qDebug() << "[PageLibraries] onEditClicked"
           << "| name:" << folder.name
           << "| id:" << folder.id
           << "| itemId:" << folder.itemId
           << "| guid:" << folder.guid
           << "| folderId:" << virtualFolderId;
  auto providers = co_await m_core->adminService()->getAvailableProviders();
  auto localization = co_await m_core->adminService()->getLocalizationOptions();

  LibraryEditDialog dlg(folder, m_core, providers.metadataFetchers,
                        providers.imageFetchers,
                        providers.subtitleDownloaders,
                        providers.lyricsFetchers, localization.cultures,
                        localization.countries, this);
  if (dlg.exec() == QDialog::Accepted) {
    try {
      const QString updatedName =
          dlg.newName().isEmpty() ? folder.name : dlg.newName();
      const QStringList addedPaths = dlg.addedPaths();
      const QStringList removedPaths = dlg.removedPaths();
      const QStringList currentPaths = dlg.paths();
      const QJsonObject updatedOptions = dlg.libraryOptions();
      const QVariant realtimeMonitorValue =
          updatedOptions.value("EnableRealtimeMonitor").toVariant();
      const QVariant metadataLanguageValue =
          updatedOptions.value("PreferredMetadataLanguage").toVariant();
      const QVariant countryValue =
          updatedOptions.value("MetadataCountryCode").toVariant();
      const QVariant imageLanguageValue =
          updatedOptions.value("PreferredImageLanguage").toVariant();

      qDebug() << "[PageLibraries] Saving library edit"
               << "| originalName:" << folder.name
               << "| updatedName:" << updatedName
               << "| folderId:" << virtualFolderId
               << "| addedPaths:" << addedPaths
               << "| removedPaths:" << removedPaths
               << "| currentPathCount:" << currentPaths.size()
               << "| realtimeMonitor:" << realtimeMonitorValue
               << "| metadataLanguage:" << metadataLanguageValue
               << "| country:" << countryValue
               << "| imageLanguage:" << imageLanguageValue;

      if (updatedName != folder.name) {
        co_await m_core->adminService()->renameVirtualFolder(folder.name,
                                                             updatedName);
      }
      for (const auto &path : addedPaths) {
        co_await m_core->adminService()->addMediaPath(
            virtualFolderId, updatedName, path);
      }
      for (const auto &path : removedPaths) {
        co_await m_core->adminService()->removeMediaPath(
            virtualFolderId, updatedName, path);
      }
      co_await m_core->adminService()->updateVirtualFolderOptions(
          virtualFolderId, updatedOptions, currentPaths);
      ModernToast::showMessage(tr("Library updated successfully"), 2000);

      
      try {
        m_scanningItemIds.insert(folder.itemId);
        co_await m_core->adminService()->refreshItemMetadata(folder.itemId);
      } catch (...) {
        m_scanningItemIds.remove(folder.itemId);
      }

      co_await loadData(false);
    } catch (const std::exception &e) {
      ModernToast::showMessage(
          tr("Failed to update library: %1").arg(QString::fromUtf8(e.what())),
          3000);
    }
  }
}

QCoro::Task<void> PageLibraries::onDeleteClicked(int row) {
  if (row < 0 || row >= m_folders.size())
    co_return;
  const VirtualFolder folder = m_folders[row];

  bool confirmed = ModernMessageBox::question(
      this, tr("Delete Library"),
      tr("Are you sure you want to delete the library \"%1\"?\n\nThis will "
         "remove the library from the server, but will not delete the actual "
         "media files.")
          .arg(folder.name),
      tr("Delete"), tr("Cancel"), ModernMessageBox::Danger,
      ModernMessageBox::Warning);

  if (!confirmed)
    co_return;

  try {
    co_await m_core->adminService()->removeVirtualFolder(folder);
    ModernToast::showMessage(tr("Library \"%1\" deleted").arg(folder.name),
                             2000);
    co_await loadData(false);
  } catch (const std::exception &e) {
    ModernToast::showMessage(
        tr("Failed to delete library: %1").arg(QString::fromUtf8(e.what())),
        3000);
  }
}

QCoro::Task<void> PageLibraries::onDeleteSelectedClicked() {
  QPointer<PageLibraries> safeThis(this);
  const int selectedCount = m_selectedLibraryIds.size();
  if (selectedCount <= 0) co_return;

  const bool confirmed = ModernMessageBox::question(
      this, localizedDeleteSelectedTitle(),
      localizedDeleteSelectedPrompt(selectedCount),
      tr("Delete"), tr("Cancel"), ModernMessageBox::Danger,
      ModernMessageBox::Warning);
  if (!confirmed) co_return;

  int deletedCount = 0;
  QString failureMessage;

  for (const VirtualFolder &folder : m_folders) {
    if (!m_selectedLibraryIds.contains(folder.itemId)) {
      continue;
    }

    qDebug() << "[PageLibraries] Batch deleting library"
             << "| itemId=" << folder.itemId
             << "| name=" << folder.name;

    try {
      co_await m_core->adminService()->removeVirtualFolder(folder);
      if (!safeThis) co_return;
      ++deletedCount;
    } catch (const std::exception &e) {
      failureMessage = QString::fromUtf8(e.what());
      qWarning() << "[PageLibraries] Failed to delete selected library"
                 << "| itemId=" << folder.itemId
                 << "| name=" << folder.name
                 << "| error=" << failureMessage;
      break;
    }
  }

  if (!safeThis) co_return;

  m_selectedLibraryIds.clear();
  if (!failureMessage.isEmpty()) {
    if (deletedCount > 0) {
      ModernToast::showMessage(
          tr("Deleted %1 library(s), then failed: %2")
              .arg(deletedCount)
              .arg(failureMessage),
          3500);
    } else {
      ModernToast::showMessage(
          tr("Failed to delete selected libraries: %1").arg(failureMessage),
          3000);
    }
  } else {
    ModernToast::showMessage(tr("Deleted %n library(s)", "", deletedCount),
                             2000);
  }

  co_await loadData(false);
}

QCoro::Task<void> PageLibraries::onScanClicked(int row) {
  if (row < 0 || row >= m_folders.size())
    co_return;
  const VirtualFolder folder = m_folders[row];

  try {
    
    m_scanningItemIds.insert(folder.itemId);
    co_await m_core->adminService()->refreshItemMetadata(folder.itemId);
    ModernToast::showMessage(tr("Scanning \"%1\"...").arg(folder.name), 2000);
  } catch (const std::exception &e) {
    m_scanningItemIds.remove(folder.itemId);
    clearCardScanProgress(folder.itemId);
    if (!hasActiveScan()) {
      resetTotalScanUi();
      m_btnScanAll->setEnabled(true);
    }
    ModernToast::showMessage(
        tr("Scan failed: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}

QCoro::Task<void> PageLibraries::onScanAllClicked() {
  try {
    m_isScanAll = true;
    setTotalScanProgress(0);
    co_await m_core->adminService()->refreshLibrary();
    ModernToast::showMessage(tr("Full library scan started"), 2000);
  } catch (const std::exception &e) {
    m_isScanAll = false;
    resetTotalScanUi();
    m_btnScanAll->setEnabled(true);
    ModernToast::showMessage(
        tr("Scan failed: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}

QCoro::Task<void> PageLibraries::pollScanProgress() {
  try {
    auto tasks = co_await m_core->adminService()->getScheduledTasks();
    bool foundRunning = false;

    qDebug() << "[HTTP Poll] ===== Polling ScheduledTasks =====";
    for (const auto &task : tasks) {
      if (task.category == "Library") {
        qDebug() << "[HTTP Poll] Task:" << task.name
                 << "| state:" << task.state
                 << "| progressPct:" << task.currentProgressPercentage
                 << "| id:" << task.id;
      }

      if (task.category == "Library" && task.isRunning()) {
        foundRunning = true;
        m_runningTaskId = task.id;

        
        if (!m_isScanAll && m_scanningItemIds.isEmpty()) {
          qDebug() << "[HTTP Poll] >> Auto-detected external scan, setting isScanAll=true";
          m_isScanAll = true;
        }

        int progress =
            qBound(0, static_cast<int>(task.currentProgressPercentage), 100);
        int currentUiProgress = m_scanProgress->value();
        bool barVisible = m_scanProgress->isVisible();

        qDebug() << "[HTTP Poll] >> Running! rawPct:"
                 << task.currentProgressPercentage
                 << "-> progress:" << progress
                 << "| UI current:" << currentUiProgress
                 << "| visible:" << barVisible
                 << "| isScanAll:" << m_isScanAll;

        
        if (progress <= 0 && barVisible && currentUiProgress > 0) {
          qDebug() << "[HTTP Poll] >> SKIP: progress=0, keeping UI at" << currentUiProgress;
          break;
        }

        
        if (barVisible && progress < currentUiProgress) {
          qDebug() << "[HTTP Poll] >> SKIP: would go backwards" << progress << "<" << currentUiProgress;
          break;
        }

        qDebug() << "[HTTP Poll] >> APPLY: setting progress to" << progress;
        if (m_isScanAll) {
          setTotalScanProgress(progress);
        }
        updateTrackedCardProgressFromTask(progress);
        break;
      }
    }

    
    if (!foundRunning && hasActiveScan()) {
      qDebug() << "[HTTP Poll] >> Scan COMPLETED — clearing progress";
      finalizeScan(true);
    }
  } catch (...) {
    qDebug() << "[HTTP Poll] >> ERROR: polling failed";
  }
}

QCoro::Task<void> PageLibraries::onCancelScanClicked() {
  if (m_runningTaskId.isEmpty())
    co_return;

  try {
    co_await m_core->adminService()->stopScheduledTask(m_runningTaskId);
    ModernToast::showMessage(tr("Scan cancelled"), 2000);

    
    clearAllCardScanProgress();

    
    resetTotalScanUi();
    m_btnScanAll->setEnabled(true);

    
    m_isScanAll = false;
    m_scanningItemIds.clear();
    m_runningTaskId.clear();
    if (m_reloadDeferredUntilScanEnd) {
      m_reloadDeferredUntilScanEnd = false;
      requestLibraryReload(false);
    }
  } catch (const std::exception &e) {
    ModernToast::showMessage(
        tr("Cancel failed: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}

QCoro::Task<void> PageLibraries::onOrderChanged(QStringList newOrderIds) {
  try {
    co_await m_core->adminService()->updateLibraryOrder(newOrderIds);
    ModernToast::showMessage(tr("Library order updated"), 1500);
  } catch (const std::exception &e) {
    ModernToast::showMessage(
        tr("Failed to save order: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}





void PageLibraries::connectWebSocket()
{
    
    disconnectWebSocket();

    auto* ws = m_core->serverManager()->activeWebSocket();
    if (!ws) return;

    m_wsConnections.append(
        connect(ws, &EmbyWebSocket::refreshProgress,
                this, &PageLibraries::onWsRefreshProgress));

    m_wsConnections.append(
        connect(ws, &EmbyWebSocket::libraryChanged,
                this, [this]() { requestLibraryReload(true); }));

    
    m_wsConnections.append(
        connect(ws, &EmbyWebSocket::connected, this, [this]() {
            qDebug() << "[PageLibraries] WebSocket connected — slowing HTTP polling to 5s";
            m_scanTimer->setInterval(5000);
        }));

    
    m_wsConnections.append(
        connect(ws, &EmbyWebSocket::disconnected, this, [this]() {
            qDebug() << "[PageLibraries] WebSocket disconnected — restoring 2s polling";
            m_scanTimer->setInterval(2000);
            if (isVisible() && !m_scanTimer->isActive()) {
                m_scanTimer->start();
                m_pendingTask = pollScanProgress();
            }
        }));

    
    if (ws->isConnected()) {
        m_scanTimer->setInterval(5000);
    }
}

void PageLibraries::disconnectWebSocket()
{
    for (auto& conn : m_wsConnections) {
        QObject::disconnect(conn);
    }
    m_wsConnections.clear();
}

void PageLibraries::onWsRefreshProgress(const QString& itemId, int progress)
{
    qDebug() << "[PageLibraries] WS refreshProgress itemId=" << itemId << "progress=" << progress;

    
    if (itemId.isEmpty()) {
        if (!m_isScanAll && m_scanningItemIds.isEmpty()) {
            m_isScanAll = true;
        }
        
        int currentProgress = m_scanProgress->value();
        if (m_scanProgress->isVisible() && progress < currentProgress && progress > 0) {
            return; 
        }
        
        if (m_isScanAll) {
            setTotalScanProgress(progress);
        }
        updateTrackedCardProgressFromTask(progress);

        if (progress >= 100) {
            finalizeScan(true);
        }
        return;
    }

    
    for (auto* card : m_grid->cards()) {
        if (card->itemId() == itemId) {
            if (progress <= 0) {
                card->clearScanProgress();
            } else if (progress >= 100) {
                card->clearScanProgress();
                m_scanningItemIds.remove(itemId);
            } else {
                card->setScanProgress(progress);
                m_scanningItemIds.insert(itemId);
            }
            return;
        }
    }

    
    
}
