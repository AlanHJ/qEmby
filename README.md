<p align="center">
  <img src="src/qEmbyApp/resources/svg/qemby_logo.svg" width="120" alt="qEmby Logo"/>
</p>

<h1 align="center">qEmby</h1>

<p align="center">
  <b>A modern desktop client for Emby & Jellyfin media servers</b><br/>
  <b>Emby & Jellyfin 媒体服务器的现代桌面客户端</b>
</p>

<p align="center">
  <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT"/></a>
  <a href="https://github.com/AlanHJ/qEmby/releases/latest"><img src="https://img.shields.io/github/v/release/AlanHJ/qEmby?include_prereleases&label=Download" alt="Release"/></a>
  <img src="https://img.shields.io/badge/Qt-6.x-green.svg" alt="Qt 6"/>
  <img src="https://img.shields.io/badge/C%2B%2B-20-orange.svg" alt="C++20"/>
  <img src="https://img.shields.io/badge/Platform-Windows%20|%20Linux%20|%20macOS-lightgrey.svg" alt="Platform: Windows | Linux | macOS"/>
</p>

<p align="center">
  <a href="#中文">中文</a> | <a href="#english">English</a>
</p>

---

<a id="中文"></a>

## 📸 应用截图

<p align="center">
  <img src="screenshots/2.png" width="45%" alt="首页"/>
  <img src="screenshots/5.png" width="45%" alt="影片详情"/>
</p>
<p align="center">
  <img src="screenshots/3.png" width="45%" alt="设置"/>
  <img src="screenshots/4.png" width="45%" alt="管理仪表盘"/>
</p>

## 📥 下载

最新版本：**v0.0.4**

