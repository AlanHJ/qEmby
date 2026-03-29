#include "externalplayerdetector.h"
#include <config/config_keys.h>
#include <config/configstore.h>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStandardPaths>
#ifdef Q_OS_WIN
#include <QSettings>
#endif

QList<DetectedPlayer> ExternalPlayerDetector::detect() {
    QList<DetectedPlayer> result;

    auto tryDetect = [&](const QString &name, const QStringList &exeNames,
                         const QStringList &extraDirs = {}) {
        for (const auto &exe : exeNames) {
            QString found = QStandardPaths::findExecutable(exe, extraDirs);
            if (!found.isEmpty()) {
                result.append({name, QDir::toNativeSeparators(found)});
                return;
            }
        }
    };

#ifdef Q_OS_WIN
    auto tryRegistry = [&](const QString &name, const QString &regKey,
                           const QString &valueName,
                           const QString &exeName = {}) {
        QSettings reg(regKey, QSettings::NativeFormat);
        QString val = reg.value(valueName).toString();
        if (val.isEmpty())
            return;
        QString fullPath = val;
        if (!exeName.isEmpty() && QFileInfo(val).isDir()) {
            fullPath = val + "/" + exeName;
        }
        if (QFile::exists(fullPath)) {
            result.append({name, QDir::toNativeSeparators(fullPath)});
        }
    };

    
    tryRegistry(
        "PotPlayer",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App "
        "Paths\\PotPlayerMini64.exe",
        ".", "");
    if (result.isEmpty() || result.last().name != "PotPlayer") {
        tryRegistry("PotPlayer",
                    "HKEY_LOCAL_MACHINE\\SOFTWARE\\DAUM\\PotPlayer64",
                    "ProgramFolder", "PotPlayerMini64.exe");
    }

    
    tryRegistry("VLC", "HKEY_LOCAL_MACHINE\\SOFTWARE\\VideoLAN\\VLC",
                "InstallDir", "vlc.exe");

    
    tryRegistry(
        "MPC-HC",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App "
        "Paths\\mpc-hc64.exe",
        ".", "");
    if (result.isEmpty() || result.last().name != "MPC-HC") {
        tryRegistry("MPC-HC",
                    "HKEY_LOCAL_"
                    "MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App "
                    "Paths\\mpc-hc.exe",
                    ".", "");
    }

    
    tryRegistry(
        "MPC-BE",
        "HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App "
        "Paths\\mpc-be64.exe",
        ".", "");
    if (result.isEmpty() || result.last().name != "MPC-BE") {
        tryRegistry("MPC-BE",
                    "HKEY_LOCAL_"
                    "MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\App "
                    "Paths\\mpc-be.exe",
                    ".", "");
    }
    if (result.isEmpty() || result.last().name != "MPC-BE") {
        QStringList beDirs;
        for (const auto &drive : QDir::drives()) {
            QString d = drive.absolutePath();
            beDirs << d + "Program Files/MPC-BE x64" << d + "Program Files/MPC-BE"
                   << d + "Program Files (x86)/MPC-BE";
        }
        tryDetect("MPC-BE", {"mpc-be64.exe", "mpc-be.exe"}, beDirs);
    }

    
    {
        QString pathMpv = QStandardPaths::findExecutable("mpv.exe");
        if (!pathMpv.isEmpty()) {
            result.append({"MPV", QDir::toNativeSeparators(pathMpv)});
        }
        QSet<QString> foundPaths;
        if (!pathMpv.isEmpty())
            foundPaths.insert(QDir::toNativeSeparators(pathMpv));

        for (const auto &drive : QDir::drives()) {
            QString d = drive.absolutePath();
            QStringList searchRoots = {d + "Program Files",
                                       d + "Program Files (x86)"};
            for (const auto &root : searchRoots) {
                QDir rootDir(root);
                if (!rootDir.exists())
                    continue;
                QStringList mpvDirs = rootDir.entryList({"mpv*"}, QDir::Dirs);
                for (const auto &sub : mpvDirs) {
                    QString exePath = root + "/" + sub + "/mpv.exe";
                    if (QFile::exists(exePath)) {
                        QString native = QDir::toNativeSeparators(exePath);
                        if (!foundPaths.contains(native)) {
                            foundPaths.insert(native);
                            QString label = QString("MPV (%1)").arg(sub);
                            result.append({label, native});
                        }
                    }
                }
            }
        }
    }
#elif defined(Q_OS_MAC)
    tryDetect("MPV", {"mpv"});
    tryDetect("VLC", {"vlc"}, {"/Applications/VLC.app/Contents/MacOS"});
    tryDetect("IINA", {"iina"}, {"/Applications/IINA.app/Contents/MacOS"});
#else
    tryDetect("MPV", {"mpv"});
    tryDetect("VLC", {"vlc"});
    tryDetect("MPC-Qt", {"mpc-qt"});
#endif
    return result;
}

void ExternalPlayerDetector::saveToConfig(const QList<DetectedPlayer> &players) {
    QJsonArray arr;
    for (const auto &p : players) {
        QJsonObject obj;
        obj["name"] = p.name;
        obj["path"] = p.path;
        arr.append(obj);
    }
    ConfigStore::instance()->set(
        ConfigKeys::ExtPlayerDetectedList,
        QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact)));
}

QList<DetectedPlayer> ExternalPlayerDetector::loadFromConfig() {
    QList<DetectedPlayer> result;
    QString jsonStr = ConfigStore::instance()->get<QString>(ConfigKeys::ExtPlayerDetectedList);
    QJsonArray arr = QJsonDocument::fromJson(jsonStr.toUtf8()).array();
    for (const auto &val : arr) {
        QJsonObject obj = val.toObject();
        QString name = obj["name"].toString();
        QString path = obj["path"].toString();
        if (!name.isEmpty() && !path.isEmpty()) {
            result.append({name, path});
        }
    }
    return result;
}
