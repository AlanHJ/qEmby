#ifndef PAGELOGS_H
#define PAGELOGS_H

#include "managepagebase.h"
#include <models/admin/adminmodels.h>
#include <qcorotask.h>
#include <optional>

class ElidedLabel;
class QLabel;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QShowEvent;

class PageLogs : public ManagePageBase {
    Q_OBJECT
public:
    explicit PageLogs(QEmbyCore* core, QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

private:
    QCoro::Task<void> loadFileList(bool isManual = false);
    QCoro::Task<void> loadLogContent(QString logName);
    QCoro::Task<void> downloadLogFile(QString logName);
    QString rebuildFileList(const QString& preferredLogName);
    void updateSelectionDetails(const QString& logName);
    void setViewerPlaceholder(const QString& text);
    QString currentSelectedLogName() const;

    QPushButton* m_btnRefresh = nullptr;
    QLabel* m_statsLabel = nullptr;

    QListWidget* m_fileList = nullptr;
    QPlainTextEdit* m_logViewer = nullptr;
    QLabel* m_emptyLabel = nullptr;
    QLabel* m_viewerPlaceholder = nullptr;
    ElidedLabel* m_currentFileNameLabel = nullptr;
    QLabel* m_currentFileMetaLabel = nullptr;

    QList<LogFileInfo> m_logFiles;
    QString m_currentLogName;

    bool m_loaded = false;
    bool m_isLoading = false;

    std::optional<QCoro::Task<void>> m_pendingTask;
    std::optional<QCoro::Task<void>> m_pendingDownloadTask;
};

#endif 
