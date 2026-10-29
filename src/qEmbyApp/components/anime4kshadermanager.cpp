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
#include <algorithm>

namespace {
const QString kShaderResourceRoot = QStringLiteral(":/shaders/anime4k/v4.0.1/");
const QString kShaderVersion = QStringLiteral("4.0.1");
const QString kExtractSubdir = QStringLiteral("anime4k/v4.0.1");
const QString kCustomSubdir = QStringLiteral("anime4k/custom");
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

// Variant rank for size/quality suffixes, ordered from fastest to highest
// quality (S < M < L < VL < UL, VeryFast < Fast < HQ).
int variantRank(const QString &fileName) {
    static const struct {
        const char *suffix;
        int rank;
    } kVariants[] = {
        {"_UL", 4}, {"_VL", 3}, {"_L", 2}, {"_M", 1}, {"_S", 0},
        {"VeryFast", 0}, {"Fast", 1}, {"HQ", 2},
    };
    const QString baseName = QFileInfo(fileName).completeBaseName();
    for (const auto &variant : kVariants) {
        if (baseName.endsWith(QLatin1String(variant.suffix))) {
            return variant.rank;
        }
    }
    return 9;
}

// Stable catalog order for the bundled shaders: grouped by function in
// pipeline order (clamp -> restore -> denoise -> deblur -> thin -> upscale ->
// darken -> auto-downscale), then by variant from fastest to highest quality,
// with the file name as the final tiebreaker.
struct BundledOrder {
    int group = 9;
    int sub = 0;
    int variant = 9;
    QString fileName;

    bool operator<(const BundledOrder &other) const {
        if (group != other.group) {
            return group < other.group;
        }
        if (sub != other.sub) {
            return sub < other.sub;
        }
        if (variant != other.variant) {
            return variant < other.variant;
        }
        return fileName < other.fileName;
    }
};

BundledOrder bundledOrder(const QString &fileName) {
    BundledOrder order;
    order.fileName = fileName;
    if (fileName == QLatin1String("Anime4K_Clamp_Highlights.glsl")) {
        order.group = 0;
        return order;
    }
    if (fileName.startsWith(QLatin1String("Anime4K_Restore_CNN_Soft_"))) {
        order.group = 1;
        order.sub = 1;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Restore_CNN_"))) {
        order.group = 1;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Denoise_"))) {
        order.group = 2;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Deblur_DoG"))) {
        order.group = 3;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Deblur_"))) {
        order.group = 3;
        order.sub = 1;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Thin_"))) {
        order.group = 4;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Upscale_CNN_x2_"))) {
        order.group = 5;
    } else if (fileName.startsWith(
                   QLatin1String("Anime4K_Upscale_Denoise_CNN_x2_"))) {
        order.group = 5;
        order.sub = 1;
    } else if (fileName.startsWith(
                   QLatin1String("Anime4K_Upscale_Deblur_DoG_x2"))) {
        order.group = 5;
        order.sub = 2;
    } else if (fileName.startsWith(
                   QLatin1String("Anime4K_Upscale_Deblur_Original_x2"))) {
        order.group = 5;
        order.sub = 3;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Upscale_DoG_x2"))) {
        order.group = 5;
        order.sub = 4;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Upscale_DTD_x2"))) {
        order.group = 5;
        order.sub = 5;
    } else if (fileName.startsWith(
                   QLatin1String("Anime4K_Upscale_Original_x2"))) {
        order.group = 5;
        order.sub = 6;
    } else if (fileName.startsWith(QLatin1String("Anime4K_Darken_"))) {
        order.group = 6;
    } else if (fileName.startsWith(QLatin1String("Anime4K_AutoDownscalePre_"))) {
        order.group = 7;
    }
    if (order.variant == 9) {
        order.variant = variantRank(fileName);
    }
    return order;
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
    const QString storedPreset = ConfigStore::instance()->get<QString>(
        ConfigKeys::PlayerAnime4kPreset, QStringLiteral("balanced"));
    if (isCustomPresetId(storedPreset)) {
        // Custom mode: the stored chain is authoritative, even when empty.
        return chainFromJson(ConfigStore::instance()->get<QString>(
            ConfigKeys::PlayerAnime4kChain));
    }
    return shaderChain(presetFromId(storedPreset));
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
    const QString targetDirPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QLatin1Char('/') + kExtractSubdir;

    QList<ShaderEntry> catalog;

    QStringList bundledNames;
    QDirIterator it(kShaderResourceRoot, QDir::Files);
    while (it.hasNext()) {
        it.next();
        if (it.fileName().endsWith(QLatin1String(".glsl"))) {
            bundledNames.append(it.fileName());
        }
    }
    std::sort(bundledNames.begin(), bundledNames.end(),
              [](const QString &left, const QString &right) {
                  return bundledOrder(left) < bundledOrder(right);
              });
    for (const QString &name : bundledNames) {
        ShaderEntry entry;
        entry.path = targetDirPath + QLatin1Char('/') + name;
        entry.displayName = readableShaderName(name);
        entry.bundled = true;
        catalog.append(entry);
    }

    QDir customDir(customShaderDir());
    const QStringList customNames =
        customDir.entryList({QStringLiteral("*.glsl")}, QDir::Files, QDir::Name);
    for (const QString &name : customNames) {
        ShaderEntry entry;
        entry.path = customDir.filePath(name);
        entry.displayName = readableShaderName(name);
        entry.bundled = false;
        catalog.append(entry);
    }
    return catalog;
}

QString Anime4KShaderManager::customShaderDir() {
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QLatin1Char('/') + kCustomSubdir;
}

QString Anime4KShaderManager::importCustomShader(const QString &srcPath) {
    const QFileInfo srcInfo(srcPath);
    if (!srcInfo.exists() || !srcInfo.isFile()) {
        qWarning() << "[Anime4KShaderManager] importCustomShader failed"
                   << "| error: source file not found"
                   << "| srcPath:" << srcPath;
        return QString();
    }

    const QString dirPath = customShaderDir();
    if (!QDir().mkpath(dirPath)) {
        qWarning() << "[Anime4KShaderManager] importCustomShader failed"
                   << "| error: cannot create directory"
                   << "| targetDir:" << dirPath;
        return QString();
    }

    const QString baseName = srcInfo.completeBaseName();
    const QString suffix = srcInfo.suffix();
    QString fileName = srcInfo.fileName();
    QString targetPath = dirPath + QLatin1Char('/') + fileName;
    for (int index = 1; QFile::exists(targetPath); ++index) {
        fileName = baseName + QLatin1Char('_') + QString::number(index);
        if (!suffix.isEmpty()) {
            fileName += QLatin1Char('.') + suffix;
        }
        targetPath = dirPath + QLatin1Char('/') + fileName;
    }

    if (!QFile::copy(srcPath, targetPath)) {
        qWarning() << "[Anime4KShaderManager] importCustomShader failed"
                   << "| error: cannot copy shader"
                   << "| srcPath:" << srcPath
                   << "| targetPath:" << targetPath;
        return QString();
    }
    return targetPath;
}

bool Anime4KShaderManager::removeCustomShader(const QString &path) {
    const QFileInfo info(path);
    const QString dirPath = QDir::cleanPath(customShaderDir());
    if (QDir::cleanPath(info.absolutePath()) != dirPath) {
        qWarning() << "[Anime4KShaderManager] removeCustomShader failed"
                   << "| error: path outside custom shader directory"
                   << "| path:" << path
                   << "| customShaderDir:" << dirPath;
        return false;
    }
    return QFile::remove(info.absoluteFilePath());
}
