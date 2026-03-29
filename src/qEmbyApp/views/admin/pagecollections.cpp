#include "pagecollections.h"
#include "../../components/collectioncard.h"
#include "../../components/collectiongrid.h"
#include "../../components/loadingoverlay.h"
#include "../../components/modernmessagebox.h"
#include "../../components/moderntoast.h"
#include "../../components/textinputdialog.h"
#include "../../managers/thememanager.h"
#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/media/mediaservice.h>

#include <QHBoxLayout>
#include <QDebug>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QShowEvent>
#include <QHash>
#include <QTransform>
#include <QVariantAnimation>
#include <QVBoxLayout>

namespace {

QDateTime parsePlaylistDateCreated(QString rawDateCreated) {
  if (rawDateCreated.isEmpty()) {
    return {};
  }

  QDateTime parsed = QDateTime::fromString(rawDateCreated, Qt::ISODate);
  if (!parsed.isValid()) {
    parsed = QDateTime::fromString(rawDateCreated, Qt::ISODateWithMs);
  }
  return parsed;
}

} 




PageCollections::PageCollections(QEmbyCore *core, QWidget *parent)
    : ManagePageBase(core, tr("Lists"), parent) {
  
  m_mainLayout->setContentsMargins(16, 30, 0, 0);

  
  auto *titleRow = new QHBoxLayout();
  titleRow->setContentsMargins(0, 0, CollectionGrid::RightPadding + 12, 0);

  m_btnRefresh = new QPushButton(this);
  m_btnRefresh->setObjectName("ManageRefreshBtn");
  m_btnRefresh->setCursor(Qt::PointingHandCursor);
  m_btnRefresh->setFixedSize(32, 32);
  m_btnRefresh->setToolTip(tr("Refresh"));
  titleRow->addWidget(m_btnRefresh);

  titleRow->addSpacing(6);

  m_btnCreate = new QPushButton(this);
  m_btnCreate->setObjectName("ManageLibCreateBtn");
  m_btnCreate->setCursor(Qt::PointingHandCursor);
  m_btnCreate->setFixedSize(32, 32);
  m_btnCreate->setToolTip(tr("Create playlist"));
  titleRow->addWidget(m_btnCreate);

  titleRow->addSpacing(6);

  m_btnSelectAll = new QPushButton(this);
  m_btnSelectAll->setObjectName("ManageListSelectBtn");
  m_btnSelectAll->setCursor(Qt::PointingHandCursor);
  m_btnSelectAll->setFixedSize(32, 32);
  m_btnSelectAll->setToolTip(tr("Select all playlists"));
  m_btnSelectAll->setEnabled(false);
  titleRow->addWidget(m_btnSelectAll);

  titleRow->addSpacing(6);

  m_btnDeleteSelected = new QPushButton(this);
  m_btnDeleteSelected->setObjectName("ManageListDeleteBtn");
  m_btnDeleteSelected->setCursor(Qt::PointingHandCursor);
  m_btnDeleteSelected->setFixedSize(32, 32);
  m_btnDeleteSelected->setToolTip(tr("Delete selected playlists"));
  m_btnDeleteSelected->setEnabled(false);
  titleRow->addWidget(m_btnDeleteSelected);

  titleRow->addSpacing(12);
  titleRow->addStretch(2);

  m_statsLabel = new QLabel(this);
  m_statsLabel->setObjectName("ManageInfoValue");
  m_statsLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  m_statsLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
  titleRow->addWidget(m_statsLabel);

  m_mainLayout->addLayout(titleRow);

  
  auto *scrollContent = new QWidget(this);
  scrollContent->setAttribute(Qt::WA_StyledBackground, true);
  auto *scrollLayout = new QVBoxLayout(scrollContent);
  scrollLayout->setContentsMargins(0, 0, 0, 40);
  scrollLayout->setSpacing(16);

  m_playlistGrid = new CollectionGrid(scrollContent);
  scrollLayout->addWidget(m_playlistGrid, 1);

  
  m_emptyStateContainer = new QWidget(scrollContent);
  m_emptyStateContainer->setSizePolicy(QSizePolicy::Preferred,
                                       QSizePolicy::Expanding);
  auto *emptyLayout = new QVBoxLayout(m_emptyStateContainer);
  emptyLayout->setContentsMargins(0, 0, 0, 0);
  emptyLayout->setSpacing(0);
  emptyLayout->addStretch(1);

  m_emptyLabel = new QLabel(tr("No playlists found"), m_emptyStateContainer);
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

  
  connect(m_btnCreate, &QPushButton::clicked, this,
          [this]() { m_pendingTask = onCreateClicked(); });
  connect(m_btnSelectAll, &QPushButton::clicked, this,
          [this]() { toggleSelectAll(); });
  connect(m_btnDeleteSelected, &QPushButton::clicked, this,
          [this]() { m_pendingTask = onDeleteSelectedClicked(); });
  connect(m_btnRefresh, &QPushButton::clicked, this,
          [this]() { m_pendingTask = loadData(true); });
  connect(m_playlistGrid, &CollectionGrid::orderChanged, this,
          [this](const QStringList &ids) { m_pendingTask = onOrderChanged(ids); });
}

