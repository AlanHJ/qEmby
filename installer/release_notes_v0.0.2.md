# qEmby v0.0.2

## What's New / 更新内容

### 🎬 Playlist / 播放列表
- Support adding to and removing from playlists / 支持添加到播放列表和从播放列表中移除

### 🔍 Media Identification / 媒体识别
- Support media identification for metadata matching / 支持识别媒体以匹配元数据

### ✏️ Metadata Editing / 元数据编辑
- Support editing media metadata / 支持修改元数据

### 📥 Download Manager / 下载管理
- Support download management / 支持下载管理

### 💬 Danmaku (Bullet Comments) / 弹幕系统
- Added danmaku support powered by [danmu_api](https://github.com/huangxd-/danmu_api), deployed via Cloudflare Workers
- You can use `https://danmu.hujie388.workers.dev/` to fetch danmaku data
- Official DanDanPlay API integration is planned for a future release
---
- 新增弹幕功能，基于 [danmu_api](https://github.com/huangxd-/danmu_api) 项目，通过自部署的 Cloudflare Workers 工作
- 你可以使用 `https://danmu.hujie388.workers.dev/` 这个地址来获取弹幕
- 弹弹Play 官方 API 对接将在后续版本中支持

### 🏠 Dashboard Customization / 主页自定义
- Support custom section ordering on the home page / 支持主页自定义排序

### ▶️ Player Enhancements / 播放器增强
- Added sidebar and bottom bar for quick episode switching / 支持内置播放器侧边栏和下边栏快速切换媒体剧集
- Added long-press fast-forward with two modes: step and speed / 支持长按快进操作（支持步进和倍速两种方式）
- Added subtitle settings panel / 支持字幕设置

### ⚙️ Media Preferences / 媒体偏好设置
- Support default audio track and subtitle customization / 支持首选音轨、字幕自定义功能
- Support video source priority by size, date, and bitrate / 支持视频按大小、日期、码率进行优先级排序

### 🐛 Bug Fixes / 问题修复
- Various known bug fixes / 其他已知 bug 修复

---

## Downloads / 下载

### Windows

| File | Description |
|---|---|
| `qEmby-v0.0.2-Win-x64.zip` | Portable version (extract and run) / 绿色便携版（解压即用） |
| `qEmby-v0.0.2-Win-x64-Setup.exe` | Installer version / 安装包版 |

### Linux

| File | Description |
|---|---|
| `qEmby-0.0.2-x86_64.AppImage` | Universal Linux AppImage (no install needed, `chmod +x` and run) / 通用 Linux AppImage（无需安装，`chmod +x` 后直接运行） |
| `qEmby_0.0.2-noble_amd64.deb` | Deb package for Ubuntu 24.04 (Noble Numbat) / Ubuntu 24.04 安装包 |
| `qEmby_0.0.2-jammy_amd64.deb` | Deb package for Ubuntu 22.04 (Jammy Jellyfish) / Ubuntu 22.04 安装包 |
| `qEmby_0.0.2-bookworm_amd64.deb` | Deb package for Debian 12 (Bookworm) / Debian 12 安装包 |
| `qEmby_0.0.2-trixie_amd64.deb` | Deb package for Debian 13 (Trixie) / Debian 13 安装包 |

> **Note / 说明：** The `.deb` filenames contain the distribution codename (e.g. `bookworm` = Debian 12, `jammy` = Ubuntu 22.04). Please download the package matching your OS version. / `.deb` 文件名中包含发行版代号（如 `bookworm` = Debian 12，`jammy` = Ubuntu 22.04），请下载与您系统版本匹配的安装包。
