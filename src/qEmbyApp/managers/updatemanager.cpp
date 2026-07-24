#include "updatemanager.h"

#include "config/config_keys.h"
#include "config/configstore.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QStringList>
#include <QVersionNumber>

namespace {

const QUrl kLatestReleaseApi(
    QStringLiteral("https://api.github.com/repos/AlanHJ/qEmby/releases/latest"));

QString normalizedVersion(QString version)
{
    version = version.trimmed();
    if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        version.remove(0, 1);
    }
    const qsizetype suffixPos = version.indexOf(QRegularExpression(QStringLiteral("[-+]")));
    return suffixPos >= 0 ? version.left(suffixPos) : version;
}

struct SelectedReleaseAsset {
    QUrl url;
    QString name;
    QString sha256Digest;
    qint64 size = 0;
};

SelectedReleaseAsset platformReleaseAsset(const QJsonArray &assets)
{
#if defined(Q_OS_WIN)
    const QStringList preferredExtensions = {QStringLiteral(".exe"),
                                              QStringLiteral(".msi")};
#elif defined(Q_OS_MACOS) || defined(Q_OS_MAC)
    const QStringList preferredExtensions = {QStringLiteral(".dmg"),
                                              QStringLiteral(".pkg")};
#else
    const QStringList preferredExtensions = {QStringLiteral(".appimage"),
                                              QStringLiteral(".deb"),
                                              QStringLiteral(".rpm")};
#endif

    for (const QString &extension : preferredExtensions) {
        for (const QJsonValue &value : assets) {
            const QJsonObject asset = value.toObject();
            const QString name = asset.value(QStringLiteral("name")).toString();
            if (!name.endsWith(extension, Qt::CaseInsensitive)) {
                continue;
            }
            const QUrl url(asset.value(QStringLiteral("browser_download_url")).toString());
            if (url.isValid() && url.scheme() == QStringLiteral("https")) {
                QString digest =
                    asset.value(QStringLiteral("digest")).toString().trimmed();
                if (digest.startsWith(QStringLiteral("sha256:"),
                                      Qt::CaseInsensitive)) {
                    digest.remove(0, 7);
                }
                return {url, name, digest.toLower(),
                        asset.value(QStringLiteral("size")).toInteger()};
            }
        }
    }
    return {};
}

} 

UpdateManager *UpdateManager::instance()
{
    static auto *manager = new UpdateManager(QCoreApplication::instance());
    return manager;
}

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent), m_networkManager(new QNetworkAccessManager(this))
{
}

bool UpdateManager::isChecking(CheckMode mode) const
{
    return mode == CheckMode::Automatic ? m_automaticCheckInProgress
                                        : m_manualCheckInProgress;
}

void UpdateManager::checkForUpdates(CheckMode mode)
{
    bool &inProgress = mode == CheckMode::Automatic
                           ? m_automaticCheckInProgress
                           : m_manualCheckInProgress;
    if (inProgress) {
        qInfo() << "UpdateManager: check already in progress"
                << "| mode="
                << (mode == CheckMode::Automatic ? "automatic" : "manual");
        return;
    }

    inProgress = true;
    qInfo() << "UpdateManager: checking GitHub release"
            << "| mode=" << (mode == CheckMode::Automatic ? "automatic" : "manual")
            << "| currentVersion=" << QCoreApplication::applicationVersion();

    QNetworkRequest request(kLatestReleaseApi);
    request.setHeader(QNetworkRequest::UserAgentHeader,
                      QStringLiteral("qEmby/%1")
                          .arg(QCoreApplication::applicationVersion()));
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setRawHeader("X-GitHub-Api-Version", "2026-03-10");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply *reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, mode]() { handleReply(reply, mode); });
}