void PageCollections::showEvent(QShowEvent *event) {
  ManagePageBase::showEvent(event);
  updateLoadingOverlayGeometry();
  if (!m_loaded) {
    m_pendingTask = loadData();
    m_loaded = true;
  }
}

void PageCollections::resizeEvent(QResizeEvent *event) {
  ManagePageBase::resizeEvent(event);
  updateLoadingOverlayGeometry();
}

void PageCollections::updateStats() {
  if (m_isLoading) {
    m_statsLabel->setText(tr("Loading..."));
    return;
  }

  const QString playlistCountText = tr("%n playlist(s)", "", m_playlists.size());
  if (m_selectedPlaylistIds.isEmpty()) {
    m_statsLabel->setText(playlistCountText);
    return;
  }

  m_statsLabel->setText(
      tr("%1  ·  %2")
          .arg(playlistCountText,
               tr("%n selected", "", m_selectedPlaylistIds.size())));
}

void PageCollections::updateContentState() {
  const bool empty = m_playlists.isEmpty();
  m_playlistGrid->setVisible(!empty);
  m_emptyStateContainer->setVisible(empty);
  m_emptyLabel->setVisible(empty);
  m_emptyLabel->setText(tr("No playlists found"));
  updateSelectionState();
}

void PageCollections::updateSelectionState() {
  m_btnRefresh->setEnabled(!m_isLoading);
  m_btnCreate->setEnabled(!m_isLoading);
  m_btnSelectAll->setEnabled(!m_isLoading && !m_playlists.isEmpty());
  m_btnDeleteSelected->setEnabled(!m_isLoading &&
                                  !m_selectedPlaylistIds.isEmpty());
  m_btnSelectAll->setToolTip(areAllPlaylistsSelected()
                                 ? tr("Clear playlist selection")
                                 : tr("Select all playlists"));
  updateStats();
}

QStringList PageCollections::playlistOrderIds() const {
  QStringList ids;
  ids.reserve(m_playlists.size());
  for (const PlaylistEntry& playlist : m_playlists) {
    ids.append(playlist.id);
  }
  return ids;
}

void PageCollections::reorderPlaylists(const QStringList& orderedIds) {
  if (orderedIds.isEmpty() || m_playlists.isEmpty()) {
    return;
  }

  QHash<QString, PlaylistEntry> playlistById;
  playlistById.reserve(m_playlists.size());
  for (const PlaylistEntry& playlist : m_playlists) {
    playlistById.insert(playlist.id, playlist);
  }
  const QList<PlaylistEntry> originalPlaylists = m_playlists;

  QList<PlaylistEntry> reordered;
  reordered.reserve(m_playlists.size());

  for (const QString& itemId : orderedIds) {
    auto it = playlistById.find(itemId);
    if (it == playlistById.end()) {
      continue;
    }

    reordered.append(it.value());
    playlistById.erase(it);
  }

  for (const PlaylistEntry& playlist : originalPlaylists) {
    if (playlistById.contains(playlist.id)) {
      reordered.append(playlist);
    }
  }

  m_playlists = reordered;
}

void PageCollections::syncCardSelection() {
  for (auto *card : m_playlistGrid->cards()) {
    card->setSelected(m_selectedPlaylistIds.contains(card->itemId()));
  }
}

void PageCollections::onCardSelectionChanged(const QString &itemId,
                                             bool selected) {
  if (selected) {
    m_selectedPlaylistIds.insert(itemId);
  } else {
    m_selectedPlaylistIds.remove(itemId);
  }

  qDebug() << "[PageCollections] Playlist selection changed"
           << "| itemId=" << itemId
           << "| selected=" << selected
           << "| selectedCount=" << m_selectedPlaylistIds.size();

  updateSelectionState();
}

