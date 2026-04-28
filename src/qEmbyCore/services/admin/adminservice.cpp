#include "adminservice.h"
#include "../manager/servermanager.h"
#include "../../api/apiclient.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <stdexcept>
#include <utility>

namespace {

QString buildLibraryOptionDebugSummary(const QJsonObject &options) {
    static const QStringList keys = {
        QStringLiteral("EnableRealtimeMonitor"),
        QStringLiteral("PreferredMetadataLanguage"),
        QStringLiteral("MetadataCountryCode"),
        QStringLiteral("PreferredImageLanguage")
    };

    QJsonObject summary;
    for (const QString &key : keys) {
        if (options.contains(key)) {
            summary.insert(key, options.value(key));
        }
    }

    return QString::fromUtf8(
        QJsonDocument(summary).toJson(QJsonDocument::Compact));
}

void logVirtualFolderSnapshot(const VirtualFolder &folder,
                              const char *prefix) {
    qDebug() << prefix
             << "| name:" << folder.name
             << "| id:" << folder.id
             << "| itemId:" << folder.itemId
             << "| guid:" << folder.guid
             << "| pathCount:" << folder.locations.size()
             << "| optionSummary:"
             << buildLibraryOptionDebugSummary(folder.libraryOptions);
}

QJsonArray extractItemsArray(const QJsonObject &response) {
    if (response.contains("Items") && response.value("Items").isArray()) {
        return response.value("Items").toArray();
    }

    if (response.contains("data") && response.value("data").isArray()) {
        return response.value("data").toArray();
    }

    return {};
}

QString buildCodecDebugSummary(const QJsonArray& items) {
    QStringList lines;

    for (const QJsonValue& value : items) {
        const QJsonObject item = value.toObject();
        if (item.isEmpty()) {
            continue;
        }

        const QString name = item.value(QStringLiteral("Name")).toString();
        const QString mediaTypeName = item.value(QStringLiteral("MediaTypeName")).toString();
        const QString videoMediaType = item.value(QStringLiteral("VideoMediaType")).toString();
        const bool av1OrVp9 =
            name.contains(QStringLiteral("AV1"), Qt::CaseInsensitive) ||
            name.contains(QStringLiteral("VP9"), Qt::CaseInsensitive) ||
            mediaTypeName.compare(QStringLiteral("AV1"), Qt::CaseInsensitive) == 0 ||
            mediaTypeName.compare(QStringLiteral("VP9"), Qt::CaseInsensitive) == 0 ||
            videoMediaType.compare(QStringLiteral("av1"), Qt::CaseInsensitive) == 0 ||
            videoMediaType.compare(QStringLiteral("vp9"), Qt::CaseInsensitive) == 0;
        if (!av1OrVp9) {
            continue;
        }

        lines.append(
            QStringLiteral("%1 | hw=%2 | media=%3 | video=%4 | id=%5 | name=%6")
                .arg(item.value(QStringLiteral("Direction")).toString(),
                     item.value(QStringLiteral("IsHardwareCodec")).toBool(false)
                         ? QStringLiteral("true")
                         : QStringLiteral("false"),
                     mediaTypeName,
                     videoMediaType,
                     item.value(QStringLiteral("Id")).toString(),
                     name));
    }

    return lines.join(QStringLiteral(" || "));
}

bool isCompactGuidString(const QString &value) {
    if (value.size() != 32) {
        return false;
    }

    for (const QChar ch : value) {
        if (!ch.isDigit() &&
            (ch.toLower() < QLatin1Char('a') || ch.toLower() > QLatin1Char('f'))) {
            return false;
        }
    }

    return true;
}

QString normalizeLibraryOptionsFolderId(ServerProfile::ServerType serverType,
                                        const QString &folderId) {
    const QString trimmedId = folderId.trimmed();
    if (serverType != ServerProfile::Jellyfin || !isCompactGuidString(trimmedId)) {
        return trimmedId;
    }

    return QStringLiteral("%1-%2-%3-%4-%5")
        .arg(trimmedId.mid(0, 8), trimmedId.mid(8, 4), trimmedId.mid(12, 4),
             trimmedId.mid(16, 4), trimmedId.mid(20, 12));
}

QString buildItemImagePath(QString itemId, QString imageType, int imageIndex) {
    itemId = itemId.trimmed();
    imageType = imageType.trimmed();
    QString path = QStringLiteral("/Items/%1/Images/%2").arg(itemId, imageType);
    if (imageIndex >= 0) {
        path += QStringLiteral("/%1").arg(imageIndex);
    }
    return path;
}

} 

AdminService::AdminService(ServerManager* serverManager, QObject *parent)
    : QObject(parent), m_serverManager(serverManager)
{
}

void AdminService::ensureValidProfile() const {
    if (!m_serverManager->activeClient() || !m_serverManager->activeProfile().isValid()) {
        throw std::runtime_error("Invalid server configuration or user not logged in");
    }
}





QCoro::Task<SystemInfo> AdminService::getSystemInfo()
{
    ensureValidProfile();
    QJsonObject response = co_await m_serverManager->activeClient()->get("/System/Info");
    co_return SystemInfo::fromJson(response);
}

QCoro::Task<void> AdminService::restartServer()
{
    ensureValidProfile();
    co_await m_serverManager->activeClient()->post("/System/Restart", QJsonObject());
}

QCoro::Task<void> AdminService::shutdownServer()
{
    ensureValidProfile();
    co_await m_serverManager->activeClient()->post("/System/Shutdown", QJsonObject());
}





QCoro::Task<QList<SessionInfo>> AdminService::getActiveSessions()
{
    ensureValidProfile();
    QJsonObject response = co_await m_serverManager->activeClient()->get("/Sessions");

    QList<SessionInfo> sessions;
    
    
    if (response.contains("Items")) {
        QJsonArray arr = response["Items"].toArray();
        for (const auto& val : arr) {
            sessions.append(SessionInfo::fromJson(val.toObject()));
        }
    } else if (response.contains("data") && response["data"].isArray()) {
        QJsonArray arr = response["data"].toArray();
        for (const auto& val : arr) {
            sessions.append(SessionInfo::fromJson(val.toObject()));
        }
    }
    co_return sessions;
}





QCoro::Task<QList<UserInfo>> AdminService::getUsers()
{
    ensureValidProfile();
    QJsonObject response = co_await m_serverManager->activeClient()->get("/Users");

    QList<UserInfo> users;
    if (response.contains("Items")) {
        QJsonArray arr = response["Items"].toArray();
        for (const auto& val : arr) {
            users.append(UserInfo::fromJson(val.toObject()));
        }
    } else if (response.contains("data") && response["data"].isArray()) {
        QJsonArray arr = response["data"].toArray();
        for (const auto& val : arr) {
            users.append(UserInfo::fromJson(val.toObject()));
        }
    }
    co_return users;
}

QCoro::Task<QJsonObject> AdminService::createUser(
    const QString& name, const QString& copyFromUserId,
    const QStringList& userCopyOptions)
{
    ensureValidProfile();

    QJsonObject payload;
    payload["Name"] = name;
    if (!copyFromUserId.trimmed().isEmpty()) {
        payload["CopyFromUserId"] = copyFromUserId.trimmed();

        QJsonArray copyOptionsArray;
        for (const QString& option : userCopyOptions) {
            const QString trimmedOption = option.trimmed();
            if (!trimmedOption.isEmpty()) {
                copyOptionsArray.append(trimmedOption);
            }
        }
        payload["UserCopyOptions"] = copyOptionsArray;
    }

    qDebug() << "[AdminService] createUser"
             << "| name=" << name
             << "| copyFromUserId=" << copyFromUserId
             << "| copyOptionsCount=" << userCopyOptions.size();

    QJsonObject response =
        co_await m_serverManager->activeClient()->post("/Users/New", payload);
    co_return response;
}

