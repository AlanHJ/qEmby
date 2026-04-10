# [开源] qEmby - 一个用 Qt6/C++20 开发的 Emby/Jellyfin 桌面客户端

## 简介

各位佬好！分享一个个人兴趣开发的项目 —— **qEmby**，一个基于 Qt6 和 C++20 的 Emby / Jellyfin 媒体服务器桌面客户端。

作为一个 Emby 用户，一直觉得官方客户端在桌面端体验一般，Web 端虽然够用但总觉得不够"原生"，于是萌生了自己写一个桌面客户端的想法。经过一段时间的开发，目前已经完成了基础功能，发个帖子和大家分享一下。

## 截图

![首页](https://github.com/AlanHJ/qEmby/raw/main/screenshots/2.png)

![影片详情](https://github.com/AlanHJ/qEmby/raw/main/screenshots/5.png)

![设置](https://github.com/AlanHJ/qEmby/raw/main/screenshots/3.png)

![管理仪表盘](https://github.com/AlanHJ/qEmby/raw/main/screenshots/4.png)

## 主要功能

- 🎬 浏览和管理 Emby / Jellyfin 媒体库
- ▶️ 内置 libmpv 驱动的视频播放器
- 🌗 深色 / 浅色主题切换
- 🌐 中英文国际化支持
- 🔍 支持搜索历史的媒体搜索
- 📺 当前支持电视剧、电影媒体类型
- 🛠️ 服务器管理仪表盘

## 技术栈

| 组件 | 技术 |
|---|---|
| 框架 | Qt 6.x (Widgets) |
| 语言 | C++20 |
| 视频播放 | libmpv |
| 异步 | QCoro (C++20 协程) |
| 日志 | spdlog |
| 窗口框架 | QWindowKit |
| 构建系统 | CMake |

## 平台支持

| 平台 | 状态 |
|---|---|
| Windows 10/11 x64 | ✅ 已适配 |
| Linux | 🚧 计划中 |
| macOS | 🚧 计划中 |

## 后续开发计划

- 支持添加到播放列表和从播放列表中移除
- 支持识别来更新元数据
- 支持修改元数据和图片
- Linux / macOS 平台适配

## 下载 & 源码

- **GitHub**: https://github.com/AlanHJ/qEmby
- **Release 下载**: https://github.com/AlanHJ/qEmby/releases/tag/v0.0.1
- **许可证**: MIT

---

项目是个人兴趣开发，目前还处于早期阶段（v0.0.1 Alpha），功能比较基础，欢迎大家试用、反馈和提 Issue！如果觉得有意思的话，也欢迎给个 ⭐ Star ~
