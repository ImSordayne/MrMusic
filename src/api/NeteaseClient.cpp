#include "api/NeteaseClient.h"

#include "api/HttpClient.h"
#include "core/Crypto.h"
#include "core/Fingerprint.h"
#include "core/Logger.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QRandomGenerator>
#include <QStringList>
#include <QUrl>

#include <algorithm>

namespace mr {
namespace {

const char *const kWeapiUserAgent =
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) "
    "Chrome/124.0.0.0 Safari/537.36 Edg/124.0.0.0";
const char *const kIphoneUserAgent =
    "NeteaseMusic 9.0.90/5038 (iPhone; iOS 16.2; zh_CN)";

const char *const kWeapiDomain = "https://music.163.com";
const char *const kEapiDomain = "https://interface.music.163.com";

const char *const kWeapiPresetKey = "0CoJUm6Qyw8W8jud";
const char *const kWeapiIv = "0102030405060708";
const char *const kEapiKey = "e82ckenh8dichen8";

const char *const kPublicKeyPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDgtQn2JZ34ZC28NWYpAUd98iZ37BUrX/aKzmFbt7clFSs6sXqHauqKWqdtLkF2KexO40H1YTX8z2lSgBBOAxLsvaklV8k4cBFK9snQXE9/DDaFt6Rr7iVZMldczhC0JNgTz+SHXT6CBHuX3e9SdB1Ua44oncaTWz7OBGLbCiK45wIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

const char *const kBase62 = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
const char *const kLowerAlpha = "abcdefghijklmnopqrstuvwxyz";

QString randomString(int length, const char *alphabet, int alphabetSize)
{
    QString out;
    out.reserve(length);
    auto *generator = QRandomGenerator::system();
    for (int i = 0; i < length; ++i)
        out.append(QLatin1Char(alphabet[generator->bounded(alphabetSize)]));
    return out;
}

QString randomHex(int length)
{
    QString out;
    out.reserve(length);
    auto *generator = QRandomGenerator::system();
    for (int i = 0; i < length; ++i)
        out.append(QStringLiteral("%1").arg(generator->bounded(16), 1, 16));
    return out;
}

QString joinArtistNames(const QJsonArray &artists)
{
    QStringList names;
    for (const QJsonValue &value : artists) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString();
        if (!name.isEmpty())
            names << name;
    }
    if (names.isEmpty())
        return QStringLiteral("未知艺术家");
    return names.join(QStringLiteral("/"));
}

} // namespace

NeteaseClient::NeteaseClient(QObject *parent)
    : QObject(parent)
    , m_http(new HttpClient(this))
{
    m_http->setUserAgent(QByteArray(kIphoneUserAgent));
}

void NeteaseClient::setProxy(const QString &proxyUrl)
{
    m_http->setProxy(proxyUrl);
}

QJsonObject NeteaseClient::cookies() const
{
    QJsonObject object;
    for (auto it = m_cookies.constBegin(); it != m_cookies.constEnd(); ++it)
        object[it.key()] = it.value();
    return object;
}

void NeteaseClient::setCookies(const QJsonObject &cookies)
{
    m_cookies.clear();
    for (auto it = cookies.constBegin(); it != cookies.constEnd(); ++it)
        m_cookies[it.key()] = it.value().toString();
    emit cookiesChanged();
}

void NeteaseClient::setAnonymousToken(const QString &token)
{
    m_anonymousToken = token;
}

void NeteaseClient::setProfile(const QJsonObject &profile)
{
    m_profile = profile;
    emit loginStateChanged();
}

QString NeteaseClient::userId() const
{
    return m_profile.value(QStringLiteral("userId")).toVariant().toString();
}

QString NeteaseClient::nickname() const
{
    return m_profile.value(QStringLiteral("nickname")).toString();
}

QString NeteaseClient::avatarUrl() const
{
    return m_profile.value(QStringLiteral("avatarUrl")).toString();
}

void NeteaseClient::clearSession()
{
    m_cookies.clear();
    m_profile = QJsonObject();
    emit cookiesChanged();
    emit loginStateChanged();
}

QString NeteaseClient::cookieObjectToString(const QMap<QString, QString> &cookies)
{
    QStringList parts;
    for (auto it = cookies.constBegin(); it != cookies.constEnd(); ++it) {
        parts << QString::fromLatin1(QUrl::toPercentEncoding(it.key())) + QLatin1Char('=')
                + QString::fromLatin1(QUrl::toPercentEncoding(it.value()));
    }
    return parts.join(QStringLiteral("; "));
}