QCoro::Task<QList<SelectableMediaFolderInfo>>
AdminService::getSelectableMediaFolders(bool includeHidden)
{
    ensureValidProfile();

    QUrlQuery query;
    query.addQueryItem("IsHidden", includeHidden ? "true" : "false");
    const QString path =
        "/Library/SelectableMediaFolders?" + query.toString(QUrl::FullyEncoded);

    qDebug() << "[AdminService] getSelectableMediaFolders"
             << "| path=" << path;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);
    const QJsonArray items = extractItemsArray(response);

    QList<SelectableMediaFolderInfo> folders;
    for (const auto& value : items) {
        folders.append(SelectableMediaFolderInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getSelectableMediaFolders completed"
             << "| count=" << folders.size();
    co_return folders;
}

QCoro::Task<QList<AdminMediaFolderInfo>>
AdminService::getMediaFolders(bool includeHidden)
{
    ensureValidProfile();

    QUrlQuery query;
    query.addQueryItem("IsHidden", includeHidden ? "true" : "false");
    const QString path =
        "/Library/MediaFolders?" + query.toString(QUrl::FullyEncoded);

    qDebug() << "[AdminService] getMediaFolders"
             << "| path=" << path;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);
    const QJsonArray items = extractItemsArray(response);

    QList<AdminMediaFolderInfo> folders;
    for (const auto& value : items) {
        folders.append(AdminMediaFolderInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getMediaFolders completed"
             << "| count=" << folders.size();
    co_return folders;
}

QCoro::Task<QList<AdminChannelInfo>>
AdminService::getChannels(bool supportsMediaDeletion)
{
    ensureValidProfile();

    QString path = QStringLiteral("/Channels");
    if (supportsMediaDeletion) {
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("SupportsMediaDeletion"),
                           QStringLiteral("true"));
        path += QStringLiteral("?") + query.toString(QUrl::FullyEncoded);
    }

    qDebug() << "[AdminService] getChannels"
             << "| path=" << path;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);
    const QJsonArray items = extractItemsArray(response);

    QList<AdminChannelInfo> channels;
    for (const auto& value : items) {
        channels.append(AdminChannelInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getChannels completed"
             << "| count=" << channels.size();
    co_return channels;
}

QCoro::Task<QList<AdminDeviceInfo>> AdminService::getDevices()
{
    ensureValidProfile();
    qDebug() << "[AdminService] getDevices";

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get("/Devices");
    const QJsonArray items = extractItemsArray(response);

    QList<AdminDeviceInfo> devices;
    for (const auto& value : items) {
        devices.append(AdminDeviceInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getDevices completed"
             << "| count=" << devices.size();
    co_return devices;
}

QCoro::Task<QList<AdminAuthProviderInfo>> AdminService::getAuthProviders()
{
    ensureValidProfile();
    qDebug() << "[AdminService] getAuthProviders";

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get("/Auth/Providers");
    const QJsonArray items = extractItemsArray(response);

    QList<AdminAuthProviderInfo> providers;
    for (const auto& value : items) {
        providers.append(AdminAuthProviderInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getAuthProviders completed"
             << "| count=" << providers.size();
    co_return providers;
}

QCoro::Task<QList<AdminFeatureInfo>> AdminService::getUserFeatures()
{
    ensureValidProfile();

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("FeatureType"), QStringLiteral("User"));
    const QString path =
        "/Features?" + query.toString(QUrl::FullyEncoded);

    qDebug() << "[AdminService] getUserFeatures"
             << "| path=" << path;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);
    const QJsonArray items = extractItemsArray(response);

    QList<AdminFeatureInfo> features;
    for (const auto& value : items) {
        features.append(AdminFeatureInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getUserFeatures completed"
             << "| count=" << features.size();
    co_return features;
}

QCoro::Task<QList<AdminParentalRatingInfo>> AdminService::getParentalRatings()
{
    ensureValidProfile();
    qDebug() << "[AdminService] getParentalRatings";

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(
            "/Localization/ParentalRatings");
    const QJsonArray items = extractItemsArray(response);

    QList<AdminParentalRatingInfo> ratings;
    for (const auto& value : items) {
        ratings.append(AdminParentalRatingInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getParentalRatings completed"
             << "| count=" << ratings.size();
    co_return ratings;
}

QCoro::Task<void> AdminService::setUserPolicy(const QString& userId, const QJsonObject& policy)
{
    ensureValidProfile();
    co_await m_serverManager->activeClient()->post(
        QString("/Users/%1/Policy").arg(userId), policy);
}

QCoro::Task<void> AdminService::updateUserConfiguration(
    const QString& userId, const QJsonObject& configuration)
{
    ensureValidProfile();
    qDebug() << "[AdminService] updateUserConfiguration"
             << "| userId=" << userId
             << "| keys=" << configuration.keys();
    co_await m_serverManager->activeClient()->post(
        QString("/Users/%1/Configuration").arg(userId), configuration);
}

QCoro::Task<QJsonObject> AdminService::getUserById(const QString& userId)
{
    ensureValidProfile();
    qDebug() << "[AdminService] getUserById"
             << "| userId=" << userId;
    QJsonObject response = co_await m_serverManager->activeClient()->get(
        QString("/Users/%1").arg(userId));
    co_return response;
}

QCoro::Task<void> AdminService::updateUser(QString userId, QJsonObject userData)
{
    ensureValidProfile();
    qDebug() << "[AdminService] updateUser"
             << "| userId=" << userId
             << "| keys=" << userData.keys();
    co_await m_serverManager->activeClient()->post(
        QString("/Users/%1").arg(userId), userData);
}

QCoro::Task<void> AdminService::deleteUser(const QString& userId)
{
    ensureValidProfile();
    qDebug() << "[AdminService] deleteUser"
             << "| userId=" << userId;
    co_await m_serverManager->activeClient()->deleteResource(
        QString("/Users/%1").arg(userId));
}

QCoro::Task<void> AdminService::updateUserPassword(const QString& userId,
                                                     const QString& currentPassword,
                                                     const QString& newPassword)
{
    ensureValidProfile();
    const ServerProfile profile = m_serverManager->activeProfile();

    qDebug() << "[AdminService] updateUserPassword"
             << "| userId=" << userId
             << "| sessionUserId=" << profile.userId
             << "| serverType="
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin");

    const QString path = QString("/Users/%1/Password").arg(userId);
    if (profile.type == ServerProfile::Emby) {
        
        QUrlQuery formData;
        formData.addQueryItem("NewPw", newPassword);
        if (!currentPassword.trimmed().isEmpty()) {
            formData.addQueryItem("CurrentPw", currentPassword);
        }

        qDebug() << "[AdminService] updateUserPassword using Emby form payload"
                 << "| path=" << path
                 << "| fields=" << formData.queryItems().size();
        co_await m_serverManager->activeClient()->postForm(path, formData);
        co_return;
    }

    QJsonObject payload;
    payload["NewPw"] = newPassword;
    if (!currentPassword.trimmed().isEmpty()) {
        payload["CurrentPw"] = currentPassword;
        payload["CurrentPassword"] = currentPassword;
    }

    co_await m_serverManager->activeClient()->post(path, payload);
}

QCoro::Task<void> AdminService::updateEasyPassword(const QString& userId,
                                                   const QString& newPassword,
                                                   bool resetPassword)
{
    ensureValidProfile();

    QUrlQuery formData;
    if (resetPassword) {
        formData.addQueryItem(QStringLiteral("resetPassword"),
                              QStringLiteral("true"));
    } else {
        formData.addQueryItem(QStringLiteral("NewPw"), newPassword);
    }

    const QString path = QString("/Users/%1/EasyPassword").arg(userId);
    qDebug() << "[AdminService] updateEasyPassword"
             << "| userId=" << userId
             << "| reset=" << resetPassword;
    co_await m_serverManager->activeClient()->postForm(path, formData);
}

QCoro::Task<void> AdminService::updateConnectLink(
    const QString& userId, const QString& connectUserName)
{
    ensureValidProfile();

    QUrlQuery formData;
    formData.addQueryItem(QStringLiteral("ConnectUsername"),
                          connectUserName.trimmed());

    const QString path = QString("/Users/%1/Connect/Link").arg(userId);
    qDebug() << "[AdminService] updateConnectLink"
             << "| userId=" << userId
             << "| hasValue=" << !connectUserName.trimmed().isEmpty();
    co_await m_serverManager->activeClient()->postForm(path, formData);
}

QCoro::Task<void> AdminService::removeConnectLink(const QString& userId)
{
    ensureValidProfile();
    const QString path = QString("/Users/%1/Connect/Link/Delete").arg(userId);
    qDebug() << "[AdminService] removeConnectLink"
             << "| userId=" << userId;
    co_await m_serverManager->activeClient()->postForm(path, QUrlQuery());
}





QCoro::Task<AdminService::LibraryProviders> AdminService::getAvailableProviders()
{
    LibraryProviders providers;

    try {
        ensureValidProfile();
        const ServerProfile profile = m_serverManager->activeProfile();
        
        const QString apiPath = (profile.type == ServerProfile::Emby)
                                    ? QStringLiteral("/Library/VirtualFolders/Query")
                                    : QStringLiteral("/Library/VirtualFolders");
        qDebug() << "[AdminService] getAvailableProviders"
                 << "| serverType:" << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
                 << "| apiPath:" << apiPath;
        QJsonObject response = co_await m_serverManager->activeClient()->get(apiPath);

        
        auto extractStrings = [](const QJsonArray& arr, QStringList& target) {
            for (const auto& val : arr) {
                QString s = val.toString().trimmed();
                if (!s.isEmpty() && !target.contains(s)) {
                    target.append(s);
                }
            }
        };

        
        auto processItems = [&](const QJsonArray& items) {
            for (const auto& folderVal : items) {
                QJsonObject folder = folderVal.toObject();
                QJsonObject libOpts = folder["LibraryOptions"].toObject();

                
                if (libOpts.contains("MetadataDownloaders"))
                    extractStrings(libOpts["MetadataDownloaders"].toArray(), providers.metadataFetchers);
                if (libOpts.contains("MetadataFetchers"))
                    extractStrings(libOpts["MetadataFetchers"].toArray(), providers.metadataFetchers);
                if (libOpts.contains("MetadataFetcherOrder"))
                    extractStrings(libOpts["MetadataFetcherOrder"].toArray(), providers.metadataFetchers);
                if (libOpts.contains("DisabledMetadataFetchers"))
                    extractStrings(libOpts["DisabledMetadataFetchers"].toArray(), providers.metadataFetchers);
                if (libOpts.contains("ImageFetchers"))
                    extractStrings(libOpts["ImageFetchers"].toArray(), providers.imageFetchers);
                if (libOpts.contains("ImageFetcherOrder"))
                    extractStrings(libOpts["ImageFetcherOrder"].toArray(), providers.imageFetchers);
                if (libOpts.contains("DisabledImageFetchers"))
                    extractStrings(libOpts["DisabledImageFetchers"].toArray(), providers.imageFetchers);
                if (libOpts.contains("SubtitleDownloaders"))
                    extractStrings(libOpts["SubtitleDownloaders"].toArray(), providers.subtitleDownloaders);
                if (libOpts.contains("SubtitleFetchers"))
                    extractStrings(libOpts["SubtitleFetchers"].toArray(), providers.subtitleDownloaders);
                if (libOpts.contains("SubtitleFetcherOrder"))
                    extractStrings(libOpts["SubtitleFetcherOrder"].toArray(), providers.subtitleDownloaders);
                if (libOpts.contains("DisabledSubtitleFetchers"))
                    extractStrings(libOpts["DisabledSubtitleFetchers"].toArray(), providers.subtitleDownloaders);
                if (libOpts.contains("LyricsFetchers"))
                    extractStrings(libOpts["LyricsFetchers"].toArray(), providers.lyricsFetchers);
                if (libOpts.contains("LyricsFetcherOrder"))
                    extractStrings(libOpts["LyricsFetcherOrder"].toArray(), providers.lyricsFetchers);
                if (libOpts.contains("LyricFetcherOrder"))
                    extractStrings(libOpts["LyricFetcherOrder"].toArray(), providers.lyricsFetchers);
                if (libOpts.contains("DisabledLyricsFetchers"))
                    extractStrings(libOpts["DisabledLyricsFetchers"].toArray(), providers.lyricsFetchers);

                
                QJsonArray typeOpts = libOpts["TypeOptions"].toArray();
                for (const auto& toVal : typeOpts) {
                    QJsonObject to = toVal.toObject();
                    if (to.contains("MetadataFetchers"))
                        extractStrings(to["MetadataFetchers"].toArray(), providers.metadataFetchers);
                    if (to.contains("MetadataFetcherOrder"))
                        extractStrings(to["MetadataFetcherOrder"].toArray(), providers.metadataFetchers);
                    if (to.contains("DisabledMetadataFetchers"))
                        extractStrings(to["DisabledMetadataFetchers"].toArray(), providers.metadataFetchers);
                    if (to.contains("ImageFetchers"))
                        extractStrings(to["ImageFetchers"].toArray(), providers.imageFetchers);
                    if (to.contains("ImageFetcherOrder"))
                        extractStrings(to["ImageFetcherOrder"].toArray(), providers.imageFetchers);
                    if (to.contains("DisabledImageFetchers"))
                        extractStrings(to["DisabledImageFetchers"].toArray(), providers.imageFetchers);
                }
            }
        };

        if (response.contains("Items"))
            processItems(response["Items"].toArray());
        else if (response.contains("data") && response["data"].isArray())
            processItems(response["data"].toArray());

    } catch (...) {
        
    }

    co_return providers;
}

QCoro::Task<AdminService::LocalizationOptions> AdminService::getLocalizationOptions()
{
    LocalizationOptions options;

    try {
        options.cultures = co_await getLocalizationCultures();
    } catch (const std::exception& e) {
        qWarning() << "[AdminService] getLocalizationCultures failed"
                   << "| error:" << QString::fromUtf8(e.what());
    }

    try {
        options.countries = co_await getLocalizationCountries();
    } catch (const std::exception& e) {
        qWarning() << "[AdminService] getLocalizationCountries failed"
                   << "| error:" << QString::fromUtf8(e.what());
    }

    qDebug() << "[AdminService] getLocalizationOptions completed"
             << "| cultureCount:" << options.cultures.size()
             << "| countryCount:" << options.countries.size();

    co_return options;
}

QCoro::Task<QList<LocalizationCulture>> AdminService::getLocalizationCultures()
{
    ensureValidProfile();
    QJsonObject response =
        co_await m_serverManager->activeClient()->get("/Localization/cultures");

    QList<LocalizationCulture> cultures;
    auto appendCultures = [&cultures](const QJsonArray& array) {
        for (const auto& value : array) {
            const LocalizationCulture culture =
                LocalizationCulture::fromJson(value.toObject());
            if (culture.canonicalCode().isEmpty()) {
                continue;
            }
            cultures.append(culture);
        }
    };

    if (response.contains("Items")) {
        appendCultures(response["Items"].toArray());
    } else if (response.contains("data") && response["data"].isArray()) {
        appendCultures(response["data"].toArray());
    }

    qDebug() << "[AdminService] getLocalizationCultures"
             << "| count:" << cultures.size();

    co_return cultures;
}

QCoro::Task<QList<LocalizationCountry>> AdminService::getLocalizationCountries()
{
    ensureValidProfile();
    QJsonObject response =
        co_await m_serverManager->activeClient()->get("/Localization/countries");

    QList<LocalizationCountry> countries;
    auto appendCountries = [&countries](const QJsonArray& array) {
        for (const auto& value : array) {
            const LocalizationCountry country =
                LocalizationCountry::fromJson(value.toObject());
            if (country.canonicalCode().isEmpty()) {
                continue;
            }
            countries.append(country);
        }
    };

    if (response.contains("Items")) {
        appendCountries(response["Items"].toArray());
    } else if (response.contains("data") && response["data"].isArray()) {
        appendCountries(response["data"].toArray());
    }

    qDebug() << "[AdminService] getLocalizationCountries"
             << "| count:" << countries.size();

    co_return countries;
}

QCoro::Task<QList<VirtualFolder>> AdminService::getVirtualFolders()
{
    ensureValidProfile();
    const ServerProfile profile = m_serverManager->activeProfile();
    
    const QString apiPath = (profile.type == ServerProfile::Emby)
                                ? QStringLiteral("/Library/VirtualFolders/Query")
                                : QStringLiteral("/Library/VirtualFolders");
    qDebug() << "[AdminService] getVirtualFolders"
             << "| serverType:" << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| apiPath:" << apiPath;
    QJsonObject response = co_await m_serverManager->activeClient()->get(apiPath);

    QList<VirtualFolder> folders;
    if (response.contains("Items")) {
        QJsonArray arr = response["Items"].toArray();
        for (const auto& val : arr) {
            const VirtualFolder folder = VirtualFolder::fromJson(val.toObject());
            logVirtualFolderSnapshot(folder,
                                     "[AdminService] getVirtualFolders item");
            folders.append(folder);
        }
    } else if (response.contains("data") && response["data"].isArray()) {
        QJsonArray arr = response["data"].toArray();
        for (const auto& val : arr) {
            const VirtualFolder folder = VirtualFolder::fromJson(val.toObject());
            logVirtualFolderSnapshot(folder,
                                     "[AdminService] getVirtualFolders item");
            folders.append(folder);
        }
    }

    qDebug() << "[AdminService] getVirtualFolders completed"
             << "| count:" << folders.size();
    co_return folders;
}

QCoro::Task<void> AdminService::addVirtualFolder(const QString& name, const QString& collectionType,
                                                   const QStringList& paths,
                                                   const QJsonObject& libraryOptions)
{
    ensureValidProfile();
    const ServerProfile profile = m_serverManager->activeProfile();

    if (profile.type == ServerProfile::Emby) {
        QUrlQuery query;
        query.addQueryItem("name", name);
        query.addQueryItem("refreshLibrary", "false");
        if (!collectionType.isEmpty()) {
            query.addQueryItem("collectionType", collectionType);
        }
        const QString path =
            "/Library/VirtualFolders?" + query.toString(QUrl::FullyEncoded);

        QJsonObject embyLibraryOptions = libraryOptions;
        QJsonArray pathInfos;
        for (const auto& p : paths) {
            QJsonObject pathInfo;
            pathInfo["Path"] = p;
            pathInfos.append(pathInfo);
        }
        embyLibraryOptions["PathInfos"] = pathInfos;

        QJsonObject payload;
        payload["LibraryOptions"] = embyLibraryOptions;

        const QString compactLibraryOptions = QString::fromUtf8(
            QJsonDocument(embyLibraryOptions).toJson(QJsonDocument::Compact));

        qDebug() << "[AdminService] addVirtualFolder using Emby POST"
                 << "| path:" << path
                 << "| libraryOptions:" << compactLibraryOptions;

        co_await m_serverManager->activeClient()->post(path, payload);
        co_return;
    }

    QUrlQuery query;
    query.addQueryItem("name", name);
    query.addQueryItem("refreshLibrary", "false");
    if (!collectionType.isEmpty()) {
        query.addQueryItem("collectionType", collectionType);
    }

    const QString path =
        "/Library/VirtualFolders?" + query.toString(QUrl::FullyEncoded);

    QJsonObject jellyfinLibraryOptions = libraryOptions;
    QJsonArray pathInfos;
    for (const auto& p : paths) {
        QJsonObject pathInfo;
        pathInfo["Path"] = p;
        pathInfos.append(pathInfo);
    }
    jellyfinLibraryOptions["PathInfos"] = pathInfos;

    QJsonObject payload;
    payload["LibraryOptions"] = jellyfinLibraryOptions;

    const QString compactLibraryOptions =
        QString::fromUtf8(QJsonDocument(jellyfinLibraryOptions).toJson(QJsonDocument::Compact));

    qDebug() << "[AdminService] addVirtualFolder using Jellyfin POST"
             << "| path:" << path
             << "| libraryOptions:" << compactLibraryOptions;

    co_await m_serverManager->activeClient()->post(path, payload);
}

QCoro::Task<void> AdminService::removeVirtualFolder(const VirtualFolder& folder)
{
    ensureValidProfile();
    const ServerProfile profile = m_serverManager->activeProfile();
    const QString folderId = folder.virtualFolderId();

    qDebug() << "[AdminService] removeVirtualFolder"
             << "| serverType:"
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| name:" << folder.name
             << "| id:" << folder.id
             << "| itemId:" << folder.itemId
             << "| guid:" << folder.guid
             << "| folderId:" << folderId;

    if (profile.type == ServerProfile::Emby) {
        QJsonObject payload;
        payload["Id"] = folderId;
        payload["RefreshLibrary"] = false;

        qDebug() << "[AdminService] removeVirtualFolder using Emby POST"
                 << "| path: /Library/VirtualFolders/Delete"
                 << "| payloadId:" << folderId;
        co_await m_serverManager->activeClient()->post(
            "/Library/VirtualFolders/Delete", payload);
        co_return;
    }

    QUrlQuery query;
    query.addQueryItem("refreshLibrary", "false");
    query.addQueryItem("name", folder.name);
    const QString path =
        "/Library/VirtualFolders?" + query.toString(QUrl::FullyEncoded);

    qDebug() << "[AdminService] removeVirtualFolder using Jellyfin DELETE"
             << "| path:" << path;
    co_await m_serverManager->activeClient()->deleteResource(path);
}

QCoro::Task<void> AdminService::renameVirtualFolder(const QString& oldName, const QString& newName)
{
    ensureValidProfile();
    QJsonObject payload;
    payload["Name"] = oldName;
    payload["NewName"] = newName;
    co_await m_serverManager->activeClient()->post("/Library/VirtualFolders/Name", payload);
}

QCoro::Task<void> AdminService::addMediaPath(const QString& libraryId,
                                             const QString& libraryName,
                                             const QString& path)
{
    ensureValidProfile();
    const ServerProfile profile = m_serverManager->activeProfile();

    if (profile.type == ServerProfile::Emby) {
        QJsonObject payload;
        payload["Id"] = libraryId;
        payload["Path"] = path;
        payload["RefreshLibrary"] = false;

        qDebug() << "[AdminService] addMediaPath using Emby POST"
                 << "| libraryId:" << libraryId
                 << "| path:" << path;
        co_await m_serverManager->activeClient()->post(
            "/Library/VirtualFolders/Paths", payload);
        co_return;
    }

    QJsonObject payload;
    payload["Name"] = libraryName;
    payload["Path"] = path;
    qDebug() << "[AdminService] addMediaPath using Jellyfin POST"
             << "| name:" << libraryName
             << "| path:" << path;
    co_await m_serverManager->activeClient()->post("/Library/VirtualFolders/Paths", payload);
}

QCoro::Task<void> AdminService::removeMediaPath(const QString& libraryId,
                                                const QString& libraryName,
                                                const QString& path)
{
    ensureValidProfile();
    const ServerProfile profile = m_serverManager->activeProfile();

    if (profile.type == ServerProfile::Emby) {
        QJsonObject payload;
        payload["Id"] = libraryId;
        payload["Path"] = path;
        payload["RefreshLibrary"] = false;

        qDebug() << "[AdminService] removeMediaPath using Emby POST"
                 << "| libraryId:" << libraryId
                 << "| path:" << path;
        co_await m_serverManager->activeClient()->post(
            "/Library/VirtualFolders/Paths/Delete", payload);
        co_return;
    }

    QString apiPath = QString("/Library/VirtualFolders/Paths?name=%1&path=%2")
                          .arg(QUrl::toPercentEncoding(libraryName),
                               QUrl::toPercentEncoding(path));
    qDebug() << "[AdminService] removeMediaPath using Jellyfin DELETE"
             << "| name:" << libraryName
             << "| path:" << path;
    co_await m_serverManager->activeClient()->deleteResource(apiPath);
}

QCoro::Task<void> AdminService::updateVirtualFolderOptions(
    const QString& folderId,
    const QJsonObject& libraryOptions,
    const QStringList& paths)
{
    ensureValidProfile();
    const ServerProfile profile = m_serverManager->activeProfile();

    QJsonObject normalizedLibraryOptions = libraryOptions;
    QJsonArray pathInfos;
    for (const auto& path : paths) {
        const QString trimmedPath = path.trimmed();
        if (trimmedPath.isEmpty()) {
            continue;
        }

        QJsonObject pathInfo;
        pathInfo["Path"] = trimmedPath;
        pathInfos.append(pathInfo);
    }
    normalizedLibraryOptions["PathInfos"] = pathInfos;

    const QString normalizedFolderId =
        normalizeLibraryOptionsFolderId(profile.type, folderId);

    QJsonObject payload;
    payload["Id"] = normalizedFolderId;
    payload["LibraryOptions"] = normalizedLibraryOptions;

    qDebug() << "[AdminService] updateVirtualFolderOptions"
             << "| folderId:" << folderId
             << "| normalizedFolderId:" << normalizedFolderId
             << "| pathCount:" << pathInfos.size()
             << "| optionSummary:"
             << buildLibraryOptionDebugSummary(normalizedLibraryOptions)
             << "| libraryOptions:"
             << QString::fromUtf8(
                    QJsonDocument(normalizedLibraryOptions).toJson(QJsonDocument::Compact));

    co_await m_serverManager->activeClient()->post(
        "/Library/VirtualFolders/LibraryOptions", payload);
}

QCoro::Task<void> AdminService::refreshLibrary()
{
    ensureValidProfile();
    co_await m_serverManager->activeClient()->post("/Library/Refresh", QJsonObject());
}





QCoro::Task<QList<AdminService::DriveInfo>> AdminService::getServerDrives()
{
    ensureValidProfile();
    QJsonObject response = co_await m_serverManager->activeClient()->get("/Environment/Drives");

    QList<DriveInfo> drives;

    
    static const QStringList excludedPaths = {
        "/proc", "/sys", "/dev", "/run", "/snap",
        "/boot", "/tmp", "/lost+found"
    };

    auto parseDrives = [&](const QJsonArray& arr) {
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            DriveInfo d;
            d.path = obj["Path"].toString();
            d.name = obj["Name"].toString();
            if (d.name.isEmpty()) d.name = d.path;

            
            bool excluded = false;
            for (const auto& ep : excludedPaths) {
                if (d.path == ep || d.path.startsWith(ep + "/")) {
                    excluded = true;
                    break;
                }
            }
            if (excluded) continue;

            drives.append(d);
        }
    };

    if (response.contains("Items"))
        parseDrives(response["Items"].toArray());
    else if (response.contains("data") && response["data"].isArray())
        parseDrives(response["data"].toArray());
    else {
        
        for (const auto& key : response.keys()) {
            if (response[key].isArray()) {
                parseDrives(response[key].toArray());
                break;
            }
        }
    }

    co_return drives;
}

QCoro::Task<QStringList> AdminService::getDirectoryContents(const QString& path)
{
    ensureValidProfile();
    QString apiPath = QString("/Environment/DirectoryContents?path=%1&includeDirectories=true&includeFiles=false")
                          .arg(QUrl::toPercentEncoding(path));
    QJsonObject response = co_await m_serverManager->activeClient()->get(apiPath);

    QStringList dirs;
    auto parseDirs = [&](const QJsonArray& arr) {
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            QString dirPath = obj["Path"].toString();
            if (!dirPath.isEmpty()) dirs.append(dirPath);
        }
    };

    if (response.contains("Items"))
        parseDirs(response["Items"].toArray());
    else if (response.contains("data") && response["data"].isArray())
        parseDirs(response["data"].toArray());
    else {
        for (const auto& key : response.keys()) {
            if (response[key].isArray()) {
                parseDirs(response[key].toArray());
                break;
            }
        }
    }

    co_return dirs;
}

QCoro::Task<QStringList> AdminService::getLibraryOrder()
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();
    QString path = QString("/Users/%1/Views?IncludeHidden=true").arg(profile.userId);
    QJsonObject response = co_await m_serverManager->activeClient()->get(path);

    
    QStringList orderedIds;
    if (response.contains("Items")) {
        QJsonArray arr = response["Items"].toArray();
        for (const auto& val : arr) {
            QJsonObject item = val.toObject();
            QString id = item["Id"].toString();
            if (!id.isEmpty()) {
                orderedIds.append(id);
            }
        }
    }
    co_return orderedIds;
}

QCoro::Task<void> AdminService::updateLibraryOrder(QStringList orderedItemIds)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    
    QStringList orderingKeys;
    if (profile.type == ServerProfile::Emby) {
        QString viewsPath = QString("/Users/%1/Views?IncludeHidden=true").arg(profile.userId);
        QJsonObject viewsResp = co_await m_serverManager->activeClient()->get(viewsPath);

        QMap<QString, QString> idToGuid;
        if (viewsResp.contains("Items")) {
            QJsonArray arr = viewsResp["Items"].toArray();
            for (const auto& val : arr) {
                QJsonObject item = val.toObject();
                idToGuid[item["Id"].toString()] = item["Guid"].toString();
            }
        }
        
        for (const auto& itemId : orderedItemIds) {
            QString guid = idToGuid.value(itemId, itemId);
            orderingKeys.append(guid);
        }
    } else {
        
        orderingKeys = orderedItemIds;
    }

    QString confPath = QString("/Users/%1/Configuration").arg(profile.userId);
    QJsonObject payload;
    QJsonArray orderedArray;
    for (const auto& key : orderingKeys) {
        orderedArray.append(key);
    }
    payload["OrderedViews"] = orderedArray;

    co_await m_serverManager->activeClient()->post(confPath, payload);
}





QCoro::Task<QJsonArray> AdminService::getUserPlaylists()
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();
    const QString viewsPath =
        QString("/Users/%1/Views?IncludeHidden=true").arg(profile.userId);
    const QJsonObject viewsResponse =
        co_await m_serverManager->activeClient()->get(viewsPath);
    const QJsonArray views = extractItemsArray(viewsResponse);

    QString playlistViewId;
    QString playlistViewName;
    for (const auto &value : views) {
        const QJsonObject view = value.toObject();
        const QString collectionType = view.value("CollectionType").toString();
        if (collectionType.compare(QStringLiteral("playlists"),
                                   Qt::CaseInsensitive) == 0) {
            playlistViewId = view.value("Id").toString();
            playlistViewName = view.value("Name").toString();
            break;
        }
    }

    QString path;
    bool usedLegacyFallback = false;
    if (!playlistViewId.isEmpty()) {
        path = QString("/Users/%1/Items?ParentId=%2"
                       "&IncludeItemTypes=Playlist"
                       "&Recursive=false"
                       "&Fields=ChildCount,DateCreated,MediaType"
                       "&SortBy=DateCreated&SortOrder=Descending")
                   .arg(profile.userId, playlistViewId);
    } else {
        usedLegacyFallback = true;
        path = QString("/Users/%1/Items?IncludeItemTypes=Playlist"
                       "&Recursive=true"
                       "&Fields=ChildCount,DateCreated,MediaType"
                       "&SortBy=DateCreated&SortOrder=Descending")
                   .arg(profile.userId);

        qWarning() << "[AdminService] getUserPlaylists: playlist view not found,"
                   << "falling back to recursive query"
                   << "| userId:" << profile.userId
                   << "| viewCount:" << views.size();
    }

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);
    const QJsonArray items = extractItemsArray(response);

    qDebug() << "[AdminService] getUserPlaylists"
             << "| playlistViewId:" << playlistViewId
             << "| playlistViewName:" << playlistViewName
             << "| fallbackRecursive:" << usedLegacyFallback
             << "| count:" << items.size();
    co_return items;
}

QCoro::Task<void> AdminService::updatePlaylistOrderByDateCreated(QStringList orderedIds)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    if (orderedIds.isEmpty()) {
        qDebug() << "[AdminService] updatePlaylistOrderByDateCreated skipped: empty order";
        co_return;
    }

    const QDateTime baseDateTime = QDateTime::currentDateTimeUtc();

    qDebug() << "[AdminService] updatePlaylistOrderByDateCreated"
             << "| count:" << orderedIds.size()
             << "| baseDateTime:" << baseDateTime.toString(Qt::ISODateWithMs);

    for (int index = 0; index < orderedIds.size(); ++index) {
        const QString itemId = orderedIds.at(index);
        if (itemId.trimmed().isEmpty()) {
            qWarning() << "[AdminService] updatePlaylistOrderByDateCreated skipped empty itemId"
                       << "| index:" << index;
            continue;
        }

        QJsonObject itemData = co_await m_serverManager->activeClient()->get(
            QString("/Users/%1/Items/%2").arg(profile.userId, itemId));

        const QDateTime dateCreated = baseDateTime.addSecs(-index);
        const QString dateCreatedText = dateCreated.toString(Qt::ISODateWithMs);
        itemData["DateCreated"] = dateCreatedText;

        qDebug() << "[AdminService] updatePlaylistOrderByDateCreated item"
                 << "| index:" << index
                 << "| itemId:" << itemId
                 << "| name:" << itemData.value("Name").toString()
                 << "| dateCreated:" << dateCreatedText;

        co_await m_serverManager->activeClient()->post(
            QString("/Items/%1").arg(itemId), itemData);
    }
}

QCoro::Task<void> AdminService::updatePlaylistDateCreated(QString itemId,
                                                          QString dateCreatedText)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    if (itemId.trimmed().isEmpty()) {
        qWarning() << "[AdminService] updatePlaylistDateCreated skipped empty itemId";
        co_return;
    }

    qDebug() << "[AdminService] updatePlaylistDateCreated"
             << "| itemId:" << itemId
             << "| dateCreated:" << dateCreatedText;

    QJsonObject itemData = co_await m_serverManager->activeClient()->get(
        QString("/Users/%1/Items/%2").arg(profile.userId, itemId));
    itemData["DateCreated"] = dateCreatedText;

    co_await m_serverManager->activeClient()->post(
        QString("/Items/%1").arg(itemId), itemData);
}