void PageCollections::toggleSelectAll() {
  if (areAllPlaylistsSelected()) {
    m_selectedPlaylistIds.clear();
  } else {
    m_selectedPlaylistIds.clear();
    for (const PlaylistEntry &playlist : m_playlists) {
      m_selectedPlaylistIds.insert(playlist.id);
    }
  }

  syncCardSelection();
  updateSelectionState();
}

bool PageCollections::areAllPlaylistsSelected() const {
  return !m_playlists.isEmpty() &&
         m_selectedPlaylistIds.size() == m_playlists.size();
}

void PageCollections::setLoadingState(bool loading) {
  if (m_isLoading == loading) {
    return;
  }

  m_isLoading = loading;
  qDebug() << "[PageCollections] Loading state changed"
           << "| loading=" << loading;
  updateSelectionState();

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

void PageCollections::updateLoadingOverlayGeometry() {
  if (!m_loadingOverlay || !m_scrollArea) {
    return;
  }

  m_loadingOverlay->setGeometry(m_scrollArea->viewport()->rect());
}

QCoro::Task<void> PageCollections::loadData(bool isManual) {
  QPointer<PageCollections> safeThis(this);
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
    struct PendingImageLoad {
      CollectionCard *card = nullptr;
      QString itemId;
    };

    QList<PlaylistEntry> loadedPlaylists;
    QSet<QString> retainedSelectionIds;
    QList<PendingImageLoad> pendingImageLoads;

    QString playlistViewId;
    QString playlistViewName;
    bool usedLegacyFallback = false;

    const QList<MediaItem> views = co_await m_core->mediaService()->getUserViews();
    if (!safeThis) co_return;

    for (const MediaItem &view : views) {
      if (view.collectionType.compare(QStringLiteral("playlists"),
                                      Qt::CaseInsensitive) == 0) {
        playlistViewId = view.id;
        playlistViewName = view.name;
        break;
      }
    }

    if (!playlistViewId.isEmpty()) {
      qDebug() << "[PageCollections] Loading playlists via media view"
               << "| playlistViewId=" << playlistViewId
               << "| playlistViewName=" << playlistViewName
               << "| sortBy=DateCreated"
               << "| sortOrder=Descending"
               << "| recursive=false";

      const QList<MediaItem> playlistItems =
          co_await m_core->mediaService()->getLibraryItems(
              playlistViewId, QStringLiteral("DateCreated"),
              QStringLiteral("Descending"), QString(), QString(), 0, 0, false,
              true);
      if (!safeThis) co_return;

      loadedPlaylists.reserve(playlistItems.size());
      pendingImageLoads.reserve(playlistItems.size());

      for (const MediaItem &item : playlistItems) {
        PlaylistEntry playlist;
        playlist.id = item.id;
        playlist.name = item.name;
        playlist.childCount = item.childCount > 0 ? item.childCount
                                                  : item.recursiveItemCount;
        playlist.dateCreatedRaw = item.dateCreatedRaw;
        loadedPlaylists.append(playlist);
      }
    } else {
      usedLegacyFallback = true;
      qWarning() << "[PageCollections] Playlist view missing in getUserViews,"
                 << "falling back to admin playlist query";

      const QJsonArray playlists =
          co_await m_core->adminService()->getUserPlaylists();
      if (!safeThis) co_return;

      loadedPlaylists.reserve(playlists.size());
      pendingImageLoads.reserve(playlists.size());

      for (int i = 0; i < playlists.size(); ++i) {
        QJsonObject item = playlists[i].toObject();
        PlaylistEntry playlist;
        playlist.id = item["Id"].toString();
        playlist.name = item["Name"].toString();
        playlist.childCount = item["ChildCount"].toInt();
        playlist.dateCreatedRaw = item["DateCreated"].toString();
        loadedPlaylists.append(playlist);
      }
    }

    const quint64 imageLoadGeneration = ++m_cardImageLoadGeneration;

    QHash<QString, CollectionCard *> existingCardsById;
    const QList<CollectionCard *> existingCards = m_playlistGrid->cards();
    existingCardsById.reserve(existingCards.size());
    for (auto *card : existingCards) {
      if (!card) {
        continue;
      }
      existingCardsById.insert(card->itemId(), card);
    }

    int reusedCardCount = 0;
    int createdCardCount = 0;
    const int existingCardCount = existingCards.size();

    
    {
      QList<CollectionCard *> cards;
      cards.reserve(loadedPlaylists.size());

      for (const PlaylistEntry &playlist : loadedPlaylists) {
        if (m_selectedPlaylistIds.contains(playlist.id)) {
          retainedSelectionIds.insert(playlist.id);
        }

        CollectionCard *card = existingCardsById.take(playlist.id);
        const bool isSelected = retainedSelectionIds.contains(playlist.id);
        bool shouldReloadImage = isManual;

        if (card) {
          ++reusedCardCount;
          card->setDisplayData(playlist.name, playlist.childCount,
                               QStringLiteral("Playlist"));
          card->setSelected(isSelected);
          shouldReloadImage = isManual || !card->hasImage();
        } else {
          ++createdCardCount;
          card = new CollectionCard(playlist.id, playlist.name,
                                    playlist.childCount, "Playlist",
                                    m_playlistGrid);
          card->setSelected(isSelected);
          connect(card, &CollectionCard::renameClicked, this,
                  [this](const QString &itemId, const QString &itemName) {
                    m_pendingTask = onRenameClicked(itemId, itemName);
                  });
          connect(card, &CollectionCard::deleteClicked, this,
                  [this](const QString &itemId, const QString &itemName) {
                    m_pendingTask = onDeleteClicked(itemId, itemName);
                  });
          connect(card, &CollectionCard::selectionChanged, this,
                  [this](const QString &itemId, bool selected) {
                    onCardSelectionChanged(itemId, selected);
                  });
          shouldReloadImage = true;
        }

        cards.append(card);
        if (shouldReloadImage) {
          PendingImageLoad imageLoad;
          imageLoad.card = card;
          imageLoad.itemId = playlist.id;
          pendingImageLoads.append(imageLoad);
        }
      }

      qDebug() << "[PageCollections] Playlist cards diff"
               << "| source="
               << (usedLegacyFallback ? "AdminServiceFallback"
                                      : "MediaServiceLibraryItems")
               << "| total=" << loadedPlaylists.size()
               << "| reused=" << reusedCardCount
               << "| created=" << createdCardCount
               << "| removed=" << (existingCardCount - reusedCardCount)
               << "| manualReload=" << isManual;

      m_playlists = loadedPlaylists;
      m_selectedPlaylistIds = retainedSelectionIds;
      m_playlistGrid->setCards(cards);
      syncCardSelection();

      
      for (const PendingImageLoad &imageLoad : pendingImageLoads) {
        loadCardImage(imageLoad.card, imageLoad.itemId, imageLoadGeneration);
      }
    }

    updateContentState();

  } catch (const std::exception &e) {
    if (!safeThis) co_return;
    ModernToast::showMessage(
        tr("Failed to load: %1").arg(QString::fromUtf8(e.what())), 3000);
  }

  if (!safeThis) co_return;
  setLoadingState(false);
}