QMap<QString, QString> NeteaseClient::processedCookies(const QString &uri) const
{
    QMap<QString, QString> cookies = m_cookies;
    cookies[QStringLiteral("__remember_me")] = QStringLiteral("true");
    cookies[QStringLiteral("ntes_kaola_ad")] = QStringLiteral("1");

    if (!cookies.contains(QStringLiteral("_ntes_nuid")))
        cookies[QStringLiteral("_ntes_nuid")] = randomHex(32);
    if (!cookies.contains(QStringLiteral("_ntes_nnid"))) {
        cookies[QStringLiteral("_ntes_nnid")] = cookies.value(QStringLiteral("_ntes_nuid")) + QLatin1Char(',')
            + QString::number(QDateTime::currentMSecsSinceEpoch());
    }
    if (!cookies.contains(QStringLiteral("WNMCID"))) {
        cookies[QStringLiteral("WNMCID")] = randomString(6, kLowerAlpha, 26) + QLatin1Char('.')
            + QString::number(QDateTime::currentMSecsSinceEpoch()) + QStringLiteral(".01.0");
    }
    if (!cookies.contains(QStringLiteral("WEVNSM")))
        cookies[QStringLiteral("WEVNSM")] = QStringLiteral("1.0.0");
    if (!cookies.contains(QStringLiteral("osver")))
        cookies[QStringLiteral("osver")] = QStringLiteral("Microsoft-Windows-10-Professional-build-19045-64bit");
    if (!cookies.contains(QStringLiteral("deviceId")))
        cookies[QStringLiteral("deviceId")] = Fingerprint::instance().neteaseDeviceId();
    if (!cookies.contains(QStringLiteral("os")))
        cookies[QStringLiteral("os")] = QStringLiteral("pc");
    if (!cookies.contains(QStringLiteral("channel")))
        cookies[QStringLiteral("channel")] = QStringLiteral("netease");
    if (!cookies.contains(QStringLiteral("appver")))
        cookies[QStringLiteral("appver")] = QStringLiteral("3.1.17.204416");

    if (!uri.contains(QLatin1String("login")))
        cookies[QStringLiteral("NMTID")] = randomHex(16);

    if (!cookies.contains(QStringLiteral("MUSIC_U")) && !m_anonymousToken.isEmpty())
        cookies[QStringLiteral("MUSIC_A")] = m_anonymousToken;

    return cookies;
}

QJsonObject NeteaseClient::weapiEncrypt(const QJsonObject &data)
{
    const QByteArray text = QJsonDocument(data).toJson(QJsonDocument::Compact);

    const QByteArray firstPass =
        Crypto::aesCbcEncrypt(text, QByteArray(kWeapiPresetKey), QByteArray(kWeapiIv));
    const QByteArray firstBase64 = firstPass.toBase64();

    const QString secretKey = randomString(16, kBase62, 62);
    const QByteArray secondPass = Crypto::aesCbcEncrypt(firstBase64, secretKey.toLatin1(), QByteArray(kWeapiIv));

    QString reversed = secretKey;
    std::reverse(reversed.begin(), reversed.end());

    // NetEase expects a raw (unpadded) modular exponentiation of the reversed
    // secret key - see `publicKey.encrypt(str, 'NONE')` in the reference project.
    const QByteArray encryptedKey =
        Crypto::rsaEncryptRawPem(QByteArray(kPublicKeyPem), reversed.toLatin1());

    QJsonObject result;
    result[QStringLiteral("params")] = QString::fromLatin1(secondPass.toBase64());
    result[QStringLiteral("encSecKey")] = Crypto::hexLower(encryptedKey);
    return result;
}

QString NeteaseClient::eapiEncrypt(const QString &uri, const QJsonObject &data)
{
    const QByteArray text = QJsonDocument(data).toJson(QJsonDocument::Compact);
    const QString digest =
        Crypto::md5Hex(QStringLiteral("nobody%1use%2md5forencrypt").arg(uri).arg(QString::fromUtf8(text)).toUtf8());

    const QByteArray joined = uri.toUtf8() + QByteArrayLiteral("-36cd479b6b5-") + text
        + QByteArrayLiteral("-36cd479b6b5-") + digest.toUtf8();

    const QByteArray encrypted = Crypto::aesEcbEncrypt(joined, QByteArray(kEapiKey));
    return Crypto::hexUpper(encrypted);
}

QString NeteaseClient::formEncode(const QJsonObject &fields)
{
    QStringList parts;
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) {
        parts << QString::fromLatin1(QUrl::toPercentEncoding(it.key())) + QLatin1Char('=')
                + QString::fromLatin1(QUrl::toPercentEncoding(it.value().toString()));
    }
    return parts.join(QLatin1Char('&'));
}

