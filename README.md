# qEmby - 基于 Qt 的 Emby/Jellyfin 客户端

qEmby是一款使用 C++ 和 Qt 框架开发的跨平台多媒体桌面客户端。本项目不仅致力于提供具有现代视觉体验的无边框界面和流畅的本地播放体验，还**全面兼容 Emby 和 Jellyfin 服务器**，并支持**多服务器配置管理与无缝切换**。 

## 1. 项目结构说明

项目采用模块化设计，将核心逻辑与 UI 界面严格分离。以下是主要的目录结构及说明：

```text
qEmby/
├── CMakeLists.txt              # 根项目 CMake 配置文件 
├── libs/                       # 第三方依赖库
│   └── qwindowkit/             # 跨平台窗口自定义套件（实现现代无边框窗口） 
├── src/                        # 源代码目录
│   ├── qEmbyCore/              # 核心库：负责业务逻辑、API 通讯、多服务器管理等
│   │   ├── CMakeLists.txt
│   │   ├── qembycore.h/cpp
│   │   └── qEmbyCore_global.h  # 库导出定义
│   └── qEmbyApp/               # 主程序：负责 UI 界面与用户交互
│       ├── CMakeLists.txt
│       ├── main.cpp            # 程序入口，处理多语言加载等
│       ├── mainwindow.h/cpp    # 主窗口实现，包含自定义标题栏逻辑
│       └── qEmby_zh_CN.ts      # 国际化翻译源文件
└── build/                      # 构建输出目录（由编译工具自动生成）
```

### 关键模块说明：

- **qEmbyCore**: 编译为共享库（Shared Library），作为项目的后端支撑。内部包含 `ServerManager`（负责多服务器配置管理）和 `ApiClient`（负责抹平 Emby 与 Jellyfin 的 API 及认证差异）。
- **qEmbyApp**: 可执行程序客户端，依赖于 `qEmbyCore` 和 `qwindowkit`，提供服务器切换与媒体浏览界面。
- **qwindowkit**: 用于实现 Windows、macOS 等平台上的原生无边框窗口效果和自定义标题栏。

## 2. 技术栈

- **语言**: C++ 20
- **框架**: Qt 6.x
- **构建系统**: CMake 3.19+
- **核心模块**: `QtWidgets`, `QtCore`, `QtNetwork`, `QtConcurrent`, `QtOpenGLWidgets`, `QtWebSockets`, `QtLinguistTools`
- **界面库**: `QWindowKit` (用于现代窗口管理)

## 3. 环境准备

在开始构建之前，请确保你的开发环境已配置：

1. **Qt SDK**: 安装 Qt 6.x 版本。推荐 Windows 与 Ubuntu 使用同一主版本。
2. **编译器**:
   - Windows: MSVC 2019/2022
   - macOS: Clang (Xcode)
   - Linux: GCC
3. **CMake**: 确保 CMake 3.19+ 已添加到系统路径。
4. **libmpv**:
   - Windows: 默认使用仓库内 `libs/libmpv`
   - Linux/macOS: 需要系统安装 `libmpv` 开发包

## 4. 构建指南

### 获取代码

由于本项目引用了 `qwindowkit` 作为子模块，请使用以下命令克隆：

Bash

```
git clone --recursive https://your-repository-url/qEmby.git
```

### 常用 CMake 变量

- `QEMBY_QT_ROOT`: Qt 安装根目录或可用于 `find_package()` 的前缀路径
- `QEMBY_LIBMPV_ROOT`: Windows 下可选，用于覆盖默认 `libs/libmpv`
- `QEMBY_LIBMPV_INCLUDE_DIR` / `QEMBY_LIBMPV_LIBRARY`: Linux/macOS 下手动指定 `libmpv`

### Windows 构建

```powershell
cmake -S . -B build -G "Ninja" -DQEMBY_QT_ROOT="E:/Qt6/6.9.2/msvc2022_64"
cmake --build build --config Release
```

### Ubuntu 22.04 构建

推荐安装官方 Qt 6 Linux GCC 版本，然后通过 `QEMBY_QT_ROOT` 指向对应目录。

```bash
sudo apt update
sudo apt install -y build-essential cmake ninja-build pkg-config libmpv-dev libgl1-mesa-dev libxcb-cursor0

cmake -S . -B build -G Ninja -DQEMBY_QT_ROOT="$HOME/Qt/6.9.2/gcc_64"
cmake --build build --config Release
```

如果你使用的是系统自带 Qt，请确保它包含以下模块：`Widgets`、`Core`、`Network`、`Concurrent`、`OpenGLWidgets`、`LinguistTools`、`WebSockets`。

远程 Ubuntu 编译的详细流程可参考 [docs/ubuntu-remote-build.md](docs/ubuntu-remote-build.md)。

## 5. 开发路线图 (TODO)

- [x] 项目基础架构与 CMake 配置搭建
- [x] 确立多服务器与 Emby/Jellyfin 兼容架构
- [x] 集成跨平台无边框窗口系统与主题切换
- [ ] 开发底层的 `NetworkManager` 与智能区分后端的 `ApiClient`
- [ ] 开发多服务器配置管理模块 (`ServerManager`)
- [ ] 媒体库数据的请求、解析与展示 (瀑布流 UI)
- [ ] 集成高性能视频播放器内核（如 libvlc 或 mpv）
- [ ] 完善多语言支持

------

**注意**: 本项目目前处于早期开发阶段，欢迎提交 Issue 或 Pull Request。
