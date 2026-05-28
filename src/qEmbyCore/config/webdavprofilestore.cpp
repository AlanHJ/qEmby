#include "webdavprofilestore.h"

#include "../utils/securesecretbox.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace
{

constexpr int kSchemaVersion = 1;

QString defaultFilePath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    return QDir(dir).filePath(QStringLiteral("webdav-profile.json"));
}



QString encryptPasswordToCipherText(const QString &plainPassword)
{
    if (plainPassword.isEmpty())
    {
        return QString();
    }
    QByteArray pwBytes = plainPassword.toUtf8();
    const QByteArray cipher = SecureSecretBox::encryptLocalSecret(pwBytes);
    SecureSecretBox::secureZero(pwBytes);
    if (cipher.isEmpty())
    {
        return QString();
    }
    return QString::fromLatin1(cipher.toBase64(QByteArray::Base64Encoding));
}


QString decryptCipherTextToPassword(const QString &cipherBase64)
{
    if (cipherBase64.isEmpty())
    {
        return QString();
    }
    const QByteArray cipher = QByteArray::fromBase64(cipherBase64.toLatin1(),
                                                     QByteArray::Base64Encoding);
    if (cipher.isEmpty())
    {
        return QString();
    }
    auto plain = SecureSecretBox::decryptLocalSecret(cipher);
    if (!plain.has_value())
    {
        return QString();
    }
    return QString::fromUtf8(plain.value());
}

} 



WebdavProfileStore::WebdavProfileStore(QObject *parent)
    : QObject(parent)
{
}

WebdavProfileStore::~WebdavProfileStore() = default;

QString WebdavProfileStore::filePath()
{
    return defaultFilePath();
}

bool WebdavProfileStore::hasProfile() const
{
    return m_hasProfile;
}

WebdavProfile WebdavProfileStore::profile() const
{
    return m_profile;
}

void WebdavProfileStore::setProfile(WebdavProfile profile)
{
    profile.normalize();
    m_profile = std::move(profile);
    m_hasProfile = m_profile.isValid();
    emit profileChanged();
}

bool WebdavProfileStore::load()
{
    const QString path = filePath();
    QFileInfo info(path);

    if (!info.exists())
    {
        qDebug() << "[WebdavProfileStore] no profile file yet at" << path;
        m_profile = WebdavProfile::defaults();
        m_hasProfile = false;
        emit profileChanged();
        return true;
    }

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
    {
        qWarning() << "[WebdavProfileStore] failed to open for read:" << path
                   << "|" << file.errorString();
        return false;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    QJsonParseError parseError{};
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    {
        qWarning() << "[WebdavProfileStore] JSON parse failed:" << parseError.errorString()
                   << "| path:" << path;
        return false;
    }

    const QJsonObject root = doc.object();
    const int version = root.value(QStringLiteral("version")).toInt(0);
    if (version != kSchemaVersion)
    {
        qWarning() << "[WebdavProfileStore] unsupported schema version" << version
                   << "(expected" << kSchemaVersion << ")";
        return false;
    }

    const QJsonObject profileObj = root.value(QStringLiteral("profile")).toObject();
    if (profileObj.isEmpty())
    {
        qDebug() << "[WebdavProfileStore] empty profile section, treating as no profile";
        m_profile = WebdavProfile::defaults();
        m_hasProfile = false;
        emit profileChanged();
        return true;
    }

    WebdavProfile loaded = WebdavProfile::fromJson(profileObj);

    
    const QString cipherBase64 =
        profileObj.value(QStringLiteral("passwordCipher")).toString();
    if (!cipherBase64.isEmpty())
    {
        const QString plain = decryptCipherTextToPassword(cipherBase64);
        if (plain.isEmpty())
        {
            qWarning() << "[WebdavProfileStore] failed to decrypt passwordCipher;"
                       << "profile loaded WITHOUT password";
            loaded.password.clear();
        }
        else
        {
            loaded.password = plain;
        }
    }
    else
    {
        
        
    }

    loaded.normalize();
    m_profile = loaded;
    m_hasProfile = m_profile.isValid();

    qDebug() << "[WebdavProfileStore] loaded profile"
             << "| hasProfile:" << m_hasProfile
             << "| baseUrl:" << m_profile.baseUrl
             << "| user:" << m_profile.username
             << "| rootDir:" << m_profile.rootDir;

    emit profileChanged();
    return true;
}

bool WebdavProfileStore::save()
{
    const QString path = filePath();
    const QString dirPath = QFileInfo(path).absolutePath();
    if (!QDir().mkpath(dirPath))
    {
        qWarning() << "[WebdavProfileStore] failed to create dir:" << dirPath;
        return false;
    }

    if (!m_hasProfile && !m_profile.isValid())
    {
        
        return clear();
    }

    WebdavProfile snapshot = m_profile;
    snapshot.normalize();

    QJsonObject profileObj = snapshot.toJson();
    profileObj.remove(QStringLiteral("password")); 

    const QString cipherBase64 = encryptPasswordToCipherText(snapshot.password);
    if (!cipherBase64.isEmpty())
    {
        profileObj.insert(QStringLiteral("passwordCipher"), cipherBase64);
    }
    else if (!snapshot.password.isEmpty())
    {
        qWarning() << "[WebdavProfileStore] password encryption failed; aborting save";
        return false;
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), kSchemaVersion);
    root.insert(QStringLiteral("profile"), profileObj);

    const QByteArray payload =
        QJsonDocument(root).toJson(QJsonDocument::Indented);

    
    const QString tmpPath = path + QStringLiteral(".tmp");
    {
        QFile out(tmpPath);
        if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate))
        {
            qWarning() << "[WebdavProfileStore] failed to open for write:" << tmpPath
                       << "|" << out.errorString();
            return false;
        }
        if (out.write(payload) != payload.size())
        {
            qWarning() << "[WebdavProfileStore] short write:" << tmpPath;
            out.close();
            QFile::remove(tmpPath);
            return false;
        }
        out.close();
    }

    QFile::remove(path); 
    if (!QFile::rename(tmpPath, path))
    {
        qWarning() << "[WebdavProfileStore] rename" << tmpPath << "to" << path
                   << "failed; falling back to copy";
        if (!QFile::copy(tmpPath, path))
        {
            qWarning() << "[WebdavProfileStore] copy fallback failed too";
            return false;
        }
        QFile::remove(tmpPath);
    }

    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner);

    qDebug() << "[WebdavProfileStore] saved profile to" << path
             << "| size:" << payload.size();
    return true;
}

bool WebdavProfileStore::clear()
{
    const QString path = filePath();
    bool ok = true;
    if (QFile::exists(path))
    {
        if (!QFile::remove(path))
        {
            qWarning() << "[WebdavProfileStore] failed to remove" << path;
            ok = false;
        }
    }

    m_profile = WebdavProfile::defaults();
    m_hasProfile = false;
    emit profileChanged();
    return ok;
}

bool WebdavProfileStore::updateLastSyncAt(qint64 unixMs)
{
    if (!m_hasProfile)
    {
        return false;
    }
    m_profile.lastSyncAt = unixMs;
    return save();
}
