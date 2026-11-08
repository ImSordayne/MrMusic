# MrMusic 项目说明

> **占位符约定 · 从 `PERSONAL.md` 读取（本轮新增，见 §10.24）**
>
> 本文档凡出现 `<大写字母/数字/下划线>` 形式的占位符，均代表**本机私有路径**，不在文档里写死绝对路径。实际值统一记录在仓库根目录 [`PERSONAL.md`](PERSONAL.md) 的 `# PERSONAL_PATH` 段（每行 `KEY = VALUE`）。阅读或执行本文中的构建命令时，用 `PERSONAL.md` 中同名键的值替换占位符，例如 `<ANDROID_SDK>` → `PERSONAL.md` 的 `ANDROID_SDK`。
>
> | 占位符 | 含义 | `PERSONAL.md` 键 |
> | --- | --- | --- |
> | `<QT_ROOT>` | Qt 6.11.1 安装根目录 | `QT_ROOT` |
> | `<QT_WINDOWS>` | Windows x64 Qt kit（`msvc2022_64`） | `QT_WINDOWS` |
> | `<QT_ANDROID>` | Android arm64 Qt kit（`android_arm64_v8a`） | `QT_ANDROID` |
> | `<QT_TOOLS>` | CMake / Ninja 工具目录 | `QT_TOOLS` |
> | `<MSVC_VCVARS>` | `vcvars64.bat` 完整路径 | `MSVC_VCVARS` |
> | `<PWSH_DIR>` | 本机 PowerShell 7 目录 | `PWSH_DIR` |
> | `<ANDROID_HOME>` | Android SDK 的上级目录 | `ANDROID_HOME` |
> | `<ANDROID_SDK>` | Android SDK 根目录 | `ANDROID_SDK` |
> | `<ANDROID_NDK>` | NDK `27.2.12479018` 目录 | `ANDROID_NDK` |
> | `<JDK17>` | JDK 17 根目录 | `JDK17` |
> | `<MUSICAPI_REPO>` | NeteaseCloudMusicApi / KuGouMusicApi 参考仓库 | `MUSICAPI_REPO` |
>
> `C:\Windows\System32\cmd.exe`、NDK 版本号 `27.2.12479018` 等属于系统 / 版本常量，不属于本机私有路径，仍直接写在文中。

---

## 0. 项目概述

编写一个跨平台音乐播放软件 **MrMusic**，使用
`<MUSICAPI_REPO>` 下两个开源项目（**NeteaseCloudMusicApi**、**KuGouMusicApi**，同时使用）所引用的上游音乐 API，用 Qt 6 + QML 实现。

关键约束：**只使用它们引用的上游 API，不使用它们代理/包装出来的本地 HTTP 接口**。即不启动 Node 服务，请求逻辑（含签名、加密、cookie 维护）全部用 C++ 重写。

---

## 1. 数据来源与请求实现

1. 双平台数据源：网易云音乐、酷狗音乐，二者**同时**支持。
2. 请求实现：**C++ + Qt Network + OpenSSL** 重写，不依赖 Node.js，不启动本地服务进程。
3. 允许**直接照抄**上游 JS 实现中的签名/加密算法与常量（RSA 公钥、AES 密钥、appid、clientver、`x-router` 头等）。
4. 各平台请求要点（移植时须保持等价）：
   - 酷狗：模块声明的 `encryptType`（如 `android` 走 RSA + AES，另有 weapi 等变体）、`x-router` 头、cookie/dfid 维护、设备指纹（对应上游 `.env` 的 `KUGOU_API_GUID / DEV / MAC / WEBGL`）。
   - 网易云：weapi/eapi 加密、`anonymous_token` 生成与刷新、cookie 维护。
5. 设备指纹（酷狗所需）**取系统指纹自动生成**，**不允许用户修改**。
6. 统一数据模型：歌曲等实体带 `source`（netease / kugou）与平台内 ID，界面列表项显示"来源"（见 `UiDesign/ListItem.png`）。
7. 端点的选取、URL 与参数以两个上游项目对应 module 的现实现为准；失败、限流、封禁的处理由实现方按重试/退避策略处理。

---

## 2. 功能范围

### 2.1 歌曲搜索

- 两个平台**都搜索**，结果**合并为一个列表**。
- 提供**来源筛选项**，可按 netease / kugou 过滤。

### 2.2 每日推荐（首页）

- 首页分别显示两个条目：**「网易云 - 每日推荐」**、**「酷狗 - 每日推荐」**。

### 2.3 基础播放

- 播放/暂停、上一首/下一首、进度、音量、播放队列、循环/随机等基础能力。
- 音质按**用户选择的档位**请求；遇到无版权或 VIP 受限曲目时**明确提示"无版权"/"VIP"**，不做静默降级。
- 后台播放与媒体键（播放/暂停/切歌）**调用各系统原生 API**：Windows SMTC、macOS Now Playing、Linux MPRIS、Android MediaSession/通知栏。
- 音频后端统一使用 **FFmpeg**。

