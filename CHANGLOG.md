# Changelog

本文件是 CI 发版时 Release 说明的来源（`TARGET.md §10.29`）。
版本号记录在 `VERSION`。以 `-alpha` / `-beta` / `-bata` / `-dev` 结尾的版本会发布为 pre-release。

## 0.1.0

- 首个 CI 自动发版版本。
- 双平台（网易云 + 酷狗）直连上游 API，签名/加密/Cookie 全部 C++ 重写，不依赖 Node 服务。
- 搜索（单曲 / 专辑 / MV）、每日推荐、播放、四档音质、下载（标签/歌词/封面）、扫码与账密登录。
- 歌手页 / 专辑页、账号收藏（歌曲 / 专辑 / MV）、MV 播放与 16:9 视频窗口。
- 用户数据 PBKDF2 + 盐 + AES-256-CBC 加密存储、单实例、托盘/关闭行为可选、明暗主题图标着色。
- Windows SMTC 原生媒体控制；Linux MPRIS / Android MediaSession / macOS Now Playing 为骨架。
- GitHub Actions 自动构建 Windows / Linux / macOS / Android 四平台产物。