QCoro::Task<QString> AdminService::createPlaylist(const QString& name, const QString& mediaType)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();

    qDebug() << "[AdminService] createPlaylist"
             << "| name:" << name
             << "| mediaType:" << mediaType
             << "| userId:" << profile.userId;

    QJsonObject payload;
    payload["Name"] = name;
    payload["UserId"] = profile.userId;
    payload["MediaType"] = mediaType;

    QJsonObject response = co_await m_serverManager->activeClient()->post("/Playlists", payload);
    const QString createdId = response["Id"].toString();

    qDebug() << "[AdminService] createPlaylist completed"
             << "| id:" << createdId
             << "| name:" << name
             << "| mediaType:" << mediaType;
    co_return createdId;
}

QCoro::Task<QJsonObject> AdminService::getPlaylistItems(QString playlistId)
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();
    QString path = QString("/Playlists/%1/Items?UserId=%2&Fields=PrimaryImageAspectRatio,ProductionYear")
                       .arg(playlistId, profile.userId);
    co_return co_await m_serverManager->activeClient()->get(path);
}

QCoro::Task<void> AdminService::addToPlaylist(QString playlistId, QStringList itemIds)
{
    ensureValidProfile();
    if (playlistId.trimmed().isEmpty() || itemIds.isEmpty()) {
        qWarning() << "[AdminService] addToPlaylist skipped"
                   << "| playlistId:" << playlistId
                   << "| itemCount:" << itemIds.size();
        co_return;
    }

    ServerProfile profile = m_serverManager->activeProfile();
    QString ids = itemIds.join(",");
    QString path = QString("/Playlists/%1/Items?Ids=%2&UserId=%3")
                       .arg(playlistId, ids, profile.userId);

    qDebug() << "[AdminService] addToPlaylist"
             << "| playlistId:" << playlistId
             << "| itemIds:" << itemIds
             << "| userId:" << profile.userId;
    co_await m_serverManager->activeClient()->post(path, QJsonObject());

    qDebug() << "[AdminService] addToPlaylist completed"
             << "| playlistId:" << playlistId
             << "| itemCount:" << itemIds.size();
}