### 2.4 歌曲下载

- 用户可指定下载目录，默认**系统音乐文件夹**，支持修改。
- 文件命名：`作者-专辑-音质-平台.<扩展名>`，扩展名按实际音频格式使用各自的（如 `.flac` / `.mp3` / `.m4a`），不统一为某一容器格式。
- 下载完成后写入 **标签 / 歌词 / 封面**。
- 并发限制：单曲不并发（每首歌单连接）；**单平台最多同时下载 5 首**。
- VIP 曲目**允许下载**。

### 2.5 登录

- 扫码登录 + 账密登录。
- **两个平台各自独立登录**；不要求单平台多账号切换。

---

## 3. 用户数据与安全

1. **token / cookie 等重要用户数据加密存储**于 JSON 文件中（其余非敏感字段可明文）。
2. 加密密钥由**系统指纹 + 每设备随机盐**通过 **PBKDF2-HMAC-SHA256（哈希 + 盐）**派生，**用户不可修改、不可自定义**（见 §9.2）。
3. 设置中允许用户选择**明文存储**（放弃加密，直接存明文 JSON）；默认加密。
4. 写入保持**原子性**（临时文件 + 替换，防崩溃写坏）。
5. 文件损坏或密钥不匹配时，**提示用户重新登录**。

---

## 4. 技术栈与工程

- UI：**Qt QML**（Qt Quick Controls 2）。
- Qt 版本：**6.11**；Qt 模块**按需引入**（Qt Quick / Quick Controls / Network / Multimedia / Sql / Svg 等，用到才加）。
- 构建：**CMake**。
- Widget 使用规则：**Android 端纯 QML，不使用 Widget**；其他平台允许但尽量不使用。

---

## 5. 平台与打包

| 平台 | 最低要求 | 打包方式 |
| --- | --- | --- |
| Windows（**首要平台**） | Windows 10+ | `windeployqt` |
| macOS | macOS 15 | `macdeployqt` |
| Linux | 5.x 内核 | AppImage 或 Zip（包含全部依赖） |
| Android | Android 10（API 29） | 自签名 APK |
| iOS | **不支持（明确排除）** | — |

开发优先级：**Windows 优先**，其余平台随后补齐。

### 5.1 构建策略

- 本机只编译并测试 **Windows x64** 与 **Android arm64** 两个目标。
- 其余平台/架构（macOS、Linux、以及未在本机验证的 ABI）**由 GitHub Actions 构建**。
- 使用环境:`C:\Windows\System32\cmd.exe /k "call "<QT_WINDOWS>\bin\qtenv2.bat" && call "<MSVC_VCVARS>" && set PATH=<PWSH_DIR>;%PATH%"`
- 如果需要openssl等额外依赖则使用vcpkg管理
- Android SDK使用`<ANDROID_HOME>`

### 5.2 数据目录

- **Windows / Linux**：程序数据直接存放在 `<可执行文件所在目录>/data/`（即安装/解压目录下的 `data`）。
- macOS / Android：本文档未作规定，由实现方按各系统标准应用数据目录决定（见 §7）。


---

## 6. UI 设计说明

- 使用**侧栏（桌面）/ 底部导航栏（手机）**切换页面；对应图标被选中时切换为 **fill 变体**（`res/*.svg` 与 `res/*-fill.svg` 成对使用）。
- 桌面端左下角两枚圆形头像：**左网易云、右酷狗**；手机端为**左网易云、右酷狗**。
- 歌曲列表项样式参照 `UiDesign/ListItem.png`（含"来源"标识）。
- 版式参照图：`UiDesign/computer.png`（桌面）、`UiDesign/phone.png`（手机）、`UiDesign/ListItem.png`（列表项）。
- `UiDesign/` 中的图**仅作为模板/风格参考**；配色、字体、尺寸、间距、圆角、深浅色、空态/加载态/错误态等具体视觉参数由实现方自行设计决定。

---

## 7. 实现方自主决定的工程细节

以下不属于需求约束，由实现方自行决策并保持内部一致：源码与目录结构（`src/`、`tests/`、`translations/` 等）、API 客户端层与模型层的划分、**直连上游端点清单的整理**（搜索 / 每日推荐 / 播放地址 / 下载地址 / 二维码登录 / token 刷新，作为"不使用代理接口"的验收依据）、缓存策略（封面/歌词/音频）、日志、i18n、代理设置、错误提示文案、测试与 CI、代码风格与提交规范、以及 §6 所述的全部视觉设计参数。

现有目录约定：

1. `res/` — 资源（图标 SVG 等）
2. `UiDesign/` — UI 设计稿（png）
3. `qml/` — QML 界面源码

---

## 8. 已确认的收尾决定