QCoro::Task<void> PageCollections::onCreateClicked() {
  QPointer<PageCollections> safeThis(this);

  bool ok = false;
  const QString playlistName = TextInputDialog::getText(
      this, tr("Create Playlist"), tr("Playlist name"), QString(),
      tr("Enter playlist name"), tr("Create"), &ok);
  const QString trimmedName = playlistName.trimmed();

  if (!ok || trimmedName.isEmpty()) co_return;

  qDebug() << "[PageCollections] Creating playlist"
           << "| name=" << trimmedName;

  try {
    const QString playlistId =
        co_await m_core->adminService()->createPlaylist(trimmedName);
    if (!safeThis) co_return;

    qDebug() << "[PageCollections] Playlist created"
             << "| id=" << playlistId
             << "| name=" << trimmedName;
    ModernToast::showMessage(tr("Playlist created"), 2000);
    co_await loadData(false);
  } catch (const std::exception &e) {
    if (!safeThis) co_return;
    ModernToast::showMessage(
        tr("Failed to create playlist: %1").arg(QString::fromUtf8(e.what())),
        3000);
  }
}

QCoro::Task<void> PageCollections::loadCardImage(CollectionCard *card,
                                                  QString itemId,
                                                  quint64 imageLoadGeneration) {
  QPointer<PageCollections> safeThis(this);
  QPointer<CollectionCard> safeCard(card);

  try {
    if (itemId.isEmpty()) co_return;

    QPixmap pixmap = co_await m_core->mediaService()->fetchImage(
        itemId, "Primary", QString(), CollectionGrid::CardWidth * 2);

    if (!safeThis || !safeCard) co_return;
    if (imageLoadGeneration != safeThis->m_cardImageLoadGeneration) co_return;
    if (safeCard->itemId() != itemId) co_return;

    if (!pixmap.isNull()) {
      safeCard->setImage(pixmap);
    }
  } catch (...) {
    qDebug() << "[PageCollections] Failed to load card image"
             << "itemId=" << itemId;
  }
}