QCoro::Task<void> AdminService::removeFromPlaylist(QString playlistId, QStringList entryIds)
{
    ensureValidProfile();
    QString ids = entryIds.join(",");
    QString path = QString("/Playlists/%1/Items?EntryIds=%2").arg(playlistId, ids);
    co_await m_serverManager->activeClient()->deleteResource(path);
}





QCoro::Task<QJsonArray> AdminService::getUserCollections()
{
    ensureValidProfile();
    ServerProfile profile = m_serverManager->activeProfile();
    QString path = QString("/Users/%1/Items?IncludeItemTypes=BoxSet"
                           "&Recursive=true"
                           "&Fields=ChildCount,PrimaryImageAspectRatio"
                           "&SortBy=SortName&SortOrder=Ascending")
                       .arg(profile.userId);

    QJsonObject response = co_await m_serverManager->activeClient()->get(path);

    QJsonArray items;
    if (response.contains("Items")) {
        items = response["Items"].toArray();
    } else if (response.contains("data") && response["data"].isArray()) {
        items = response["data"].toArray();
    }

    qDebug() << "[AdminService] getUserCollections"
             << "| count:" << items.size();
    co_return items;
}

QCoro::Task<QString> AdminService::createCollection(const QString& name, const QStringList& itemIds)
{
    ensureValidProfile();
    QString path = QString("/Collections?Name=%1").arg(QUrl::toPercentEncoding(name));
    if (!itemIds.isEmpty()) {
        path += QString("&Ids=%1").arg(itemIds.join(","));
    }
    QJsonObject response = co_await m_serverManager->activeClient()->post(path, QJsonObject());
    co_return response["Id"].toString();
}

