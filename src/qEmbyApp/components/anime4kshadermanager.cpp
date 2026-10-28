#include "anime4kshadermanager.h"

#include <config/config_keys.h>
#include <config/configstore.h>
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStandardPaths>

namespace {
const QString kShaderResourceRoot = QStringLiteral(":/shaders/anime4k/v4.0.1/");
const QString kShaderVersion = QStringLiteral("4.0.1");
const QString kExtractSubdir = QStringLiteral("anime4k/v4.0.1");
const QString kMarkerFileName = QStringLiteral(".complete");

QString chainToJson(const QStringList &paths) {
    QJsonArray arr;
    for (const QString &path : paths) {
        arr.append(path);
    }
    return QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
}

QStringList chainFromJson(const QString &json) {
    QStringList paths;
    const QJsonDocument doc = QJsonDocument::fromJson(json.toUtf8());
    if (!doc.isArray()) {
        return paths;
    }
    for (const QJsonValue &value : doc.array()) {
        if (value.isString()) {
            const QString path = value.toString();
            if (!path.isEmpty()) {
                paths.append(path);
            }
        }
    }
    return paths;
}

QString readableShaderName(const QString &path) {
    QString name = QFileInfo(path).completeBaseName();
    name.replace(QLatin1Char('_'), QLatin1Char(' '));
    name.replace(QLatin1Char('-'), QLatin1Char(' '));
    return name;
}
}

Anime4KShaderManager &Anime4KShaderManager::instance() {
    static Anime4KShaderManager *s_instance = new Anime4KShaderManager(qApp);
    return *s_instance;
}

Anime4KShaderManager::Anime4KShaderManager(QObject *parent) : QObject(parent) {}

bool Anime4KShaderManager::ensureShadersExtracted() {
    if (m_ready) {
        return true;
    }

    QStringList shaderNames;
    QDirIterator it(kShaderResourceRoot, QDir::Files);
    while (it.hasNext()) {
        it.next();
        shaderNames.append(it.fileName());
    }
    shaderNames.sort();
    if (shaderNames.isEmpty()) {
        qWarning() << "[Anime4KShaderManager] ensureShadersExtracted failed"
                   << "| error: no shader resources found"
                   << "| resourceDir:" << kShaderResourceRoot;
        return false;
    }

    const QString targetDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QLatin1Char('/') + kExtractSubdir;
    const QString markerPath = targetDirPath + QLatin1Char('/') + kMarkerFileName;

    QFile markerFile(markerPath);
    if (markerFile.open(QIODevice::ReadOnly)) {
        const QString markerVersion = QString::fromUtf8(markerFile.readAll()).trimmed();
        markerFile.close();
        if (markerVersion == kShaderVersion) {
            bool allPresent = true;
            for (const QString &name : shaderNames) {
                if (!QFile::exists(targetDirPath + QLatin1Char('/') + name)) {
                    allPresent = false;
                    break;
                }
            }
            if (allPresent) {
                m_ready = true;
                return true;
            }
        }
    }

    if (!QDir().mkpath(targetDirPath)) {
        qWarning() << "[Anime4KShaderManager] ensureShadersExtracted failed"
                   << "| error: cannot create directory"
                   << "| targetDir:" << targetDirPath;
        return false;
    }

    for (const QString &name : shaderNames) {
        const QString targetPath = targetDirPath + QLatin1Char('/') + name;
        QFile::remove(targetPath);
        if (!QFile::copy(kShaderResourceRoot + name, targetPath)) {
            qWarning() << "[Anime4KShaderManager] ensureShadersExtracted failed"
                       << "| error: cannot extract shader"
                       << "| shader:" << name
                       << "| targetPath:" << targetPath;
            QFile::remove(markerPath);
            return false;
        }
    }

    QFile markerOut(markerPath);
    if (!markerOut.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qWarning() << "[Anime4KShaderManager] ensureShadersExtracted failed"
                   << "| error: cannot write marker file"
                   << "| markerPath:" << markerPath;
        return false;
    }
    markerOut.write(kShaderVersion.toUtf8());
    markerOut.write("\n");
    markerOut.close();

    m_ready = true;
    return true;
}

bool Anime4KShaderManager::isReady() const {
    return m_ready;
}

QStringList Anime4KShaderManager::shaderChain(Preset preset) const {
    if (!m_ready) {
        qWarning() << "[Anime4KShaderManager] shaderChain requested before shaders were extracted"
                   << "| preset:" << presetId(preset);
        return {};
    }

    QStringList names;
    switch (preset) {
    case Preset::Fast:
        names = {QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
                 QStringLiteral("Anime4K_Restore_CNN_S.glsl"),
                 QStringLiteral("Anime4K_Upscale_CNN_x2_S.glsl")};
        break;
    case Preset::Balanced:
        names = {QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
                 QStringLiteral("Anime4K_Restore_CNN_M.glsl"),
                 QStringLiteral("Anime4K_Upscale_CNN_x2_M.glsl")};
        break;
    case Preset::Quality:
        names = {QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
                 QStringLiteral("Anime4K_Restore_CNN_L.glsl"),
                 QStringLiteral("Anime4K_Upscale_CNN_x2_L.glsl")};
        break;
    }

    const QString targetDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QLatin1Char('/') + kExtractSubdir;
    QStringList paths;
    paths.reserve(names.size());
    for (const QString &name : names) {
        paths.append(targetDirPath + QLatin1Char('/') + name);
    }
    return paths;
}