void NeteaseClient::request(const QString &uri, QJsonObject data, Mode mode, JsonCallback callback)
{
    const QMap<QString, QString> cookies = processedCookies(uri);

    HttpRequest request;
    request.method = QByteArrayLiteral("POST");
    request.setHeader("Content-Type", "application/x-www-form-urlencoded");

    if (mode == Mode::Weapi) {
        data[QStringLiteral("csrf_token")] = cookies.value(QStringLiteral("__csrf"));
        const QJsonObject encrypted = weapiEncrypt(data);
        request.url = QUrl(QString::fromLatin1(kWeapiDomain) + QStringLiteral("/weapi/") + uri.mid(5));
        request.setHeader("Referer", kWeapiDomain);
        request.setHeader("User-Agent", kWeapiUserAgent);
        request.setHeader("Cookie", cookieObjectToString(cookies).toUtf8());
        request.body = formEncode(encrypted).toUtf8();
    } else {
        QJsonObject header;
        header[QStringLiteral("osver")] = cookies.value(QStringLiteral("osver"));
        header[QStringLiteral("deviceId")] = cookies.value(QStringLiteral("deviceId"));
        header[QStringLiteral("os")] = cookies.value(QStringLiteral("os"));
        header[QStringLiteral("appver")] = cookies.value(QStringLiteral("appver"));
        header[QStringLiteral("versioncode")] = cookies.value(QStringLiteral("versioncode"), QStringLiteral("140"));
        header[QStringLiteral("mobilename")] = cookies.value(QStringLiteral("mobilename"));
        header[QStringLiteral("buildver")] =
            QString::number(QDateTime::currentMSecsSinceEpoch()).left(10);
        header[QStringLiteral("resolution")] = cookies.value(QStringLiteral("resolution"), QStringLiteral("1920x1080"));
        header[QStringLiteral("__csrf")] = cookies.value(QStringLiteral("__csrf"));
        header[QStringLiteral("channel")] = cookies.value(QStringLiteral("channel"));
        header[QStringLiteral("requestId")] = QStringLiteral("%1_%2")
                                                  .arg(QDateTime::currentMSecsSinceEpoch())
                                                  .arg(QRandomGenerator::global()->bounded(1000), 4, 10, QLatin1Char('0'));

        if (cookies.contains(QStringLiteral("MUSIC_U")))
            header[QStringLiteral("MUSIC_U")] = cookies.value(QStringLiteral("MUSIC_U"));
        if (cookies.contains(QStringLiteral("MUSIC_A")))
            header[QStringLiteral("MUSIC_A")] = cookies.value(QStringLiteral("MUSIC_A"));

        data[QStringLiteral("header")] = header;
        data[QStringLiteral("e_r")] = false;

        const QString params = eapiEncrypt(uri, data);
        request.url = QUrl(QString::fromLatin1(kEapiDomain) + QStringLiteral("/eapi/") + uri.mid(5));

        QMap<QString, QString> headerCookies;
        for (auto it = header.constBegin(); it != header.constEnd(); ++it) {
            if (it.value().isString())
                headerCookies[it.key()] = it.value().toString();
        }
        request.setHeader("Cookie", cookieObjectToString(headerCookies).toUtf8());
        request.setHeader("User-Agent", kIphoneUserAgent);
        request.body = QByteArrayLiteral("params=") + QUrl::toPercentEncoding(params);
    }

    Logger::debug(QStringLiteral("netease → %1").arg(request.url.toString()));

    m_http->send(request, [this, callback, uri](const HttpResponse &response) {
        if (!response.error.isEmpty()) {
            if (callback)
                callback(false, {}, response.error);
            return;
        }

        mergeCookies(response);
        emit rawResponse(uri, response.status, response.body);

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(response.body, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (callback)
                callback(false, {},
                         QStringLiteral("响应解析失败 (%1): %2")
                             .arg(response.status)
                             .arg(QString::fromUtf8(response.body.left(120))));
            return;
        }

        QJsonObject body = document.object();
        const int code = body.value(QStringLiteral("code")).toInt(200);
        if (code != 200 && code != 800 && code != 801 && code != 802 && code != 803) {
            const QString message = body.value(QStringLiteral("message")).toString(
                body.value(QStringLiteral("msg")).toString(QStringLiteral("请求失败")));
            Logger::warn(QStringLiteral("netease %1 failed: %2 %3").arg(uri).arg(code).arg(message));
            if (callback)
                callback(false, body, message);
            return;
        }

        if (callback)
            callback(true, body, QString());
    });
}

void NeteaseClient::mergeCookies(const HttpResponse &response)
{
    bool changed = false;
    for (const QByteArray &raw : response.setCookies) {
        const QString line = QString::fromUtf8(raw);
        const QString first = line.section(QLatin1Char(';'), 0, 0);
        const int equals = first.indexOf(QLatin1Char('='));
        if (equals <= 0)
            continue;
        const QString key = first.left(equals).trimmed();
        const QString value = first.mid(equals + 1).trimmed();
        if (key.isEmpty())
            continue;
        if (m_cookies.value(key) != value) {
            m_cookies[key] = value;
            changed = true;
        }
    }
    if (changed)
        emit cookiesChanged();
}