QCoro::Task<void> AdminService::addToCollection(const QString& collectionId, const QStringList& itemIds)
{
    ensureValidProfile();
    QString ids = itemIds.join(",");
    QString path = QString("/Collections/%1/Items?Ids=%2").arg(collectionId, ids);
    co_await m_serverManager->activeClient()->post(path, QJsonObject());
}

QCoro::Task<void> AdminService::removeFromCollection(const QString& collectionId, const QStringList& itemIds)
{
    ensureValidProfile();
    QString ids = itemIds.join(",");
    QString path = QString("/Collections/%1/Items?Ids=%2").arg(collectionId, ids);
    co_await m_serverManager->activeClient()->deleteResource(path);
}





QCoro::Task<QList<RemoteSearchResult>> AdminService::searchRemoteMetadata(
    QString remoteSearchType, QString itemId, QJsonObject searchInfo,
    QString searchProviderName, bool includeDisabledProviders)
{
    ensureValidProfile();

    remoteSearchType = remoteSearchType.trimmed();
    itemId = itemId.trimmed();
    searchProviderName = searchProviderName.trimmed();

    if (remoteSearchType.isEmpty() || itemId.isEmpty()) {
        qWarning() << "[AdminService] searchRemoteMetadata skipped"
                   << "| remoteSearchType:" << remoteSearchType
                   << "| itemId:" << itemId;
        co_return {};
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("ItemId"), itemId);
    payload.insert(QStringLiteral("SearchInfo"), searchInfo);
    payload.insert(QStringLiteral("IncludeDisabledProviders"),
                   includeDisabledProviders);
    if (!searchProviderName.isEmpty()) {
        payload.insert(QStringLiteral("SearchProviderName"),
                       searchProviderName);
    }

    const QString path =
        QStringLiteral("/Items/RemoteSearch/%1").arg(remoteSearchType);
    const QJsonObject providerIds =
        searchInfo.value(QStringLiteral("ProviderIds")).toObject();
    qDebug() << "[AdminService] searchRemoteMetadata"
             << "| remoteSearchType:" << remoteSearchType
             << "| itemId:" << itemId
             << "| queryName:" << searchInfo.value(QStringLiteral("Name")).toString()
             << "| year:" << searchInfo.value(QStringLiteral("Year")).toInt()
             << "| providerIds:"
             << QString::fromUtf8(
                    QJsonDocument(providerIds).toJson(QJsonDocument::Compact))
             << "| searchProviderName:" << searchProviderName
             << "| includeDisabledProviders:" << includeDisabledProviders;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->post(path, payload);
    const QJsonArray items = extractItemsArray(response);

    QList<RemoteSearchResult> results;
    results.reserve(items.size());
    for (const QJsonValue& value : items) {
        const QJsonObject obj = value.toObject();
        if (obj.isEmpty()) {
            continue;
        }

        results.append(RemoteSearchResult::fromJson(obj));
    }

    qDebug() << "[AdminService] searchRemoteMetadata completed"
             << "| remoteSearchType:" << remoteSearchType
             << "| itemId:" << itemId
             << "| resultCount:" << results.size();
    co_return results;
}

