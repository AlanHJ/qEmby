#ifndef ADMINSERVICE_H
#define ADMINSERVICE_H

#include "../../qEmbyCore_global.h"
#include "../../models/admin/adminmodels.h"
#include <QByteArray>
#include <QJsonArray>
#include <QObject>
#include <QList>
#include <QJsonObject>
#include <qcorotask.h>

class ServerManager;

class QEMBYCORE_EXPORT AdminService : public QObject
{
    Q_OBJECT
public:
    explicit AdminService(ServerManager* serverManager, QObject *parent = nullptr);

    
    
    
    QCoro::Task<SystemInfo> getSystemInfo();
    QCoro::Task<void> restartServer();
    QCoro::Task<void> shutdownServer();

    
    
    
    QCoro::Task<QList<SessionInfo>> getActiveSessions();

    
    
    
    QCoro::Task<QList<UserInfo>> getUsers();
    QCoro::Task<QJsonObject> getUserById(const QString& userId);
    QCoro::Task<QJsonObject> createUser(const QString& name,
                                        const QString& copyFromUserId = {},
                                        const QStringList& userCopyOptions = {});
    QCoro::Task<QList<AdminMediaFolderInfo>> getMediaFolders(
        bool includeHidden = false);
    QCoro::Task<QList<SelectableMediaFolderInfo>> getSelectableMediaFolders(
        bool includeHidden = false);
    QCoro::Task<QList<AdminChannelInfo>> getChannels(
        bool supportsMediaDeletion = false);
    QCoro::Task<QList<AdminDeviceInfo>> getDevices();
    QCoro::Task<QList<AdminAuthProviderInfo>> getAuthProviders();
    QCoro::Task<QList<AdminFeatureInfo>> getUserFeatures();
    QCoro::Task<QList<AdminParentalRatingInfo>> getParentalRatings();
    QCoro::Task<void> updateUser(QString userId, QJsonObject userData);
    QCoro::Task<void> setUserPolicy(const QString& userId, const QJsonObject& policy);
    QCoro::Task<void> updateUserConfiguration(const QString& userId,
                                              const QJsonObject& configuration);
    QCoro::Task<void> deleteUser(const QString& userId);
    QCoro::Task<void> updateUserPassword(const QString& userId,
                                          const QString& currentPassword,
                                          const QString& newPassword);
    QCoro::Task<void> updateEasyPassword(const QString& userId,
                                         const QString& newPassword,
                                         bool resetPassword = false);
    QCoro::Task<void> updateConnectLink(const QString& userId,
                                        const QString& connectUserName);
    QCoro::Task<void> removeConnectLink(const QString& userId);

    
    
    

    
    struct LibraryProviders {
        QStringList metadataFetchers;
        QStringList imageFetchers;
        QStringList subtitleDownloaders;
        QStringList lyricsFetchers;
    };

    struct LocalizationOptions {
        QList<LocalizationCulture> cultures;
        QList<LocalizationCountry> countries;
    };

    
    
    QCoro::Task<LibraryProviders> getAvailableProviders();
    QCoro::Task<LocalizationOptions> getLocalizationOptions();
    QCoro::Task<QList<LocalizationCulture>> getLocalizationCultures();
    QCoro::Task<QList<LocalizationCountry>> getLocalizationCountries();