QList<Track> NeteaseClient::parseSongs(const QJsonArray &songs)
{
    QList<Track> tracks;
    tracks.reserve(songs.size());
    for (const QJsonValue &value : songs) {
        const QJsonObject song = value.toObject();

        // `/api/search/get` returns the legacy field names (artists/album/duration)
        // while `/api/v3/...` returns the modern ones (ar/al/dt). Support both.
        QJsonArray artistArray = song.value(QStringLiteral("ar")).toArray();
        if (artistArray.isEmpty())
            artistArray = song.value(QStringLiteral("artists")).toArray();

        QJsonObject album = song.value(QStringLiteral("al")).toObject();
        if (album.isEmpty())
            album = song.value(QStringLiteral("album")).toObject();

        qint64 duration = static_cast<qint64>(song.value(QStringLiteral("dt")).toDouble());
        if (duration <= 0)
            duration = static_cast<qint64>(song.value(QStringLiteral("duration")).toDouble());

        Track track;
        track.source = QStringLiteral("netease");
        track.id = QString::number(song.value(QStringLiteral("id")).toVariant().toLongLong());
        track.title = song.value(QStringLiteral("name")).toString();
        track.artists = joinArtistNames(artistArray);
        track.album = album.value(QStringLiteral("name")).toString();
        track.albumId = album.value(QStringLiteral("id")).toVariant().toString();
        if (!artistArray.isEmpty())
            track.artistId = artistArray.first().toObject().value(QStringLiteral("id")).toVariant().toString();
        track.coverUrl = album.value(QStringLiteral("picUrl")).toString();
        if (track.coverUrl.isEmpty())
            track.coverUrl = album.value(QStringLiteral("picUrl_str")).toString();
        track.durationMs = duration;
        track.fee = song.value(QStringLiteral("fee")).toInt();
        track.vip = track.fee != 0 && track.fee != 8;

        const QJsonObject privilege = song.value(QStringLiteral("privilege")).toObject();
        if (!privilege.isEmpty()) {
            const int st = privilege.value(QStringLiteral("st")).toInt(0);
            if (st < 0)
                track.vip = true;
        }
        if (track.id.isEmpty() || track.title.isEmpty())
            continue;
        tracks.append(track);
    }
    return tracks;
}

void NeteaseClient::search(const QString &keywords, int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("s")] = keywords;
    data[QStringLiteral("type")] = 1;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;

    request(QStringLiteral("/api/cloudsearch/pc"), data, Mode::Eapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                const QJsonObject result = body.value(QStringLiteral("result")).toObject();
                QJsonObject out;
                QJsonArray tracks;
                for (const Track &track : parseSongs(result.value(QStringLiteral("songs")).toArray()))
                    tracks.append(QJsonValue(track.toJson()));
                out[QStringLiteral("tracks")] = tracks;
                out[QStringLiteral("total")] = result.value(QStringLiteral("songCount")).toInt(
                    result.value(QStringLiteral("songs")).toArray().size());
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::dailyRecommend(JsonCallback callback)
{
    request(QStringLiteral("/api/v3/discovery/recommend/songs"), QJsonObject(), Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                const QJsonObject data = body.value(QStringLiteral("data")).toObject();
                QJsonArray tracks;
                for (const Track &track : parseSongs(data.value(QStringLiteral("dailySongs")).toArray()))
                    tracks.append(QJsonValue(track.toJson()));
                QJsonObject out;
                out[QStringLiteral("tracks")] = tracks;
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::songUrl(const Track &track, Quality quality, StreamCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("ids")] = QStringLiteral("[%1]").arg(track.id);
    data[QStringLiteral("level")] = neteaseLevelForQuality(quality);
    data[QStringLiteral("encodeType")] = QStringLiteral("flac");

    request(QStringLiteral("/api/song/enhance/player/url/v1"), data, Mode::Eapi,
            [callback, quality](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                const QJsonArray list = body.value(QStringLiteral("data")).toArray();
                if (list.isEmpty()) {
                    if (callback)
                        callback(false, {}, QStringLiteral("无版权"));
                    return;
                }
                const QJsonObject item = list.first().toObject();
                AudioStream stream;
                stream.url = item.value(QStringLiteral("url")).toString();
                stream.extension = item.value(QStringLiteral("type")).toString();
                stream.bitrate = item.value(QStringLiteral("br")).toInt();
                stream.size = static_cast<qint64>(item.value(QStringLiteral("size")).toDouble());
                stream.durationMs = static_cast<int>(item.value(QStringLiteral("time")).toDouble());
                stream.trial = !item.value(QStringLiteral("freeTrialInfo")).isNull()
                    && item.value(QStringLiteral("freeTrialInfo")).isObject();

                if (stream.url.isEmpty()) {
                    const int fee = item.value(QStringLiteral("fee")).toInt();
                    stream.message = (fee == 1 || fee == 4) ? QStringLiteral("VIP")
                                                           : QStringLiteral("无版权");
                    if (callback)
                        callback(false, stream, stream.message);
                    return;
                }
                Q_UNUSED(quality)
                if (callback)
                    callback(true, stream, QString());
            });
}

void NeteaseClient::lyric(const Track &track, TextCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("id")] = track.id;
    data[QStringLiteral("tv")] = -1;
    data[QStringLiteral("lv")] = -1;
    data[QStringLiteral("rv")] = -1;
    data[QStringLiteral("kv")] = -1;
    data[QStringLiteral("_nmclfl")] = 1;

    request(QStringLiteral("/api/song/lyric"), data, Mode::Eapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                const QString lyric = body.value(QStringLiteral("lrc")).toObject().value(QStringLiteral("lyric")).toString();
                if (callback)
                    callback(true, lyric, QString());
            });
}

