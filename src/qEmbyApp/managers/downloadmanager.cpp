#include "downloadmanager.h"

#include "../components/moderntoast.h"
#include "../utils/mediasourcepreferenceutils.h"
#include "../../qEmbyCore/config/config_keys.h"
#include "../../qEmbyCore/config/configstore.h"
#include <qembycore.h>
#include <services/media/mediaservice.h>
#include <services/manager/servermanager.h>

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>
#include <QSslError>

#include <algorithm>
#include <utility>

namespace {

constexpr int kMaxHistoryRecords = 200;

QString fallbackDownloadDirectory()
{
    QString directory =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (directory.trimmed().isEmpty()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    }
    return QDir(directory).absolutePath();
}

QString sanitizeFileName(QString fileName)
{
    fileName = fileName.trimmed();
    if (fileName.isEmpty()) {
        return QStringLiteral("download.bin");
    }

    static const QRegularExpression invalidChars(
        QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    fileName.replace(invalidChars, QStringLiteral("_"));
    fileName.replace(QRegularExpression(QStringLiteral(R"(\s+)")),
                     QStringLiteral(" "));
    fileName = fileName.trimmed();

    while (fileName.endsWith(QLatin1Char('.')) ||
           fileName.endsWith(QLatin1Char(' '))) {
        fileName.chop(1);
    }

    return fileName.isEmpty() ? QStringLiteral("download.bin") : fileName;
}

QString ensureFileExtension(QString baseName, QString extension)
{
    baseName = sanitizeFileName(std::move(baseName));
    extension = extension.trimmed();
    if (extension.isEmpty()) {
        return baseName;
    }

    if (!extension.startsWith(QLatin1Char('.'))) {
        extension.prepend(QLatin1Char('.'));
    }

    if (baseName.endsWith(extension, Qt::CaseInsensitive)) {
        return baseName;
    }

    return baseName + extension;
}

QString uniqueFilePath(const QString& directory, const QString& fileName)
{
    const QFileInfo originalInfo(QDir(directory).filePath(fileName));
    if (!originalInfo.exists()) {
        return originalInfo.absoluteFilePath();
    }

    const QString baseName = originalInfo.completeBaseName();
    const QString suffix = originalInfo.completeSuffix();
    for (int index = 1; index < 1000; ++index) {
        QString candidate = QStringLiteral("%1 (%2)").arg(baseName).arg(index);
        if (!suffix.isEmpty()) {
            candidate += QStringLiteral(".") + suffix;
        }

        const QString candidatePath = QDir(directory).filePath(candidate);
        if (!QFileInfo::exists(candidatePath)) {
            return candidatePath;
        }
    }

    return QDir(directory).filePath(
        ensureFileExtension(
            QStringLiteral("%1-%2")
                .arg(baseName,
                     QUuid::createUuid().toString(QUuid::WithoutBraces)),
            suffix));
}

QString buildSuggestedFileName(const MediaItem& item,
                               const MediaSourceInfo* sourceInfo)
{
    QString fileName;
    if (sourceInfo && !sourceInfo->path.trimmed().isEmpty()) {
        fileName = QFileInfo(sourceInfo->path).fileName().trimmed();
    }
    if (fileName.isEmpty() && !item.path.trimmed().isEmpty()) {
        fileName = QFileInfo(item.path).fileName().trimmed();
    }
    if (!fileName.isEmpty()) {
        return sanitizeFileName(fileName);
    }

    QString baseName = item.name.trimmed();
    if (baseName.isEmpty()) {
        baseName = DownloadManager::tr("Downloaded Media");
    }

    QString extension;
    if (sourceInfo && !sourceInfo->container.trimmed().isEmpty()) {
        extension = sourceInfo->container.trimmed().toLower();
    } else if (!item.container.trimmed().isEmpty()) {
        extension = item.container.trimmed().toLower();
    }

    return ensureFileExtension(baseName, extension);
}

int resolvePreferredMediaSourceIndex(const QList<MediaSourceInfo>& mediaSources)
{
    return MediaSourcePreferenceUtils::resolvePreferredMediaSourceIndex(
        mediaSources,
        ConfigStore::instance()
            ->get<QString>(ConfigKeys::PlayerPreferredVersion)
            .trimmed());
}

bool recordMatchesProfile(const DownloadManager::DownloadRecord& record,
                          const ServerProfile& profile)
{
    const QString profileUrl = profile.url.trimmed();
    const QString recordUrl = record.serverUrl.trimmed();
    if (profileUrl.isEmpty() || recordUrl.isEmpty() ||
        profileUrl != recordUrl) {
        return false;
    }

    const QString profileUserId = profile.userId.trimmed();
    const QString recordUserId = record.serverUserId.trimmed();
    if (!profileUserId.isEmpty() && !recordUserId.isEmpty()) {
        return profileUserId == recordUserId;
    }

    return true;
}

QList<DownloadManager::DownloadRecord> filterRecordsForProfile(
    QList<DownloadManager::DownloadRecord> records, const ServerProfile& profile)
{
    if (!profile.isValid()) {
        return {};
    }

    records.erase(std::remove_if(records.begin(), records.end(),
                                 [&profile](const DownloadManager::DownloadRecord& record) {
                                     return !recordMatchesProfile(record, profile);
                                 }),
                  records.end());
    return records;
}

QJsonObject recordToJson(const DownloadManager::DownloadRecord& record)
{
    QJsonObject json;
    json[QStringLiteral("id")] = record.id;
    json[QStringLiteral("itemId")] = record.itemId;
    json[QStringLiteral("itemName")] = record.itemName;
    json[QStringLiteral("mediaSourceId")] = record.mediaSourceId;
    json[QStringLiteral("fileName")] = record.fileName;
    json[QStringLiteral("filePath")] = record.filePath;
    json[QStringLiteral("serverName")] = record.serverName;
    json[QStringLiteral("serverUrl")] = record.serverUrl;
    json[QStringLiteral("serverUserId")] = record.serverUserId;
    json[QStringLiteral("statusText")] = record.statusText;
    json[QStringLiteral("errorText")] = record.errorText;
    json[QStringLiteral("bytesReceived")] = QString::number(record.bytesReceived);
    json[QStringLiteral("bytesTotal")] = QString::number(record.bytesTotal);
    json[QStringLiteral("createdAtMs")] = QString::number(record.createdAtMs);
    json[QStringLiteral("finishedAtMs")] = QString::number(record.finishedAtMs);
    json[QStringLiteral("isCompleted")] = record.isCompleted;
    json[QStringLiteral("isPaused")] = record.isPaused;
    return json;
}

DownloadManager::DownloadRecord recordFromJson(const QJsonObject& json)
{
    DownloadManager::DownloadRecord record;
    record.id = json.value(QStringLiteral("id")).toString();
    record.itemId = json.value(QStringLiteral("itemId")).toString();
    record.itemName = json.value(QStringLiteral("itemName")).toString();
    record.mediaSourceId = json.value(QStringLiteral("mediaSourceId")).toString();
    record.fileName = json.value(QStringLiteral("fileName")).toString();
    record.filePath = json.value(QStringLiteral("filePath")).toString();
    record.serverName = json.value(QStringLiteral("serverName")).toString();
    record.serverUrl = json.value(QStringLiteral("serverUrl")).toString();
    record.serverUserId = json.value(QStringLiteral("serverUserId")).toString();
    record.statusText = json.value(QStringLiteral("statusText")).toString();
    record.errorText = json.value(QStringLiteral("errorText")).toString();
    record.bytesReceived =
        json.value(QStringLiteral("bytesReceived")).toString().toLongLong();
    record.bytesTotal =
        json.value(QStringLiteral("bytesTotal")).toString().toLongLong();
    record.createdAtMs =
        json.value(QStringLiteral("createdAtMs")).toString().toLongLong();
    record.finishedAtMs =
        json.value(QStringLiteral("finishedAtMs")).toString().toLongLong();
    record.isCompleted = json.value(QStringLiteral("isCompleted")).toBool();
    record.isPaused = json.value(QStringLiteral("isPaused")).toBool();
    record.isActive = false;
    return record;
}

bool sortByNewest(const DownloadManager::DownloadRecord& left,
                  const DownloadManager::DownloadRecord& right)
{
    const qint64 leftKey =
        left.finishedAtMs > 0 ? left.finishedAtMs : left.createdAtMs;
    const qint64 rightKey =
        right.finishedAtMs > 0 ? right.finishedAtMs : right.createdAtMs;
    if (leftKey != rightKey) {
        return leftKey > rightKey;
    }

    return left.itemName.localeAwareCompare(right.itemName) < 0;
}

} // namespace

struct DownloadManager::ActiveDownload {
    DownloadRecord record;
    QPointer<QNetworkReply> reply;
    QFile* file = nullptr;
    bool cancelRequested = false;
    bool pauseRequested = false;
    bool deleteFileOnCancel = true;
    bool keepHistoryOnCancel = false;
    qint64 resumeOffset = 0;
    bool resumeChecked = false;

