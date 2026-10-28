#ifndef GPUDEVICEMANAGER_H
#define GPUDEVICEMANAGER_H

#include <QList>
#include <QString>

// GPU 设备选择（平台级强制 + 自动枚举）。
//
// 播放器由 MpvWidget（QOpenGLWidget）托管 GL 上下文，mpv 以 vo=libmpv 渲染
// 进该上下文，GPU 实际由 Qt/系统层决定，mpv 没有 GPU 设备选项。因此这里在
// 平台层面"尽力而为"地强制渲染 GPU：
//  - Linux：枚举 /sys/class/drm/card*/device 的 PCI 设备，强制通过 DRI_PRIME
//    环境变量实现（Mesa 每次创建 GL 上下文时读取），设备 id 为
//    pci-XXXX_XX_XX_X 形式的 PCI 路径（DRI_PRIME 兼容格式）。
//  - Windows：通过 Win32_VideoController（powershell）枚举；强制通过注册表
//    HKCU\Software\Microsoft\DirectX\UserGpuPreferences 写入 GpuPreference
//    偏好（值名为当前 exe 绝对路径）。Windows 无法任意指定适配器，仅能设置
//    "高性能 / 节能"偏好，由系统据此选择 GPU。
//  - macOS：不支持手动选择（枚举返回空，强制为 no-op）。
//
// 限制：强制仅对之后新创建的播放渲染表面生效，已存在的 GL 上下文不受影响；
// 枚举失败时返回空列表，UI 层据此禁用手动选择。

struct GpuDevice {
  QString id;
  QString name;
};

class GpuDeviceManager {
public:
  // 尽力枚举可用 GPU 设备；失败或平台不支持时返回空列表。
  static QList<GpuDevice> availableDevices();
  // 强制之后新创建的渲染上下文使用指定 GPU；id 为 "auto" 时清除强制。
  static void applyDevice(const QString &id);
  // 平台是否支持强制指定 GPU（macOS 为 false）。
  static bool isSupported();
};

#endif