    QCoro::Task<QList<VirtualFolder>> getVirtualFolders();
    QCoro::Task<void> addVirtualFolder(const QString& name, const QString& collectionType,
                                        const QStringList& paths,
                                        const QJsonObject& libraryOptions = {});
    QCoro::Task<void> removeVirtualFolder(const VirtualFolder& folder);
    QCoro::Task<void> renameVirtualFolder(const QString& oldName, const QString& newName);
    QCoro::Task<void> addMediaPath(const QString& libraryId,
                                   const QString& libraryName,
                                   const QString& path);
    QCoro::Task<void> removeMediaPath(const QString& libraryId,
                                      const QString& libraryName,
                                      const QString& path);
    QCoro::Task<void> updateVirtualFolderOptions(const QString& folderId,
                                                 const QJsonObject& libraryOptions,
                                                 const QStringList& paths = {});
    QCoro::Task<void> refreshLibrary();

    
    
    
    struct DriveInfo {
        QString path;   
        QString name;   
    };
    QCoro::Task<QList<DriveInfo>> getServerDrives();
    QCoro::Task<QStringList> getDirectoryContents(const QString& path);
    QCoro::Task<QStringList> getLibraryOrder();
    QCoro::Task<void> updateLibraryOrder(QStringList orderedIds);

    
    
    
    QCoro::Task<QJsonArray> getUserPlaylists();
    QCoro::Task<void> updatePlaylistOrderByDateCreated(QStringList orderedIds);
    QCoro::Task<void> updatePlaylistDateCreated(QString itemId, QString dateCreatedText);
    QCoro::Task<QString> createPlaylist(const QString& name, const QString& mediaType = "Video");
    QCoro::Task<QJsonObject> getPlaylistItems(const QString& playlistId);
    QCoro::Task<void> addToPlaylist(const QString& playlistId, const QStringList& itemIds);
    QCoro::Task<void> removeFromPlaylist(const QString& playlistId, const QStringList& entryIds);

    
    
    
    QCoro::Task<QJsonArray> getUserCollections();
    QCoro::Task<QString> createCollection(const QString& name, const QStringList& itemIds = {});
    QCoro::Task<void> addToCollection(const QString& collectionId, const QStringList& itemIds);
    QCoro::Task<void> removeFromCollection(const QString& collectionId, const QStringList& itemIds);

    
    
    
    QCoro::Task<QList<RemoteSearchResult>> searchRemoteMetadata(
        QString remoteSearchType, QString itemId, QJsonObject searchInfo,
        QString searchProviderName = QString(),
        bool includeDisabledProviders = false);
    QCoro::Task<QJsonObject> getItemMetadata(QString itemId);
    QCoro::Task<QList<ItemImageInfo>> getItemImages(QString itemId);
    QCoro::Task<void> applyRemoteSearchResult(QString itemId,
                                              RemoteSearchResult result);
    QCoro::Task<void> updateItemMetadata(const QString& itemId, const QJsonObject& itemData);
    QCoro::Task<void> refreshItemMetadata(const QString& itemId,
                                          bool replaceAllMetadata = false,
                                          bool replaceAllImages = false);
    QCoro::Task<void> uploadItemImage(QString itemId, QString imageType,
                                      QByteArray imageData, QString mimeType,
                                      int imageIndex = -1);
    QCoro::Task<void> deleteItemImage(QString itemId, QString imageType,
                                      int imageIndex = -1);

    
    
    
    QCoro::Task<void> deleteItem(const QString& itemId);
    QCoro::Task<void> renameItem(const QString& itemId, const QString& newName);

    
    
    
    QCoro::Task<QJsonObject> getEncodingConfiguration();
    QCoro::Task<void> updateEncodingConfiguration(QJsonObject config);
    QCoro::Task<QJsonArray> getVideoCodecInformation();
    QCoro::Task<QJsonArray> getDefaultCodecConfigurations();
    QCoro::Task<QJsonObject> getCodecParameters(QString codecId,
                                                QString parameterContext = QStringLiteral("Playback"));
    QCoro::Task<void> updateCodecParameters(QString codecId,
                                            QJsonObject parameters,
                                            QString parameterContext = QStringLiteral("Playback"));

    
    
    
    QCoro::Task<QList<ScheduledTaskInfo>> getScheduledTasks();
    QCoro::Task<QJsonObject> getScheduledTaskById(const QString& taskId);
    QCoro::Task<void> runScheduledTask(const QString& taskId);
    QCoro::Task<void> stopScheduledTask(const QString& taskId);
    QCoro::Task<void> updateTaskTriggers(const QString& taskId, const QJsonArray& triggers);

    
    
    
    QCoro::Task<QList<LogFileInfo>> getLogFiles();
    QCoro::Task<QString> getLogFileContent(const QString& logName);

    
    
    
    QCoro::Task<QList<ActivityLogEntry>> getActivityLog(int limit = 30);

private:
    ServerManager* m_serverManager;

    void ensureValidProfile() const;
};

#endif 
