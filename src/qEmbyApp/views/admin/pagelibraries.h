#ifndef PAGELIBRARIES_H
#define PAGELIBRARIES_H

#include "managepagebase.h"
#include "librarycard.h"
#include "librarygrid.h"
#include <models/admin/adminmodels.h>
#include <qcorotask.h>
#include <optional>
#include <QSet>

class EmbyWebSocket;
class LoadingOverlay;

class QLabel;
class QPushButton;
class QProgressBar;
class QResizeEvent;
class QScrollArea;
class QTimer;
class QWidget;




class PageLibraries : public ManagePageBase {
    Q_OBJECT
public:
    explicit PageLibraries(QEmbyCore* core, QWidget* parent = nullptr);
    QScrollArea* scrollArea() const { return m_scrollArea; }

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QCoro::Task<void> loadData(bool isManual = false);
    QCoro::Task<void> loadCardImage(LibraryCard* card, VirtualFolder folder,
                                    quint64 imageLoadGeneration);
    QCoro::Task<void> onCreateClicked();
    QCoro::Task<void> onEditClicked(int row);
    QCoro::Task<void> onDeleteClicked(int row);
    QCoro::Task<void> onDeleteSelectedClicked();
    QCoro::Task<void> onScanClicked(int row);
    QCoro::Task<void> onScanAllClicked();
    QCoro::Task<void> onOrderChanged(QStringList newOrderIds);
    QCoro::Task<void> onCancelScanClicked();
    QCoro::Task<void> pollScanProgress();

    
    void onWsRefreshProgress(const QString& itemId, int progress);
    void connectWebSocket();      
    void disconnectWebSocket();   
    bool hasActiveScan() const;
    void setTotalScanProgress(int progress);
    void resetTotalScanUi();
    void setCardScanProgress(const QString& itemId, int progress);
    void clearCardScanProgress(const QString& itemId);
    void clearAllCardScanProgress();
    void updateTrackedCardProgressFromTask(int progress);
    void finalizeScan(bool reloadLibraries);
    void requestLibraryReload(bool deferIfScanActive = true);
    void updateContentState();
    void updateStats();
    void updateSelectionState();
    void syncCardSelection();
    void onCardSelectionChanged(const QString& itemId, bool selected);
    void toggleSelectAll();
    bool areAllLibrariesSelected() const;
    void setLoadingState(bool loading);
    void updateLoadingOverlayGeometry();

    LibraryGrid* m_grid;
    QPushButton* m_btnRefresh;
    QPushButton* m_btnScanAll;
    QPushButton* m_btnCreate;
    QPushButton* m_btnSelectAll;
    QPushButton* m_btnDeleteSelected;
    QPushButton* m_btnCancelScan;
    QLabel* m_statsLabel;
    QWidget* m_emptyStateContainer;
    QLabel* m_emptyLabel;
    LoadingOverlay* m_loadingOverlay = nullptr;
    QProgressBar* m_scanProgress;
    QLabel* m_scanPercentLabel;
    QTimer* m_scanTimer;
    QScrollArea* m_scrollArea;

    QList<VirtualFolder> m_folders;
    QSet<QString> m_selectedLibraryIds;
    bool m_loaded = false;
    bool m_isLoading = false;

    
    QSet<QString> m_scanningItemIds;  
    bool m_isScanAll = false;          
    QString m_runningTaskId;           

    
    std::optional<QCoro::Task<void>> m_pendingTask;

    
    QList<QMetaObject::Connection> m_wsConnections;
    quint64 m_cardImageLoadGeneration = 0;
    bool m_reloadDeferredUntilScanEnd = false;
};

#endif 
