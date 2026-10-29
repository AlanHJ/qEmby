#include "gpudevicemanager.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QtGlobal>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <string>
#endif

namespace {

#ifdef Q_OS_LINUX
QString vendorDisplayName(const QString &vendorId) {
  const QString vid = vendorId.trimmed().toLower();
  if (vid == QLatin1String("0x10de")) {
    return QStringLiteral("NVIDIA");
  }
  if (vid == QLatin1String("0x1002")) {
    return QStringLiteral("AMD");
  }
  if (vid == QLatin1String("0x8086")) {
    return QStringLiteral("Intel");
  }
  return QStringLiteral("Unknown");
}

QString readSysfsValue(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return QString();
  }
  return QString::fromLatin1(file.readAll()).trimmed();
}
#endif

#ifdef Q_OS_WIN
QList<GpuDevice> g_enumeratedDevices;
#endif

} // namespace

QList<GpuDevice> GpuDeviceManager::availableDevices() {
  QList<GpuDevice> devices;

#ifdef Q_OS_LINUX
  // 扫描 /sys/class/drm/card*（仅 cardN 本体，不含 cardN-xxx 输出节点），
  // id 取 device 符号链接 basename 转换成的 pci-XXXX_XX_XX_X 路径。
  static const QRegularExpression cardPattern(QStringLiteral("^card\\d+$"));
  const QDir drmDir(QStringLiteral("/sys/class/drm"));
  QStringList cards =
      drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
  for (const QString &card : cards) {
    if (!cardPattern.match(card).hasMatch()) {
      continue;
    }
    const QFileInfo deviceInfo(
        drmDir.filePath(card + QStringLiteral("/device")));
    const QString target = deviceInfo.symLinkTarget();
    if (target.isEmpty()) {
      continue;
    }

    QString id = QFileInfo(target).fileName(); // 例如 0000:01:00.0
    if (id.isEmpty()) {
      continue;
    }
    if (!id.startsWith(QLatin1String("pci-"))) {
      id.replace(QLatin1Char(':'), QLatin1Char('_'));
      id.replace(QLatin1Char('.'), QLatin1Char('_'));
      id.prepend(QStringLiteral("pci-"));
    }

    const QString vendorId =
        readSysfsValue(target + QStringLiteral("/vendor"));
    const QString deviceId =
        readSysfsValue(target + QStringLiteral("/device"));
    QString name = vendorDisplayName(vendorId);
    if (!vendorId.isEmpty() || !deviceId.isEmpty()) {
      name += QStringLiteral(" (%1:%2)").arg(vendorId, deviceId);
    }
    devices.append({id, name});
  }
#elif defined(Q_OS_WIN)
  QProcess proc;
  proc.start(QStringLiteral("powershell"),
             {QStringLiteral("-NoProfile"), QStringLiteral("-Command"),
              QStringLiteral("Get-CimInstance Win32_VideoController | "
                             "Select-Object Name,AdapterCompatibility | "
                             "ConvertTo-Json")});
  if (!proc.waitForFinished(3000)) {
    proc.kill();
    proc.waitForFinished(500);
    return devices;
  }
  if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
    return devices;
  }

  const QByteArray raw = proc.readAllStandardOutput();
  QJsonDocument doc = QJsonDocument::fromJson(raw);
  if (doc.isNull()) {
    doc = QJsonDocument::fromJson(QString::fromLocal8Bit(raw).toUtf8());
  }

  const auto parseEntry = [](const QJsonObject &obj, int index) {
    QString display = obj.value(QStringLiteral("Name")).toString().trimmed();
    if (display.isEmpty()) {
      display =
          obj.value(QStringLiteral("AdapterCompatibility")).toString().trimmed();
    }
    if (display.isEmpty()) {
      display = QStringLiteral("GPU %1").arg(index);
    }
    return GpuDevice{QString::number(index), display};
  };

  if (doc.isArray()) {
    const QJsonArray arr = doc.array();
    for (const QJsonValue &value : arr) {
      const QJsonObject obj = value.toObject();
      if (obj.isEmpty()) {
        continue;
      }
      devices.append(parseEntry(obj, devices.size()));
    }
  } else if (doc.isObject()) {
    const QJsonObject obj = doc.object();
    if (!obj.isEmpty()) {
      devices.append(parseEntry(obj, 0));
    }
  }
#endif

#ifdef Q_OS_WIN
  g_enumeratedDevices = devices;
#endif
  return devices;
}

void GpuDeviceManager::applyDevice(const QString &id) {
  const bool isAuto = id.isEmpty() || id == QLatin1String("auto");
  qDebug() << "[GpuDeviceManager] applying GPU device:"
           << (isAuto ? QStringLiteral("auto") : id);

#ifdef Q_OS_LINUX
  // DRI_PRIME 由 Mesa 在 GL 栈初始化时读取，auto 时清除；运行中变更仅对
  // 下次启动生效。
  if (isAuto) {
    qunsetenv("DRI_PRIME");
  } else {
    qputenv("DRI_PRIME", id.toUtf8());
  }
#elif defined(Q_OS_WIN)
  // Windows 无法任意指定适配器，仅能写 GPU 性能偏好：
  // GpuPreference=0 清除偏好 / 1 节能（集显）/ 2 高性能（独显）。
  int preference = 0;
  if (!isAuto) {
    preference = 2;
    if (g_enumeratedDevices.isEmpty()) {
      availableDevices();
    }
    for (const GpuDevice &device : g_enumeratedDevices) {
      if (device.id != id) {
        continue;
      }
      const QString name = device.name.toLower();
      if (name.contains(QLatin1String("intel")) &&
          !name.contains(QLatin1String("arc"))) {
        preference = 1;
      }
      break;
    }
  }

  // 注：QSettings 的注册表后端把键中的 '\' 当作子键分隔符（Qt 文档明确禁止
  // 键名含 '/' 或 '\'），无法写出"exe 绝对路径"形式的值名，因此直接调用
  // 注册表 API（advapi32 由 CMake 默认链接）。本函数会在 QApplication 构造前
  // （main 启动早期）调用，QCoreApplication::applicationFilePath() 此时返回
  // 空，故用 Win32 API 取当前 exe 绝对路径。
  wchar_t modulePath[MAX_PATH] = {0};
  GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
  const QString valueName =
      QDir::toNativeSeparators(QString::fromWCharArray(modulePath));
  const QString valueData =
      QStringLiteral("GpuPreference=%1;").arg(preference);
  HKEY key = nullptr;
  if (RegCreateKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\DirectX\\UserGpuPreferences", 0,
                      nullptr, 0, KEY_SET_VALUE, nullptr, &key,
                      nullptr) != ERROR_SUCCESS) {
    return;
  }
  const std::wstring nativeName = valueName.toStdWString();
  const std::wstring nativeData = valueData.toStdWString();
  RegSetValueExW(key, nativeName.c_str(), 0, REG_SZ,
                 reinterpret_cast<const BYTE *>(nativeData.c_str()),
                 static_cast<DWORD>((nativeData.size() + 1) * sizeof(wchar_t)));
  RegCloseKey(key);
#else
  Q_UNUSED(id);
#endif
}

bool GpuDeviceManager::isSupported() {
#ifdef Q_OS_MACOS
  return false;
#else
  return true;
#endif
}
