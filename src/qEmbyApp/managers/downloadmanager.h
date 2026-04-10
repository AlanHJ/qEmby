#ifndef DOWNLOADMANAGER_H
#define DOWNLOADMANAGER_H

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <optional>
#include <qcorotask.h>

#include <models/media/mediaitem.h>

class QEmbyCore;

class DownloadManager : public QObject
{
    Q_OBJECT
public:
    struct DownloadRecord {
        QString id;
        QString itemId;
        QString itemName;
        QString mediaSourceId;
        QString fileName;
        QString filePath;
        QString serverName;
        QString serverUrl;
        QString serverUserId;
        QString statusText;
        QString errorText;
        qint64 bytesReceived = 0;
        qint64 bytesTotal = -1;
        qint64 createdAtMs = 0;
        qint64 finishedAtMs = 0;
        bool isActive = false;
        bool isCompleted = false;
        bool isPaused = false;
    };

    static DownloadManager* instance();
    ~DownloadManager() override;

    void init(QEmbyCore* core);

    bool hasDownloadPermission() const;
    bool canDownload(const MediaItem& item) const;

    QString downloadDirectory() const;
    void setDownloadDirectory(QString directory);

    QList<DownloadRecord> activeDownloads() const;
    QList<DownloadRecord> activeDownloadsForCurrentServer() const;
    QList<DownloadRecord> historyDownloads() const;
    QList<DownloadRecord> historyDownloadsForCurrentServer() const;
    std::optional<DownloadRecord> recordById(const QString& recordId) const;

    void enqueueDownload(MediaItem item);
    bool pauseDownload(const QString& recordId);
    bool resumeDownload(const QString& recordId);
    bool stopDownload(const QString& recordId);
    bool cancelDownload(const QString& recordId, bool deleteFile = true,
                        bool keepHistory = false);

    bool openDownloadedFile(const QString& recordId) const;
    bool openContainingFolder(const QString& recordId) const;
    bool removeHistoryRecord(const QString& recordId, bool deleteFile);

Q_SIGNALS:
    void downloadsChanged();
    void downloadDirectoryChanged(const QString& directory);

private:
    explicit DownloadManager(QObject* parent = nullptr);

    struct ActiveDownload;

    void ensureHistoryLoaded() const;
    void saveHistory() const;
    void notifyDownloadsChanged();
    void upsertHistoryRecord(DownloadRecord record);

    void handleReplyReadyRead(const QString& downloadId);
    void handleReplyProgress(const QString& downloadId, qint64 received,
                             qint64 total);
    void handleReplyFinished(const QString& downloadId);

    QCoro::Task<void> startDownload(
        MediaItem item,
        std::optional<DownloadRecord> resumeRecord = std::nullopt);

    QEmbyCore* m_core = nullptr;
    class QNetworkAccessManager* m_networkManager = nullptr;
    QHash<QString, ActiveDownload*> m_activeDownloads;
    mutable QList<DownloadRecord> m_historyDownloads;
    mutable bool m_historyLoaded = false;
};

#endif 