    ~ActiveDownload()
    {
        if (file && file->isOpen()) {
            file->close();
        }
        delete file;
    }
};

DownloadManager* DownloadManager::instance()
{
    static DownloadManager* s_instance = new DownloadManager(qApp);
    return s_instance;
}

DownloadManager::DownloadManager(QObject* parent) : QObject(parent)
{
    m_networkManager = new QNetworkAccessManager(this);
}

DownloadManager::~DownloadManager()
{
    for (ActiveDownload* activeDownload : std::as_const(m_activeDownloads)) {
        if (!activeDownload) {
            continue;
        }

        if (activeDownload->reply) {
            activeDownload->reply->disconnect(this);
            activeDownload->reply->abort();
            activeDownload->reply->deleteLater();
            activeDownload->reply = nullptr;
        }
    }
    qDeleteAll(m_activeDownloads);
    m_activeDownloads.clear();
}

void DownloadManager::init(QEmbyCore* core) { m_core = core; }

bool DownloadManager::hasDownloadPermission() const
{
    if (!m_core || !m_core->serverManager()) {
        return false;
    }

    const ServerProfile profile = m_core->serverManager()->activeProfile();
    return profile.isValid() && profile.canDownloadMedia;
}

bool DownloadManager::canDownload(const MediaItem& item) const
{
    return hasDownloadPermission() && !item.id.trimmed().isEmpty() &&
           item.canDownload;
}

QString DownloadManager::downloadDirectory() const
{
    QString directory = ConfigStore::instance()->get<QString>(
        ConfigKeys::DownloadDirectory, fallbackDownloadDirectory());
    directory = directory.trimmed();
    if (directory.isEmpty()) {
        directory = fallbackDownloadDirectory();
    }
    return QDir(directory).absolutePath();
}

void DownloadManager::setDownloadDirectory(QString directory)
{
    directory = directory.trimmed();
    if (directory.isEmpty()) {
        directory = fallbackDownloadDirectory();
    }

    const QString normalizedDirectory = QDir(directory).absolutePath();
    if (downloadDirectory() == normalizedDirectory) {
        return;
    }

    qDebug() << "[DownloadManager] Download directory changed"
             << "| directory:" << normalizedDirectory;
    ConfigStore::instance()->set(ConfigKeys::DownloadDirectory,
                                 normalizedDirectory);
    Q_EMIT downloadDirectoryChanged(normalizedDirectory);
}

QList<DownloadManager::DownloadRecord> DownloadManager::activeDownloads() const
{
    QList<DownloadRecord> records;
    for (auto it = m_activeDownloads.cbegin(); it != m_activeDownloads.cend();
         ++it) {
        records.append(it.value()->record);
    }
    std::sort(records.begin(), records.end(), sortByNewest);
    return records;
}

QList<DownloadManager::DownloadRecord>
DownloadManager::activeDownloadsForCurrentServer() const
{
    if (!m_core || !m_core->serverManager()) {
        return {};
    }

    return filterRecordsForProfile(activeDownloads(),
                                   m_core->serverManager()->activeProfile());
}

QList<DownloadManager::DownloadRecord> DownloadManager::historyDownloads() const
{
    ensureHistoryLoaded();
    QList<DownloadRecord> records = m_historyDownloads;
    std::sort(records.begin(), records.end(), sortByNewest);
    return records;
}

QList<DownloadManager::DownloadRecord>
DownloadManager::historyDownloadsForCurrentServer() const
{
    if (!m_core || !m_core->serverManager()) {
        return {};
    }

    return filterRecordsForProfile(historyDownloads(),
                                   m_core->serverManager()->activeProfile());
}

std::optional<DownloadManager::DownloadRecord> DownloadManager::recordById(
    const QString& recordId) const
{
    const QString trimmedId = recordId.trimmed();
    if (trimmedId.isEmpty()) {
        return std::nullopt;
    }

    const auto activeIt = m_activeDownloads.constFind(trimmedId);
    if (activeIt != m_activeDownloads.cend()) {
        return activeIt.value()->record;
    }

    ensureHistoryLoaded();
    for (const DownloadRecord& record : m_historyDownloads) {
        if (record.id == trimmedId) {
            return record;
        }
    }

    return std::nullopt;
}

void DownloadManager::enqueueDownload(MediaItem item)
{
    if (!m_core || !m_core->mediaService()) {
        return;
    }

    QCoro::connect(startDownload(std::move(item)), this, []() {});
}

bool DownloadManager::pauseDownload(const QString& recordId)
{
    const QString trimmedId = recordId.trimmed();
    if (trimmedId.isEmpty()) {
        return false;
    }

    const auto it = m_activeDownloads.find(trimmedId);
    if (it == m_activeDownloads.end() || !it.value()) {
        return false;
    }

    ActiveDownload* activeDownload = it.value();
    activeDownload->pauseRequested = true;
    activeDownload->cancelRequested = false;
    activeDownload->record.statusText = tr("Pausing");

    qDebug() << "[DownloadManager] Pausing active download"
             << "| recordId:" << trimmedId
             << "| itemId:" << activeDownload->record.itemId
             << "| itemName:" << activeDownload->record.itemName
             << "| filePath:" << activeDownload->record.filePath;

    if (activeDownload->reply) {
        activeDownload->reply->abort();
    } else {
        handleReplyFinished(trimmedId);
    }

    notifyDownloadsChanged();
    return true;
}

bool DownloadManager::resumeDownload(const QString& recordId)
{
    if (!m_core || !m_core->mediaService() || !m_core->serverManager()) {
        return false;
    }

    ensureHistoryLoaded();
    for (int index = 0; index < m_historyDownloads.size(); ++index) {
        const DownloadRecord record = m_historyDownloads.at(index);
        if (record.id != recordId || record.isCompleted || !record.isPaused) {
            continue;
        }

        const ServerProfile profile = m_core->serverManager()->activeProfile();
        if (!recordMatchesProfile(record, profile)) {
            qWarning() << "[DownloadManager] Resume rejected because active "
                          "server does not match paused download"
                       << "| recordId:" << recordId
                       << "| recordServerUrl:" << record.serverUrl
                       << "| recordServerUserId:" << record.serverUserId
                       << "| activeServerUrl:" << profile.url
                       << "| activeServerUserId:" << profile.userId;
            ModernToast::showMessage(
                tr("Switch back to the original server before resuming this download"),
                2600);
            return false;
        }

        m_historyDownloads.removeAt(index);
        saveHistory();

        MediaItem item;
        item.id = record.itemId;
        item.name = record.itemName;

        qDebug() << "[DownloadManager] Resuming paused download"
                 << "| recordId:" << record.id
                 << "| itemId:" << record.itemId
                 << "| itemName:" << record.itemName
                 << "| filePath:" << record.filePath
                 << "| bytesReceived:" << record.bytesReceived;

        QCoro::connect(startDownload(std::move(item), record), this, []() {});
        notifyDownloadsChanged();
        return true;
    }

    return false;
}

bool DownloadManager::stopDownload(const QString& recordId)
{
    const QString trimmedId = recordId.trimmed();
    if (trimmedId.isEmpty()) {
        return false;
    }

    if (cancelDownload(trimmedId, true, true)) {
        return true;
    }

    ensureHistoryLoaded();
    for (int index = 0; index < m_historyDownloads.size(); ++index) {
        DownloadRecord record = m_historyDownloads.at(index);
        if (record.id != trimmedId || !record.isPaused) {
            continue;
        }

        qDebug() << "[DownloadManager] Stopping paused download"
                 << "| recordId:" << trimmedId
                 << "| itemId:" << record.itemId
                 << "| itemName:" << record.itemName
                 << "| filePath:" << record.filePath;

        if (!record.filePath.trimmed().isEmpty()) {
            const QFileInfo fileInfo(record.filePath);
            if (fileInfo.exists() && fileInfo.isFile() &&
                !QFile::remove(fileInfo.absoluteFilePath())) {
                qWarning() << "[DownloadManager] Failed to remove paused"
                              " download file while stopping"
                           << "| recordId:" << trimmedId
                           << "| filePath:" << fileInfo.absoluteFilePath();
            }
        }

        record.isActive = false;
        record.isCompleted = false;
        record.isPaused = false;
        record.finishedAtMs = QDateTime::currentMSecsSinceEpoch();
        record.statusText = tr("Cancelled");
        record.errorText.clear();

        m_historyDownloads.removeAt(index);
        upsertHistoryRecord(record);
        ModernToast::showMessage(tr("Download cancelled"), 1800);
        notifyDownloadsChanged();
        return true;
    }

    return false;
}

bool DownloadManager::cancelDownload(const QString& recordId, bool deleteFile,
                                     bool keepHistory)
{
    const QString trimmedId = recordId.trimmed();
    if (trimmedId.isEmpty()) {
        return false;
    }

    const auto it = m_activeDownloads.find(trimmedId);
    if (it == m_activeDownloads.end() || !it.value()) {
        return false;
    }

    ActiveDownload* activeDownload = it.value();
    activeDownload->cancelRequested = true;
    activeDownload->pauseRequested = false;
    activeDownload->deleteFileOnCancel = deleteFile;
    activeDownload->keepHistoryOnCancel = keepHistory;
    activeDownload->record.statusText = tr("Cancelling");

    qDebug() << "[DownloadManager] Cancelling active download"
             << "| recordId:" << trimmedId
             << "| itemId:" << activeDownload->record.itemId
             << "| itemName:" << activeDownload->record.itemName
             << "| deleteFile:" << deleteFile
             << "| keepHistory:" << keepHistory;

    if (activeDownload->reply) {
        activeDownload->reply->abort();
    } else {
        handleReplyFinished(trimmedId);
    }

    notifyDownloadsChanged();
    return true;
}

bool DownloadManager::openDownloadedFile(const QString& recordId) const
{
    const auto record = recordById(recordId);
    if (!record || !record->isCompleted || record->filePath.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(record->filePath);
    if (!fileInfo.exists() || !fileInfo.isFile()) {
        return false;
    }

    qDebug() << "[DownloadManager] Opening downloaded file"
             << "| recordId:" << recordId
             << "| filePath:" << fileInfo.absoluteFilePath();
    return QDesktopServices::openUrl(
        QUrl::fromLocalFile(fileInfo.absoluteFilePath()));
}

bool DownloadManager::openContainingFolder(const QString& recordId) const
{
    const auto record = recordById(recordId);
    if (!record || record->filePath.trimmed().isEmpty()) {
        return false;
    }

    const QFileInfo fileInfo(record->filePath);
    QString directory =
        fileInfo.exists() ? (fileInfo.isDir() ? fileInfo.absoluteFilePath()
                                              : fileInfo.absolutePath())
                          : QFileInfo(record->filePath).absolutePath();
    if (directory.trimmed().isEmpty()) {
        return false;
    }

    qDebug() << "[DownloadManager] Opening containing folder"
             << "| recordId:" << recordId
             << "| filePath:" << fileInfo.absoluteFilePath()
             << "| directory:" << directory;

#if defined(Q_OS_WIN)
    if (fileInfo.exists() && fileInfo.isFile()) {
        const bool started = QProcess::startDetached(
            QStringLiteral("explorer.exe"),
            {QStringLiteral("/select,"),
             QDir::toNativeSeparators(fileInfo.absoluteFilePath())});
        if (started) {
            return true;
        }

        qWarning() << "[DownloadManager] Failed to launch Explorer with"
                      " file selection"
                   << "| recordId:" << recordId
                   << "| filePath:" << fileInfo.absoluteFilePath();
    }
#elif defined(Q_OS_MACOS)
    if (fileInfo.exists() && fileInfo.isFile()) {
        const bool started = QProcess::startDetached(
            QStringLiteral("open"),
            {QStringLiteral("-R"), fileInfo.absoluteFilePath()});
        if (started) {
            return true;
        }

        qWarning() << "[DownloadManager] Failed to reveal file in Finder"
                   << "| recordId:" << recordId
                   << "| filePath:" << fileInfo.absoluteFilePath();
    }
#endif

    return QDesktopServices::openUrl(QUrl::fromLocalFile(directory));
}

bool DownloadManager::removeHistoryRecord(const QString& recordId,
                                          bool deleteFile)
{
    ensureHistoryLoaded();

    for (int index = 0; index < m_historyDownloads.size(); ++index) {
        const DownloadRecord record = m_historyDownloads.at(index);
        if (record.id != recordId) {
            continue;
        }

        if (deleteFile && !record.filePath.trimmed().isEmpty()) {
            if (!QFile::remove(record.filePath)) {
                qWarning() << "[DownloadManager] Failed to remove downloaded file"
                           << "| recordId:" << recordId
                           << "| filePath:" << record.filePath;
            }
        }

        qDebug() << "[DownloadManager] Removing download history record"
                 << "| recordId:" << recordId
                 << "| itemId:" << record.itemId
                 << "| itemName:" << record.itemName
                 << "| deleteFile:" << deleteFile;
        m_historyDownloads.removeAt(index);
        saveHistory();
        notifyDownloadsChanged();
        return true;
    }

    return false;
}

void DownloadManager::ensureHistoryLoaded() const
{
    if (m_historyLoaded) {
        return;
    }

    m_historyLoaded = true;
    m_historyDownloads.clear();

    const QString rawHistory = ConfigStore::instance()->get<QString>(
        ConfigKeys::DownloadHistoryRecords, QString());
    if (rawHistory.trimmed().isEmpty()) {
        return;
    }

    const QJsonDocument document = QJsonDocument::fromJson(rawHistory.toUtf8());
    if (!document.isArray()) {
        return;
    }

    for (const QJsonValue& value : document.array()) {
        if (!value.isObject()) {
            continue;
        }

        DownloadRecord record = recordFromJson(value.toObject());
        if (!record.id.isEmpty()) {
            m_historyDownloads.append(record);
        }
    }
}

void DownloadManager::saveHistory() const
{
    QJsonArray array;
    for (const DownloadRecord& record : m_historyDownloads) {
        array.append(recordToJson(record));
    }

    ConfigStore::instance()->set(
        ConfigKeys::DownloadHistoryRecords,
        QString::fromUtf8(
            QJsonDocument(array).toJson(QJsonDocument::Compact)));
}

void DownloadManager::upsertHistoryRecord(DownloadRecord record)
{
    ensureHistoryLoaded();

    for (int index = 0; index < m_historyDownloads.size(); ++index) {
        if (m_historyDownloads.at(index).id == record.id) {
            m_historyDownloads.removeAt(index);
            break;
        }
    }

    record.isActive = false;
    m_historyDownloads.prepend(std::move(record));
    if (m_historyDownloads.size() > kMaxHistoryRecords) {
        m_historyDownloads.erase(
            m_historyDownloads.begin() + kMaxHistoryRecords,
            m_historyDownloads.end());
    }
    saveHistory();
}

void DownloadManager::notifyDownloadsChanged() { Q_EMIT downloadsChanged(); }

void DownloadManager::handleReplyReadyRead(const QString& downloadId)
{
    const auto it = m_activeDownloads.find(downloadId);
    if (it == m_activeDownloads.end()) {
        return;
    }

    ActiveDownload* activeDownload = it.value();
    if (!activeDownload || !activeDownload->reply || !activeDownload->file) {
        return;
    }

    if (activeDownload->resumeOffset > 0 && !activeDownload->resumeChecked) {
        activeDownload->resumeChecked = true;
        const int statusCode =
            activeDownload->reply
                ->attribute(QNetworkRequest::HttpStatusCodeAttribute)
                .toInt();
        if (statusCode != 206) {
            qWarning() << "[DownloadManager] Range resume was not honored, "
                          "falling back to restart"
                       << "| recordId:" << downloadId
                       << "| httpStatus:" << statusCode
                       << "| filePath:" << activeDownload->record.filePath;

            activeDownload->file->close();
            if (!activeDownload->file->open(QIODevice::WriteOnly |
                                            QIODevice::Truncate)) {
                activeDownload->record.errorText =
                    tr("Failed to open download file");
                activeDownload->record.statusText =
                    activeDownload->record.errorText;
                activeDownload->reply->abort();
                return;
            }

            activeDownload->resumeOffset = 0;
            activeDownload->record.bytesReceived = 0;
            activeDownload->record.bytesTotal = -1;
        }
    }

    const QByteArray chunk = activeDownload->reply->readAll();
    if (chunk.isEmpty()) {
        return;
    }

    const qint64 written = activeDownload->file->write(chunk);
    if (written != chunk.size()) {
        activeDownload->record.errorText = tr("Failed to write download file");
        activeDownload->record.statusText = activeDownload->record.errorText;
        activeDownload->reply->abort();
    }
}

void DownloadManager::handleReplyProgress(const QString& downloadId,
                                          qint64 received, qint64 total)
{
    const auto it = m_activeDownloads.find(downloadId);
    if (it == m_activeDownloads.end()) {
        return;
    }

    ActiveDownload* activeDownload = it.value();
    if (!activeDownload) {
        return;
    }

    const qint64 resumeOffset = activeDownload->resumeOffset;
    activeDownload->record.bytesReceived = resumeOffset + received;
    if (total > 0) {
        activeDownload->record.bytesTotal = resumeOffset + total;
    } else if (resumeOffset <= 0) {
        activeDownload->record.bytesTotal = total;
    }
    activeDownload->record.statusText =
        activeDownload->pauseRequested
            ? tr("Pausing")
            : (activeDownload->cancelRequested ? tr("Cancelling")
                                              : tr("Downloading"));
    notifyDownloadsChanged();
}

void DownloadManager::handleReplyFinished(const QString& downloadId)
{
    const auto it = m_activeDownloads.find(downloadId);
    if (it == m_activeDownloads.end()) {
        return;
    }

    ActiveDownload* activeDownload = it.value();
    if (!activeDownload) {
        return;
    }

    DownloadRecord record = activeDownload->record;
    if (activeDownload->reply && activeDownload->file) {
        const QByteArray remaining = activeDownload->reply->readAll();
        if (!remaining.isEmpty()) {
            const qint64 written = activeDownload->file->write(remaining);
            if (written != remaining.size() && record.errorText.isEmpty()) {
                record.errorText = tr("Failed to write download file");
            }
        }
        activeDownload->file->flush();
        activeDownload->file->close();
    }

    if (activeDownload->pauseRequested) {
        if (activeDownload->reply) {
            activeDownload->reply->deleteLater();
        }

        record.isActive = false;
        record.isCompleted = false;
        record.isPaused = true;
        record.finishedAtMs = 0;
        record.errorText.clear();
        record.statusText = tr("Paused");
        if (!record.filePath.trimmed().isEmpty()) {
            const QFileInfo fileInfo(record.filePath);
            if (fileInfo.exists() && fileInfo.isFile()) {
                record.bytesReceived =
                    qMax(record.bytesReceived, fileInfo.size());
            }
        }

        m_activeDownloads.erase(it);
        delete activeDownload;

        upsertHistoryRecord(record);
        ModernToast::showMessage(tr("Download paused"), 1800);
        notifyDownloadsChanged();
        return;
    }

    if (activeDownload->cancelRequested) {
        const bool keepHistoryOnCancel = activeDownload->keepHistoryOnCancel;

        if (activeDownload->deleteFileOnCancel &&
            !record.filePath.trimmed().isEmpty() &&
            !QFile::remove(record.filePath)) {
            qWarning() << "[DownloadManager] Failed to remove cancelled"
                          " download file"
                       << "| recordId:" << record.id
                       << "| filePath:" << record.filePath;
        }

        if (activeDownload->reply) {
            activeDownload->reply->deleteLater();
        }

        m_activeDownloads.erase(it);
        delete activeDownload;

        if (keepHistoryOnCancel) {
            record.isActive = false;
            record.isCompleted = false;
            record.isPaused = false;
            record.finishedAtMs = QDateTime::currentMSecsSinceEpoch();
            record.statusText = tr("Cancelled");
            record.errorText.clear();

            upsertHistoryRecord(record);
            ModernToast::showMessage(tr("Download cancelled"), 1800);
        } else {
            ModernToast::showMessage(tr("Download removed"), 1800);
        }

        notifyDownloadsChanged();
        return;
    }

    const bool success =
        activeDownload->reply &&
        activeDownload->reply->error() == QNetworkReply::NoError &&
        record.errorText.isEmpty();

    record.isActive = false;
    record.isPaused = false;
    record.finishedAtMs = QDateTime::currentMSecsSinceEpoch();

    if (success) {
        if (record.bytesReceived <= 0) {
            record.bytesReceived = QFileInfo(record.filePath).size();
        }
        if (record.bytesTotal <= 0) {
            record.bytesTotal = record.bytesReceived;
        }
        record.isCompleted = true;
        record.statusText = tr("Completed");
        qDebug() << "[DownloadManager] Download completed"
                 << "| itemId:" << record.itemId
                 << "| itemName:" << record.itemName
                 << "| filePath:" << record.filePath
                 << "| bytes:" << record.bytesReceived;
        ModernToast::showMessage(tr("Download completed"), 2000);
    } else {
        if (record.errorText.isEmpty() && activeDownload->reply) {
            record.errorText = activeDownload->reply->errorString();
        }
        if (record.errorText.isEmpty()) {
            record.errorText = tr("Download failed");
        }
        record.statusText = record.errorText;
        record.isCompleted = false;
        QFile::remove(record.filePath);

        qWarning() << "[DownloadManager] Download failed"
                   << "| itemId:" << record.itemId
                   << "| itemName:" << record.itemName
                   << "| filePath:" << record.filePath
                   << "| error:" << record.errorText;
        ModernToast::showMessage(record.errorText, 3000);
    }

    if (activeDownload->reply) {
        activeDownload->reply->deleteLater();
    }

    m_activeDownloads.erase(it);
    delete activeDownload;

    upsertHistoryRecord(record);
    notifyDownloadsChanged();
}

QCoro::Task<void> DownloadManager::startDownload(
    MediaItem item, std::optional<DownloadRecord> resumeRecord)
{
    if (!m_core || !m_core->mediaService() || !m_core->serverManager()) {
        if (resumeRecord) {
            upsertHistoryRecord(*resumeRecord);
            notifyDownloadsChanged();
        }
        co_return;
    }

    const auto restoreResumeRecord = [this, resumeRecord]() {
        if (!resumeRecord) {
            return;
        }

        upsertHistoryRecord(*resumeRecord);
        notifyDownloadsChanged();
    };

    QPointer<DownloadManager> safeThis(this);
    MediaItem detail;
    try {
        detail = co_await m_core->mediaService()->getItemDetail(item.id);
    } catch (const std::exception& e) {
        if (safeThis) {
            qWarning() << "[DownloadManager] Failed to fetch media detail before"
                          " download"
                       << "| itemId:" << item.id
                       << "| error:" << e.what();
            ModernToast::showMessage(
                tr("Failed to prepare download: %1")
                    .arg(QString::fromUtf8(e.what())),
                3000);
            restoreResumeRecord();
        }
        co_return;
    }

    if (!safeThis) {
        co_return;
    }

    if (!canDownload(detail)) {
        qDebug() << "[DownloadManager] Download rejected"
                 << "| itemId:" << detail.id
                 << "| itemName:" << detail.name
                 << "| canDownloadItem:" << detail.canDownload
                 << "| canDownloadUser:"
                 << m_core->serverManager()->activeProfile().canDownloadMedia;
        ModernToast::showMessage(
            tr("This media is not available for download"), 2500);
        restoreResumeRecord();
        co_return;
    }

    const ServerProfile profile = m_core->serverManager()->activeProfile();
    const MediaSourceInfo* selectedSource = nullptr;
    QString mediaSourceId;
    if (resumeRecord) {
        mediaSourceId = resumeRecord->mediaSourceId.trimmed();
        for (const auto& sourceInfo : detail.mediaSources) {
            if (sourceInfo.id.trimmed() == mediaSourceId) {
                selectedSource = &sourceInfo;
                break;
            }
        }

        if (mediaSourceId.isEmpty() || !selectedSource) {
            qWarning() << "[DownloadManager] Failed to resume download because "
                          "the original media source is no longer available"
                       << "| recordId:" << resumeRecord->id
                       << "| itemId:" << detail.id
                       << "| mediaSourceId:" << mediaSourceId;
            ModernToast::showMessage(
                tr("Failed to prepare download: %1")
                    .arg(tr("The original media source is no longer available")),
                3000);
            restoreResumeRecord();
            co_return;
        }
    } else {
        const int sourceIndex =
            resolvePreferredMediaSourceIndex(detail.mediaSources);
        selectedSource =
            (sourceIndex >= 0 && sourceIndex < detail.mediaSources.size())
                ? &detail.mediaSources.at(sourceIndex)
                : nullptr;
        mediaSourceId =
            selectedSource && !selectedSource->id.trimmed().isEmpty()
                ? selectedSource->id.trimmed()
                : detail.id.trimmed();
    }

    const QString downloadUrl =
        m_core->mediaService()->getStreamUrl(detail.id, mediaSourceId);

    if (downloadUrl.trimmed().isEmpty()) {
        qWarning() << "[DownloadManager] Failed to resolve download url"
                   << "| itemId:" << detail.id
                   << "| mediaSourceId:" << mediaSourceId;
        ModernToast::showMessage(tr("Failed to resolve download address"),
                                 3000);
        restoreResumeRecord();
        co_return;
    }

    QString targetDirectory;
    QString targetPath;
    qint64 resumeOffset = 0;
    if (resumeRecord) {
        targetPath = resumeRecord->filePath.trimmed();
        if (targetPath.isEmpty()) {
            const QString fallbackName =
                resumeRecord->fileName.trimmed().isEmpty()
                    ? buildSuggestedFileName(detail, selectedSource)
                    : resumeRecord->fileName;
            targetPath = QDir(downloadDirectory()).filePath(fallbackName);
        }

        QFileInfo resumeFileInfo(targetPath);
        targetDirectory = resumeFileInfo.absolutePath();
        if (resumeFileInfo.exists() && resumeFileInfo.isFile()) {
            resumeOffset = resumeFileInfo.size();
        }
    } else {
        targetDirectory = downloadDirectory();
    }

    if (!QDir().mkpath(targetDirectory)) {
        qWarning() << "[DownloadManager] Failed to create download directory"
                   << "| directory:" << targetDirectory;
        ModernToast::showMessage(tr("Failed to create download directory"),
                                 3000);
        restoreResumeRecord();
        co_return;
    }

    if (targetPath.trimmed().isEmpty()) {
        const QString fileName = buildSuggestedFileName(detail, selectedSource);
        targetPath = uniqueFilePath(targetDirectory, fileName);
    }

    auto* targetFile = new QFile(targetPath);
    QIODevice::OpenMode openMode = QIODevice::WriteOnly;
    if (resumeRecord) {
        openMode |=
            (resumeOffset > 0 ? QIODevice::Append : QIODevice::Truncate);
    }

    if (!targetFile->open(openMode)) {
        qWarning() << "[DownloadManager] Failed to open target download file"
                   << "| filePath:" << targetPath
                   << "| error:" << targetFile->errorString();
        delete targetFile;
        ModernToast::showMessage(tr("Failed to open download file"), 3000);
        restoreResumeRecord();
        co_return;
    }

    QNetworkRequest request{QUrl(downloadUrl)};
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    if (resumeOffset > 0) {
        request.setRawHeader(
            QByteArrayLiteral("Range"),
            QByteArrayLiteral("bytes=") + QByteArray::number(resumeOffset) +
                QByteArrayLiteral("-"));
    }

    QNetworkReply* reply = m_networkManager->get(request);
    if (profile.ignoreSslVerification) {
        connect(reply, &QNetworkReply::sslErrors, reply,
                [reply](const QList<QSslError>&) { reply->ignoreSslErrors(); });
    }

    auto* activeDownload = new ActiveDownload();
    activeDownload->record =
        resumeRecord ? *resumeRecord : DownloadRecord{};
    activeDownload->record.id =
        resumeRecord ? resumeRecord->id
                     : QUuid::createUuid().toString(QUuid::WithoutBraces);
    activeDownload->record.itemId = detail.id;
    activeDownload->record.itemName = detail.name;
    activeDownload->record.mediaSourceId = mediaSourceId;
    activeDownload->record.fileName = QFileInfo(targetPath).fileName();
    activeDownload->record.filePath = targetPath;
    activeDownload->record.serverName = profile.name;
    activeDownload->record.serverUrl = profile.url;
    activeDownload->record.serverUserId = profile.userId;
    activeDownload->record.statusText = tr("Downloading");
    activeDownload->record.errorText.clear();
    activeDownload->record.createdAtMs =
        resumeRecord ? resumeRecord->createdAtMs
                     : QDateTime::currentMSecsSinceEpoch();
    activeDownload->record.finishedAtMs = 0;
    activeDownload->record.isActive = true;
    activeDownload->record.isCompleted = false;
    activeDownload->record.isPaused = false;
    activeDownload->record.bytesReceived = resumeOffset;
    if (!resumeRecord) {
        activeDownload->record.bytesTotal = -1;
    } else if (resumeRecord->bytesTotal > 0) {
        activeDownload->record.bytesTotal =
            qMax(resumeRecord->bytesTotal, resumeOffset);
    }
    activeDownload->reply = reply;
    activeDownload->file = targetFile;
    activeDownload->resumeOffset = resumeOffset;
    activeDownload->resumeChecked = (resumeOffset <= 0);

    const QString downloadId = activeDownload->record.id;
    m_activeDownloads.insert(downloadId, activeDownload);

    connect(reply, &QNetworkReply::readyRead, this,
            [this, downloadId]() { handleReplyReadyRead(downloadId); });
    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, downloadId](qint64 received, qint64 total) {
                handleReplyProgress(downloadId, received, total);
            });
    connect(reply, &QNetworkReply::finished, this,
            [this, downloadId]() { handleReplyFinished(downloadId); });

    qDebug() << "[DownloadManager] Download started"
             << "| itemId:" << detail.id
             << "| itemName:" << detail.name
             << "| mediaSourceId:" << mediaSourceId
             << "| resumeOffset:" << resumeOffset
             << "| filePath:" << targetPath;
    ModernToast::showMessage(
        resumeRecord ? tr("Download resumed") : tr("Download started"), 2000);
    notifyDownloadsChanged();
}