- 下载文件扩展名：按各平台/各音质的实际音频格式使用各自的扩展名（见 §2.4）。
- 直连上游端点清单：不在本文档中维护，由实现方在开发过程中整理（见 §7）。
- 设备指纹与加密密钥：均由系统指纹自动派生，用户不可修改；仅在设置中提供"明文存储"开关作为用户选项（见 §1.5、§3、§9.2）。
- UI 视觉参数：由实现方自行设计，`UiDesign/` 仅提供模板参考（见 §6）。
- 构建与数据目录：本机只做 Windows x64 / Android arm64，其余平台交给 GitHub Actions；Windows / Linux 数据目录为可执行文件旁的 `data/`（见 §5.1、§5.2）。

---

## 9. 会话中新增的要求（按时间顺序，均需实现）

1. **开发顺序：先完成 Windows 版本。** Windows x64 需构建通过、可运行、功能自测通过之后，再补齐 Android 及其它平台。
2. **用户数据加密改为「哈希 + 盐」。** 密钥不再直接使用指纹哈希，而是
   `PBKDF2-HMAC-SHA256(系统指纹种子, 每设备随机盐, 迭代 120000, 32 字节)`；
   盐随机生成一次并保存在 `<data>/.salt`，指纹与盐都不可由用户修改（同时覆盖 §3.2）。
3. **流程要求（协作约定）：** 每当出现新的要求，**先把所有新要求同步写入本文件（TARGET.md），再修改代码**；实现偏离或补充决定同样记录在案。
4. **本机构建环境（补充 §5.1）：**
   - Windows：`<QT_WINDOWS>` + MSVC 19.51（VS 18 Community）+ CMake/Ninja（`<QT_TOOLS>`）。
   - Android：SDK `<ANDROID_HOME>\Sdk`（含 NDK `27.2.12479018`、platform-tools、build-tools），JDK 17（`<JDK17>`）。
5. **加密实现方式：** §1.2 提到的 OpenSSL 并非硬性依赖；按 §5.1「需要时再用 vcpkg」的原则，本期采用**自带加密原语**（AES-128/192/256、RSA modpow、MD5/SHA/HMAC/PBKDF2、zlib inflate 全部内置实现），因此 Windows / Android 均无额外二进制依赖。若后续确有需要（如新增协议要求 TLS 客户端证书），再按 §5.1 引入 vcpkg + OpenSSL。
6. **界面可验证性：** 为方便本机验证，程序需支持开发用命令行开关（`--search`、`--play-first`、`--download-first`、`--download-dir`、`--page`、`--compact`、`--screenshot[--screenshot-delay]`），用于自动化冒烟测试与截图核对。
7. **关闭行为可配置（桌面端）：** 设置中新增「关闭窗口时的行为」选项：
   - **在托盘运行**（Windows/macOS 最小化到系统托盘图标；Linux 无托盘支持时等价于在后台继续运行）；
   - **直接关闭**（退出程序）。
   默认值：在托盘运行。Android 端不适用（返回键行为由系统决定）。
8. **单实例运行：** 同一时刻只允许一个 MrMusic 实例。当已有实例在后台（托盘/最小化）运行时，再次启动程序应**唤起已有实例的 UI**（显示、还原、置顶并聚焦），而不是启动第二个 App；开发用命令行开关（见 §9.6）运行时不受此限制，以便自动化测试。
9. **酷狗每日推荐必须显示封面。** 酷狗 `/everyday_song_recommend` 的返回本身不含封面字段，实现方需在拿到列表后**补齐封面**（批量查询歌曲资源信息），不得以占位图标长期展示。
10. **系统托盘「退出」必须真正结束进程。** 托盘菜单中的退出项需终止程序（而不是只隐藏窗口或仅退出窗口事件循环）。
11. **侧栏/底部导航图标颜色随主题明暗切换：** 深色主题下为**白色**，浅色主题下为**黑色**（选中态通过 fill 变体 + 胶囊底色区分）。
12. **图标着色必须真的生效（回归修正）：** 首版用 `MultiEffect` 做染色，实测运行时完全不生效（截图像素采样显示侧栏图标仍是 `#000000`，播放按钮的三角也是黑色）。改为**不依赖着色器**的方案：C++ 侧 `QQuickImageProvider` 把 SVG 中的 `currentColor` 替换为目标颜色后光栅化（`image://mricon/<name>?c=<颜色>&s=<像素>`），保证任意主题/任意渲染后端下颜色都正确。
13. **默认头像不得与文字重叠（回归修正）：** 未登录时头像位只能显示**一种**占位内容——统一显示平台字符（左「网」/右「酷」），不得再叠加人形图标；已登录但头像图片加载失败时同样回退为平台字符。
14. **构建范围收敛（流程要求）：** 除非用户**明确要求**，实现方**不再构建 Android APK**。日常改动只做 **Windows x64 构建 + 自检 + 截图**验证即可；Android 源码需始终保持可编译，但构建/打包改由用户按需触发（`scripts/build-android.ps1`）。GitHub Actions 中的 Android job 保留，仅在 CI 或用户要求时运行。
15. **GitHub Actions 自动构建（交付要求）：** 仓库需提供可直接使用的 CI 工作流，覆盖：
   - `push`（main/master）/ `pull_request`：自动构建 **Windows x64、Linux x64、macOS** 并上传产物，且必须执行 `mrmusic_selftest`（失败即中断）；
   - 打 `v*` tag：额外构建 **Android arm64 APK（自签名）**，并把全部产物发布到 GitHub Release；
   - `workflow_dispatch`：可手动触发，支持指定 Qt 版本与是否构建 APK；
   - 产物形式：Windows = `windeployqt` 后的 zip、Linux = AppImage、macOS = dmg、Android = 自签名 apk；
   - Android 签名优先使用 Secret `ANDROID_KEYSTORE_BASE64`，未配置时由 CI 生成一次性自签名证书。
