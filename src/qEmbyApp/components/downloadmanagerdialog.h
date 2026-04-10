#ifndef DOWNLOADMANAGERDIALOG_H
#define DOWNLOADMANAGERDIALOG_H

#include <QHash>
#include <QMetaObject>
#include <QStringList>

#include "moderndialogbase.h"
#include "../managers/downloadmanager.h"

class QEmbyCore;
class ElidedLabel;
class QFrame;
class QLabel;
class QPushButton;
class QScrollArea;
class QVBoxLayout;
class QWidget;

class DownloadManagerDialog : public ModernDialogBase
{
    Q_OBJECT
public:
    static void showManager(QEmbyCore* core, QWidget* parent = nullptr);

    explicit DownloadManagerDialog(QEmbyCore* core, QWidget* parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void bindServerSignals();
    struct RecordRowWidgets {
        QFrame* row = nullptr;
        QLabel* indexLabel = nullptr;
        ElidedLabel* titleLabel = nullptr;
        QLabel* sizeLabel = nullptr;
        QLabel* startedLabel = nullptr;
        QLabel* finishedLabel = nullptr;
        ElidedLabel* progressLabel = nullptr;
        QWidget* actionsContainer = nullptr;
        QPushButton* primaryButton = nullptr;
        QPushButton* stopButton = nullptr;
        QPushButton* folderButton = nullptr;
        QPushButton* deleteButton = nullptr;
    };

    void applyIcons();
    void refreshUi();
    void rebuildRecordList(const QList<DownloadManager::DownloadRecord>& records);
    RecordRowWidgets createRecordRow();
    void updateRecordRow(const QString& recordId,
                         const DownloadManager::DownloadRecord& record,
                         int index);
    void rebuildRecordLayout(const QStringList& orderedIds);
    void removeRecordRow(const QString& recordId);
    void handleStopRecord(const DownloadManager::DownloadRecord& record);
    void handleDeleteRecord(const DownloadManager::DownloadRecord& record);

    QEmbyCore* m_core = nullptr;

    QLabel* m_summaryLabel = nullptr;
    QWidget* m_recordsHeader = nullptr;
    QScrollArea* m_recordsScrollArea = nullptr;
    QWidget* m_recordsContainer = nullptr;
    QVBoxLayout* m_recordsLayout = nullptr;
    QWidget* m_emptyState = nullptr;
    QLabel* m_emptyIconLabel = nullptr;
    QLabel* m_emptyTitleLabel = nullptr;
    QLabel* m_emptyHintLabel = nullptr;
    QLabel* m_locationValueLabel = nullptr;
    QPushButton* m_openDirectoryButton = nullptr;
    QPushButton* m_changeDirectoryButton = nullptr;
    QMetaObject::Connection m_activeServerChangedConnection;
    QHash<QString, RecordRowWidgets> m_recordRows;
    QStringList m_rowOrder;
};

#endif 