QCoro::Task<void> PageCollections::onRenameClicked(QString itemId,
                                                    QString itemName) {
  QPointer<PageCollections> safeThis(this);

  bool ok = false;
  const QString newName = TextInputDialog::getText(
      this, tr("Rename Playlist"), tr("Playlist name"), itemName,
      tr("Enter playlist name"), tr("Save"), &ok);
  const QString trimmedName = newName.trimmed();

  if (!ok || trimmedName.isEmpty() || trimmedName == itemName)
    co_return;

  qDebug() << "[PageCollections] Renaming playlist"
           << "| itemId=" << itemId
           << "| oldName=" << itemName
           << "| newName=" << trimmedName;

  try {
    co_await m_core->adminService()->renameItem(itemId, trimmedName);
    if (!safeThis) co_return;
    ModernToast::showMessage(tr("Renamed successfully"), 2000);
    co_await loadData(false);
  } catch (const std::exception &e) {
    if (!safeThis) co_return;
    ModernToast::showMessage(
        tr("Rename failed: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}

QCoro::Task<void> PageCollections::onDeleteClicked(QString itemId,
                                                    QString itemName) {
  QPointer<PageCollections> safeThis(this);

  bool confirmed = ModernMessageBox::question(
      this, tr("Delete Playlist"),
      tr("Are you sure you want to delete playlist \"%1\"?\n\nThis action "
         "cannot be undone.")
          .arg(itemName),
      tr("Delete"), tr("Cancel"), ModernMessageBox::Danger,
      ModernMessageBox::Warning);

  if (!confirmed) co_return;

  qDebug() << "[PageCollections] Deleting playlist"
           << "| itemId=" << itemId
           << "| name=" << itemName;

  try {
    co_await m_core->adminService()->deleteItem(itemId);
    if (!safeThis) co_return;
    ModernToast::showMessage(tr("\"%1\" deleted").arg(itemName), 2000);
    co_await loadData(false);
  } catch (const std::exception &e) {
    if (!safeThis) co_return;
    ModernToast::showMessage(
        tr("Delete failed: %1").arg(QString::fromUtf8(e.what())), 3000);
  }
}

QCoro::Task<void> PageCollections::onDeleteSelectedClicked() {
  QPointer<PageCollections> safeThis(this);
  const int selectedCount = m_selectedPlaylistIds.size();
  if (selectedCount <= 0) co_return;

  const bool confirmed = ModernMessageBox::question(
      this, tr("Delete Selected"),
      tr("Are you sure you want to delete %n selected playlist(s)?\n\nThis "
         "action cannot be undone.",
         "", selectedCount),
      tr("Delete"), tr("Cancel"), ModernMessageBox::Danger,
      ModernMessageBox::Warning);
  if (!confirmed) co_return;

  int deletedCount = 0;
  QString failureMessage;

  for (const PlaylistEntry &playlist : m_playlists) {
    if (!m_selectedPlaylistIds.contains(playlist.id)) {
      continue;
    }

    qDebug() << "[PageCollections] Batch deleting playlist"
             << "| itemId=" << playlist.id
             << "| name=" << playlist.name;

    try {
      co_await m_core->adminService()->deleteItem(playlist.id);
      if (!safeThis) co_return;
      ++deletedCount;
    } catch (const std::exception &e) {
      failureMessage = QString::fromUtf8(e.what());
      qWarning() << "[PageCollections] Failed to delete selected playlist"
                 << "| itemId=" << playlist.id
                 << "| name=" << playlist.name
                 << "| error=" << failureMessage;
      break;
    }
  }

  if (!safeThis) co_return;

  m_selectedPlaylistIds.clear();
  if (!failureMessage.isEmpty()) {
    if (deletedCount > 0) {
      ModernToast::showMessage(
          tr("Deleted %1 playlist(s), then failed: %2")
              .arg(deletedCount)
              .arg(failureMessage),
          3500);
    } else {
      ModernToast::showMessage(
          tr("Failed to delete selected playlists: %1").arg(failureMessage),
          3000);
    }
  } else {
    ModernToast::showMessage(tr("Deleted %n playlist(s)", "", deletedCount),
                             2000);
  }

  co_await loadData(false);
}

QCoro::Task<void> PageCollections::onOrderChanged(QStringList newOrderIds) {
  if (newOrderIds.isEmpty()) {
    co_return;
  }

  QPointer<PageCollections> safeThis(this);
  const QStringList oldOrderIds = playlistOrderIds();
  if (oldOrderIds == newOrderIds) {
    qDebug() << "[PageCollections] Playlist order unchanged, skip save";
    co_return;
  }

  setLoadingState(true);
  QElapsedTimer saveTimer;
  saveTimer.start();
  bool shouldReloadAfterFailure = false;

  try {
    struct PlaylistDateUpdate {
      QString itemId;
      QString dateCreatedRaw;
    };

    QString movedItemId;
    int movedNewIndex = -1;
    int firstMismatch = -1;
    int lastMismatch = -1;

    for (int i = 0; i < oldOrderIds.size() && i < newOrderIds.size(); ++i) {
      if (oldOrderIds[i] != newOrderIds[i]) {
        if (firstMismatch < 0) {
          firstMismatch = i;
        }
        lastMismatch = i;
      }
    }

    if (firstMismatch >= 0 && lastMismatch >= 0) {
      if (oldOrderIds[firstMismatch] == newOrderIds[lastMismatch]) {
        movedItemId = oldOrderIds[firstMismatch];
        movedNewIndex = lastMismatch;
      } else if (oldOrderIds[lastMismatch] == newOrderIds[firstMismatch]) {
        movedItemId = oldOrderIds[lastMismatch];
        movedNewIndex = firstMismatch;
      }
    }

    if (movedNewIndex >= 0) {
      const auto dateAt = [&](int orderedIndex) -> QString {
        if (orderedIndex < 0 || orderedIndex >= newOrderIds.size()) {
          return {};
        }

        for (const PlaylistEntry& playlist : m_playlists) {
          if (playlist.id == newOrderIds[orderedIndex]) {
            return playlist.dateCreatedRaw;
          }
        }
        return {};
      };

      qDebug() << "[PageCollections] Playlist reorder neighborhood"
               << "| movedItemId=" << movedItemId
               << "| movedNewIndex=" << movedNewIndex
               << "| prevDateCreated=" << dateAt(movedNewIndex - 1)
               << "| currentDateCreated="
               << [this, &movedItemId]() {
                    for (const PlaylistEntry& playlist : m_playlists) {
                      if (playlist.id == movedItemId) {
                        return playlist.dateCreatedRaw;
                      }
                    }
                    return QString();
                  }()
               << "| nextDateCreated=" << dateAt(movedNewIndex + 1);
    }

    auto playlistById = [this](const QString& itemId) -> const PlaylistEntry* {
      for (const PlaylistEntry& playlist : m_playlists) {
        if (playlist.id == itemId) {
          return &playlist;
        }
      }
      return nullptr;
    };

    auto buildSingleDateUpdatePlan =
        [&](const QStringList& orderedIds,
            int targetIndex) -> std::optional<QList<PlaylistDateUpdate>> {
      if (movedItemId.isEmpty() || targetIndex < 0 ||
          targetIndex >= orderedIds.size()) {
        return std::nullopt;
      }

      auto playlistTimeAt = [&](int orderedIndex) -> QDateTime {
        if (orderedIndex < 0 || orderedIndex >= orderedIds.size()) {
          return {};
        }

        const PlaylistEntry* playlist = playlistById(orderedIds[orderedIndex]);
        return playlist ? parsePlaylistDateCreated(playlist->dateCreatedRaw)
                        : QDateTime();
      };

      QDateTime targetDateCreated;
      if (orderedIds.size() == 1) {
        targetDateCreated = QDateTime::currentDateTimeUtc();
      } else if (targetIndex == 0) {
        const QDateTime nextTime = playlistTimeAt(1);
        if (!nextTime.isValid()) {
          return std::nullopt;
        }
        targetDateCreated = nextTime.addSecs(1);
      } else if (targetIndex == orderedIds.size() - 1) {
        const QDateTime prevTime = playlistTimeAt(targetIndex - 1);
        if (!prevTime.isValid()) {
          return std::nullopt;
        }
        targetDateCreated = prevTime.addSecs(-1);
      } else {
        const QDateTime prevTime = playlistTimeAt(targetIndex - 1);
        const QDateTime nextTime = playlistTimeAt(targetIndex + 1);
        if (!prevTime.isValid() || !nextTime.isValid()) {
          return std::nullopt;
        }

        const qint64 gapSecs = nextTime.secsTo(prevTime);
        if (gapSecs <= 1) {
          return std::nullopt;
        }

        targetDateCreated = nextTime.addSecs(gapSecs / 2);
      }

      PlaylistDateUpdate update;
      update.itemId = movedItemId;
      update.dateCreatedRaw =
          targetDateCreated.toUTC().toString(Qt::ISODateWithMs);
      QList<PlaylistDateUpdate> updates;
      updates.append(update);
      return updates;
    };

    auto buildLocalDateUpdatePlan =
        [&](const QStringList& orderedIds,
            int initialWindowStart,
            int initialWindowEnd) -> std::optional<QList<PlaylistDateUpdate>> {
      if (orderedIds.isEmpty() || initialWindowStart < 0 ||
          initialWindowEnd < initialWindowStart ||
          initialWindowEnd >= orderedIds.size()) {
        return std::nullopt;
      }

      const int lastIndex = orderedIds.size() - 1;
      int windowStart = initialWindowStart;
      int windowEnd = initialWindowEnd;
      bool preferExpandLeft =
          movedNewIndex >= 0 && movedNewIndex <= initialWindowStart;

      auto anchorTimeAt = [&](int orderedIndex) -> QDateTime {
        if (orderedIndex < 0 || orderedIndex > lastIndex) {
          return {};
        }

        const PlaylistEntry* playlist = playlistById(orderedIds[orderedIndex]);
        return playlist ? parsePlaylistDateCreated(playlist->dateCreatedRaw)
                        : QDateTime();
      };

      auto hasEnoughRoom = [&](const QDateTime& leftAnchor,
                               const QDateTime& rightAnchor) -> bool {
        const int windowCount = windowEnd - windowStart + 1;
        const bool openTop = (windowStart == 0);
        const bool openBottom = (windowEnd == lastIndex);

        if (openTop && openBottom) {
          return true;
        }

        if (openTop) {
          return rightAnchor.isValid();
        }

        if (openBottom) {
          return leftAnchor.isValid();
        }

        if (!leftAnchor.isValid() || !rightAnchor.isValid()) {
          return false;
        }

        const qint64 leftSecs = leftAnchor.toSecsSinceEpoch();
        const qint64 rightSecs = rightAnchor.toSecsSinceEpoch();
        return leftSecs > rightSecs &&
               (leftSecs - rightSecs - 1) >= windowCount;
      };

      while (true) {
        const QDateTime leftAnchor = anchorTimeAt(windowStart - 1);
        const QDateTime rightAnchor = anchorTimeAt(windowEnd + 1);

        if (hasEnoughRoom(leftAnchor, rightAnchor)) {
          QList<PlaylistDateUpdate> updates;
          const int windowCount = windowEnd - windowStart + 1;
          updates.reserve(windowCount);

          if (windowStart == 0 && windowEnd == lastIndex) {
            qint64 baseSecs =
                QDateTime::currentDateTimeUtc().toSecsSinceEpoch() + windowCount;
            for (int i = 0; i < windowCount; ++i) {
              const qint64 dateSecs = baseSecs - i;
              PlaylistDateUpdate update;
              update.itemId = orderedIds[windowStart + i];
              update.dateCreatedRaw =
                  QDateTime::fromSecsSinceEpoch(dateSecs, Qt::UTC)
                      .toString(Qt::ISODateWithMs);
              updates.append(update);
            }
          } else if (windowStart == 0) {
            const qint64 rightSecs = rightAnchor.toSecsSinceEpoch();
            for (int i = 0; i < windowCount; ++i) {
              const qint64 dateSecs = rightSecs + (windowCount - i);
              PlaylistDateUpdate update;
              update.itemId = orderedIds[windowStart + i];
              update.dateCreatedRaw =
                  QDateTime::fromSecsSinceEpoch(dateSecs, Qt::UTC)
                      .toString(Qt::ISODateWithMs);
              updates.append(update);
            }
          } else if (windowEnd == lastIndex) {
            const qint64 leftSecs = leftAnchor.toSecsSinceEpoch();
            for (int i = 0; i < windowCount; ++i) {
              const qint64 dateSecs = leftSecs - (i + 1);
              PlaylistDateUpdate update;
              update.itemId = orderedIds[windowStart + i];
              update.dateCreatedRaw =
                  QDateTime::fromSecsSinceEpoch(dateSecs, Qt::UTC)
                      .toString(Qt::ISODateWithMs);
              updates.append(update);
            }
          } else {
            const qint64 leftSecs = leftAnchor.toSecsSinceEpoch();
            const qint64 rightSecs = rightAnchor.toSecsSinceEpoch();
            const qint64 availableSecs = leftSecs - rightSecs - 1;

            for (int i = 0; i < windowCount; ++i) {
              const qint64 offset =
                  ((static_cast<qint64>(i) + 1) * (availableSecs + 1)) /
                  (windowCount + 1);
              const qint64 dateSecs = leftSecs - offset;
              PlaylistDateUpdate update;
              update.itemId = orderedIds[windowStart + i];
              update.dateCreatedRaw =
                  QDateTime::fromSecsSinceEpoch(dateSecs, Qt::UTC)
                      .toString(Qt::ISODateWithMs);
              updates.append(update);
            }
          }

          qDebug() << "[PageCollections] Built localized playlist date update plan"
                   << "| windowStart=" << windowStart
                   << "| windowEnd=" << windowEnd
                   << "| updateCount=" << updates.size();
          return updates;
        }

        if (windowStart == 0 && windowEnd == lastIndex) {
          return std::nullopt;
        }

        bool expanded = false;
        if (preferExpandLeft && windowStart > 0) {
          --windowStart;
          expanded = true;
        } else if (!preferExpandLeft && windowEnd < lastIndex) {
          ++windowEnd;
          expanded = true;
        } else if (windowStart > 0) {
          --windowStart;
          expanded = true;
        } else if (windowEnd < lastIndex) {
          ++windowEnd;
          expanded = true;
        }

        if (!expanded) {
          return std::nullopt;
        }

        preferExpandLeft = !preferExpandLeft;
      }
    };

    std::optional<QList<PlaylistDateUpdate>> localizedUpdatePlan;
    const std::optional<QList<PlaylistDateUpdate>> singleUpdatePlan =
        buildSingleDateUpdatePlan(newOrderIds, movedNewIndex);
    if (singleUpdatePlan.has_value()) {
      localizedUpdatePlan = singleUpdatePlan;
    } else if (movedNewIndex >= 0) {
      localizedUpdatePlan =
          buildLocalDateUpdatePlan(newOrderIds, movedNewIndex, movedNewIndex);
    } else if (firstMismatch >= 0 && lastMismatch >= 0) {
      localizedUpdatePlan =
          buildLocalDateUpdatePlan(newOrderIds, firstMismatch, lastMismatch);
    }
    const bool canUseLocalizedUpdate = localizedUpdatePlan.has_value() &&
                                       !localizedUpdatePlan->isEmpty();

    qDebug() << "[PageCollections] Saving playlist order by DateCreated"
             << "| count=" << newOrderIds.size()
             << "| ids=" << newOrderIds
             << "| movedItemId=" << movedItemId
             << "| movedNewIndex=" << movedNewIndex
             << "| singleUpdate=" << singleUpdatePlan.has_value()
             << "| localizedUpdate=" << canUseLocalizedUpdate
             << "| localizedUpdateCount="
             << (canUseLocalizedUpdate ? localizedUpdatePlan->size() : 0);

    if (canUseLocalizedUpdate) {
      for (const PlaylistDateUpdate& update : *localizedUpdatePlan) {
        co_await m_core->adminService()->updatePlaylistDateCreated(
            update.itemId, update.dateCreatedRaw);

        for (PlaylistEntry& playlist : m_playlists) {
          if (playlist.id == update.itemId) {
            playlist.dateCreatedRaw = update.dateCreatedRaw;
            break;
          }
        }
      }
    } else {
      qWarning() << "[PageCollections] Falling back to full playlist reindex"
                 << "| movedItemId=" << movedItemId
                 << "| movedNewIndex=" << movedNewIndex
                 << "| firstMismatch=" << firstMismatch
                 << "| lastMismatch=" << lastMismatch;
      co_await m_core->adminService()->updatePlaylistOrderByDateCreated(newOrderIds);
    }
    if (!safeThis) co_return;

    reorderPlaylists(newOrderIds);
    qDebug() << "[PageCollections] Playlist order save completed"
             << "| elapsedMs=" << saveTimer.elapsed()
             << "| reloaded=false";
    ModernToast::showMessage(tr("Playlist order updated"), 1500);
  } catch (const std::exception &e) {
    if (!safeThis) co_return;

    qWarning() << "[PageCollections] Playlist order save failed"
               << "| elapsedMs=" << saveTimer.elapsed()
               << "| error=" << e.what();
    ModernToast::showMessage(
        tr("Failed to save playlist order: %1").arg(QString::fromUtf8(e.what())),
        3000);

    if (!oldOrderIds.isEmpty()) {
      shouldReloadAfterFailure = true;
    }
  }

  if (!safeThis) co_return;
  if (shouldReloadAfterFailure) {
    co_await loadData(false);
    co_return;
  }
  setLoadingState(false);
}
