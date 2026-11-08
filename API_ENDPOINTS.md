# 直连上游端点清单

本文件是 TARGET.md §7 要求的「直连上游端点清单」，同时作为「**不使用代理接口**」的验收依据：
下表每一行都是 MrMusic 直接发往上游域名的请求，请求参数、签名与加密方式与上游 JS 实现等价。
仓库中不存在任何本地 HTTP 服务，也不存在把 `NeteaseCloudMusicApi` / `KuGouMusicApi` 当作后端的情况。

## 1. 网易云音乐

公共约定：

* 域名：weapi → `https://music.163.com`，eapi → `https://interface.music.163.com`
* weapi：`AES-128-CBC(presetKey="0CoJUm6Qyw8W8jud", iv="0102030405060708")` 两次 + `RSA`（**无填充 modpow**，1024 bit 公钥）
* eapi：`AES-128-ECB(key="e82ckenh8dichen8")`，摘要串 `nobody{url}use{json}md5forencrypt`
* 设备指纹：`deviceId` 由系统指纹派生并持久化；`_ntes_nuid`、`WNMCID`、`NMTID` 随机生成后随 cookie 保存

| 功能 | 方法 | 路径 | 加密 | 说明 |
| --- | --- | --- | --- | --- |
| 搜索（单曲） | POST | `/api/cloudsearch/pc` | eapi | `s`, `type=1`, `limit`, `offset`, `total` |
| 每日推荐 | POST | `/api/v3/discovery/recommend/songs` | weapi | 需要登录 |
| 播放地址 | POST | `/api/song/enhance/player/url/v1` | eapi | `ids`, `level`(standard/exhigh/lossless/hires), `encodeType=flac` |
| 歌词 | POST | `/api/song/lyric` | eapi | `id`, `tv/lv/rv/kv=-1` |
| 二维码 key | POST | `/api/login/qrcode/unikey` | weapi | `type=3` |
| 二维码轮询 | POST | `/api/login/qrcode/client/login` | weapi | `key`, `type=3`；800 过期 / 801 待扫 / 802 待确认 / 803 成功 |
| 手机号密码登录 | POST | `/api/w/login/cellphone` | weapi | `phone`, `password`(md5), `remember` |
| 账号信息 | POST | `/api/nuser/account/get` | weapi | 登录后拉取资料 |
| 退出登录 | POST | `/api/logout` | weapi | — |
| 匿名注册 | POST | `/api/register/anonimous` | weapi | `username = base64("<deviceId> <base64(md5(xor(deviceId)))>")` |

## 2. 酷狗音乐

公共约定：

* 网关：`https://gateway.kugou.com`，其余按接口使用 `userservice.kugou.com` / `login-user.kugou.com` / `lyrics.kugou.com` / `tracker.kugou.com`
* 默认参数：`dfid`、`mid`（系统指纹 → md5 → 十进制）、`uuid="-"`、`appid=1005`、`clientver=20489`、`clienttime`
* 签名（android）：`md5(salt + sort("k=v") + body + salt)`，salt = `OIlwieks28dk2k092lksi2UIkp`；web 变体 salt = `NVPh5oo715z5DIWAeQlhMDsWXXQV4hwt`
* `key` 参数（部分接口）：`md5(hash + "57ae12eb6890223e355ccfcb74edf70d" + appid + mid + userid)`
* 请求头：`dfid`、`clienttime`、`mid`、`kg-rc`、`kg-thash`、`kg-rec`、`kg-rf`、按接口的 `x-router`
* 空值参数会同时从 query 与签名中剔除（服务端行为）

| 功能 | 方法 | 路径 | 域名 / x-router | 说明 |
| --- | --- | --- | --- | --- |
| 设备注册（dfid） | POST | `/risk/v2/r_register_dev` | `userservice.kugou.com` | body 为 AES-128-CBC(base64)，`p` 为 RSA PKCS#1 v1.5 |
| 搜索（单曲） | GET | `/v3/search/song` | `complexsearch.kugou.com` | `keyword`, `page`, `pagesize`, `platform=AndroidFilter` |
| 每日推荐 | POST | `/everyday_song_recommend` | `everydayrec.service.kugou.com` | 返回 `data.song_list` |
| 播放地址（主） | GET | `/v5/url` | `trackercdn.kugou.com` | `hash`, `album_id`, `album_audio_id`, `quality`, `key` |
| 播放地址（备） | POST | `/v6/priv_url` | `tracker.kugou.com` | JSON body：`resource` + `tracker_param` |
| 歌词搜索 | GET | `/search` | `lyrics.kugou.com` | `ver=1`, `man=yes`, `client=pc`, `keyword`, `duration`, `hash` |
| 歌词下载 | GET | `/download` | `lyrics.kugou.com` | `ver=1`, `client=android`, `id`, `accesskey`, `fmt=krc`；KRC = 去 4 字节头 → XOR → zlib |
| 二维码 key | GET | `/v2/qrcode` | `login-user.kugou.com` | `appid=1001`, `type=1`, `plat=4`, `srcappid=2919`（web 签名） |
| 二维码轮询 | GET | `/v2/get_userinfo_qrcode` | `login-user.kugou.com` | `qrcode`, `dev`；status 0 过期 / 1 待扫 / 2 待确认 / 4 成功 |
| 账号密码登录 | POST | `/v9/login_by_pwd` | `login.user.kugou.com` | body 中 `params` 为 AES-256-CBC(hex)，`pk` 为 raw RSA(hex) |
| 用户信息 | POST | `/v3/get_my_info` | `usercenter.kugou.com` | `p` 为 raw RSA(hex) |

## 3. 媒体流与图片

| 用途 | 来源 |
| --- | --- |
| 音频流下载 | 网易云 `data[0].url` / 酷狗 `url[0]`（均为 CDN 直链，直接 GET） |
| 封面 | 网易云 `al.picUrl` / 酷狗 `Image`（`{size}` 替换为 240） |
| 歌词文本 | 见上表歌词接口 |

## 4. 验证记录（本机，Windows x64）

* 网易云：`/cloudsearch/pc` 返回 30 条，`/song/enhance/player/url/v1` 返回 320 kbps mp3 直链并可播放；`/song/lyric` 返回 LRC。
* 酷狗：`/v3/search/song` 返回 30 条（含 `FileHash` / `AlbumAudioID` / `SQ.Hash` 等）；`/everyday_song_recommend` 返回 30 首；`/search` + `/download` 返回 KRC 并成功转成 LRC（1386 字符）；`/risk/v2/r_register_dev` 成功取得 `dfid`。
* 酷狗 `/v5/url`、`/v6/priv_url` 在未登录状态下返回 `Hash not found` / `本次请求需要验证`，代码保留两条路径并透传服务端原因。