QString Anime4KShaderManager::presetId(Preset preset) {
    switch (preset) {
    case Preset::Fast:
        return QStringLiteral("fast");
    case Preset::Balanced:
        return QStringLiteral("balanced");
    case Preset::Quality:
        return QStringLiteral("quality");
    }
    return QStringLiteral("balanced");
}

Anime4KShaderManager::Preset Anime4KShaderManager::presetFromId(const QString &id, Preset fallback) {
    const QString normalized = id.trimmed().toLower();
    if (normalized == QStringLiteral("fast")) {
        return Preset::Fast;
    }
    if (normalized == QStringLiteral("balanced")) {
        return Preset::Balanced;
    }
    if (normalized == QStringLiteral("quality")) {
        return Preset::Quality;
    }
    return fallback;
}

QString Anime4KShaderManager::presetDisplayName(Preset preset) {
    switch (preset) {
    case Preset::Fast:
        return tr("Fast");
    case Preset::Balanced:
        return tr("Balanced");
    case Preset::Quality:
        return tr("Quality");
    }
    return tr("Balanced");
}

QList<Anime4KShaderManager::Preset> Anime4KShaderManager::availablePresets() {
    return {Preset::Fast, Preset::Balanced, Preset::Quality};
}

QString Anime4KShaderManager::presetIdOfCustom() {
    return QStringLiteral("custom");
}

bool Anime4KShaderManager::isCustomPresetId(const QString &id) {
    return id.trimmed().toLower() == QStringLiteral("custom");
}

QStringList Anime4KShaderManager::configuredChain() const {
    const QStringList stored =
        chainFromJson(ConfigStore::instance()->get<QString>(
            ConfigKeys::PlayerAnime4kChain));
    if (!stored.isEmpty()) {
        return stored;
    }
    return shaderChain(presetFromId(ConfigStore::instance()->get<QString>(
        ConfigKeys::PlayerAnime4kPreset, QStringLiteral("balanced"))));
}

void Anime4KShaderManager::setConfiguredChain(const QStringList &paths) {
    ConfigStore::instance()->set(ConfigKeys::PlayerAnime4kChain,
                                 chainToJson(paths));
    ConfigStore::instance()->set(ConfigKeys::PlayerAnime4kPreset,
                                 presetIdOfCustom());
}

void Anime4KShaderManager::applyPresetTemplate(Preset preset) {
    ConfigStore::instance()->set(ConfigKeys::PlayerAnime4kChain,
                                 chainToJson(shaderChain(preset)));
    ConfigStore::instance()->set(ConfigKeys::PlayerAnime4kPreset,
                                 presetId(preset));
}

QList<Anime4KShaderManager::ShaderEntry> Anime4KShaderManager::shaderCatalog() const {
    struct BundledShaderInfo {
        const char *fileName;
        const char *displayName;
    };
    static const BundledShaderInfo kBundledShaders[] = {
        {"Anime4K_Clamp_Highlights.glsl", QT_TR_NOOP("Anime4K Clamp Highlights")},
        {"Anime4K_Restore_CNN_S.glsl", QT_TR_NOOP("Anime4K Restore CNN S")},
        {"Anime4K_Restore_CNN_M.glsl", QT_TR_NOOP("Anime4K Restore CNN M")},
        {"Anime4K_Restore_CNN_L.glsl", QT_TR_NOOP("Anime4K Restore CNN L")},
        {"Anime4K_Upscale_CNN_x2_S.glsl", QT_TR_NOOP("Anime4K Upscale CNN x2 S")},
        {"Anime4K_Upscale_CNN_x2_M.glsl", QT_TR_NOOP("Anime4K Upscale CNN x2 M")},
        {"Anime4K_Upscale_CNN_x2_L.glsl", QT_TR_NOOP("Anime4K Upscale CNN x2 L")},
    };

    const QString targetDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QLatin1Char('/') + kExtractSubdir;

    QList<ShaderEntry> catalog;
    for (const BundledShaderInfo &info : kBundledShaders) {
        ShaderEntry entry;
        entry.path = targetDirPath + QLatin1Char('/') + QString::fromLatin1(info.fileName);
        entry.displayName = tr(info.displayName);
        entry.bundled = true;
        catalog.append(entry);
    }

    for (const QString &path : configuredChain()) {
        bool listed = false;
        for (const ShaderEntry &entry : catalog) {
            if (entry.path == path) {
                listed = true;
                break;
            }
        }
        if (listed) {
            continue;
        }
        ShaderEntry entry;
        entry.path = path;
        entry.displayName = readableShaderName(path);
        entry.bundled = false;
        catalog.append(entry);
    }
    return catalog;
}