16. **Android 包名：** APK 的 `applicationId` / manifest `package` 统一为 **`io.sordayne.github.mrmusic`**；Java 侧包名、`MediaActionReceiver` 的 intent action、C++ 侧 JNI 类名同步改成同一路径（`io/sordayne/github/mrmusic/...`）。macOS bundle identifier 同步为同一标识，保持各平台一致。
17. **Logo：直接使用 `UiDesign/Logo.svg`（不做美化）。** 该文件是最终标志的唯一来源（音符形状的礼帽，描边风格）。
    - 图标全部由它渲染，不再使用自制插画：`scripts/render-icons.ps1` 调用程序内置的 `--render-svg <svg> <out.png> <px> [背景]` 开关（QSvgRenderer），生成 `res/app.png`(256)、`res/logo-512.png`(512)、`UiDesign/Logo.png`(1024 透明) 与 Android `mipmap-*`（48/72/96/144/192）；
    - 应用图标使用**白色圆角底板**（`#FFFFFF:round`），保证黑色描边在深色任务栏/启动器中仍然可见；
    - 界面内部使用 `res/logo-mark.svg`（同一图形，描边改为 `currentColor`），经 `IconProvider` 着色：深色主题白色、浅色主题黑色，放在桌面侧栏标题「MrMusic」左侧（侧栏宽度 78 → 104 px 以容纳「标志 + 文字」）；
    - 落地范围：窗口/托盘图标、安装包图标、Android 启动图标、侧栏标题标志。

---

## 10. 第二阶段功能（本轮新增要求）

18. **搜索页返回按钮：** 搜索页左上角提供返回按钮，回到进入搜索前的页面；键盘/手势返回同样有效。
19. **搜索支持多类型结果：** 搜索页提供结果类型切换（**单曲 / 专辑 / MV**），三类结果都来自各平台的上游搜索接口（网易云 `type=1/10/1004`，酷狗对应 song/album/mv 搜索），仍按平台合并展示并保留来源筛选。
20. **歌手页 / 专辑页：**
    - 歌曲列表每一项的**歌手名**可点击 → 打开**歌手页**：显示歌手头像/简介、其**音乐**（热门/全部歌曲）、**专辑**、**MV** 三个分页，并提供**关注 / 取消关注歌手**按钮；
    - 歌曲列表每一项的**专辑名**（以及专辑搜索结果、专辑封面所在行）可点击 → 打开**专辑页**：显示专辑封面/发行信息、**专辑包含的歌曲**，并提供**收藏 / 取消收藏专辑**按钮。
    - 两个页面都支持从页面内继续跳转（歌曲 → 歌手/专辑 → 歌曲 …），并有返回栈。
21. **收藏改为账号维度（走 API）：** 歌曲收藏不再只写本地 JSON，而是调用两个平台的**收藏/喜欢接口**写入账号收藏夹；界面收藏页展示账号收藏（网易云「我喜欢的音乐」/ 酷狗收藏），并新增**收藏的专辑**与**收藏的 MV** 分页，可查看并可取消收藏。未登录时点击收藏要给出明确提示（提示登录，而不是静默失败）。
    - 本地 `library.json` 仍保留：作为**历史记录**与未登录时的**本地收藏**（离线兜底），登录后以账号收藏为准并与本地合并展示。

**实现进度（截至本轮）**