QCoro::Task<QJsonObject> AdminService::getItemMetadata(QString itemId)
{
    ensureValidProfile();

    itemId = itemId.trimmed();
    if (itemId.isEmpty()) {
        qWarning() << "[AdminService] getItemMetadata skipped: empty itemId";
        co_return {};
    }

    const ServerProfile profile = m_serverManager->activeProfile();
    qDebug() << "[AdminService] getItemMetadata"
             << "| userId:" << profile.userId
             << "| itemId:" << itemId;

    const QJsonObject itemData = co_await m_serverManager->activeClient()->get(
        QString("/Users/%1/Items/%2").arg(profile.userId, itemId));

    qDebug() << "[AdminService] getItemMetadata completed"
             << "| itemId:" << itemId
             << "| name:" << itemData.value(QStringLiteral("Name")).toString()
             << "| type:" << itemData.value(QStringLiteral("Type")).toString()
             << "| sortName:"
             << itemData.value(QStringLiteral("SortName")).toString()
             << "| providerIds:"
             << QString::fromUtf8(
                    QJsonDocument(itemData.value(QStringLiteral("ProviderIds"))
                                      .toObject())
                        .toJson(QJsonDocument::Compact))
             << "| lockedFields:"
             << itemData.value(QStringLiteral("LockedFields")).toArray()
             << "| lockData:"
             << itemData.value(QStringLiteral("LockData")).toBool(false);
    co_return itemData;
}

QCoro::Task<void> AdminService::applyRemoteSearchResult(
    QString itemId, RemoteSearchResult result)
{
    ensureValidProfile();

    itemId = itemId.trimmed();
    if (itemId.isEmpty()) {
        qWarning() << "[AdminService] applyRemoteSearchResult skipped: empty itemId";
        co_return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("replaceAllImages"),
                       QStringLiteral("true"));
    const QString path = QStringLiteral("/Items/RemoteSearch/Apply/%1?%2")
                             .arg(itemId, query.toString(QUrl::FullyEncoded));

    const QJsonObject payload = result.toJson();
    qDebug() << "[AdminService] applyRemoteSearchResult"
             << "| itemId:" << itemId
             << "| resultName:" << result.name
             << "| searchProvider:" << result.searchProviderName
             << "| providerIds:" << result.providerIdSummary()
             << "| imageUrl:" << result.imageUrl
             << "| payloadKeys:" << payload.keys()
             << "| path:" << path;

    co_await m_serverManager->activeClient()->post(
        path, payload);
}

QCoro::Task<void> AdminService::updateItemMetadata(const QString& itemId, const QJsonObject& itemData)
{
    ensureValidProfile();
    qDebug() << "[AdminService] updateItemMetadata"
             << "| itemId:" << itemId
             << "| name:" << itemData.value(QStringLiteral("Name")).toString()
             << "| type:" << itemData.value(QStringLiteral("Type")).toString()
             << "| sortName:"
             << itemData.value(QStringLiteral("SortName")).toString()
             << "| providerIds:"
             << QString::fromUtf8(
                    QJsonDocument(itemData.value(QStringLiteral("ProviderIds"))
                                      .toObject())
                        .toJson(QJsonDocument::Compact))
             << "| lockData:"
             << itemData.value(QStringLiteral("LockData")).toBool(false)
             << "| keyCount:" << itemData.keys().size();
    co_await m_serverManager->activeClient()->post(
        QString("/Items/%1").arg(itemId), itemData);
}

