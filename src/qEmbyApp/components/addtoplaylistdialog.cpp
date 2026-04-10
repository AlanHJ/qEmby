#include "addtoplaylistdialog.h"

#include "loadingoverlay.h"
#include "moderntoast.h"
#include "playlistcreateeditorrow.h"
#include "../managers/thememanager.h"
#include "../utils/playlistutils.h"

#include <qembycore.h>
#include <services/admin/adminservice.h>
#include <services/media/mediaservice.h>
#include <services/manager/servermanager.h>
#include <models/profile/serverprofile.h>

#include <QAbstractItemView>
#include <QAction>
#include <QDebug>
#include <QHash>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPointer>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <stdexcept>
#include <utility>

namespace {

QHash<QString, QString> s_playlistViewIdCache;
constexpr int kCreatePlaylistItemRole = Qt::UserRole + 2;

} 

AddToPlaylistDialog::AddToPlaylistDialog(QEmbyCore* core,
                                         QString targetPlaylistMediaType,
                                         QWidget* parent)
    : ModernDialogBase(parent),
      m_core(core),
      m_targetPlaylistMediaType(
          PlaylistUtils::normalizePlaylistMediaType(targetPlaylistMediaType))
{
    setMinimumWidth(400);
    setTitle(tr("Add to Playlist"));
    contentLayout()->setSpacing(0);

    m_promptLabel = new QLabel(tr("Select a playlist"), this);
    m_promptLabel->setObjectName("dialog-text");
    contentLayout()->addWidget(m_promptLabel);
    contentLayout()->addSpacing(8);

    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setObjectName("PlaylistSearchEdit");
    m_searchEdit->setPlaceholderText(tr("Search playlists..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->addAction(
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/search.svg")),
        QLineEdit::LeadingPosition);
    contentLayout()->addWidget(m_searchEdit);
    contentLayout()->addSpacing(10);

    m_playlistListContainer = new QWidget(this);
    auto* listLayout = new QVBoxLayout(m_playlistListContainer);
    listLayout->setContentsMargins(0, 0, 0, 0);
    listLayout->setSpacing(0);

    m_playlistList = new QListWidget(m_playlistListContainer);
    m_playlistList->setObjectName("ManageLibPathList");
    m_playlistList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_playlistList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_playlistList->setMinimumHeight(260);
    m_playlistList->setAlternatingRowColors(false);
    m_playlistList->setUniformItemSizes(false);
    listLayout->addWidget(m_playlistList);

    m_loadingOverlay = new LoadingOverlay(m_playlistListContainer);
    m_loadingOverlay->setHudPanelVisible(false);
    m_loadingOverlay->setSubtleOverlay(true);

    contentLayout()->addWidget(m_playlistListContainer);
    contentLayout()->addSpacing(24);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->setSpacing(12);

    buttonLayout->addStretch();

    auto* cancelButton = new QPushButton(tr("Cancel"), this);
    cancelButton->setObjectName("dialog-btn-cancel");
    cancelButton->setCursor(Qt::PointingHandCursor);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_addButton = new QPushButton(tr("Add"), this);
    m_addButton->setObjectName("dialog-btn-primary");
    m_addButton->setCursor(Qt::PointingHandCursor);
    m_addButton->setEnabled(false);
    buttonLayout->addWidget(m_addButton);

    contentLayout()->addLayout(buttonLayout);

    connect(m_searchEdit, &QLineEdit::textChanged, this,
            [this]() { rebuildPlaylistList(); });
    connect(m_searchEdit, &QLineEdit::returnPressed, this, [this]() {
        if (!m_playlistList->currentItem() && m_playlistList->count() == 1) {
            m_playlistList->setCurrentRow(0);
        }
        acceptSelectedPlaylist();
    });
    connect(m_playlistList, &QListWidget::itemSelectionChanged, this,
            [this]() { updateAddButtonState(); });
    connect(m_playlistList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem*) { acceptSelectedPlaylist(); });
    connect(m_addButton, &QPushButton::clicked, this,
            [this]() { acceptSelectedPlaylist(); });

    updateUiState();
    updateLoadingOverlayGeometry();
}