| 条目 | 状态 | 说明 |
| --- | --- | --- |
| 18 搜索页返回按钮 | ✅ 完成 | 搜索页左上角返回箭头 → 回到进入搜索前的页面（`window.leaveSearch()`） |
| 19 搜索多类型 | ✅ 完成 | 单曲/专辑/MV 三档：网易云 `/api/cloudsearch/pc` type=1/10/1004，酷狗 `/v3/search/song`、`/v1/search/album`、`/v1/search/mv`，结果按平台合并，专辑/MV 分页可「加载更多」 |
| 20 歌手页 / 专辑页 | ✅ 完成 | 网易云 `/api/artist/head/info/get`、`/api/v1/artist/songs`、`/api/artist/albums/{id}`、`/api/artist/mvs`、`/api/v1/album/{id}`；酷狗 `/kmr/v3/author`、`/kmr/v1/audio_group/author`、`/kmr/v1/author/albums`、`/kmr/v1/author/videos`、`/kmr/v2/albums`、`/v1/album_audio/lite`。歌曲行的**歌手名 / 专辑名可点击**跳转，专辑页的歌手名同样可点击 |
| 21 收藏走账号 API | ✅ 完成（酷狗待补） | 网易云：歌曲收藏走 `/api/radio/like` 并同步本地；收藏页三个分页（歌曲/专辑/MV）分别来自 `/api/song/like/get` + `/api/v3/song/detail`、`/api/album/sublist`、`/api/cloudvideo/allvideo/sublist`，专辑/MV 可直接取消收藏；未登录时给出明确提示并回落到本地收藏。酷狗：MV 收藏列表接口已实现，歌曲收藏/专辑收藏/关注歌手接口尚未接入（按钮会提示） |

22. **MV 播放接入播放器：** 点击 MV（搜索结果 / 歌手页 MV 分页 / 收藏的 MV）应直接在播放器中播放该 MV，播放条显示名字、歌手与封面；网易云走 `/api/song/enhance/play/mv/url`，酷狗走 `/v2/interface/index`（`trackermv.kugou.com`，带 `hash`）。播放器提供「MV 画面」开关：打开后弹出视频画面窗口（Qt Multimedia 的 `QVideoSink` + QML `VideoOutput`），关闭后回到纯音频播放。MV 播放不进入歌曲队列（队列为空时也不影响）；酷狗 MV 若取不到地址需给出明确提示。
   - **已完成并实测**：`PlayerController::playMv()` 解析地址后交给 Qt Multimedia 播放（网易云 mp4 / 酷狗 mkv，均由 FFmpeg 解码）；播放条显示 MV 名、歌手、封面与「正在播放 MV」状态，并出现「MV」按钮；点开后弹出 16:9 画面窗口（`QVideoSink` + QML `VideoOutput`），关闭即回到纯音频。实测酷狗 MV《晴天》画面正常渲染（截图 `docs/screenshots/mv.png`），网易云 MV 地址接口返回有效 mp4。酷狗封面改用 `https://imge.kugou.com/mvhdpic/240/<file>` 修正 404。

23. **协作约定（重申并升级 §9.3）：** 每次有新的要求，**必须先把要求原文写入本文件（TARGET.md）对应章节，再动代码**。本条由用户再次强调，适用范围包括功能需求、流程约束与文档调整。
24. **本机私有路径抽离到 `PERSONAL.md`：** TARGET.md 中不得写死本机绝对路径（Qt 安装目录、MSVC、Android SDK/NDK、JDK、上游 API 参考仓库等）。统一改为**占位符**（如 `<ANDROID_SDK>`），实际值记录在仓库根目录 `PERSONAL.md` 的 `# PERSONAL_PATH` 段（`KEY = VALUE` 一行一个）；`PERSONAL.md` 不进版本库（加入 `.gitignore`）。占位符清单见本文开头「占位符约定」。涉及路径的脚本（`scripts/build-android.ps1` 等）保留默认参数为占位符对应的本机值或改为从环境变量读取，但文档不再硬编码。
   - **已完成**：全文路径替换为占位符；`PERSONAL.md` 已创建并加入 `.gitignore`。
25. **审查 README.md 的严谨性（本轮，用户澄清）：** 先逐条核对 README.md 与代码 / 文件系统的实际情况，**列出不严谨、过时或与实现不符之处**，确认后再修改；不直接重写。重点核对：私有路径是否仍写死（见 §10.24）、徽章与外链、功能状态表里的类名、QML 上下文属性清单、开发开关清单、截图说明、目录结构与 CI 描述。
   - **已完成**：审查清单交由用户确认后按条修正 README.md——私有路径改占位符并说明从 `PERSONAL.md` 读取；修掉失实的类名 `ApiClient`、CI「所有 job 都跑自检」、版本号 0.0.1、无效徽章/空链接；补齐 `browse` 上下文属性、`BrowseController`/`CatalogModel`/`IconProvider`/`QrCode`、14 个开发开关与 10 张截图清单、目录结构缺项；修正指纹实现与 `library.json` 描述；外链补 `https://`。
