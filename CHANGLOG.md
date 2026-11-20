# Changelog

> 本文件是 CI 发版时 Release 说明的来源（`REQUIREMENTS.md §10.29`）。
>
> **只保留最新一版**：每次发版只写当前 `VERSION` 的一段，旧条目不再累积（见 `REQUIREMENTS.md §10.33`）。历史变更见 [GitHub Releases](https://github.com/ImSordayne/MrMusic/releases)。
>
> 版本号记录在 `VERSION`；以 `-alpha` / `-beta` / `-bata` / `-dev` 结尾的版本会发布为 pre-release。

## 0.1.2

- 修复 Android 启动即闪退：Android MediaSession 的构造/回调在 Qt 原生线程（无 Looper）上执行导致 `RuntimeException: Can't create handler ... Looper.prepare()`，进程 SIGABRT。现全部投递到主 Looper 执行。
