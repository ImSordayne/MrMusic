# MrMusic

<p align="center">
  <img alt="MrMusic" src="res/logo-512.png" width="512">
</p>

<p align="center"><b>跨平台音乐播放器：同时接入网易云音乐与酷狗音乐，用 Qt 6 + QML 实现，请求逻辑（签名 / 加密 / Cookie）全部用 C++ 重写，不启动任何 Node 服务，也不使用任何第三方代理接口。</b></p>

<p align="center">
  <img src="https://img.shields.io/badge/language-C++-blue.svg" alt="语言">
  <a href="https://github.com/ImSordayne/MrMusic/releases"><img src="https://img.shields.io/badge/version-0.1.0-green.svg" alt="版本"></a>
  <a href="https://github.com/ImSordayne/MrMusic/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-MIT-orange.svg" alt="许可证"></a>
  <img src="https://img.shields.io/badge/Qt-6.11-blue.svg" alt="Qt 版本">
</p>

> **本机路径说明**：本文中的 `<大写占位符>`（如 `<QT_WINDOWS>`、`<ANDROID_SDK>`）都是本机私有路径，不写死在文档里；实际值记录在仓库根目录的 [`PERSONAL.md`](PERSONAL.md)（`# PERSONAL_PATH`，`KEY = VALUE`），该文件已加入 `.gitignore`，不进版本库。执行构建命令时把它们替换为 `PERSONAL.md` 里同名键的值即可。详见 `TARGET.md` 开头的「占位符约定」。

## 注意事项

> [!IMPORTANT]
>
> - 需求来源：`TARGET.md`（本仓库根目录）。
> - 本软件完全由 AI 根据网络上公开 API 编写，与网易云音乐及酷狗音乐无任何从属或合作关系。
> - 为方便其他开发者开发，保留了原始 `TARGET.md`。

## 免责声明

> 1. 本项目仅供学习使用，请尊重版权，请勿利用此项目从事商业行为及非法用途!
> 2. 使用本项目的过程中可能会产生版权数据。对于这些版权数据，本项目不拥有它们的所有权。为了避免侵权，使用者务必在 24 小时内清除使用本项目的过程中所产生的版权数据。
> 3. 由于使用本项目产生的包括由于本协议或由于使用或无法使用本项目而引起的任何性质的任何直接、间接、特殊、偶然或结果性损害（包括但不限于因商誉损失、停工、计算机故障或故障引起的损害赔偿，或任何及所有其他商业损害或损失）由使用者负责。
> 4. **禁止在违反当地法律法规的情况下使用本项目。** 对于使用者在明知或不知当地法律法规不允许的情况下使用本项目所造成的任何违法违规行为由使用者承担，本项目不承担由此造成的任何直接、间接、特殊、偶然或结果性责任。
> 5. 音乐平台不易，请尊重版权，支持正版。
> 6. 本项目仅用于对技术可行性的探索及研究，不接受任何商业（包括但不限于广告等）合作及捐赠。
> 7. 如果官方音乐平台觉得本项目不妥，可联系本项目更改或移除。

## 本项目的 API 来源

本项目只参考这些开源项目所引用的**上游 API**（端点、参数、签名/加密算法），不运行它们、也不使用它们包装出来的本地 HTTP 接口：

