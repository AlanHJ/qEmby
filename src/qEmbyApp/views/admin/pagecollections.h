#ifndef PAGECOLLECTIONS_H
#define PAGECOLLECTIONS_H

#include "managepagebase.h"
#include <QDateTime>
#include <QList>
#include <QSet>
#include <QStringList>
#include <optional>
#include <qcorotask.h>

class CollectionGrid;
class CollectionCard;
class LoadingOverlay;
class QLabel;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QShowEvent;
class QWidget;




class PageCollections : public ManagePageBase
{
    Q_OBJECT
    public:
        explicit PageCollections(QEmbyCore * core, QWidget *parent = nullptr);
        QScrollArea *scrollArea() const
        {
            return m_scrollArea;
        }

    protected:
        void showEvent(QShowEvent * event) override;
        void resizeEvent(QResizeEvent * event) override;

    private:
        struct PlaylistEntry
        {
            QString id;
            QString name;
            int childCount = 0;
            QString dateCreatedRaw;
        };

        QCoro::Task<void> loadData(bool isManual = false);
        QCoro::Task<void> loadCardImage(CollectionCard * card, QString itemId, quint64 imageLoadGeneration);
        QCoro::Task<void> onCreateClicked();
        QCoro::Task<void> onRenameClicked(QString itemId, QString itemName);
        QCoro::Task<void> onDeleteClicked(QString itemId, QString itemName);
        QCoro::Task<void> onDeleteSelectedClicked();
        QCoro::Task<void> onOrderChanged(QStringList newOrderIds);

        void updateStats();
        void updateContentState();
        void updateSelectionState();
        QStringList playlistOrderIds() const;
        void reorderPlaylists(const QStringList &orderedIds);
        void syncCardSelection();
        void onCardSelectionChanged(const QString &itemId, bool selected);
        void toggleSelectAll();
        bool areAllPlaylistsSelected() const;
        void setLoadingState(bool loading);
        void updateLoadingOverlayGeometry();

        
        QPushButton *m_btnRefresh;
        QPushButton *m_btnCreate;
        QPushButton *m_btnSelectAll;
        QPushButton *m_btnDeleteSelected;
        QLabel *m_statsLabel;

        
        QScrollArea *m_scrollArea;
        CollectionGrid *m_playlistGrid;
        QWidget *m_emptyStateContainer;
        QLabel *m_emptyLabel;
        LoadingOverlay *m_loadingOverlay = nullptr;

        bool m_loaded = false;
        bool m_isLoading = false;
        QList<PlaylistEntry> m_playlists;
        QSet<QString> m_selectedPlaylistIds;
        quint64 m_cardImageLoadGeneration = 0;

        std::optional<QCoro::Task<void>> m_pendingTask;
};

#endif 