void NeteaseClient::registerAnonymous(JsonCallback callback)
{
    // deviceId: 52 random hex chars, username = base64("<deviceId> <base64(md5(xor(deviceId)))>")
    QString deviceId;
    deviceId.reserve(52);
    auto *generator = QRandomGenerator::system();
    const char *hexChars = "0123456789ABCDEF";
    for (int i = 0; i < 52; ++i)
        deviceId.append(QLatin1Char(hexChars[generator->bounded(16)]));

    const QByteArray xorKey = QByteArrayLiteral("3go8&$8*3*3h0k(2)2");
    QByteArray xored(deviceId.size(), '\0');
    for (int i = 0; i < deviceId.size(); ++i)
        xored[i] = static_cast<char>(deviceId.at(i).toLatin1() ^ xorKey.at(i % xorKey.size()));
    // The reference implementation hashes the *UTF-8 encoding* of the XOR'ed
    // characters (CryptoJS.enc.Utf8.parse), not the raw bytes.
    const QByteArray xoredUtf8 = QString::fromLatin1(xored).toUtf8();
    const QString encodedId = Crypto::base64(Crypto::md5(xoredUtf8));
    const QString username = Crypto::base64((deviceId + QLatin1Char(' ') + encodedId).toUtf8());

    QJsonObject data;
    data[QStringLiteral("username")] = username;

    request(QStringLiteral("/api/register/anonimous"), data, Mode::Weapi,
            [this, callback, deviceId](bool ok, const QJsonObject &body, const QString &error) {
                if (ok) {
                    m_cookies[QStringLiteral("deviceId")] = deviceId;
                    const QString token = m_cookies.value(QStringLiteral("MUSIC_A"));
                    if (!token.isEmpty()) {
                        m_anonymousToken = token;
                        emit cookiesChanged();
                    }
                }
                if (callback)
                    callback(ok, body, error);
            });
}

void NeteaseClient::qrKey(JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("type")] = 3;

    request(QStringLiteral("/api/login/qrcode/unikey"), data, Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                QString unikey = body.value(QStringLiteral("unikey")).toString();
                if (unikey.isEmpty())
                    unikey = body.value(QStringLiteral("data")).toObject().value(QStringLiteral("unikey")).toString();
                QJsonObject out;
                out[QStringLiteral("key")] = unikey;
                out[QStringLiteral("url")] = QStringLiteral("https://music.163.com/login?codekey=%1").arg(unikey);
                if (callback)
                    callback(!unikey.isEmpty(), out, unikey.isEmpty() ? QStringLiteral("获取二维码失败") : QString());
            });
}

void NeteaseClient::qrCheck(const QString &key, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("key")] = key;
    data[QStringLiteral("type")] = 3;

    request(QStringLiteral("/api/login/qrcode/client/login"), data, Mode::Weapi,
            [this, callback](bool ok, const QJsonObject &body, const QString &error) {
                Q_UNUSED(ok)
                Q_UNUSED(error)
                const int code = body.value(QStringLiteral("code")).toInt(0);
                QJsonObject out = body;
                out[QStringLiteral("code")] = code;
                if (code == 803) {
                    // fetch the profile for the header avatar
                    accountInfo(nullptr);
                    emit loginStateChanged();
                }
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::loginCellphone(const QString &phone, const QString &password, const QString &captcha,
                                  JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("type")] = QStringLiteral("1");
    data[QStringLiteral("https")] = QStringLiteral("true");
    data[QStringLiteral("phone")] = phone;
    data[QStringLiteral("countrycode")] = QStringLiteral("86");
    data[QStringLiteral("remember")] = QStringLiteral("true");
    if (!captcha.isEmpty()) {
        data[QStringLiteral("captcha")] = captcha;
    } else {
        data[QStringLiteral("password")] = Crypto::md5Hex(password.toUtf8());
    }

    request(QStringLiteral("/api/w/login/cellphone"), data, Mode::Weapi,
            [this, callback](bool ok, const QJsonObject &body, const QString &error) {
                if (ok) {
                    const QJsonObject profile = body.value(QStringLiteral("profile")).toObject();
                    if (!profile.isEmpty())
                        m_profile = profile;
                    emit loginStateChanged();
                }
                if (callback)
                    callback(ok, body, error);
            });
}

void NeteaseClient::accountInfo(JsonCallback callback)
{
    request(QStringLiteral("/api/nuser/account/get"), QJsonObject(), Mode::Weapi,
            [this, callback](bool ok, const QJsonObject &body, const QString &error) {
                if (ok) {
                    const QJsonObject profile = body.value(QStringLiteral("profile")).toObject();
                    if (!profile.isEmpty()) {
                        m_profile = profile;
                        emit loginStateChanged();
                    }
                }
                if (callback)
                    callback(ok, body, error);
            });
}

void NeteaseClient::logout(JsonCallback callback)
{
    request(QStringLiteral("/api/logout"), QJsonObject(), Mode::Weapi,
            [this, callback](bool ok, const QJsonObject &body, const QString &error) {
                clearSession();
                if (callback)
                    callback(ok, body, error);
            });
}


// ---------------------------------------------------------------------------
// Catalogue browsing (TARGET.md §10.19 / §10.20)
// ---------------------------------------------------------------------------
namespace {

QString jsonString(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isString() && !value.toString().isEmpty())
            return value.toString();
        if (value.isDouble())
            return QString::number(static_cast<qint64>(value.toDouble()));
    }
    return QString();
}

