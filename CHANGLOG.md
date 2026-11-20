# Changelog

本文件是 CI 发版时 Release 说明的来源（`TARGET.md §10.29`）。
版本号记录在 `VERSION`。以 `-alpha` / `-beta` / `-bata` / `-dev` 结尾的版本会发布为 pre-release。

## 0.1.2

- 修复 Android 启动即闪退：Android MediaSession 的构造/回调在 Qt 原生线程（无 Looper）上执行导致 RuntimeException: Can't create handler ... Looper.prepare()，进程 SIGABRT。现全部投递到主 Looper 执行。

## 0.1.1

- 修复网易云二维码登录在手机确认后无后续：QR 端点改走 eapi、不再吞掉轮询失败、成功时校验登录态；账密登录受平台限制时明确提示改用扫码。
- 修复酷狗扫码登录后头像显示为方形：头像图片加圆形遮罩裁剪。
- Windows 可执行文件通过 `.rc` 使用 `res/logo.ico` 作为预览图标（其它图标不变）。
- CI：Windows 产物压缩包改为扁平结构（解压即得 exe 与 `data/`，无额外 `bin/` 层）。
- 明确版本变更规则：`x.y.z` 中 `x` 为重大更新、`y` 为功能更新、`z` 为 Bug 修复。

## 0.1.0

- 首个 CI 自动发版版本。
- 双平台（网易云 + 酷狗）直连上游 API，签名/加密/Cookie 全部 C++ 重写，不依赖 Node 服务。
- 搜索（单曲 / 专辑 / MV）、每日推荐、播放、四档音质、下载（标签/歌词/封面）、扫码与账密登录。
- 歌手页 / 专辑页、账号收藏（歌曲 / 专辑 / MV）、MV 播放与 16:9 视频窗口。
- 用户数据 PBKDF2 + 盐 + AES-256-CBC 加密存储、单实例、托盘/关闭行为可选、明暗主题图标着色。
- Windows SMTC 原生媒体控制；Linux MPRIS / Android MediaSession / macOS Now Playing 为骨架。
- GitHub Actions 自动构建 Windows / Linux / macOS / Android 四平台产物。