26. **提交并推送（本轮交付动作）：** 文档整理完成后执行 `git add .`、`git commit -m "提交代码"`、`git push -u origin main`。注意 `PERSONAL.md` 含本机私有路径，必须保持被 `.gitignore` 忽略，不得进入版本库；`build/`、`dist/`、`*.apk` 等构建产物同样不得提交。
27. **修复 GitHub Actions 首次真实运行的失败（本轮）：** 依据 `D:\tmp\action_logs\` 的 CI 日志排查并修复，至少包括：① macOS job 报 `The configuration 'macos-14-arm64-us-default' is not supported`；② Linux `Configure` 阶段 `find_package` 找不到 Qt `Multimedia` 组件（install-qt-action 需显式装模块）；③ Windows `Install Qt` 阶段 aqt 找不到 Qt `6.11.1` 的 `Updates.xml`。修复后保持 Windows x64 本机构建 + 自检通过，并重新提交推送。
   - **根因与修复（均已实测验证）**：
     - ① `macos-14` 已退役，runner 分配阶段即失败；改为 `macos-15`（用户提供的 runner 表中 macOS 可用标签）。
     - ② install-qt-action 默认不装 addon，日志显示只下载了 qtbase/qtsvg/qtdeclarative…，而 `CMakeLists.txt:21` 要求 `Qt6::Multimedia`；给 Windows/Linux/macOS 与 Android host Qt 的安装步骤加 `modules: qtmultimedia`。Android 目标 Qt **不加**（实测 Android 6.11 仓库的 `Updates.xml` 不含 qtmultimedia 包，加了会报错）。
     - ③ **不是镜像问题**：Qt 6.11 起 Windows 仓库改版为 `qt6_6111/qt6_6111_msvc2022_64/Updates.xml`（官方实测 200），而 aqt 3.3.0（PyPI 最新）仍拼老路径 `qt6_6111/qt6_6111/Updates.xml`（官方实测 404）；Linux/macOS 6.11.1 仍是老布局故不受影响。aqtinstall master 已加入 `EXTENSIONS_WIN_X64_QT6_11` 修复，但尚未发布。Windows job 用 `aqtsource: https://codeload.github.com/miurahr/aqtinstall/tar.gz/5a49f2b391ae8ee1d3c0dd87f8b33c1d49a4deb7` 装修复版（`v3.3.1.dev168`），用 tarball 而非 git 协议更稳。
     - 本地用该 aqt 版本 `--dry-run` 验证：Windows 正确解析新布局并落盘 `6.11.1/msvc2022_64`（与 install-qt-action 计算的 `QT_ROOT_DIR` 一致），`qtmultimedia` 在下载列表内；aqt 3.3.0 在 Linux 上带 `--modules qtmultimedia` 亦能正确解析。
28. **提交、推送并用 gh cli 跟踪 CI（本轮）：** 完成后依次执行 `git add .`、`git commit -m "修复构建"`、`git push -u origin main`，然后用 **GitHub CLI（`gh`）** 查看本次 push 触发的工作流运行情况；**若仍有失败，按日志继续修复并重复「改 → 提交 → 推送 → 查看」直到成功或明确受阻**。
   - **第 1 轮 gh 跟踪结果**：`gh run list` / `gh run view` / `gh api .../actions/jobs/<id>/logs` 显示上一轮修复已让三个平台全部通过 Configure（说明 `modules: qtmultimedia` 与 Windows aqt 修复生效），但 **Build/link 阶段三个平台各自失败**，根因与修复：
     - **Linux** `ld: cannot open output file MrMusic: Is a directory`：Qt 把可执行 QML 模块的 tooling 输出放在 `<build>/MrMusic/`，与 Linux 无后缀可执行文件同名。给 Linux 目标设置 `RUNTIME_OUTPUT_DIRECTORY=<build>/bin`（install 规则不变，打包脚本零改动）。
     - **macOS** `Undefined symbols: MprisMediaControls::propertiesChanged()`：该类在 `.cpp` 内声明 `signals:` 但无 `Q_OBJECT`，moc 不生成实现；且 MPRIS 是 Linux D-Bus 规范，本不该在 macOS 启用。删除该无效 signal，并把 `MRMUSIC_HAVE_DBUS` 限定为 `CMAKE_SYSTEM_NAME STREQUAL "Linux"`。
     - **Windows** `LNK2019 WINRT_IMPL_RoGetActivationFactory / RoOriginateLanguageException`：CI 的 CMake 探测不到 C++/WinRT 头 → 定义 `MRMUSIC_NO_WINRT` 且不链 `windowsapp.lib`，但该宏**此前在源码中从未被使用**，WinRT 代码仍被编译。给 `MediaControls.cpp` 的 WinRT 分支加 `&& !defined(MRMUSIC_NO_WINRT)`，并把 `MRMUSIC_NO_WINRT` 提升为正式 CMake option。
   - **验证**：本机 Windows 正常构建仍启用 SMTC（`windowsapp` 在链接行）+ 自检通过；另以 `-DMRMUSIC_NO_WINRT=ON` 模拟 CI 构建，`MrMusic.exe` / `mrmusic_selftest.exe` 均生成、自检通过、`MediaControls.obj` 无 `WINRT_IMPL_*` 符号。
   - **第 2 轮 gh 跟踪结果**：三平台 Build + Self test **全部通过**；仅 Linux `Package AppImage` 失败：`file RPATH_SET could not write new RPATH ... The replacement path is too long for the RUNPATH entry`（Qt 部署脚本给插件写更长的 `$ORIGIN/../../lib/x86_64-linux-gnu`，而 CMake 的 `file(RPATH_SET)` 只能定长原地替换）。修复：CI 与 `scripts/package-linux.sh` 的 configure 增加 `-DQT_DEPLOY_USE_PATCHELF=ON`，让 Qt 用已安装的 `patchelf` 改写 RUNPATH（可增长），而非 CMake 定长 API。本地实测该 `-D` 会写入生成的 `.qt/QtDeploySupport.cmake`（`__QT_DEPLOY_USE_PATCHELF "ON"`）。
   - **最终结果**：gh 跟踪到 run `42452225408`（`8fac51e`）`conclusion=success`——`Windows x64` / `macOS` / `Linux x64` 三个 job 全部通过，三份产物均上传（`MrMusic-windows-x64` 76.8 MB、`MrMusic-linux-x64-appimage` 60.6 MB、`MrMusic-macos` 79.8 MB）；`Android arm64` 与 `Publish release` 按设计未触发（非 tag）。仅剩 `actions/checkout@v4` 等 Node.js 20 弃用警告，不影响构建。