qint64 jsonNumber(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QJsonValue value = object.value(key);
        if (value.isDouble())
            return static_cast<qint64>(value.toDouble());
        if (value.isString() && !value.toString().isEmpty())
            return value.toString().toLongLong();
    }
    return 0;
}

Album parseAlbum(const QJsonObject &object)
{
    Album album;
    album.source = QStringLiteral("netease");
    album.id = jsonString(object, {QStringLiteral("id")});
    album.name = jsonString(object, {QStringLiteral("name"), QStringLiteral("albumName")});
    const QJsonObject artist = object.value(QStringLiteral("artist")).toObject();
    album.artist = jsonString(artist, {QStringLiteral("name")});
    album.artistId = jsonString(artist, {QStringLiteral("id")});
    if (album.artist.isEmpty())
        album.artist = jsonString(object, {QStringLiteral("artistName")});
    album.coverUrl = jsonString(object, {QStringLiteral("picUrl"), QStringLiteral("blurPicUrl"),
                                         QStringLiteral("coverImgUrl")});
    const qint64 publishTime = jsonNumber(object, {QStringLiteral("publishTime"),
                                                   QStringLiteral("companyId")}) ;
    Q_UNUSED(publishTime)
    const qint64 stamp = jsonNumber(object, {QStringLiteral("publishTime")});
    if (stamp > 0) {
        const QDateTime date = QDateTime::fromMSecsSinceEpoch(stamp);
        album.publishDate = date.toString(QStringLiteral("yyyy-MM-dd"));
    }
    album.company = jsonString(object, {QStringLiteral("company")});
    album.description = jsonString(object, {QStringLiteral("description")});
    album.songCount = static_cast<int>(jsonNumber(object, {QStringLiteral("size"),
                                                           QStringLiteral("songCount")}));
    return album;
}

Mv parseMv(const QJsonObject &object)
{
    Mv mv;
    mv.source = QStringLiteral("netease");
    mv.id = jsonString(object, {QStringLiteral("id"), QStringLiteral("vid")});
    mv.name = jsonString(object, {QStringLiteral("name"), QStringLiteral("title")});
    mv.artist = jsonString(object, {QStringLiteral("artistName")});
    mv.artistId = jsonString(object, {QStringLiteral("artistId")});
    mv.coverUrl = jsonString(object, {QStringLiteral("cover"), QStringLiteral("imgurl"),
                                      QStringLiteral("picUrl")});
    mv.durationMs = jsonNumber(object, {QStringLiteral("duration")});
    mv.playCount = jsonNumber(object, {QStringLiteral("playCount")});
    const qint64 stamp = jsonNumber(object, {QStringLiteral("publishTime")});
    if (stamp > 0)
        mv.publishTime = QDateTime::fromMSecsSinceEpoch(stamp).toString(QStringLiteral("yyyy-MM-dd"));
    return mv;
}

QJsonArray albumsToJson(const QList<Album> &albums)
{
    QJsonArray array;
    for (const Album &album : albums)
        array.append(QJsonObject::fromVariantMap(album.toVariantMap()));
    return array;
}

QJsonArray mvsToJson(const QList<Mv> &mvs)
{
    QJsonArray array;
    for (const Mv &mv : mvs)
        array.append(QJsonObject::fromVariantMap(mv.toVariantMap()));
    return array;
}

} // namespace