| 安装包 | 说明 |
|---|---|
| [qEmby-v0.0.4-Win-x64-Setup.exe](https://github.com/AlanHJ/qEmby/releases/download/v0.0.4/qEmby-v0.0.4-Win-x64-Setup.exe) | Windows 10/11 x64 安装包 |
| [qEmby-v0.0.4-Win-x64.zip](https://github.com/AlanHJ/qEmby/releases/download/v0.0.4/qEmby-v0.0.4-Win-x64.zip) | Windows 10/11 x64 绿色便携版 |
| [qemby-0.0.4-macos-arm64.dmg](https://github.com/AlanHJ/qEmby/releases/download/v0.0.4/qemby-0.0.4-macos-arm64.dmg) | macOS 26+ (Apple 芯片) |

旧版 Windows、Linux 和 macOS 构建可以在 [Releases](https://github.com/AlanHJ/qEmby/releases) 页面下载。

## 🚀 v0.0.4 更新内容

- 新增 macOS 26+ (Apple 芯片 / arm64) 支持。
- 新增全局代理支持，覆盖所有网络请求。
- 支持为每个服务器单独设置单例模式。
- 新增自定义 WebDAV 云同步支持。
- 内置播放器新增本地字幕加载支持。
- 新增法语支持，感谢志愿者 @Arkoth79 的贡献。
- 改善了多个界面的动画效果。
- 优化了整体流畅度和响应速度。
- 修复了一些已知问题。

## ✨ 功能特性

- 🎬 浏览和管理你的 Emby / Jellyfin 媒体库
- ▶️ 内置 **libmpv** 驱动的视频播放器
- 💬 弹幕播放，支持搜索、匹配、缓存和原生覆盖层渲染
- 🧩 支持元数据编辑、媒体识别、图片更新和播放列表管理
- 📥 下载管理器
- 🌗 深色 / 浅色主题切换
- 🌐 国际化支持（中文 / 英文）
- 🔍 支持搜索历史的媒体搜索
- 📺 当前支持电视剧、电影媒体类型
- 📦 提供 Windows 安装包 / 绿色版，以及 Linux AppImage / deb 包
- ⚡ 基于 C++20 协程的异步操作（QCoro）
- 🪟 原生风格的自定义窗口边框（QWindowKit）

## 💻 平台支持

| 平台 | 状态 |
|---|---|
| Windows 10/11 x64 | ✅ 已适配 |
| Linux x64 (AppImage / deb) | ✅ 已适配 |
| macOS 26+ (Apple Silicon) | ✅ 已适配 |

## 📋 开发路线图

- [x] Emby / Jellyfin 媒体库浏览
- [x] 内置视频播放器（libmpv）
- [x] 深色 / 浅色主题
- [x] 国际化支持（中文 / 英文）
- [x] 媒体搜索与搜索历史
- [x] 电视剧、电影支持
- [x] 服务器管理仪表盘
- [x] 支持添加到播放列表和从播放列表中移除
- [x] 支持识别来更新元数据
- [x] 支持修改元数据和图片
- [x] 弹幕系统（搜索、匹配、设置、渲染）
- [x] 下载管理器
- [ ] AI 字幕生成
- [x] Linux 平台适配
- [x] macOS 平台适配

> 本项目为个人兴趣开发，欢迎贡献和反馈！

## 🛠️ 技术栈

| 组件 | 技术 |
|---|---|
| 框架 | Qt 6.x (Widgets) |
| 语言 | C++20 |
| 视频播放 | libmpv |
| 异步 | QCoro (Qt C++20 协程) |
| 日志 | spdlog |
| 窗口框架 | QWindowKit |
| 构建系统 | CMake |

## 📦 环境要求

- **Qt 6.x**（包含 Widgets、Core、Network、Concurrent、OpenGLWidgets、LinguistTools、WebSockets 模块）
- **CMake** ≥ 3.16
- 支持 **C++20** 的编译器（推荐 MSVC 2022）
- **libmpv** 开发文件（见下方说明）
- **Git**（用于克隆子模块）

## 🚀 构建指南

### 1. 克隆仓库

```bash
git clone --recursive https://github.com/AlanHJ/qEmby.git
cd qEmby
```

### 2. 获取 libmpv

下载 libmpv 开发包，并放置到 `libs/libmpv/` 目录下，结构如下：

```
libs/libmpv/
├── bin/
│   └── libmpv-2.dll
├── include/
│   └── mpv/
│       ├── client.h
│       └── render.h (等)
└── lib/
    └── libmpv.dll.a
```

libmpv 获取方式：
- [shinchiro/mpv-winbuild-cmake](https://github.com/shinchiro/mpv-winbuild-cmake/releases)（Windows 预编译版本）
- [mpv-player/mpv](https://github.com/mpv-player/mpv)（从源码编译）

### 3. 配置和构建

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/path/to/Qt6/lib/cmake"
cmake --build build --config Release
```

> **提示：** 在 Windows 上使用 MSVC 时，也可以直接在 Qt Creator 或 Visual Studio 中打开 CMake 项目。

## 📁 项目结构

```
qEmby/
├── CMakeLists.txt              # 根 CMake 配置
├── libs/
│   ├── libmpv/                 # libmpv SDK（未纳入版本控制，见构建指南）
│   └── qwindowkit/             # QWindowKit（git 子模块）
└── src/
    ├── qEmbyCore/              # 核心库（API、模型、服务）
    │   ├── api/                # Emby/Jellyfin API 客户端
    │   ├── config/             # 配置管理
    │   ├── models/             # 数据模型
    │   └── services/           # 业务逻辑服务
    └── qEmbyApp/               # 桌面应用
        ├── components/         # 可复用 UI 组件
        ├── managers/           # 应用管理器
        ├── resources/          # 图标、主题、翻译
        ├── utils/              # 工具类
        └── views/              # 应用视图
```

## 💬 交流社区

加入 Telegram 交流群：[https://t.me/+qXQ-zU56z9gxOWNl](https://t.me/+qXQ-zU56z9gxOWNl)

> **注意：** 本项目是为爱发电项目，测试覆盖不全，敬请谅解。如有问题请通过 [GitHub Issues](https://github.com/AlanHJ/qEmby/issues) 反馈。

## 📄 许可证

本项目基于 [MIT 许可证](LICENSE) 开源。

## 🙏 致谢

- [Qt](https://www.qt.io/) — 应用框架 (LGPL v3)
- [mpv](https://mpv.io/) — 媒体播放引擎 (LGPL v2.1+)
- [QWindowKit](https://github.com/stdware/qwindowkit) — 自定义窗口框架 (Apache-2.0)
- [QCoro](https://github.com/danvratil/qcoro) — Qt C++20 协程库 (MIT)
- [spdlog](https://github.com/gabime/spdlog) — 高性能日志库 (MIT)

---

<a id="english"></a>

## 📸 Screenshots

<p align="center">
  <img src="screenshots/2.png" width="45%" alt="Home"/>
  <img src="screenshots/5.png" width="45%" alt="Detail"/>
</p>
<p align="center">
  <img src="screenshots/3.png" width="45%" alt="Settings"/>
  <img src="screenshots/4.png" width="45%" alt="Admin Dashboard"/>
</p>

## 📥 Download

Latest release: **v0.0.4**

| Package | Description |
|---|---|
| [qEmby-v0.0.4-Win-x64-Setup.exe](https://github.com/AlanHJ/qEmby/releases/download/v0.0.4/qEmby-v0.0.4-Win-x64-Setup.exe) | Windows 10/11 x64 installer |
| [qEmby-v0.0.4-Win-x64.zip](https://github.com/AlanHJ/qEmby/releases/download/v0.0.4/qEmby-v0.0.4-Win-x64.zip) | Windows 10/11 x64 portable package |
| [qemby-0.0.4-macos-arm64.dmg](https://github.com/AlanHJ/qEmby/releases/download/v0.0.4/qemby-0.0.4-macos-arm64.dmg) | macOS 26+ (Apple Silicon) |

Older Windows, Linux and macOS builds are available on the [Releases](https://github.com/AlanHJ/qEmby/releases) page.

## 🚀 What's New in v0.0.4

- Added macOS 26+ support for Apple Silicon (arm64) Macs.
- Added global proxy support for all network requests.
- Added per-server singleton configuration support.
- Added custom WebDAV cloud sync support for user data.
- Added local subtitle loading support in built-in player.
- Added French language support, thanks to volunteer @Arkoth79.
- Improved UI animations in several areas.
- Optimized overall smoothness and responsiveness.
- Fixed several known issues.

## ✨ Features

- 🎬 Browse and manage your Emby / Jellyfin media library
- ▶️ Built-in video player powered by **libmpv**
- 💬 Danmaku playback with search, matching, cache and native overlay rendering
- 🧩 Metadata editing, media identification, image updates and playlist tools
- 📥 Download manager
- 🌗 Dark and Light theme support
- 🌐 Internationalization support (Chinese / English)
- 🔍 Media search with history
- 📺 TV series and movies media types
- 📦 Windows installer / portable packages and Linux AppImage / deb packages
- ⚡ Asynchronous operations with C++20 coroutines (QCoro)
- 🪟 Custom window frame with native look (QWindowKit)

## 💻 Platform Support

| Platform | Status |
|---|---|
| Windows 10/11 x64 | ✅ Supported |
| Linux x64 (AppImage / deb) | ✅ Supported |
| macOS 26+ (Apple Silicon) | ✅ Supported |

## 📋 Roadmap

- [x] Emby / Jellyfin media library browsing
- [x] Built-in video player (libmpv)
- [x] Dark / Light theme
- [x] Internationalization (Chinese / English)
- [x] Media search with history
- [x] TV series & movies support
- [x] Server administration dashboard
- [x] Playlist support (add/remove items)
- [x] Media identification & metadata refresh
- [x] Metadata and image editing
- [x] Danmaku (bullet comments) system
- [x] Download manager
- [ ] AI-powered subtitle generation
- [x] Linux platform support
- [x] macOS platform support

> This is a personal hobby project, developed out of interest. Contributions and feedback are welcome!

## 🛠️ Tech Stack

| Component | Technology |
|---|---|
| Framework | Qt 6.x (Widgets) |
| Language | C++20 |
| Video Player | libmpv |
| Async | QCoro (C++20 Coroutines for Qt) |
| Logging | spdlog |
| Window Kit | QWindowKit |
| Build System | CMake |

## 📦 Prerequisites

- **Qt 6.x** (with Widgets, Core, Network, Concurrent, OpenGLWidgets, LinguistTools, WebSockets)
- **CMake** ≥ 3.16
- **C++20** compatible compiler (MSVC 2022 recommended)
- **libmpv** development files (see below)
- **Git** (for cloning submodules)

## 🚀 Build

### 1. Clone the repository

```bash
git clone --recursive https://github.com/AlanHJ/qEmby.git
cd qEmby
```

### 2. Get libmpv

Download the libmpv development package and place it in `libs/libmpv/` with the following structure:

```
libs/libmpv/
├── bin/
│   └── libmpv-2.dll
├── include/
│   └── mpv/
│       ├── client.h
│       └── render.h (etc.)
└── lib/
    └── libmpv.dll.a
```

You can get libmpv from:
- [shinchiro/mpv-winbuild-cmake](https://github.com/shinchiro/mpv-winbuild-cmake/releases) (Windows builds)
- [mpv-player/mpv](https://github.com/mpv-player/mpv) (build from source)

### 3. Configure and build

```bash
cmake -B build -DCMAKE_PREFIX_PATH="/path/to/Qt6/lib/cmake"
cmake --build build --config Release
```

> **Tip:** On Windows with MSVC, you can also open the project in Qt Creator or Visual Studio with CMake support.

## 📁 Project Structure

```
qEmby/
├── CMakeLists.txt              # Root CMake configuration
├── libs/
│   ├── libmpv/                 # libmpv SDK (not tracked, see Build section)
│   └── qwindowkit/             # QWindowKit (git submodule)
└── src/
    ├── qEmbyCore/              # Core library (API, models, services)
    │   ├── api/                # Emby/Jellyfin API client
    │   ├── config/             # Configuration management
    │   ├── models/             # Data models
    │   └── services/           # Business logic services
    └── qEmbyApp/               # Desktop application
        ├── components/         # Reusable UI components
        ├── managers/           # Application managers
        ├── resources/          # Icons, themes, translations
        ├── utils/              # Utility classes
        └── views/              # Application views
```

## 💬 Community

Join our Telegram group: [https://t.me/+qXQ-zU56z9gxOWNl](https://t.me/+qXQ-zU56z9gxOWNl)

> **Note:** This is a passion project with limited testing. Your understanding is appreciated. Please report any issues via [GitHub Issues](https://github.com/AlanHJ/qEmby/issues).

## 📄 License

This project is licensed under the [MIT License](LICENSE).

## 🙏 Acknowledgements

- [Qt](https://www.qt.io/) — Application framework (LGPL v3)
- [mpv](https://mpv.io/) — Media player engine (LGPL v2.1+)
- [QWindowKit](https://github.com/stdware/qwindowkit) — Custom window frame (Apache-2.0)
- [QCoro](https://github.com/danvratil/qcoro) — C++20 Coroutines for Qt (MIT)
- [spdlog](https://github.com/gabime/spdlog) — Fast logging library (MIT)
