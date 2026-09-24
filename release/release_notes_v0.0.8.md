# qEmby v0.0.8

## What's New / 更新内容

### ▶️ Player & Danmaku / 播放器与弹幕
- Added a danmaku density heatmap to the playback progress bar / 播放进度条新增弹幕密度热图。
- Reworked native danmaku timing, layout and OpenGL compositing for smoother playback / 重做原生弹幕的时钟、布局与 OpenGL 合成，改善播放流畅度。
- Improved video first-frame handling and playback window transitions / 改进视频首帧显示与播放窗口切换。

### 🌐 Network & Accounts / 网络与账号
- Added configurable User-Agent values globally or per server, including playback requests / 新增全局及按服务器配置 User-Agent，并应用于播放请求。
- Preserved local network settings when signing in to an existing server profile / 重新登录已有服务器时保留本地网络设置。

### 📚 Library & Interface / 媒体库与界面
- Added library visibility controls in the sidebar and improved home-library refresh behavior / 侧边栏新增媒体库显示控制，并改进首页媒体库刷新行为。
- Remembered the selected library tab and expanded person biography details / 记住媒体库当前标签，并完善人物简介展示。
- Refined player controls, dialogs and theme styling / 调整播放器控件、对话框与主题样式。

---

## Downloads / 下载

| Platform / 平台 | File / 文件 | Description / 说明 |
|---|---|---|
| Windows x64 | `qEmby-0.0.8-Win-x64-Setup.exe` | Installer / 安装包 |
| Windows x64 | `qEmby-0.0.8-Win-x64.zip` | Portable package / 绿色便携版 |
| macOS Apple Silicon | `qemby-0.0.8-macos-arm64.dmg` | macOS 26+ |
| Linux x64 | `qemby-0.0.8-x86_64.AppImage` | AppImage (`chmod +x` before running / 运行前执行 `chmod +x`) |
| Ubuntu 22.04 | `qemby_0.0.8-jammy_amd64.deb` | Jammy |
| Ubuntu 24.04 | `qemby_0.0.8-noble_amd64.deb` | Noble |
| Ubuntu 26.04 | `qemby_0.0.8-resolute_amd64.deb` | Resolute; uses system Qt packages / 使用系统 Qt 软件包 |
| Debian 12 | `qemby_0.0.8-bookworm_amd64.deb` | Bookworm |
| Debian 13 | `qemby_0.0.8-trixie_amd64.deb` | Trixie |

Download the `.deb` matching your distribution version. The macOS package supports Apple Silicon only. / 请下载与发行版匹配的 `.deb`；macOS 安装包仅支持 Apple 芯片。