QString AddToPlaylistDialog::selectedPlaylistId() const
{
    const QListWidgetItem* item = m_playlistList ? m_playlistList->currentItem()
                                                 : nullptr;
    if (!item || item->data(kCreatePlaylistItemRole).toBool()) {
        return {};
    }

    return item->data(Qt::UserRole).toString();
}

QString AddToPlaylistDialog::selectedPlaylistName() const
{
    const QListWidgetItem* item = m_playlistList ? m_playlistList->currentItem()
                                                 : nullptr;
    if (!item || item->data(kCreatePlaylistItemRole).toBool()) {
        return {};
    }

    return item->data(Qt::UserRole + 1).toString();
}

void AddToPlaylistDialog::showEvent(QShowEvent* event)
{
    ModernDialogBase::showEvent(event);
    updateLoadingOverlayGeometry();
    m_searchEdit->setFocus();

    if (!m_loaded) {
        m_loaded = true;
        m_pendingTask = loadPlaylists();
    }
}

void AddToPlaylistDialog::resizeEvent(QResizeEvent* event)
{
    ModernDialogBase::resizeEvent(event);
    updateLoadingOverlayGeometry();
}

QString AddToPlaylistDialog::playlistViewCacheKey() const
{
    if (!m_core || !m_core->serverManager()) {
        return {};
    }

    const ServerProfile profile = m_core->serverManager()->activeProfile();
    if (!profile.isValid()) {
        return {};
    }

    return QStringLiteral("%1::%2").arg(profile.id, profile.userId);
}

QCoro::Task<QString> AddToPlaylistDialog::resolvePlaylistViewId(bool forceRefresh)
{
    QPointer<QEmbyCore> safeCore(m_core);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);

    if (!safeCore || !mediaService) {
        co_return {};
    }

    auto findPlaylistViewId = [](const QList<MediaItem>& views) -> QString {
        for (const MediaItem& view : views) {
            if (view.collectionType.compare(QStringLiteral("playlists"),
                                            Qt::CaseInsensitive) == 0 &&
                !view.id.isEmpty()) {
                return view.id;
            }
        }
        return {};
    };

    const QString cacheKey = playlistViewCacheKey();
    if (!forceRefresh && !cacheKey.isEmpty()) {
        const auto it = s_playlistViewIdCache.constFind(cacheKey);
        if (it != s_playlistViewIdCache.constEnd() && !it.value().isEmpty()) {
            co_return it.value();
        }
    }

    if (forceRefresh) {
        mediaService->clearUserViewsCache();
    }

    const QList<MediaItem> views = co_await mediaService->getUserViews();
    if (!safeCore || !mediaService) {
        co_return {};
    }

    QString playlistViewId = findPlaylistViewId(views);
    if (playlistViewId.isEmpty()) {
        const QList<MediaItem> hiddenViews =
            co_await mediaService->getUserViews(true);
        if (!safeCore || !mediaService) {
            co_return {};
        }

        playlistViewId = findPlaylistViewId(hiddenViews);
    }

    if (!cacheKey.isEmpty()) {
        if (playlistViewId.isEmpty()) {
            s_playlistViewIdCache.remove(cacheKey);
        } else {
            s_playlistViewIdCache.insert(cacheKey, playlistViewId);
        }
    }

    co_return playlistViewId;
}