QCoro::Task<QList<ItemImageInfo>> AdminService::getItemImages(QString itemId)
{
    ensureValidProfile();

    itemId = itemId.trimmed();
    if (itemId.isEmpty()) {
        qWarning() << "[AdminService] getItemImages skipped: empty itemId";
        co_return {};
    }

    qDebug() << "[AdminService] getItemImages"
             << "| itemId:" << itemId;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(
            QStringLiteral("/Items/%1/Images").arg(itemId));

    QJsonArray imagesArray;
    if (response.contains(QStringLiteral("data")) &&
        response.value(QStringLiteral("data")).isArray()) {
        imagesArray = response.value(QStringLiteral("data")).toArray();
    } else if (response.contains(QStringLiteral("Items")) &&
               response.value(QStringLiteral("Items")).isArray()) {
        imagesArray = response.value(QStringLiteral("Items")).toArray();
    }

    QList<ItemImageInfo> images;
    for (const QJsonValue& value : imagesArray) {
        if (!value.isObject()) {
            continue;
        }
        images.append(ItemImageInfo::fromJson(value.toObject()));
    }

    qDebug() << "[AdminService] getItemImages completed"
             << "| itemId:" << itemId
             << "| count:" << images.size();
    co_return images;
}

QCoro::Task<void> AdminService::refreshItemMetadata(const QString& itemId,
                                                    bool replaceAllMetadata,
                                                    bool replaceAllImages)
{
    ensureValidProfile();

    const QString trimmedItemId = itemId.trimmed();
    if (trimmedItemId.isEmpty()) {
        qWarning() << "[AdminService] refreshItemMetadata skipped: empty itemId";
        co_return;
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("Recursive"), QStringLiteral("true"));
    query.addQueryItem(QStringLiteral("MetadataRefreshMode"),
                       QStringLiteral("FullRefresh"));
    query.addQueryItem(QStringLiteral("ImageRefreshMode"),
                       QStringLiteral("FullRefresh"));
    query.addQueryItem(QStringLiteral("ReplaceAllMetadata"),
                       replaceAllMetadata ? QStringLiteral("true")
                                          : QStringLiteral("false"));
    query.addQueryItem(QStringLiteral("ReplaceAllImages"),
                       replaceAllImages ? QStringLiteral("true")
                                        : QStringLiteral("false"));

    const QString path = QStringLiteral("/Items/%1/Refresh?%2")
                             .arg(trimmedItemId,
                                  query.toString(QUrl::FullyEncoded));

    qDebug() << "[AdminService] refreshItemMetadata"
             << "| itemId:" << trimmedItemId
             << "| replaceAllMetadata:" << replaceAllMetadata
             << "| replaceAllImages:" << replaceAllImages
             << "| path:" << path;

    
    
    co_await m_serverManager->activeClient()->post(path, QJsonObject());
}

QCoro::Task<void> AdminService::uploadItemImage(QString itemId,
                                                QString imageType,
                                                QByteArray imageData,
                                                QString mimeType,
                                                int imageIndex)
{
    ensureValidProfile();

    itemId = itemId.trimmed();
    imageType = imageType.trimmed();
    mimeType = mimeType.trimmed();
    if (itemId.isEmpty() || imageType.isEmpty() || imageData.isEmpty()) {
        qWarning() << "[AdminService] uploadItemImage skipped"
                   << "| itemId:" << itemId
                   << "| imageType:" << imageType
                   << "| imageIndex:" << imageIndex
                   << "| dataSize:" << imageData.size();
        co_return;
    }

    if (mimeType.isEmpty()) {
        mimeType = QStringLiteral("application/octet-stream");
    }

    const QString path = buildItemImagePath(itemId, imageType, imageIndex);
    const ServerProfile profile = m_serverManager->activeProfile();
    const QByteArray encodedBody = imageData.toBase64();

    qDebug() << "[AdminService] uploadItemImage"
             << "| itemId:" << itemId
             << "| imageType:" << imageType
             << "| imageIndex:" << imageIndex
             << "| mimeType:" << mimeType
             << "| dataSize:" << imageData.size()
             << "| base64Size:" << encodedBody.size()
             << "| serverType:"
             << (profile.type == ServerProfile::Jellyfin ? "Jellyfin"
                                                         : "Emby")
             << "| requestBody: raw-base64";

    try {
        qDebug() << "[AdminService] uploadItemImage primary attempt"
                 << "| itemId:" << itemId
                 << "| imageType:" << imageType
                 << "| imageIndex:" << imageIndex
                 << "| contentType: text/plain";
        co_await m_serverManager->activeClient()->postBytes(
            path, encodedBody, QStringLiteral("text/plain"));
        co_return;
    } catch (const std::exception& e) {
        qWarning() << "[AdminService] uploadItemImage primary attempt failed"
                   << "| itemId:" << itemId
                   << "| imageType:" << imageType
                   << "| imageIndex:" << imageIndex
                   << "| contentType: text/plain"
                   << "| error:" << e.what();
    }

    try {
        qWarning() << "[AdminService] uploadItemImage retrying with fallback header"
                   << "| itemId:" << itemId
                   << "| imageType:" << imageType
                   << "| imageIndex:" << imageIndex
                   << "| contentType:" << mimeType;
        co_await m_serverManager->activeClient()->postBytes(
            path, encodedBody, mimeType);

        qWarning() << "[AdminService] uploadItemImage fallback header succeeded"
                   << "| itemId:" << itemId
                   << "| imageType:" << imageType
                   << "| imageIndex:" << imageIndex
                   << "| contentType:" << mimeType;
    } catch (const std::exception& e) {
        qWarning() << "[AdminService] uploadItemImage fallback header failed"
                   << "| itemId:" << itemId
                   << "| imageType:" << imageType
                   << "| imageIndex:" << imageIndex
                   << "| contentType:" << mimeType
                   << "| error:" << e.what();
        throw;
    }
}

QCoro::Task<void> AdminService::deleteItemImage(QString itemId, QString imageType,
                                                int imageIndex)
{
    ensureValidProfile();

    itemId = itemId.trimmed();
    imageType = imageType.trimmed();
    if (itemId.isEmpty() || imageType.isEmpty()) {
        qWarning() << "[AdminService] deleteItemImage skipped"
                   << "| itemId:" << itemId
                   << "| imageType:" << imageType
                   << "| imageIndex:" << imageIndex;
        co_return;
    }

    qDebug() << "[AdminService] deleteItemImage"
             << "| itemId:" << itemId
             << "| imageType:" << imageType
             << "| imageIndex:" << imageIndex;

    co_await m_serverManager->activeClient()->deleteResource(
        buildItemImagePath(itemId, imageType, imageIndex));
}





QCoro::Task<void> AdminService::deleteItem(const QString& itemId)
{
    ensureValidProfile();
    co_await m_serverManager->activeClient()->deleteResource(
        QString("/Items/%1").arg(itemId));
}

QCoro::Task<void> AdminService::renameItem(const QString& itemId, const QString& newName)
{
    ensureValidProfile();
    
    ServerProfile profile = m_serverManager->activeProfile();
    QJsonObject itemData = co_await m_serverManager->activeClient()->get(
        QString("/Users/%1/Items/%2").arg(profile.userId, itemId));
    itemData["Name"] = newName;
    co_await m_serverManager->activeClient()->post(
        QString("/Items/%1").arg(itemId), itemData);
}





QCoro::Task<QJsonObject> AdminService::getEncodingConfiguration()
{
    ensureValidProfile();

    const ServerProfile profile = m_serverManager->activeProfile();
    const QString preferredPath = QStringLiteral("/System/Configuration/encoding");
    const QString fallbackPath = QStringLiteral("/System/Configuration/Encoding");

    qDebug() << "[AdminService] getEncodingConfiguration"
             << "| serverType:"
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| preferredPath:" << preferredPath;

    QJsonObject response;
    bool useFallbackPath = false;
    try {
        response = co_await m_serverManager->activeClient()->get(preferredPath);
    } catch (const std::exception& e) {
        if (profile.type != ServerProfile::Jellyfin) {
            throw;
        }

        qWarning() << "[AdminService] getEncodingConfiguration preferred path failed"
                   << "| serverType: Jellyfin"
                   << "| path:" << preferredPath
                   << "| error:" << QString::fromUtf8(e.what())
                   << "| fallbackPath:" << fallbackPath;
        useFallbackPath = true;
    }

    if (useFallbackPath) {
        response = co_await m_serverManager->activeClient()->get(fallbackPath);
    }

    qDebug() << "[AdminService] Encoding configuration received"
             << "| keyCount=" << response.keys().size()
             << "| keys=" << response.keys();

    co_return response;
}

