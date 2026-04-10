#ifndef ADDTOPLAYLISTDIALOG_H
#define ADDTOPLAYLISTDIALOG_H

#include "moderndialogbase.h"

#include <QList>
#include <QPointer>
#include <QString>
#include <optional>
#include <qcorotask.h>

class QLabel;
class QLineEdit;
class QListWidget;
class QResizeEvent;
class QShowEvent;
class QEmbyCore;
class QWidget;
class LoadingOverlay;
class PlaylistCreateEditorRow;
class QPushButton;

class AddToPlaylistDialog : public ModernDialogBase
{
    Q_OBJECT

public:
    explicit AddToPlaylistDialog(QEmbyCore* core,
                                 QString targetPlaylistMediaType,
                                 QWidget* parent = nullptr);

    QString selectedPlaylistId() const;
    QString selectedPlaylistName() const;

protected:
    void showEvent(QShowEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct PlaylistEntry {
        QString id;
        QString name;
        QString mediaType;
        int childCount = 0;
    };

    QString playlistViewCacheKey() const;
    QCoro::Task<QString> resolvePlaylistViewId(bool forceRefresh = false);
    QCoro::Task<void> loadPlaylists(QString preferredPlaylistId = QString());
    QCoro::Task<void> createPlaylist(QString playlistName);

    void beginCreatePlaylistEditing();
    void cancelCreatePlaylistEditing();
    void rebuildPlaylistList(QString preferredPlaylistId = QString());
    void syncCreatePlaylistEditor();
    void updateLoadingOverlayGeometry();
    void updateUiState();
    void updateAddButtonState();
    void acceptSelectedPlaylist();

    QEmbyCore* m_core = nullptr;
    QString m_targetPlaylistMediaType;

    QLabel* m_promptLabel = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QWidget* m_playlistListContainer = nullptr;
    QListWidget* m_playlistList = nullptr;
    LoadingOverlay* m_loadingOverlay = nullptr;
    QPushButton* m_addButton = nullptr;
    bool m_isCreateItemEditing = false;
    QString m_createPlaylistDraft;
    QPointer<PlaylistCreateEditorRow> m_createPlaylistEditor;

    bool m_loaded = false;
    bool m_isLoading = false;
    QList<PlaylistEntry> m_playlists;
    std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