- 酷狗音乐：[MakcRe/KuGouMusicApi](https://github.com/MakcRe/KuGouMusicApi)
- 网易云音乐：[neteasecloudmusicapienhanced/api-enhanced](https://github.com/neteasecloudmusicapienhanced/api-enhanced)（与 [Binaryify/NeteaseCloudMusicApi](https://github.com/Binaryify/NeteaseCloudMusicApi) 同源）
- QQ 音乐（暂不在需求范围内，仅备选）：[L-1124/QQMusicApi](https://github.com/L-1124/QQMusicApi)

上游项目在本机的位置由 `<MUSICAPI_REPO>` 给出（见 `PERSONAL.md`）。

---

## 1. 功能状态

| 需求 | 状态 | 说明 |
| --- | --- | --- |
| 双平台数据源 | ✅ | 网易云 + 酷狗同时可用，结果合并到同一列表 |
| C++ 直连上游 API | ✅ | weapi / eapi 加密、酷狗 android 签名、dfid 设备注册均为 C++ 实现 |
| 歌曲搜索 + 来源筛选 | ✅ | `AppController` 调用 `NeteaseClient` / `KugouClient` 并合并结果，界面提供「全部 / 网易云 / 酷狗」筛选 |
| 每日推荐（首页两个条目） | ✅ | 「网易云 - 每日推荐」「酷狗 - 每日推荐」各自独立加载 |
| 基础播放 | ✅ | 播放/暂停、上下曲、进度、音量、队列、顺序/循环/单曲/随机 |
| 音质选择与明确提示 | ✅ | 四档音质；无版权 / VIP 受限时给出明确提示，不静默降级 |
| 后台播放与媒体键 | ✅ Windows / ⚠️ 其它 | Windows SMTC 已实现；Linux MPRIS、Android MediaSession、macOS Now Playing 见 §7 |
| FFmpeg 音频后端 | ✅ | 通过 Qt Multimedia 的 FFmpeg 后端（`QT_MEDIA_BACKEND=ffmpeg`），运行时日志确认 FFmpeg 7.1.3 |
| 歌曲下载 | ✅ | 目录可选（默认系统音乐目录）、`作者-专辑-音质-平台.ext` 命名、每平台最多 5 并发、单曲单连接 |
| 下载后写入标签/歌词/封面 | ✅ MP3 / FLAC | ID3v2.3（TIT2/TPE1/TALB/TYER/USLT/APIC）、FLAC VORBIS_COMMENT + PICTURE；M4A 暂未写入（见 §7） |
| 登录 | ✅ | 两个平台各自独立：扫码登录（内置二维码生成）+ 账号密码登录 |
| 用户数据加密 | ✅ | token/cookie 用「系统指纹 + 随机盐 → PBKDF2-HMAC-SHA256 → AES-256-CBC + HMAC-SHA256」加密保存，可切换明文 |
| 原子写入 / 损坏提示 | ✅ | `QSaveFile` 临时文件替换；MAC/解密失败 → 界面提示重新登录 |
| 关闭行为可选 | ✅ | 设置中可选「在托盘运行」（Linux 无托盘则后台运行）或「直接关闭」，默认托盘运行；托盘菜单「退出」走 `QCoreApplication::exit`（带强制退出兜底） |
| 单实例 | ✅ | 再次启动会唤起已运行实例的窗口（显示 / 还原 / 置顶），不会再起第二个进程 |
| 多类型搜索 | ✅ | 单曲 / 专辑 / MV 三档，两个平台合并，可「加载更多」；搜索页有返回按钮 |
| 歌手页 / 专辑页 | ✅ | 歌曲行的歌手名、专辑名可点击进入详情；歌手页含音乐/专辑/MV 三页与「关注歌手」，专辑页含曲目与「收藏专辑」 |
| MV 播放 | ✅ | 搜索结果 / 歌手页 MV / 收藏的 MV 点击即在播放器中播放（网易云 mp4、酷狗 mkv），「MV」按钮可弹出 16:9 视频窗口 |
| 收藏走账号 API | ✅ 网易云 / 🚧 酷狗 | 收藏页分三种分页（歌曲 / 专辑 / MV），网易云已接：喜欢歌曲、收藏专辑与 MV 及其列表；酷狗侧暂只接了 MV 收藏列表，详见 TARGET.md §10 |
| 封面完整性 | ✅ | 酷狗每日推荐本身不带封面，程序用 `/v2/get_res_privilege/lite` 批量补齐（实测 30/30 命中） |
| 主题一致性 | ✅ | 侧栏/底部导航图标在深色主题下为白色、浅色主题下为黑色（选中态用 fill 变体 + 胶囊底色） |
| UI 设计 | ✅ | 桌面侧栏 / 手机底部导航、图标 fill 变体、左下角双头像、列表项含「来源」 |

---

## 2. 目录结构

```
MrMusic/
├── CMakeLists.txt
├── CMakePresets.json            # windows-msvc / android-arm64 预设
├── TARGET.md                    # 需求文档（含占位符约定）
├── README.md
├── API_ENDPOINTS.md             # 直连上游端点清单（"不使用代理接口" 的验收依据）
├── PERSONAL.md                  # 本机私有路径（已 .gitignore，不进版本库）
├── Logo.svg / logo-mark.svg     # 根目录 Logo（渲染输入的副本）
├── src/
│   ├── main.cpp                 # 组装各层，注入 QML 上下文
│   ├── core/                    # 路径、日志、设置、加密、指纹、JSON 存储、inflate
│   ├── api/                     # HttpClient / NeteaseClient / KugouClient / 数据模型
│   ├── account/                 # 加密的账号存储
│   ├── player/                  # 播放引擎、队列、原生媒体控制
│   ├── download/                # 下载队列 + 标签写入
│   └── ui/                      # QML 控制器与模型（AppController / BrowseController / CatalogModel / IconProvider / QrCode 等）
├── qml/                         # QML 界面（Theme 单例 + components + pages）
├── res/                         # SVG 图标 + res.qrc
├── UiDesign/                    # Logo.svg 及版式参考图
├── docs/screenshots/            # 界面截图（见 §8）
├── third_party/qrcodegen/       # 二维码生成（MIT，Nayuki）
├── tests/                       # selftest（加密/指纹自检）与开发用 API 探针
├── translations/                # i18n
├── android/                     # Android 打包源（Manifest、Java MediaSession）
├── packaging/                   # Linux desktop 文件等
├── scripts/                     # 各平台构建/打包脚本
│   ├── package-windows.ps1      # cmake install + windeployqt
│   ├── package-linux.sh         # linuxdeploy → AppImage
│   ├── package-macos.sh         # macdeployqt → dmg
│   ├── package-android.ps1      # 本机 Android 打包辅助
│   ├── build-android.ps1        # 本地 Windows 构建 + 自签名 APK
│   ├── build-android.sh         # CI (Linux) 构建 + 自签名 APK
│   ├── update-translations.ps1  # lupdate/lrelease
│   ├── check-colors.ps1         # 截图取色（验证图标着色）
│   ├── render-icons.ps1         # 由 UiDesign/Logo.svg 渲染全部图标
│   └── check-workflow.py        # 工作流结构自检（PyYAML）
└── .github/workflows/build.yml  # 多平台 CI（见 §9）
```

---

## 3. 构建（Windows x64，首要平台）

把 `<QT_WINDOWS>`、`<MSVC_VCVARS>` 等替换为 `PERSONAL.md` 中的实际值。

```bat
call "<QT_WINDOWS>\bin\qtenv2.bat"
call "<MSVC_VCVARS>"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release ^
      -DCMAKE_PREFIX_PATH=<QT_WINDOWS>
cmake --build build
```

产物：`build/MrMusic.exe`（运行时数据写入 `build/data/`，也就是可执行文件旁的 `data/`）。

运行自检（加密算法、指纹、inflate、PBKDF2）：

```bat
build\mrmusic_selftest.exe
```

安装 + 部署（windeployqt）：

```powershell
pwsh scripts/package-windows.ps1
```

## 4. 构建（Android arm64）

把 `<QT_ANDROID>`、`<QT_WINDOWS>`、`<ANDROID_SDK>`、`<ANDROID_NDK>` 替换为 `PERSONAL.md` 中的实际值。

```bat
call "<QT_ANDROID>\bin\qtenv2.bat"

cmake -S . -B build-android -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE=<QT_ANDROID>/lib/cmake/Qt6/qt.toolchain.cmake ^
  -DQT_HOST_PATH=<QT_WINDOWS> ^
  -DANDROID_SDK_ROOT=<ANDROID_SDK> ^
  -DANDROID_NDK_ROOT=<ANDROID_NDK> ^
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 ^
  -DQT_ANDROID_PACKAGE_SOURCE_DIR=$PWD/android
cmake --build build-android
```

`scripts/build-android.ps1` 封装了同样的流程，并在最后用 `zipalign` + `apksigner` 生成自签名 APK。

> **构建约定（TARGET.md §9.14）**：日常改动只做 Windows x64 构建 + 自检 + 截图验证；**Android APK 只在用户明确要求时构建**（`pwsh scripts/build-android.ps1`），平时保持源码可编译即可。

---

## 5. 运行期数据布局

| 文件 | 内容 | 是否加密 |
| --- | --- | --- |
| `<exe>/data/config.json` | 设置（音质、下载目录、主题、代理…） | 否 |
| `<exe>/data/library.json` | 播放历史；账号收藏在离线/未登录时作为本地回落 | 否 |
| `<exe>/data/accounts.json` | 两平台 token / cookie / 匿名 token / 资料 | **是**（默认） |
| `<exe>/data/.salt` | 每台设备独立的随机盐 | — |
| `<exe>/data/cache/` | 封面、音频等缓存 | 否 |
| `<exe>/data/logs/mrmusic.log` | 日志 | 否 |

数据目录规则遵循 TARGET.md §5.2：Windows / Linux 为可执行文件旁的 `data/`；macOS / Android 使用系统标准应用数据目录。

### 加密方案

```
key = PBKDF2-HMAC-SHA256(seed, salt, 120000 rounds, 32 bytes)
file = { "iv": base64, "data": AES-256-CBC(json), "mac": HMAC-SHA256(iv||data) }
```

* 设备种子来自 `QSysInfo::machineUniqueId()`（Qt 在不同系统上取值不同，例如 Windows MachineGuid、Linux `/etc/machine-id`）；若取不到，则回退到主机名 / 系统类型 / 内核版本等组合。种子与派生结果均经 SHA-256 处理，用户不可修改。
* 盐保存在 `.salt`，随机生成一次；指纹 + 盐命中失败（换机、删盐）时提示重新登录。
* 设置中可切换「明文存储」，此时直接写 JSON 对象。
* 所有写入经 `QSaveFile`（临时文件 + 原子替换），崩溃不会写坏文件。

---

## 6. 架构

```
QML (Theme/components/pages)
    │  上下文属性：app, browse, player, library, accounts, downloads, settings
    ▼
ui/ (AppController, BrowseController, PlayerController, LibraryController,
     AccountController, DownloadManager, TrackListModel, CatalogModel,
     IconProvider, QrCode)
    ▼
api/ (NeteaseClient, KugouClient → HttpClient)
    │  签名 / 加密：core/Crypto (AES, RSA, MD5, SHA, PBKDF2)
    ▼
上游：music.163.com / interface.music.163.com / gateway.kugou.com / tracker.kugou.com / lyrics.kugou.com …
```

* `core/Crypto` 自带 AES-128/192/256（CBC/ECB，PKCS#7）、RSA（PKCS#1 v1.5 与 raw modpow）、MD5/SHA1/SHA256/HMAC/PBKDF2，**不依赖 OpenSSL**，因此 Windows / Android 无需额外二进制依赖。PBKDF2 用预计算 ipad/opad 的 HMAC 实现，启动时的密钥派生约 0.4 秒。
* `core/Inflate` 自带 zlib 解压，用于酷狗 KRC 歌词（XOR + zlib）。
* `ui/QrCode` 使用 vendored 的 Nayuki qrcodegen（MIT）生成登录二维码。
* `ui/IconProvider` 是 QML 图片提供器 `image://mricon/<name>?c=<颜色>&s=<像素>`：把 `res/*.svg` 里的 `currentColor` 替换成目标颜色后再用 `QSvgRenderer` 光栅化。**不使用 `MultiEffect`/着色器着色**（那套方案实测在运行时失效，图标会全黑）。

---

## 7. 已知限制

1. **酷狗播放地址**：`/v5/url` 与 `/v6/priv_url` 均已实现，但未登录时服务端对多数曲目返回 `Hash not found` / 需要校验；登录（含 VIP）后带 token 请求。失败时界面会给出服务端返回的明确原因，不会静默降级。
2. **网易云匿名 token**：`/api/register/anonimous` 目前返回 `code 400`（不影响搜索 / 播放 / 歌词 / 二维码登录；每日推荐使用 weapi，未登录时返回需要登录）。
3. **M4A 标签**：MP3 / FLAC 会写入完整标签，M4A 暂不写入（当前四档音质只请求 mp3 / flac）。
4. **Linux MPRIS / Android MediaSession / macOS Now Playing**：代码骨架已就位（MPRIS 需 Qt6::DBus，Android 见 `android/src`，macOS 走 `MPRemoteCommandCenter`），需在对应平台上做最终联调；Windows SMTC 已在本机验证（日志 `SMTC initialised`）。
5. iOS 按需求不支持。

---

## 8. 测试与开发用开关

```bat
build\mrmusic_selftest.exe            :: 单元自检（AES KAT、RSA、PBKDF2、inflate、指纹）
```

自动化 / 截图核对用的命令行开关（TARGET.md §9.6）：

| 开关 | 作用 |
| --- | --- |
| `--search <关键字>` | 启动后立即搜索两个平台 |
| `--search-kind <0\|1\|2>` | 搜索类型：0 单曲 / 1 专辑 / 2 MV（须在 `--search` 之前解析） |
| `--play-first` | 搜索完成后播放第一条结果 |
| `--download-first` `--download-dir <目录>` | 搜索完成后下载第一条结果到指定目录 |
| `--page <0..4>` | 启动时切页（0 首页 / 1 最近 / 2 收藏 / 3 设置 / 4 搜索，由 `qml/Main.qml` 读取） |
| `--compact` `--size <W>x<H>` | 强制手机布局 / 指定窗口尺寸 |
| `--open-first <artist\|album\|mv>` | 自动打开第一条结果对应的歌手页 / 专辑页 / 播放 MV（QML 侧解析） |
| `--screenshot <文件> [--screenshot-delay <ms>]` | 渲染完成后截图并退出；带此开关会跳过单实例限制 |
| `--quit-after <ms>` | 延迟后走与托盘「退出」相同的退出路径 |
| `--allow-multiple` | 跳过单实例限制（与 `--screenshot` 二者之一即可绕过；其余开关单独使用仍会受单实例约束） |
| `--render-svg <in.svg> <out.png> <px> [background]` | 用 `QSvgRenderer` 渲染图标（供 `scripts/render-icons.ps1` 使用），执行后直接退出 |

界面截图见 `docs/screenshots/`：`home.png` / `home-light.png`（深浅色首页）、`phone.png`（手机布局）、`search.png` / `search-albums.png`（单曲 / 专辑搜索）、`album.png`、`artist.png`、`mv.png`、`favorites.png`、`settings.png`。

---

## 9. GitHub Actions 自动构建

工作流：`.github/workflows/build.yml`

| 触发 | 行为 |
| --- | --- |
| `push`（main / master） | 构建 Windows + Linux + macOS，跑 `mrmusic_selftest`，上传三份产物 |
| `pull_request` | 同上（用于验证 PR，可自动取消旧运行） |
| `push` tag `v*` | 在上一行基础上**额外构建 Android APK**，并把所有产物发布到 GitHub Release |
| `workflow_dispatch` | 手动触发，可指定 `qt_version`，并用 `build_android` 勾选是否构建 APK |

产物（`actions/upload-artifact`，保留 14 天）：

| Job | 产物 | 说明 |
| --- | --- | --- |
| `windows`（`windows-latest`） | `MrMusic-windows-x64.zip` | MSVC + Ninja 构建，`windeployqt` 部署后 `Compress-Archive` 打包 `dist/bin/` |
| `linux`（`ubuntu-22.04`） | `MrMusic-linux-x64-appimage` | `linuxdeploy` 生成 AppImage（`APPIMAGE_EXTRACT_AND_RUN=1`，适配无 FUSE 的容器） |
| `macos`（`macos-15`） | `MrMusic-macos` | `macdeployqt` 生成 dmg |
| `android`（`ubuntu-22.04`） | `MrMusic-android-arm64` | NDK `27.2.12479018` + JDK 17；`scripts/build-android.sh` 构建并用 `apksigner` 自签名 |
| `release` | GitHub Release | 仅 tag 触发，汇总全部产物并自动生成变更说明 |

要点与配置：

* Qt 版本由 `env.QT_VERSION` 控制（默认 `6.11.1`），通过 `jurplel/install-qt-action` 安装并启用缓存。
* Windows 用 `ilammy/msvc-dev-cmd` 注入 MSVC 环境；若镜像缺少 Ninja 会 `choco install ninja`。
* Android 签名：仓库 Secret `ANDROID_KEYSTORE_BASE64`（`base64` 后的 keystore）可选；未配置时 CI 现场生成一次性自签名证书（口令 `mrmusic`，别名 `mrmusic`）。
* 所有桌面 job 显式安装 `qtmultimedia` 模块（`modules: qtmultimedia`），因为 `CMakeLists.txt` 要求 `Qt6::Multimedia`，而 install-qt-action 默认只装基础模块；Android 目标 Qt 不装（该仓库无此 addon）。
* Windows job 通过 `aqtsource` 使用 aqtinstall 的 6.11-Windows-布局修复版（tarball）：Qt 6.11 起 Windows 仓库目录改为 `qt6_6111_msvc2022_64/`，PyPI 上的 aqt 3.3.0 尚不认识，会报 `Failed to locate XML data`。
* Android job 默认**不随普通 push 运行**（符合 TARGET.md §9.14：APK 仅在明确需要时构建）。
* `windows` / `linux` / `macos` 三个 job 会执行 `mrmusic_selftest`（AES/RSA/PBKDF2/inflate/指纹自检），失败即中断；`android` 与 `release` job 不跑自检（前者只构建 APK，后者只汇总产物）。
* 首次真实运行（2026-11-08）修复了以上三类失败；该工作流仍建议在推送后按实际 Runner 环境复核。