QCoro::Task<void> AdminService::updateEncodingConfiguration(QJsonObject config)
{
    ensureValidProfile();

    const ServerProfile profile = m_serverManager->activeProfile();
    const QString preferredPath = QStringLiteral("/System/Configuration/encoding");
    const QString fallbackPath = QStringLiteral("/System/Configuration/Encoding");

    qDebug() << "[AdminService] updateEncodingConfiguration"
             << "| serverType:"
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| preferredPath:" << preferredPath
             << "| keyCount=" << config.keys().size()
             << "| keys=" << config.keys();

    bool useFallbackPath = false;
    try {
        co_await m_serverManager->activeClient()->post(preferredPath, config);
    } catch (const std::exception& e) {
        if (profile.type != ServerProfile::Jellyfin) {
            throw;
        }

        qWarning() << "[AdminService] updateEncodingConfiguration preferred path failed"
                   << "| serverType: Jellyfin"
                   << "| path:" << preferredPath
                   << "| error:" << QString::fromUtf8(e.what())
                   << "| fallbackPath:" << fallbackPath;
        useFallbackPath = true;
    }

    if (useFallbackPath) {
        co_await m_serverManager->activeClient()->post(fallbackPath, config);
    }
}

QCoro::Task<QJsonArray> AdminService::getVideoCodecInformation()
{
    ensureValidProfile();

    const ServerProfile profile = m_serverManager->activeProfile();
    const QString path = QStringLiteral("/Encoding/CodecInformation/Video");
    qDebug() << "[AdminService] getVideoCodecInformation"
             << "| serverType:"
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| path:" << path;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);
    const QJsonArray items = extractItemsArray(response);

    qDebug() << "[AdminService] Video codec information received"
             << "| count=" << items.size()
             << "| profileUrl:" << profile.url
             << "| av1Vp9Summary:" << buildCodecDebugSummary(items);
    co_return items;
}

QCoro::Task<QJsonArray> AdminService::getDefaultCodecConfigurations()
{
    ensureValidProfile();

    const ServerProfile profile = m_serverManager->activeProfile();
    const QString path = QStringLiteral("/Encoding/CodecConfiguration/Defaults");
    qDebug() << "[AdminService] getDefaultCodecConfigurations"
             << "| serverType:"
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| path:" << path;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);
    const QJsonArray items = extractItemsArray(response);

    qDebug() << "[AdminService] Default codec configurations received"
             << "| count=" << items.size();
    co_return items;
}

QCoro::Task<QJsonObject> AdminService::getCodecParameters(QString codecId,
                                                          QString parameterContext)
{
    ensureValidProfile();

    const ServerProfile profile = m_serverManager->activeProfile();
    codecId = codecId.trimmed();
    parameterContext = parameterContext.trimmed();
    if (parameterContext.isEmpty()) {
        parameterContext = QStringLiteral("Playback");
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("CodecId"), codecId);
    query.addQueryItem(QStringLiteral("ParameterContext"), parameterContext);
    const QString path =
        QStringLiteral("/Encoding/CodecParameters?") + query.toString(QUrl::FullyEncoded);

    qDebug() << "[AdminService] getCodecParameters"
             << "| serverType:"
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| codecId:" << codecId
             << "| parameterContext:" << parameterContext
             << "| path:" << path;

    const QJsonObject response =
        co_await m_serverManager->activeClient()->get(path);

    qDebug() << "[AdminService] Codec parameters received"
             << "| codecId:" << codecId
             << "| keyCount:" << response.keys().size()
             << "| keys:" << response.keys();
    co_return response;
}

QCoro::Task<void> AdminService::updateCodecParameters(QString codecId,
                                                      QJsonObject parameters,
                                                      QString parameterContext)
{
    ensureValidProfile();

    const ServerProfile profile = m_serverManager->activeProfile();
    codecId = codecId.trimmed();
    parameterContext = parameterContext.trimmed();
    if (parameterContext.isEmpty()) {
        parameterContext = QStringLiteral("Playback");
    }

    QUrlQuery query;
    query.addQueryItem(QStringLiteral("CodecId"), codecId);
    query.addQueryItem(QStringLiteral("ParameterContext"), parameterContext);
    const QString path =
        QStringLiteral("/Encoding/CodecParameters?") + query.toString(QUrl::FullyEncoded);

    qDebug() << "[AdminService] updateCodecParameters"
             << "| serverType:"
             << (profile.type == ServerProfile::Emby ? "Emby" : "Jellyfin")
             << "| codecId:" << codecId
             << "| parameterContext:" << parameterContext
             << "| path:" << path
             << "| keyCount:" << parameters.keys().size()
             << "| keys:" << parameters.keys();

    co_await m_serverManager->activeClient()->post(path, parameters);
}





QCoro::Task<QList<ScheduledTaskInfo>> AdminService::getScheduledTasks()
{
    ensureValidProfile();
    QJsonObject response = co_await m_serverManager->activeClient()->get("/ScheduledTasks");

    QList<ScheduledTaskInfo> tasks;
    if (response.contains("Items")) {
        QJsonArray arr = response["Items"].toArray();
        for (const auto& val : arr) {
            tasks.append(ScheduledTaskInfo::fromJson(val.toObject()));
        }
    } else if (response.contains("data") && response["data"].isArray()) {
        QJsonArray arr = response["data"].toArray();
        for (const auto& val : arr) {
            tasks.append(ScheduledTaskInfo::fromJson(val.toObject()));
        }
    }
    co_return tasks;
}

QCoro::Task<QJsonObject> AdminService::getScheduledTaskById(const QString& taskId)
{
    ensureValidProfile();
    QJsonObject response = co_await m_serverManager->activeClient()->get(
        QString("/ScheduledTasks/%1").arg(taskId));

    qDebug() << "[AdminService] getScheduledTaskById"
             << "| taskId=" << taskId
             << "| triggersCount=" << response["Triggers"].toArray().size();

    co_return response;
}

QCoro::Task<void> AdminService::runScheduledTask(const QString& taskId)
{
    ensureValidProfile();
    co_await m_serverManager->activeClient()->post(
        QString("/ScheduledTasks/Running/%1").arg(taskId), QJsonObject());
}

QCoro::Task<void> AdminService::stopScheduledTask(const QString& taskId)
{
    ensureValidProfile();
    co_await m_serverManager->activeClient()->deleteResource(
        QString("/ScheduledTasks/Running/%1").arg(taskId));
}

QCoro::Task<void> AdminService::updateTaskTriggers(const QString& taskId, const QJsonArray& triggers)
{
    ensureValidProfile();

    qDebug() << "[AdminService] updateTaskTriggers"
             << "| taskId=" << taskId
             << "| triggersCount=" << triggers.size();

    co_await m_serverManager->activeClient()->postArray(
        QString("/ScheduledTasks/%1/Triggers").arg(taskId), triggers);
}





QCoro::Task<QList<LogFileInfo>> AdminService::getLogFiles()
{
    ensureValidProfile();
    QJsonObject response = co_await m_serverManager->activeClient()->get("/System/Logs");

    QList<LogFileInfo> logs;
    if (response.contains("Items")) {
        QJsonArray arr = response["Items"].toArray();
        for (const auto& val : arr) {
            logs.append(LogFileInfo::fromJson(val.toObject()));
        }
    } else if (response.contains("data") && response["data"].isArray()) {
        QJsonArray arr = response["data"].toArray();
        for (const auto& val : arr) {
            logs.append(LogFileInfo::fromJson(val.toObject()));
        }
    }
    co_return logs;
}

QCoro::Task<QString> AdminService::getLogFileContent(const QString& logName)
{
    ensureValidProfile();
    
    QString path = QString("/System/Logs/%1").arg(logName);

    qDebug() << "[AdminService] Fetching log content"
             << "| logName=" << logName
             << "| path=" << path;

    QString content = co_await m_serverManager->activeClient()->getText(path);

    qDebug() << "[AdminService] Log content received"
             << "| logName=" << logName
             << "| contentLength=" << content.length();

    co_return content;
}





QCoro::Task<QList<ActivityLogEntry>> AdminService::getActivityLog(int limit)
{
    ensureValidProfile();
    QString path = QString("/System/ActivityLog/Entries?startIndex=0&limit=%1").arg(limit);
    QJsonObject response = co_await m_serverManager->activeClient()->get(path);

    QList<ActivityLogEntry> entries;
    if (response.contains("Items")) {
        QJsonArray arr = response["Items"].toArray();
        for (const auto& val : arr) {
            entries.append(ActivityLogEntry::fromJson(val.toObject()));
        }
    } else if (response.contains("data") && response["data"].isArray()) {
        QJsonArray arr = response["data"].toArray();
        for (const auto& val : arr) {
            entries.append(ActivityLogEntry::fromJson(val.toObject()));
        }
    }
    co_return entries;
}