void UpdateManager::handleReply(QNetworkReply *reply, CheckMode mode)
{
    if (mode == CheckMode::Automatic) {
        m_automaticCheckInProgress = false;
    } else {
        m_manualCheckInProgress = false;
    }
    const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QByteArray payload = reply->readAll();
    const QNetworkReply::NetworkError networkError = reply->error();
    const QString networkErrorText = reply->errorString();
    reply->deleteLater();

    
    if (status == 404) {
        qInfo() << "UpdateManager: repository has no published release";
        Q_EMIT noUpdateAvailable(mode);
        return;
    }
    if (networkError != QNetworkReply::NoError) {
        qWarning() << "UpdateManager: release request failed"
                   << "| status=" << status << "| error=" << networkErrorText;
        Q_EMIT checkFailed(tr("Could not connect to GitHub: %1").arg(networkErrorText),
                           mode);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        qWarning() << "UpdateManager: invalid GitHub response"
                   << "| parseError=" << parseError.errorString();
        Q_EMIT checkFailed(tr("GitHub returned invalid update information."), mode);
        return;
    }

    const QJsonObject release = document.object();
    const QString tag = release.value(QStringLiteral("tag_name")).toString();
    const QString remoteVersionText = normalizedVersion(tag);
    const QString localVersionText =
        normalizedVersion(QCoreApplication::applicationVersion());
    const QVersionNumber remoteVersion = QVersionNumber::fromString(remoteVersionText);
    const QVersionNumber localVersion = QVersionNumber::fromString(localVersionText);
    if (remoteVersion.isNull() || localVersion.isNull()) {
        qWarning() << "UpdateManager: invalid version"
                   << "| local=" << localVersionText << "| remote=" << tag;
        Q_EMIT checkFailed(tr("The release contains an invalid version number: %1")
                               .arg(tag),
                           mode);
        return;
    }

    qInfo() << "UpdateManager: release response parsed"
            << "| local=" << localVersionText << "| remote=" << remoteVersionText
            << "| assets=" << release.value(QStringLiteral("assets")).toArray().size();
    if (QVersionNumber::compare(remoteVersion, localVersion) <= 0) {
        Q_EMIT noUpdateAvailable(mode);
        return;
    }

    UpdateInfo info;
    info.version = tag.trimmed().isEmpty() ? remoteVersionText : tag.trimmed();
    info.name = release.value(QStringLiteral("name")).toString().trimmed();
    info.releaseNotes = release.value(QStringLiteral("body")).toString().trimmed();
    info.publishedAt = QDateTime::fromString(
        release.value(QStringLiteral("published_at")).toString(), Qt::ISODate);
    info.releaseUrl = QUrl(release.value(QStringLiteral("html_url")).toString());
    const SelectedReleaseAsset selectedAsset = platformReleaseAsset(
        release.value(QStringLiteral("assets")).toArray());
    info.downloadUrl = selectedAsset.url;
    info.assetName = selectedAsset.name;
    info.sha256Digest = selectedAsset.sha256Digest;
    info.assetSize = selectedAsset.size;

    const QString ignoredVersion = ConfigStore::instance()->get<QString>(
        ConfigKeys::IgnoredUpdateVersion, QString());
    if (mode == CheckMode::Automatic && ignoredVersion == info.version) {
        qInfo() << "UpdateManager: release ignored by user"
                << "| version=" << info.version;
        return;
    }
    if (mode == CheckMode::Automatic && m_openedVersion == info.version) {
        qInfo() << "UpdateManager: release already opened this session"
                << "| version=" << info.version;
        return;
    }

    Q_EMIT updateAvailable(info, mode);
}

bool UpdateManager::openUpdate(const UpdateInfo &info)
{
    const QUrl target = info.downloadUrl.isValid() ? info.downloadUrl
                                                    : info.releaseUrl;
    if (!target.isValid() || target.scheme() != QStringLiteral("https")) {
        qWarning() << "UpdateManager: refusing invalid update URL" << target;
        return false;
    }

    qInfo() << "UpdateManager: opening update"
            << "| version=" << info.version
            << "| directAsset=" << info.downloadUrl.isValid();
    const bool opened = QDesktopServices::openUrl(target);
    if (opened) {
        m_openedVersion = info.version;
        Q_EMIT updateOpened(info.version);
    }
    return opened;
}