QCoro::Task<void> AddToPlaylistDialog::loadPlaylists(QString preferredPlaylistId)
{
    QPointer<AddToPlaylistDialog> safeThis(this);
    QPointer<QEmbyCore> safeCore(m_core);
    QPointer<MediaService> mediaService(m_core ? m_core->mediaService() : nullptr);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);

    if (!safeThis || !safeCore || !mediaService) {
        co_return;
    }

    m_isLoading = true;
    updateUiState();

    qDebug() << "[AddToPlaylistDialog] Loading playlists"
             << "| targetMediaType=" << m_targetPlaylistMediaType
             << "| preferredPlaylistId=" << preferredPlaylistId;

    try {
        if (!safeThis) {
            co_return;
        }

        QList<PlaylistEntry> loadedPlaylists;
        bool usedLegacyFallback = false;

        QString playlistViewId = co_await safeThis->resolvePlaylistViewId();
        if (!safeThis || !safeCore || !mediaService) {
            co_return;
        }

        if (!playlistViewId.isEmpty()) {
            for (int attempt = 0; attempt < 2; ++attempt) {
                bool retryWithRefreshedViewId = false;
                QString failedViewId;
                QString failedErrorMessage;

                try {
                    qDebug() << "[AddToPlaylistDialog] Loading playlists via media view"
                             << "| playlistViewId=" << playlistViewId
                             << "| attempt=" << (attempt + 1)
                             << "| targetMediaType=" << safeThis->m_targetPlaylistMediaType;

                    const QList<MediaItem> playlistItems =
                        co_await mediaService->getLibraryItems(
                            playlistViewId, QStringLiteral("DateCreated"),
                            QStringLiteral("Descending"), QString(), QString(), 0, 0,
                            false, false);
                    if (!safeThis || !safeCore || !mediaService) {
                        co_return;
                    }

                    loadedPlaylists.clear();
                    loadedPlaylists.reserve(playlistItems.size());
                    for (const MediaItem& item : playlistItems) {
                        PlaylistEntry entry;
                        entry.id = item.id;
                        entry.name = item.name;
                        entry.mediaType =
                            PlaylistUtils::normalizePlaylistMediaType(item.mediaType);
                        entry.childCount = item.childCount > 0 ? item.childCount
                                                               : item.recursiveItemCount;

                        if (entry.id.isEmpty() || entry.name.trimmed().isEmpty()) {
                            continue;
                        }

                        if (!PlaylistUtils::isPlaylistCompatible(
                                entry.mediaType,
                                safeThis->m_targetPlaylistMediaType)) {
                            continue;
                        }

                        loadedPlaylists.append(entry);
                    }

                    break;
                } catch (const std::exception& e) {
                    if (!safeThis || !safeCore || !mediaService) {
                        co_return;
                    }

                    failedViewId = playlistViewId;
                    failedErrorMessage = QString::fromUtf8(e.what());
                    const QString cacheKey = safeThis->playlistViewCacheKey();
                    if (!cacheKey.isEmpty()) {
                        s_playlistViewIdCache.remove(cacheKey);
                    }

                    if (attempt == 0) {
                        retryWithRefreshedViewId = true;
                    } else {
                        usedLegacyFallback = true;
                        qWarning() << "[AddToPlaylistDialog] Media-view playlist load failed,"
                                   << "falling back to admin query"
                                   << "| playlistViewId=" << failedViewId
                                   << "| error=" << failedErrorMessage;
                        break;
                    }
                }

                if (!retryWithRefreshedViewId) {
                    continue;
                }

                playlistViewId = co_await safeThis->resolvePlaylistViewId(true);
                if (!safeThis || !safeCore || !mediaService) {
                    co_return;
                }

                if (!playlistViewId.isEmpty() &&
                    playlistViewId != failedViewId) {
                    qWarning()
                        << "[AddToPlaylistDialog] Cached playlist view load failed,"
                        << "retrying with refreshed playlist view id"
                        << "| cachedPlaylistViewId=" << failedViewId
                        << "| refreshedPlaylistViewId=" << playlistViewId
                        << "| error=" << failedErrorMessage;
                    continue;
                }

                usedLegacyFallback = true;
                qWarning() << "[AddToPlaylistDialog] Media-view playlist load failed,"
                           << "falling back to admin query"
                           << "| playlistViewId=" << failedViewId
                           << "| error=" << failedErrorMessage;
                break;
            }
        } else {
            usedLegacyFallback = true;
            qWarning() << "[AddToPlaylistDialog] Playlist view missing,"
                       << "falling back to admin query";
        }

        if (usedLegacyFallback) {
            if (!adminService) {
                throw std::runtime_error("Admin service is unavailable");
            }

            const QJsonArray playlists =
                co_await adminService->getUserPlaylists();
            if (!safeThis || !safeCore || !adminService) {
                co_return;
            }

            loadedPlaylists.clear();
            loadedPlaylists.reserve(playlists.size());

            for (const QJsonValue& value : playlists) {
                const QJsonObject obj = value.toObject();

                PlaylistEntry entry;
                entry.id = obj.value(QStringLiteral("Id")).toString();
                entry.name = obj.value(QStringLiteral("Name")).toString();
                entry.mediaType = PlaylistUtils::normalizePlaylistMediaType(
                    obj.value(QStringLiteral("MediaType")).toString());
                entry.childCount = obj.value(QStringLiteral("ChildCount")).toInt();

                if (entry.id.isEmpty() || entry.name.trimmed().isEmpty()) {
                    continue;
                }

                if (!PlaylistUtils::isPlaylistCompatible(
                        entry.mediaType,
                        safeThis->m_targetPlaylistMediaType)) {
                    continue;
                }

                loadedPlaylists.append(entry);
            }
        }

        safeThis->m_playlists = loadedPlaylists;
        safeThis->m_isLoading = false;
        safeThis->rebuildPlaylistList(preferredPlaylistId);
        safeThis->updateUiState();

        qDebug() << "[AddToPlaylistDialog] Playlists loaded"
                 << "| compatibleCount=" << loadedPlaylists.size()
                 << "| fallback=" << usedLegacyFallback
                 << "| targetMediaType=" << safeThis->m_targetPlaylistMediaType;
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_playlists.clear();
        safeThis->m_isLoading = false;
        safeThis->rebuildPlaylistList();
        safeThis->updateUiState();

        qWarning() << "[AddToPlaylistDialog] Failed to load playlists"
                   << "| targetMediaType=" << safeThis->m_targetPlaylistMediaType
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to load playlists: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

QCoro::Task<void> AddToPlaylistDialog::createPlaylist(QString playlistName)
{
    QPointer<AddToPlaylistDialog> safeThis(this);
    QPointer<QEmbyCore> safeCore(m_core);
    QPointer<AdminService> adminService(m_core ? m_core->adminService() : nullptr);

    if (!safeThis || !safeCore || !adminService) {
        co_return;
    }

    const QString trimmedName = playlistName.trimmed();
    if (trimmedName.isEmpty()) {
        co_return;
    }

    const QString mediaType =
        m_targetPlaylistMediaType.isEmpty() ? QStringLiteral("Video")
                                            : m_targetPlaylistMediaType;

    m_isLoading = true;
    updateUiState();

    qDebug() << "[AddToPlaylistDialog] Creating playlist"
             << "| name=" << trimmedName
             << "| mediaType=" << mediaType;

    try {
        const QString playlistId =
            co_await adminService->createPlaylist(trimmedName, mediaType);
        if (!safeThis || !safeCore || !adminService) {
            co_return;
        }

        qDebug() << "[AddToPlaylistDialog] Playlist created"
                 << "| id=" << playlistId
                 << "| name=" << trimmedName
                 << "| mediaType=" << mediaType;

        ModernToast::showMessage(tr("Playlist created"), 2000);
        safeThis->m_isCreateItemEditing = false;
        safeThis->m_createPlaylistDraft.clear();
        safeThis->m_createPlaylistEditor = nullptr;
        safeThis->m_searchEdit->clear();
        co_await safeThis->loadPlaylists(playlistId);
    } catch (const std::exception& e) {
        if (!safeThis) {
            co_return;
        }

        safeThis->m_isLoading = false;
        safeThis->m_isCreateItemEditing = true;
        safeThis->m_createPlaylistDraft = trimmedName;
        safeThis->syncCreatePlaylistEditor();
        safeThis->updateUiState();

        qWarning() << "[AddToPlaylistDialog] Failed to create playlist"
                   << "| name=" << trimmedName
                   << "| mediaType=" << mediaType
                   << "| error=" << e.what();
        ModernToast::showMessage(
            tr("Failed to create playlist: %1").arg(QString::fromUtf8(e.what())),
            3000);
    }
}

void AddToPlaylistDialog::beginCreatePlaylistEditing()
{
    if (m_isLoading || m_isCreateItemEditing || !m_playlistList) {
        return;
    }

    m_isCreateItemEditing = true;
    if (m_playlistList->count() > 0) {
        m_playlistList->setCurrentRow(0);
    }
    syncCreatePlaylistEditor();
    updateAddButtonState();
}

void AddToPlaylistDialog::cancelCreatePlaylistEditing()
{
    if (!m_isCreateItemEditing) {
        return;
    }

    m_isCreateItemEditing = false;
    m_createPlaylistDraft.clear();
    m_createPlaylistEditor = nullptr;
    rebuildPlaylistList();
    if (m_playlistList && m_playlistList->count() > 0) {
        m_playlistList->setCurrentRow(0);
    }
    updateAddButtonState();
}

void AddToPlaylistDialog::rebuildPlaylistList(QString preferredPlaylistId)
{
    const QString filter = m_searchEdit ? m_searchEdit->text().trimmed() : QString();
    const QListWidgetItem* currentItem =
        m_playlistList ? m_playlistList->currentItem() : nullptr;
    const bool hadCreateItemSelected =
        currentItem && currentItem->data(kCreatePlaylistItemRole).toBool();
    const QString selectedId =
        preferredPlaylistId.isEmpty() ? selectedPlaylistId() : preferredPlaylistId;

    QSignalBlocker blocker(m_playlistList);
    m_playlistList->clear();
    m_createPlaylistEditor = nullptr;

    auto* createItem = new QListWidgetItem(
        ThemeManager::getAdaptiveIcon(QStringLiteral(":/svg/light/add.svg")),
        tr("New Playlist"), m_playlistList);
    createItem->setData(kCreatePlaylistItemRole, true);
    createItem->setToolTip(tr("New Playlist"));

    int selectedRow = (m_isCreateItemEditing || hadCreateItemSelected) ? 0 : -1;

    for (const PlaylistEntry& playlist : std::as_const(m_playlists)) {
        if (!filter.isEmpty() &&
            !playlist.name.contains(filter, Qt::CaseInsensitive)) {
            continue;
        }

        auto* item = new QListWidgetItem(playlist.name, m_playlistList);
        item->setData(Qt::UserRole, playlist.id);
        item->setData(Qt::UserRole + 1, playlist.name);
        item->setToolTip(playlist.name);

        if (!selectedId.isEmpty() && playlist.id == selectedId) {
            selectedRow = m_playlistList->count() - 1;
        }
    }

    if (selectedRow >= 0) {
        m_playlistList->setCurrentRow(selectedRow);
        m_playlistList->scrollToItem(m_playlistList->item(selectedRow));
    }

    syncCreatePlaylistEditor();
    updateUiState();
    updateAddButtonState();
}

void AddToPlaylistDialog::syncCreatePlaylistEditor()
{
    if (!m_playlistList || m_playlistList->count() == 0) {
        m_createPlaylistEditor = nullptr;
        return;
    }

    QListWidgetItem* createItem = m_playlistList->item(0);
    if (!createItem || !createItem->data(kCreatePlaylistItemRole).toBool()) {
        m_createPlaylistEditor = nullptr;
        return;
    }

    if (!m_isCreateItemEditing) {
        m_playlistList->removeItemWidget(createItem);
        m_createPlaylistEditor = nullptr;
        return;
    }

    createItem->setText(QString());
    createItem->setIcon(QIcon());

    auto* editor =
        qobject_cast<PlaylistCreateEditorRow*>(m_playlistList->itemWidget(createItem));
    if (!editor) {
        editor = new PlaylistCreateEditorRow(m_playlistList);
        editor->setPlaceholderText(tr("Enter playlist name"));
        editor->setText(m_createPlaylistDraft);
        createItem->setSizeHint(
            QSize(0, qMax(editor->sizeHint().height(), 32)));
        m_playlistList->setItemWidget(createItem, editor);
        m_createPlaylistEditor = editor;

        connect(editor, &PlaylistCreateEditorRow::textChanged, this,
                [this](QString text) { m_createPlaylistDraft = std::move(text); });
        connect(editor, &PlaylistCreateEditorRow::confirmed, this,
                [this](QString text) {
                    m_createPlaylistDraft = text;
                    m_pendingTask = createPlaylist(text);
                });
        connect(editor, &PlaylistCreateEditorRow::cancelled, this,
                [this]() { cancelCreatePlaylistEditing(); });
    } else {
        editor->setPlaceholderText(tr("Enter playlist name"));
        editor->setText(m_createPlaylistDraft);
        createItem->setSizeHint(
            QSize(0, qMax(editor->sizeHint().height(), 32)));
        m_createPlaylistEditor = editor;
    }

    if (m_playlistList->currentRow() != 0) {
        m_playlistList->setCurrentRow(0);
    }
    if (m_createPlaylistEditor) {
        m_createPlaylistEditor->focusEditor();
    }
}

void AddToPlaylistDialog::updateLoadingOverlayGeometry()
{
    if (!m_loadingOverlay || !m_playlistListContainer) {
        return;
    }

    m_loadingOverlay->setGeometry(m_playlistListContainer->rect());
}

void AddToPlaylistDialog::updateUiState()
{
    if (m_searchEdit) {
        m_searchEdit->setEnabled(!m_isLoading);
    }
    if (m_playlistList) {
        m_playlistList->setEnabled(!m_isLoading);
    }
    if (m_createPlaylistEditor) {
        m_createPlaylistEditor->setEnabled(!m_isLoading);
    }
    if (m_loadingOverlay) {
        updateLoadingOverlayGeometry();
        if (m_isLoading) {
            m_loadingOverlay->start();
        } else {
            m_loadingOverlay->stop();
        }
    }

    updateAddButtonState();
}

void AddToPlaylistDialog::updateAddButtonState()
{
    if (!m_addButton) {
        return;
    }

    const QListWidgetItem* currentItem =
        m_playlistList ? m_playlistList->currentItem() : nullptr;
    const bool hasSelection = currentItem != nullptr;
    const bool isCreateAction =
        currentItem && currentItem->data(kCreatePlaylistItemRole).toBool();
    const bool canTriggerPrimary =
        hasSelection && !(isCreateAction && m_isCreateItemEditing);

    m_addButton->setText(isCreateAction ? tr("Create") : tr("Add"));
    m_addButton->setEnabled(!m_isLoading && canTriggerPrimary);
}

void AddToPlaylistDialog::acceptSelectedPlaylist()
{
    if (!m_playlistList || !m_playlistList->currentItem() || m_isLoading) {
        return;
    }

    if (m_playlistList->currentItem()->data(kCreatePlaylistItemRole).toBool()) {
        if (!m_isCreateItemEditing) {
            beginCreatePlaylistEditing();
        }
        return;
    }

    qDebug() << "[AddToPlaylistDialog] Playlist selected"
             << "| playlistId=" << selectedPlaylistId()
             << "| playlistName=" << selectedPlaylistName();
    accept();
}