29. **CI 触发与发版规则改版（本轮，覆盖 §9.14 的 CI 部分与 §9.15）：** 用户要求逐条如下：
    1. **workflow 在正常情况下也构建 Android**（每次 CI 都构建 arm64 APK）；§9.14「除非明确要求不构建 Android APK」**只约束本地开发**，CI 不受此限。
    2. **每次都执行 Publish release**，版本号取仓库根目录 `VERSION` 文件的内容（作为 tag / release 版本，不加额外前缀）。
    3. `VERSION` 内容以 `-alpha` / `-bata` / `-dev` 结尾时，release 推送为 **pre-release**。
    4. release 的 **Changelog 取 `CHANGLOG.md` 内容**（`body_path`），不用自动生成的 notes。
    5. **新增触发排除规则**：仅推送 `LICENSE` / `README.md` / `TARGET.md` / `VERSION` 等文档类文件时不触发工作流；仓库中存在 `NO_BUILD` 文件时也不触发工作流。
    - **实现解读（写在此处供复核）**：
      - 文件名按用户原文 `CHANGLOG.md`（注意与常见 `CHANGELOG.md` 拼写不同）；workflow 同时兼容 `CHANGELOG.md`（优先 `CHANGLOG.md`），避免之后改名失效。预发布后缀同时识别 `-bata` 与常见的 `-beta`（用户原文 `-bata` 疑为笔误），即后缀 `-(alpha|beta|bata|dev)`（可带 `.数字`）。
      - `paths-ignore` 具体化为：`LICENSE`、`README.md`、`TARGET.md`、`PERSONAL.md`、`API_ENDPOINTS.md`、`VERSION`、`CHANGLOG.md`、`CHANGELOG.md`、`NO_BUILD`、`**.md`、`docs/**`。GitHub 的 `paths-ignore` 语义是「**当且仅当本次所有变更文件都命中**」才跳过，与代码同批提交的文档不会让工作流跳过。
      - `NO_BUILD` 无法在 `on:` 阶段读取仓库文件内容（GitHub 不支持），因此新增 `precheck` job（checkout 后 `test -f NO_BUILD`），其余 job `needs: precheck` 且 `if: needs.precheck.outputs.build == 'true'`。**技术限制**：此时 run 仍会被创建，但全部构建/发版 job 被跳过；同时把 `NO_BUILD` 也列入 `paths-ignore`（推送该文件本身亦不触发）。`NO_BUILD` 不加入 `.gitignore`，以便可提交为全局开关。
      - 去掉 `on.push.tags: ['v*']`（新模型按 `VERSION` 发版，保留 tag 触发会在同一次 push 产生重复 release）；`workflow_dispatch` 的 `build_android` 输入删除（Android 恒构建），保留 `qt_version`。
      - `VERSION` 初始值取 `CMakeLists.txt` 的 `project(VERSION 0.1.0)` → `0.1.0`；`CHANGLOG.md` 建立一个初始条目。
      - 覆盖说明：§9.14 仍约束**本地**（不主动构建 APK、日常只做 Windows 构建+自检+截图）；§9.15 中「tag 才构建 Android / 才发 Release」由此条取代。
   - **第 1 轮 gh 跟踪（run `42456439992`）失败与修复**：
     - **Windows / macOS / Android 的 Build 全部失败**，根因是新增的 `VERSION` 文件在大小写不敏感文件系统上被当成 C++20 标准头 `<version>`（Qt 6.11 的 `qtconfiginclude.h` 会 `#include <version>`），编译器把 `0.1.0` 当源码解析（`expected unqualified-id` / `C2059`）。`qt_add_qml_module()` 会无条件把仓库根目录加入 `MrMusic` 的 include 路径（QTBUG-93443）。修复：关掉多余的 `CMAKE_INCLUDE_CURRENT_DIR`，并在 `qt_add_qml_module` 之后从 `MrMusic` target 的 `INCLUDE_DIRECTORIES` 移除 `CMAKE_CURRENT_SOURCE_DIR`（项目内所有 `#include` 都走 `src/`，不依赖根目录）。本机 Windows 构建 + 自检复验通过。
     - **Android `Setup Android SDK` 失败**：`android-actions/setup-android@v3` 内部执行 `sdkmanager tools`，但新版 SDK 镜像已无该旧包 → `Failed to find package 'tools'`。修复：去掉该 action，直接用 runner 预装的 `sdkmanager --install "ndk;..." "platforms;android-34" "build-tools;34.0.0"` 并设置 `ANDROID_SDK_ROOT` / `ANDROID_NDK_ROOT`（回退 `ANDROID_HOME`）。
   - **第 2 轮 gh 跟踪（run `42457274058`）**：Windows / Linux / macOS / precheck **全部通过**；仅 Android `Install NDK` 报 `sdkmanager: command not found`（exit 127）——去掉 `setup-android@v3` 后无人把 cmdline-tools 加进 PATH。修复：在步骤里用 `find "$SDK_ROOT/cmdline-tools" -name sdkmanager` 动态定位（不硬编码 `16.0`），把其目录写入 `$GITHUB_PATH`，并显式导出 `ANDROID_SDK_ROOT` / `ANDROID_HOME` / `ANDROID_NDK_ROOT`。本机用等价 bash 表达式验证了路径拼接。
   - **第 3 轮 gh 跟踪（run `42457948048`）**：NDK 安装通过；`Install Qt for Android` 失败 `aqt list-qt linux android --spec 6.11.1 --latest-version` → `No latest version available`。实测 `linux/android` 仓库只到 6.7.3，Qt 6.11 的 Android 包已迁到 **`all_os/android`**（`qt6_6111/qt6_6111_arm64_v8a/`，含 `qtmultimedia`）；且 aqt 3.3.0 的 `install-qt` 不接受 `all_os`，只有 aqtinstall 修复版支持。修复：Android job 改用 `host: all_os` + 与 Windows 相同的 `aqtsource` 修复版 + `modules: qtmultimedia`。本地用 aqt `v3.3.1.dev168` `--dry-run` 验证：`install-qt all_os android 6.11.1 android_arm64_v8a --modules qtmultimedia` 正确解析并落盘 `6.11.1/android_arm64_v8a`（与 workflow 里 `QT_ANDROID_PATH` 一致）。
   - **第 4 轮 gh 跟踪（run `42458956035`）**：`Install Qt for Android` 通过；`Build APK` 失败 `Could not find toolchain file: .../QtAndroid/6.11.1/android_arm64_v8a/lib/cmake/Qt6/qt.toolchain.cmake`。日志显示 install-qt-action 调 aqt 时 `--outputdir` 实际是 `<dir>/Qt`（action 自动追加 `/Qt`），所以 kit 在 `<dir>/Qt/6.11.1/...`，而脚本里手写的 `QT_HOST_PATH` / `QT_ANDROID_PATH` 少了这一层。修复：补上 `/Qt/`；同时给 Android job 加 `Ensure Ninja` 步骤（连带报错 `unable to find Ninja`，去掉 `setup-android@v3` 后不再有人加 PATH）。另注：`all_os` + `--autodesktop` 会同时装 host gcc_64，故 `Install host Qt` 步骤变为冗余但保留无害。
   - **最终结果**：gh 跟踪到 run `42459617034`（`1c7ec3b`）`conclusion=success`——`Precheck` / `Windows x64` / `macOS` / `Linux x64` / `Android arm64` / `Publish release` **六个 job 全部通过**；四份产物上传（Android APK 33.2 MB、Windows 76.8 MB、Linux AppImage 60.6 MB、macOS 79.8 MB）；Release `0.1.0` 已创建（`isPrerelease=false`），挂着 4 个资产（`MrMusic-android-arm64.apk` / `MrMusic-windows-x64.zip` / `MrMusic-x86_64.AppImage` / `MrMusic.dmg`）。仅剩 Node.js 20 弃用警告，不影响构建。

**验证记录**：`mrmusic_catalogprobe`（`MRMUSIC_BUILD_PROBES=ON`）实测两个平台的专辑/MV 搜索、专辑详情与曲目、歌手详情/歌曲/专辑/MV 全部返回正确数据（网易云专辑 13 首曲目、酷狗专辑 11 首）；界面截图见 `docs/screenshots/album.png`、`artist.png`、`search-albums.png`。





