#include "anime4kshadermanager.h"

#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QStandardPaths>

namespace {
const QString kShaderResourceRoot = QStringLiteral(":/shaders/anime4k/v4.0.1/");
const QString kShaderVersion = QStringLiteral("4.0.1");
const QString kExtractSubdir = QStringLiteral("anime4k/v4.0.1");
const QString kMarkerFileName = QStringLiteral(".complete");
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