void NeteaseClient::searchAlbums(const QString &keywords, int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("s")] = keywords;
    data[QStringLiteral("type")] = 10;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;

    request(QStringLiteral("/api/cloudsearch/pc"), data, Mode::Eapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                const QJsonObject result = body.value(QStringLiteral("result")).toObject();
                QList<Album> albums;
                for (const QJsonValue &value : result.value(QStringLiteral("albums")).toArray())
                    albums.append(parseAlbum(value.toObject()));
                QJsonObject out;
                out[QStringLiteral("albums")] = albumsToJson(albums);
                out[QStringLiteral("total")] = result.value(QStringLiteral("albumCount")).toInt(albums.size());
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::searchMvs(const QString &keywords, int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("s")] = keywords;
    data[QStringLiteral("type")] = 1004;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;

    request(QStringLiteral("/api/cloudsearch/pc"), data, Mode::Eapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                const QJsonObject result = body.value(QStringLiteral("result")).toObject();
                QList<Mv> mvs;
                for (const QJsonValue &value : result.value(QStringLiteral("mvs")).toArray())
                    mvs.append(parseMv(value.toObject()));
                QJsonObject out;
                out[QStringLiteral("mvs")] = mvsToJson(mvs);
                out[QStringLiteral("total")] = result.value(QStringLiteral("mvCount")).toInt(mvs.size());
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::songDetail(const QList<QString> &ids, JsonCallback callback)
{
    if (ids.isEmpty()) {
        if (callback)
            callback(true, QJsonObject{{QStringLiteral("tracks"), QJsonArray()}}, QString());
        return;
    }
    QStringList parts;
    parts.reserve(ids.size());
    for (const QString &id : ids)
        parts << QStringLiteral("{\"id\":%1}").arg(id);

    QJsonObject data;
    data[QStringLiteral("c")] = QStringLiteral("[%1]").arg(parts.join(QLatin1Char(',')));

    request(QStringLiteral("/api/v3/song/detail"), data, Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                QList<Track> tracks = parseSongs(body.value(QStringLiteral("songs")).toArray());
                QJsonArray array;
                for (const Track &track : tracks)
                    array.append(QJsonValue(track.toJson()));
                QJsonObject out;
                out[QStringLiteral("tracks")] = array;
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::albumDetail(const QString &albumId, JsonCallback callback)
{
    const QString uri = QStringLiteral("/api/v1/album/%1").arg(albumId);
    request(uri, QJsonObject(), Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                const QJsonObject albumObject = body.value(QStringLiteral("album")).toObject();
                Album album = parseAlbum(albumObject);
                if (album.id.isEmpty())
                    album.id = jsonString(albumObject, {QStringLiteral("id")});
                QList<Track> tracks = parseSongs(body.value(QStringLiteral("songs")).toArray());
                if (album.songCount == 0)
                    album.songCount = tracks.size();
                if (album.coverUrl.isEmpty() && !tracks.isEmpty())
                    album.coverUrl = tracks.first().coverUrl;

                QJsonArray trackArray;
                for (const Track &track : tracks)
                    trackArray.append(QJsonValue(track.toJson()));

                QJsonObject out;
                out[QStringLiteral("album")] = QJsonObject::fromVariantMap(album.toVariantMap());
                out[QStringLiteral("tracks")] = trackArray;
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::artistDetail(const QString &artistId, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("id")] = artistId;
    request(QStringLiteral("/api/artist/head/info/get"), data, Mode::Eapi,
            [callback, artistId](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                // The eapi reply nests the payload under "data"; older builds
                // returned it at the top level, so both shapes are handled.
                QJsonObject payload = body.value(QStringLiteral("data")).toObject();
                if (payload.isEmpty())
                    payload = body;
                QJsonObject artistObject = payload.value(QStringLiteral("artist")).toObject();
                if (artistObject.isEmpty())
                    artistObject = payload.value(QStringLiteral("baseInfo")).toObject();
                if (artistObject.isEmpty())
                    artistObject = payload;

                Artist artist;
                artist.source = QStringLiteral("netease");
                artist.id = jsonString(artistObject, {QStringLiteral("id"), QStringLiteral("artistId")});
                if (artist.id.isEmpty())
                    artist.id = artistId;
                artist.name = jsonString(artistObject, {QStringLiteral("name"), QStringLiteral("artistName")});
                artist.avatarUrl = jsonString(artistObject, {QStringLiteral("avatar"),
                                                            QStringLiteral("cover"),
                                                            QStringLiteral("picUrl"),
                                                            QStringLiteral("img1v1Url")});
                artist.description = jsonString(artistObject, {QStringLiteral("briefDesc"),
                                                              QStringLiteral("description"),
                                                              QStringLiteral("intro")});
                artist.songCount = static_cast<int>(jsonNumber(artistObject, {QStringLiteral("musicSize"),
                                                                              QStringLiteral("songSize")}));
                artist.albumCount = static_cast<int>(jsonNumber(artistObject, {QStringLiteral("albumSize")}));
                artist.mvCount = static_cast<int>(jsonNumber(artistObject, {QStringLiteral("mvSize")}));
                artist.fansCount = jsonNumber(artistObject, {QStringLiteral("fansCount"),
                                                             QStringLiteral("followedCount")});
                const QJsonObject user = payload.value(QStringLiteral("user")).toObject();
                artist.followed = user.value(QStringLiteral("followed")).toBool(false);

                QJsonObject out;
                out[QStringLiteral("artist")] = QJsonObject::fromVariantMap(artist.toVariantMap());
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::artistSongs(const QString &artistId, int limit, int offset, bool hot,
                                JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("id")] = artistId;
    data[QStringLiteral("private_cloud")] = QStringLiteral("true");
    data[QStringLiteral("work_type")] = 1;
    data[QStringLiteral("order")] = hot ? QStringLiteral("hot") : QStringLiteral("time");
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("limit")] = limit;

    request(QStringLiteral("/api/v1/artist/songs"), data, Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                QList<Track> tracks = parseSongs(body.value(QStringLiteral("songs")).toArray());
                QJsonArray array;
                for (const Track &track : tracks)
                    array.append(QJsonValue(track.toJson()));
                QJsonObject out;
                out[QStringLiteral("tracks")] = array;
                out[QStringLiteral("total")] = body.value(QStringLiteral("total")).toInt(tracks.size());
                out[QStringLiteral("more")] = body.value(QStringLiteral("more")).toBool(false);
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::artistAlbums(const QString &artistId, int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;

    const QString uri = QStringLiteral("/api/artist/albums/%1").arg(artistId);
    request(uri, data, Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                QList<Album> albums;
                for (const QJsonValue &value : body.value(QStringLiteral("hotAlbums")).toArray())
                    albums.append(parseAlbum(value.toObject()));
                QJsonObject out;
                out[QStringLiteral("albums")] = albumsToJson(albums);
                out[QStringLiteral("total")] = body.value(QStringLiteral("artist"))
                                                    .toObject()
                                                    .value(QStringLiteral("albumSize"))
                                                    .toInt(albums.size());
                out[QStringLiteral("more")] = body.value(QStringLiteral("more")).toBool(false);
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::artistMvs(const QString &artistId, int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("artistId")] = artistId;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;

    request(QStringLiteral("/api/artist/mvs"), data, Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                QList<Mv> mvs;
                for (const QJsonValue &value : body.value(QStringLiteral("mvs")).toArray())
                    mvs.append(parseMv(value.toObject()));
                QJsonObject out;
                out[QStringLiteral("mvs")] = mvsToJson(mvs);
                out[QStringLiteral("total")] = body.value(QStringLiteral("hasMore")).toBool(false)
                    ? mvs.size() + 1
                    : mvs.size();
                out[QStringLiteral("more")] = body.value(QStringLiteral("hasMore")).toBool(false);
                if (callback)
                    callback(true, out, QString());
            });
}

// ---------------------------------------------------------------------------
// Account actions
// ---------------------------------------------------------------------------
void NeteaseClient::likeSong(const QString &trackId, bool like, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("alg")] = QStringLiteral("itembased");
    data[QStringLiteral("trackId")] = trackId;
    data[QStringLiteral("like")] = like;
    data[QStringLiteral("time")] = QStringLiteral("3");
    request(QStringLiteral("/api/radio/like"), data, Mode::Weapi, callback);
}

void NeteaseClient::likedSongs(const QString &uid, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("uid")] = uid;
    request(QStringLiteral("/api/song/like/get"), data, Mode::Eapi, callback);
}

void NeteaseClient::albumSubscribe(const QString &albumId, bool subscribe, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("id")] = albumId;
    const QString uri = subscribe ? QStringLiteral("/api/album/sub") : QStringLiteral("/api/album/unsub");
    request(uri, data, Mode::Weapi, callback);
}

void NeteaseClient::albumSubList(int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;
    request(QStringLiteral("/api/album/sublist"), data, Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                QList<Album> albums;
                for (const QJsonValue &value : body.value(QStringLiteral("data")).toArray()) {
                    Album album = parseAlbum(value.toObject());
                    album.subscribed = true;
                    albums.append(album);
                }
                QJsonObject out;
                out[QStringLiteral("albums")] = albumsToJson(albums);
                out[QStringLiteral("count")] = body.value(QStringLiteral("count")).toInt(albums.size());
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::mvSubscribe(const QString &mvId, bool subscribe, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("mvId")] = mvId;
    data[QStringLiteral("mvIds")] = QStringLiteral("[\"%1\"]").arg(mvId);
    const QString uri = subscribe ? QStringLiteral("/api/mv/sub") : QStringLiteral("/api/mv/unsub");
    request(uri, data, Mode::Weapi, callback);
}

void NeteaseClient::mvSubList(int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;
    request(QStringLiteral("/api/cloudvideo/allvideo/sublist"), data, Mode::Weapi,
            [callback](bool ok, const QJsonObject &body, const QString &error) {
                if (!ok) {
                    if (callback)
                        callback(false, {}, error);
                    return;
                }
                QList<Mv> mvs;
                for (const QJsonValue &value : body.value(QStringLiteral("data")).toArray()) {
                    Mv mv = parseMv(value.toObject());
                    mv.subscribed = true;
                    mvs.append(mv);
                }
                QJsonObject out;
                out[QStringLiteral("mvs")] = mvsToJson(mvs);
                out[QStringLiteral("count")] = body.value(QStringLiteral("count")).toInt(mvs.size());
                if (callback)
                    callback(true, out, QString());
            });
}

void NeteaseClient::artistSubscribe(const QString &artistId, bool follow, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("artistId")] = artistId;
    data[QStringLiteral("artistIds")] = QStringLiteral("[%1]").arg(artistId);
    const QString uri = follow ? QStringLiteral("/api/artist/sub") : QStringLiteral("/api/artist/unsub");
    request(uri, data, Mode::Weapi, callback);
}

void NeteaseClient::artistSubList(int limit, int offset, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("limit")] = limit;
    data[QStringLiteral("offset")] = offset;
    data[QStringLiteral("total")] = true;
    request(QStringLiteral("/api/artist/sublist"), data, Mode::Weapi, callback);
}

void NeteaseClient::mvUrl(const QString &mvId, int resolution, JsonCallback callback)
{
    QJsonObject data;
    data[QStringLiteral("id")] = mvId;
    data[QStringLiteral("r")] = resolution;
    request(QStringLiteral("/api/song/enhance/play/mv/url"), data, Mode::Weapi, callback);
}

} // namespace mr
