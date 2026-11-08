#include "api/KugouClient.h"

#include "api/HttpClient.h"
#include "core/Crypto.h"
#include "core/Fingerprint.h"
#include "core/Logger.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QRegularExpression>
#include <QRandomGenerator>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

namespace mr {
namespace {

const char *const kSaltAndroid = "OIlwieks28dk2k092lksi2UIkp";
const char *const kSaltWeb = "NVPh5oo715z5DIWAeQlhMDsWXXQV4hwt";
const char *const kSaltSignKey = "57ae12eb6890223e355ccfcb74edf70d";
const char *const kUserAgent = "Android15-1070-11083-46-0-DiscoveryDRADProtocol-wifi";

const char *const kPublicKeyPem =
    "-----BEGIN PUBLIC KEY-----\n"
    "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDIAG7QOELSYoIJvTFJhMpe1s/gbjDJX51HBNnEl5HXqTW6lQ7LC8jr9fWZTwusknp+sVGzwd40MwP6U5yDE27M/X1+UR4tvOGOqp94TJtQ1EPnWGWXngpeIW5GxoQGao1rmYWAu6oi1z9XkChrsUdC6DJE5E221wf/4WLFxwAtRQIDAQAB\n"
    "-----END PUBLIC KEY-----\n";

const char *const kRandomPool = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";

QString randomPoolString(int length)
{
    QString out;
    out.reserve(length);
    auto *generator = QRandomGenerator::system();
    for (int i = 0; i < length; ++i)
        out.append(QLatin1Char(kRandomPool[generator->bounded(36)]));
    return out;
}

QJsonValue fieldValue(const QJsonObject &object, const QStringList &keys)
{
    for (const QString &key : keys) {
        const auto it = object.constFind(key);
        if (it != object.constEnd())
            return it.value();
    }
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        for (const QString &key : keys) {
            if (it.key().compare(key, Qt::CaseInsensitive) == 0)
                return it.value();
        }
    }
    return QJsonValue();
}

QString stringField(const QJsonObject &object, const QStringList &keys, const QString &fallback = QString())
{
    const QJsonValue value = fieldValue(object, keys);
    if (value.isString())
        return value.toString();
    if (value.isDouble())
        return QString::number(value.toVariant().toLongLong());
    return fallback;
}

qint64 intField(const QJsonObject &object, const QStringList &keys, qint64 fallback = 0)
{
    const QJsonValue value = fieldValue(object, keys);
    if (value.isDouble())
        return static_cast<qint64>(value.toDouble());
    if (value.isString())
        return value.toString().toLongLong();
    return fallback;
}

/// KuGou titles are often "Singer - Song"; the UI shows the artist separately.
QString cleanKugouTitle(const QString &title, const QString &artists)
{
    if (artists.isEmpty())
        return title;
    const QString prefix = artists + QStringLiteral(" - ");
    if (title.startsWith(prefix, Qt::CaseInsensitive))
        return title.mid(prefix.size()).trimmed();
    return title;
}

/// Covers are returned as templates containing "{size}".
QString normalizeKugouCover(const QString &url)
{
    if (url.isEmpty())
        return url;
    QString out = url;
    out.replace(QStringLiteral("{size}"), QStringLiteral("240"));
    return out;
}

/// Normalises the different shapes returned by `/v5/url` and `/v6/priv_url`.
AudioStream parseKugouStream(const QJsonObject &body)
{
    AudioStream stream;

    auto pickUrl = [](const QJsonObject &object) -> QString {
        const QJsonValue url = object.value(QStringLiteral("url"));
        if (url.isString())
            return url.toString();
        if (url.isArray()) {
            for (const QJsonValue &entry : url.toArray()) {
                if (entry.isString())
                    return entry.toString();
                if (entry.isObject())
                    return entry.toObject().value(QStringLiteral("url")).toString();
            }
        }
        const QString playUrl = object.value(QStringLiteral("play_url")).toString();
        return playUrl;
    };

    QJsonObject payload = body.value(QStringLiteral("data")).toObject();
    if (payload.isEmpty())
        payload = body;

    stream.url = pickUrl(payload);
    if (stream.url.isEmpty() && payload != body)
        stream.url = pickUrl(body);

    stream.extension = stringField(payload, {QStringLiteral("extname"), QStringLiteral("ext"),
                                             QStringLiteral("extName")},
                                   stringField(body, {QStringLiteral("extname"), QStringLiteral("ext")}));
    stream.bitrate = static_cast<int>(intField(payload, {QStringLiteral("bitrate")},
                                               intField(body, {QStringLiteral("bitrate")})));
    stream.size = intField(payload, {QStringLiteral("fileSize"), QStringLiteral("file_size"), QStringLiteral("filesize")},
                           intField(body, {QStringLiteral("fileSize"), QStringLiteral("file_size")}));
    stream.durationMs = static_cast<int>(intField(payload, {QStringLiteral("time_length")},
                                                  intField(body, {QStringLiteral("time_length")})));

    if (stream.url.isEmpty()) {
        const int errorCode = static_cast<int>(intField(body, {QStringLiteral("error_code")},
                                                        intField(payload, {QStringLiteral("error_code")})));
        QString message = stringField(body, {QStringLiteral("error"), QStringLiteral("errmsg"),
                                             QStringLiteral("msg")});
        if (message.isEmpty())
            message = stringField(payload, {QStringLiteral("error"), QStringLiteral("errmsg")});
        if (message.isEmpty()) {
            switch (errorCode) {
            case 35104:
                message = QObject::tr("该音质不可用（可能需要 VIP）");
                break;
            case 20006:
                message = QObject::tr("签名校验失败");
                break;
            default:
                message = QObject::tr("无版权");
                break;
            }
        }
        stream.message = message;
    }

    if (stream.extension.isEmpty() && !stream.url.isEmpty()) {
        const QString path = QUrl(stream.url).path();
        const QString suffix = path.section(QLatin1Char('.'), -1);
        if (suffix.size() <= 4)
            stream.extension = suffix.toLower();
    }
    return stream;
}

QString formatLrcTimestamp(qint64 milliseconds){
    const qint64 minutes = milliseconds / 60000;
    const qint64 seconds = (milliseconds / 1000) % 60;
    const qint64 hundredths = (milliseconds % 1000) / 10;
    return QStringLiteral("%1:%2.%3")
        .arg(minutes, 2, 10, QLatin1Char('0'))
        .arg(seconds, 2, 10, QLatin1Char('0'))
        .arg(hundredths, 2, 10, QLatin1Char('0'));
}

/// KuGou KRC -> standard LRC (the format accepted by every player and tagger).
QString krcToLrc(const QString &krc)
{
    static const QRegularExpression wordTag(QStringLiteral("<[^>]*>"));
    QStringList output;
    const QStringList lines = krc.split(QLatin1Char('\n'));
    for (const QString &rawLine : lines) {
        QString line = rawLine.trimmed();
        if (line.isEmpty())
            continue;
        if (!line.startsWith(QLatin1Char('['))) {
            output << line;
            continue;
        }
        const int close = line.indexOf(QLatin1Char(']'));
        if (close <= 0) {
            output << line;
            continue;
        }
        const QString head = line.mid(1, close - 1);
        if (!head.contains(QLatin1Char(','))) {
            output << line; // [ti:...] / [ar:...] / [by:...]
            continue;
        }
        const QStringList parts = head.split(QLatin1Char(','));
        bool startOk = false;
        const qint64 start = parts.value(0).toLongLong(&startOk);
        QString text = line.mid(close + 1);
        text.remove(wordTag);
        if (!startOk)
            continue;
        output << QStringLiteral("[%1]%2").arg(formatLrcTimestamp(start), text.trimmed());
    }
    return output.join(QLatin1Char('\n'));
}

QString signatureForParams(const QMap<QString, QString> &params, const QString &data, bool web)
{
    // Empty valued parameters (`module` in /v5/url) are dropped by the server
    // before it verifies the signature, so they must not be signed either.
    QStringList items;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (it.value().isEmpty())
            continue;
        items << it.key() + QLatin1Char('=') + it.value();
    }
    std::sort(items.begin(), items.end());
    const QString joined = items.join(QString());
    const QString salt = QString::fromLatin1(web ? kSaltWeb : kSaltAndroid);
    return Crypto::md5Hex((salt + joined + data + salt).toUtf8());
}

QString signKeyFor(const QString &hash, const QString &mid, const QString &userid, const QString &appid)
{
    return Crypto::md5Hex((hash + QLatin1String(kSaltSignKey) + appid + mid
                           + (userid.isEmpty() ? QStringLiteral("0") : userid))
                              .toUtf8());
}

/// AES-256-CBC (key = md5(secret) first 32 chars, iv = last 16 chars) used by the
/// KuGou login payloads.
QString kugouAesEncrypt(const QByteArray &plain, const QString &secretKey, QString *outKey)
{
    const QString digest = Crypto::md5Hex(secretKey.toUtf8());
    const QByteArray key = digest.left(32).toLatin1();
    const QByteArray iv = digest.mid(16, 16).toLatin1();
    if (outKey)
        *outKey = secretKey;
    return Crypto::hexLower(Crypto::aesCbcEncrypt(plain, key, iv));
}

QByteArray kugouAesDecrypt(const QString &hexCipher, const QString &secretKey)
{
    const QString digest = Crypto::md5Hex(secretKey.toUtf8());
    const QByteArray key = digest.left(32).toLatin1();
    const QByteArray iv = digest.mid(16, 16).toLatin1();
    return Crypto::aesCbcDecrypt(Crypto::fromHex(hexCipher), key, iv);
}

/// AES-128-CBC with key/iv derived from a 6 character secret (register_dev).
QJsonObject playlistAesEncrypt(const QJsonObject &data, QString *outKey)
{
    const QString secret = randomPoolString(6).toLower();
    const QString digest = Crypto::md5Hex(secret.toUtf8());
    const QByteArray key = digest.left(16).toLatin1();
    const QByteArray iv = digest.mid(16, 16).toLatin1();
    const QByteArray plain = QJsonDocument(data).toJson(QJsonDocument::Compact);
    const QByteArray cipher = Crypto::aesCbcEncrypt(plain, key, iv);

    if (outKey)
        *outKey = secret;
    QJsonObject out;
    out[QStringLiteral("key")] = secret;
    out[QStringLiteral("str")] = QString::fromLatin1(cipher.toBase64());
    return out;
}

QJsonObject playlistAesDecrypt(const QString &base64Cipher, const QString &secret)
{
    const QString digest = Crypto::md5Hex(secret.toUtf8());
    const QByteArray key = digest.left(16).toLatin1();
    const QByteArray iv = digest.mid(16, 16).toLatin1();
    const QByteArray plain = Crypto::aesCbcDecrypt(QByteArray::fromBase64(base64Cipher.toLatin1()), key, iv);
    return QJsonDocument::fromJson(plain).object();
}

} // namespace

KugouClient::KugouClient(QObject *parent)
    : QObject(parent)
    , m_http(new HttpClient(this))
{
    m_http->setUserAgent(QByteArray(kUserAgent));
}

void KugouClient::setProxy(const QString &proxyUrl)
{
    m_http->setProxy(proxyUrl);
}

QJsonObject KugouClient::cookies() const
{
    QJsonObject object;
    for (auto it = m_cookies.constBegin(); it != m_cookies.constEnd(); ++it)
        object[it.key()] = it.value();
    return object;
}

void KugouClient::setCookies(const QJsonObject &cookies)
{
    m_cookies.clear();
    for (auto it = cookies.constBegin(); it != cookies.constEnd(); ++it)
        m_cookies[it.key()] = it.value().toString();
    emit cookiesChanged();
}

QString KugouClient::nickname() const
{
    return m_profile.value(QStringLiteral("nickname")).toString();
}

QString KugouClient::avatarUrl() const
{
    return m_profile.value(QStringLiteral("avatarUrl")).toString();
}

void KugouClient::setProfile(const QJsonObject &profile)
{
    m_profile = profile;
    emit loginStateChanged();
}

void KugouClient::clearSession()
{
    m_cookies.remove(QStringLiteral("token"));
    m_cookies.remove(QStringLiteral("userid"));
    m_cookies.remove(QStringLiteral("vip_type"));
    m_cookies.remove(QStringLiteral("vip_token"));
    m_cookies.remove(QStringLiteral("t1"));
    m_profile = QJsonObject();
    emit cookiesChanged();
    emit loginStateChanged();
}

QString KugouClient::defaultDfid() const
{
    const QString dfid = m_cookies.value(QStringLiteral("dfid"));
    return dfid.isEmpty() ? QStringLiteral("-") : dfid;
}

void KugouClient::mergeCookies(const HttpResponse &response)
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

HttpRequest KugouClient::buildHttp(const Request &request)
{
    QMap<QString, QString> params;
    if (!request.clearDefaultParams) {
        params[QStringLiteral("dfid")] = defaultDfid();
        params[QStringLiteral("mid")] = Fingerprint::instance().kugouMid();
        params[QStringLiteral("uuid")] = QStringLiteral("-");
        params[QStringLiteral("appid")] = QStringLiteral("1005");
        params[QStringLiteral("clientver")] = QStringLiteral("20489");
        params[QStringLiteral("clienttime")] = QString::number(QDateTime::currentSecsSinceEpoch());

        const QString token = m_cookies.value(QStringLiteral("token"));
        if (!token.isEmpty())
            params[QStringLiteral("token")] = token;
        const QString userid = m_cookies.value(QStringLiteral("userid"));
        if (!userid.isEmpty() && userid != QLatin1String("0"))
            params[QStringLiteral("userid")] = userid;
    }

    for (auto it = request.params.constBegin(); it != request.params.constEnd(); ++it)
        params[it.key()] = it.value();

    if (request.encryptKey) {
        const QString hash = request.signKeyHash.isEmpty() ? params.value(QStringLiteral("hash"))
                                                           : request.signKeyHash;
        params[QStringLiteral("key")] = signKeyFor(hash, params.value(QStringLiteral("mid")),
                                                   params.value(QStringLiteral("userid")),
                                                   params.value(QStringLiteral("appid")));
    }

    if (!request.notSign) {
        params[QStringLiteral("signature")] =
            signatureForParams(params, QString::fromUtf8(request.body), request.encryptType == QLatin1String("web"));
    }

    QUrl url(request.baseUrl + request.path);
    QUrlQuery query;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        // The KuGou gateway rejects requests that carry empty valued parameters
        // (they are also excluded from the signature).
        if (it.value().isEmpty())
            continue;
        query.addQueryItem(it.key(), it.value());
    }
    url.setQuery(query);

    HttpRequest httpRequest;
    httpRequest.url = url;
    httpRequest.method = request.method.toUtf8();

    QMap<QString, QString> headers;
    headers[QStringLiteral("dfid")] = params.value(QStringLiteral("dfid"));
    headers[QStringLiteral("clienttime")] = params.value(QStringLiteral("clienttime"));
    headers[QStringLiteral("mid")] = params.value(QStringLiteral("mid"));
    headers[QStringLiteral("kg-rc")] = QStringLiteral("1");
    headers[QStringLiteral("kg-thash")] = QStringLiteral("5d816a0");
    headers[QStringLiteral("kg-rec")] = QStringLiteral("1");
    headers[QStringLiteral("kg-rf")] = QStringLiteral("B9EDA08A64250DEFFBCADDEE00F8F25F");
    for (auto it = request.headers.constBegin(); it != request.headers.constEnd(); ++it)
        headers[it.key()] = it.value();

    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it)
        httpRequest.setHeader(it.key().toUtf8(), it.value().toUtf8());

    if (!request.body.isEmpty()) {
        httpRequest.body = request.body;
        if (httpRequest.body.startsWith('{'))
            httpRequest.setHeader("Content-Type", "application/json");
    }

    return httpRequest;
}

void KugouClient::sendRaw(Request request, std::function<void(bool, const QByteArray &, const QString &)> callback)
{
    const HttpRequest httpRequest = buildHttp(request);
    Logger::debug(QStringLiteral("kugou → %1").arg(httpRequest.url.toString(QUrl::RemoveQuery)));

    m_http->send(httpRequest, [this, callback](const HttpResponse &response) {
        mergeCookies(response);
        if (!response.error.isEmpty()) {
            if (callback)
                callback(false, response.body, response.error);
            return;
        }
        if (callback)
            callback(true, response.body, QString());
    });
}

void KugouClient::send(Request request, JsonCallback callback)
{
    const QString endpoint = request.path;

    sendRaw(std::move(request), [this, callback, endpoint](bool transportOk, const QByteArray &payload,
                                                           const QString &transportError) {
        if (!transportOk) {
            if (callback)
                callback(false, {}, transportError);
            return;
        }

        emit rawResponse(endpoint, payload);

        QJsonParseError parseError{};
        const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
        if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
            if (callback)
                callback(false, {}, QStringLiteral("响应解析失败"));
            return;
        }

        const QJsonObject body = document.object();
        const int status = body.value(QStringLiteral("status")).toInt(1);
        const int errorCode = body.value(QStringLiteral("error_code")).toInt(0);
        if (status == 0 || errorCode != 0) {
            const QString message = body.value(QStringLiteral("error")).toString(
                body.value(QStringLiteral("msg")).toString(QStringLiteral("请求失败")));
            Logger::warn(QStringLiteral("kugou %1 failed: %2 %3").arg(endpoint).arg(status).arg(message));
            if (callback)
                callback(false, body, message);
            return;
        }

        if (callback)
            callback(true, body, QString());
    });
}

void KugouClient::search(const QString &keywords, int page, int pageSize, JsonCallback callback)
{
    Request request;
    request.path = QStringLiteral("/v3/search/song");
    request.method = QStringLiteral("GET");
    request.params[QStringLiteral("albumhide")] = QStringLiteral("0");
    request.params[QStringLiteral("iscorrection")] = QStringLiteral("1");
    request.params[QStringLiteral("keyword")] = keywords;
    request.params[QStringLiteral("nocollect")] = QStringLiteral("0");
    request.params[QStringLiteral("page")] = QString::number(page);
    request.params[QStringLiteral("pagesize")] = QString::number(pageSize);
    request.params[QStringLiteral("platform")] = QStringLiteral("AndroidFilter");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("complexsearch.kugou.com");

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        const QJsonObject data = body.value(QStringLiteral("data")).toObject();
        QJsonArray list = data.value(QStringLiteral("lists")).toArray();
        if (list.isEmpty())
            list = data.value(QStringLiteral("info")).toArray();

        QJsonArray tracks;
        for (const QJsonValue &value : list) {
            const QJsonObject item = value.toObject();
            Track track;
            track.source = QStringLiteral("kugou");
            track.hash = stringField(item, {QStringLiteral("FileHash"), QStringLiteral("hash")});
            track.id = track.hash;
            track.albumAudioId = stringField(item, {QStringLiteral("AlbumAudioID"), QStringLiteral("album_audio_id"),
                                                    QStringLiteral("audio_id"), QStringLiteral("Audioid")});
            track.albumId = stringField(item, {QStringLiteral("AlbumID"), QStringLiteral("album_id")});
            track.title = stringField(item, {QStringLiteral("SongName"), QStringLiteral("songname"),
                                             QStringLiteral("filename")});
            track.artists = stringField(item, {QStringLiteral("SingerName"), QStringLiteral("singername"),
                                               QStringLiteral("author_name")});
            track.album = stringField(item, {QStringLiteral("AlbumName"), QStringLiteral("album_name")});
            track.coverUrl = stringField(item, {QStringLiteral("Image"), QStringLiteral("img"),
                                                QStringLiteral("album_image")});
            track.durationMs = intField(item, {QStringLiteral("Duration"), QStringLiteral("timelen")}) * 1000;
            track.title = cleanKugouTitle(track.title, track.artists);
            if (track.coverUrl.isEmpty()) {
                const QJsonObject trans = item.value(QStringLiteral("trans_param")).toObject();
                track.coverUrl = stringField(trans, {QStringLiteral("union_cover")});
            }
            track.coverUrl = normalizeKugouCover(track.coverUrl);
            const int privilege = static_cast<int>(intField(item, {QStringLiteral("Privilege")}, 0));
            track.fee = privilege;
            if (track.title.isEmpty() || track.hash.isEmpty())
                continue;
            tracks.append(QJsonValue(track.toJson()));
        }

        QJsonObject out;
        out[QStringLiteral("tracks")] = tracks;
        out[QStringLiteral("total")] = data.value(QStringLiteral("total")).toInt(tracks.size());
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::dailyRecommend(JsonCallback callback)
{
    Request request;
    request.path = QStringLiteral("/everyday_song_recommend");
    request.method = QStringLiteral("POST");
    request.params[QStringLiteral("platform")] = QStringLiteral("ios");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("everydayrec.service.kugou.com");

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }

        QJsonArray list = body.value(QStringLiteral("data")).toArray();
        if (list.isEmpty()) {
            const QJsonObject data = body.value(QStringLiteral("data")).toObject();
            list = data.value(QStringLiteral("song_list")).toArray();
            if (list.isEmpty())
                list = data.value(QStringLiteral("lists")).toArray();
            if (list.isEmpty())
                list = data.value(QStringLiteral("info")).toArray();
        }

        QJsonArray tracks;
        for (const QJsonValue &value : list) {
            const QJsonObject item = value.toObject();
            Track track;
            track.source = QStringLiteral("kugou");
            track.hash = stringField(item, {QStringLiteral("hash"), QStringLiteral("FileHash")});
            track.id = track.hash;
            track.albumAudioId = stringField(item, {QStringLiteral("album_audio_id"), QStringLiteral("AlbumAudioID"),
                                                    QStringLiteral("audio_id"), QStringLiteral("scid"),
                                                    QStringLiteral("mixsongid")});
            track.albumId = stringField(item, {QStringLiteral("album_id"), QStringLiteral("AlbumID")});
            track.title = stringField(item, {QStringLiteral("songname"), QStringLiteral("filename"),
                                             QStringLiteral("SongName"), QStringLiteral("song_name")});
            track.artists = stringField(item, {QStringLiteral("singername"), QStringLiteral("author_name"),
                                               QStringLiteral("SingerName")});
            track.album = stringField(item, {QStringLiteral("album_name"), QStringLiteral("remark"),
                                             QStringLiteral("AlbumName")});
            track.coverUrl = stringField(item, {QStringLiteral("img"), QStringLiteral("album_image"),
                                                QStringLiteral("Image")});
            track.durationMs = intField(item, {QStringLiteral("timelen"), QStringLiteral("time_length"),
                                               QStringLiteral("duration"), QStringLiteral("Duration")}) * 1000;
            track.title = cleanKugouTitle(track.title, track.artists);
            track.coverUrl = normalizeKugouCover(track.coverUrl);
            if (track.title.isEmpty() || track.hash.isEmpty())
                continue;
            tracks.append(QJsonValue(track.toJson()));
        }

        QJsonObject out;
        out[QStringLiteral("tracks")] = tracks;
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::songUrl(const Track &track, Quality quality, StreamCallback callback)
{
    // `/v5/url` (reference project) first, `/v6/priv_url` as a fallback when the
    // gateway answers with an unrelated error.
    songUrlV5(track, quality, [this, track, quality, callback](bool ok, const AudioStream &stream,
                                                               const QString &error) {
        if (ok) {
            if (callback)
                callback(true, stream, QString());
            return;
        }
        songUrlV6(track, quality, [callback, stream, error](bool ok2, const AudioStream &stream2,
                                                            const QString &error2) {
            if (ok2) {
                if (callback)
                    callback(true, stream2, QString());
                return;
            }
            // Prefer the message that actually explains the failure.
            const QString message = !stream2.message.isEmpty() && stream2.message != QLatin1String("无版权")
                ? stream2.message
                : (!stream.message.isEmpty() ? stream.message
                                             : (error.isEmpty() ? error2 : error));
            AudioStream result = stream2;
            result.message = message;
            if (callback)
                callback(false, result, message);
        });
    });
}

void KugouClient::songUrlV5(const Track &track, Quality quality, StreamCallback callback)
{
    Request request;
    request.path = QStringLiteral("/v5/url");
    request.method = QStringLiteral("GET");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("trackercdn.kugou.com");
    request.encryptKey = true;
    request.signKeyHash = track.hash;

    request.params[QStringLiteral("album_id")] = track.albumId.isEmpty() ? QStringLiteral("0") : track.albumId;
    request.params[QStringLiteral("area_code")] = QStringLiteral("1");
    request.params[QStringLiteral("hash")] = track.hash;
    request.params[QStringLiteral("ssa_flag")] = QStringLiteral("is_fromtrack");
    request.params[QStringLiteral("version")] = QStringLiteral("11430");
    request.params[QStringLiteral("page_id")] = QStringLiteral("151369488");
    request.params[QStringLiteral("quality")] = kugouQualityForQuality(quality);
    request.params[QStringLiteral("album_audio_id")] =
        track.albumAudioId.isEmpty() ? QStringLiteral("0") : track.albumAudioId;
    request.params[QStringLiteral("behavior")] = QStringLiteral("play");
    request.params[QStringLiteral("pid")] = QStringLiteral("2");
    request.params[QStringLiteral("cmd")] = QStringLiteral("26");
    request.params[QStringLiteral("pidversion")] = QStringLiteral("3001");
    request.params[QStringLiteral("IsFreePart")] = QStringLiteral("0");
    request.params[QStringLiteral("ppage_id")] = QStringLiteral("463467626,350369493,788954147");
    request.params[QStringLiteral("cdnBackup")] = QStringLiteral("1");
    request.params[QStringLiteral("clientver")] = QStringLiteral("11430");

    if (m_cookies.value(QStringLiteral("dfid")).isEmpty())
        request.params[QStringLiteral("dfid")] = randomPoolString(24);

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        Q_UNUSED(ok)
        const AudioStream stream = parseKugouStream(body);
        if (!stream.url.isEmpty()) {
            if (callback)
                callback(true, stream, QString());
            return;
        }
        if (callback)
            callback(false, stream, stream.message.isEmpty() ? error : stream.message);
    });
}

void KugouClient::songUrlV6(const Track &track, Quality quality, StreamCallback callback)
{
    Q_UNUSED(quality)
    Request request;
    request.baseUrl = QStringLiteral("http://tracker.kugou.com");
    request.path = QStringLiteral("/v6/priv_url");
    request.method = QStringLiteral("POST");

    const qint64 clientTimeMs = QDateTime::currentMSecsSinceEpoch();
    const QString userid = m_cookies.value(QStringLiteral("userid"), QStringLiteral("0"));
    const QString token = m_cookies.value(QStringLiteral("token"));
    const QString vipToken = m_cookies.value(QStringLiteral("vip_token"));
    const QString vipType = m_cookies.value(QStringLiteral("vip_type"), QStringLiteral("0"));
    const QString mid = Fingerprint::instance().kugouMid();

    QJsonArray qualities;
    for (const char *entry : {"128", "320", "flac", "high"})
        qualities.append(QString::fromLatin1(entry));

    QJsonObject resource;
    resource[QStringLiteral("album_audio_id")] = track.albumAudioId.toLongLong();
    resource[QStringLiteral("collect_list_id")] = QStringLiteral("3");
    resource[QStringLiteral("collect_time")] = static_cast<double>(clientTimeMs);
    resource[QStringLiteral("hash")] = track.hash;
    resource[QStringLiteral("id")] = 0;
    resource[QStringLiteral("page_id")] = 1;
    resource[QStringLiteral("type")] = QStringLiteral("audio");

    QJsonObject trackerParam;
    trackerParam[QStringLiteral("all_m")] = 1;
    trackerParam[QStringLiteral("auth")] = QString();
    trackerParam[QStringLiteral("is_free_part")] = 0;
    trackerParam[QStringLiteral("key")] = Crypto::md5Hex(
        (track.hash + QLatin1String("185672dd44712f60bb1736df5a377e82") + QStringLiteral("1005") + mid
         + (userid.isEmpty() ? QStringLiteral("0") : userid))
            .toUtf8());
    trackerParam[QStringLiteral("module_id")] = 0;
    trackerParam[QStringLiteral("need_climax")] = 1;
    trackerParam[QStringLiteral("need_xcdn")] = 1;
    trackerParam[QStringLiteral("open_time")] = QString();
    trackerParam[QStringLiteral("pid")] = QStringLiteral("411");
    trackerParam[QStringLiteral("pidversion")] = QStringLiteral("3001");
    trackerParam[QStringLiteral("priv_vip_type")] = QStringLiteral("6");
    trackerParam[QStringLiteral("viptoken")] = vipToken;

    QJsonObject body;
    body[QStringLiteral("area_code")] = QStringLiteral("1");
    body[QStringLiteral("behavior")] = QStringLiteral("play");
    body[QStringLiteral("qualities")] = qualities;
    body[QStringLiteral("resource")] = resource;
    body[QStringLiteral("token")] = token;
    body[QStringLiteral("tracker_param")] = trackerParam;
    body[QStringLiteral("userid")] = userid;
    body[QStringLiteral("vip")] = vipType.toInt();

    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        Q_UNUSED(ok)
        const AudioStream stream = parseKugouStream(body);
        if (!stream.url.isEmpty()) {
            if (callback)
                callback(true, stream, QString());
            return;
        }
        if (callback)
            callback(false, stream, stream.message.isEmpty() ? error : stream.message);
    });
}
void KugouClient::lyric(const Track &track, TextCallback callback)
{
    Request search;
    search.baseUrl = QStringLiteral("https://lyrics.kugou.com");
    search.path = QStringLiteral("/search");
    search.method = QStringLiteral("GET");

    search.params[QStringLiteral("ver")] = QStringLiteral("1");
    search.params[QStringLiteral("man")] = QStringLiteral("yes");
    search.params[QStringLiteral("client")] = QStringLiteral("pc");
    search.params[QStringLiteral("keyword")] = track.title + QLatin1Char('-') + track.artists;
    search.params[QStringLiteral("duration")] = QString::number(track.durationMs / 1000);
    search.params[QStringLiteral("hash")] = track.hash;

    send(search, [this, callback, track](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        const QJsonArray candidates = body.value(QStringLiteral("candidates")).toArray();
        if (candidates.isEmpty()) {
            if (callback)
                callback(true, QString(), QString());
            return;
        }
        const QJsonObject first = candidates.first().toObject();
        const QString id = stringField(first, {QStringLiteral("id")});
        const QString accessKey = stringField(first, {QStringLiteral("accesskey")});

        Request download;
        download.baseUrl = QStringLiteral("https://lyrics.kugou.com");
        download.path = QStringLiteral("/download");
        download.method = QStringLiteral("GET");

        download.params[QStringLiteral("ver")] = QStringLiteral("1");
        download.params[QStringLiteral("client")] = QStringLiteral("android");
        download.params[QStringLiteral("id")] = id;
        download.params[QStringLiteral("accesskey")] = accessKey;
        download.params[QStringLiteral("fmt")] = QStringLiteral("krc");
        download.params[QStringLiteral("charset")] = QStringLiteral("utf8");

        send(download, [callback, track](bool ok2, const QJsonObject &body2, const QString &error2) {
            Q_UNUSED(track)
            if (!ok2) {
                if (callback)
                    callback(false, {}, error2);
                return;
            }
            const QString content = body2.value(QStringLiteral("content")).toString();
            const int contentType = body2.value(QStringLiteral("contenttype")).toInt(0);
            if (content.isEmpty()) {
                if (callback)
                    callback(true, QString(), QString());
                return;
            }

            if (contentType != 0) {
                if (callback)
                    callback(true, QString::fromUtf8(QByteArray::fromBase64(content.toLatin1())), QString());
                return;
            }

            // KRC: 4 byte header, XOR with a fixed 16 byte key, then zlib inflate.
            QByteArray bytes = QByteArray::fromBase64(content.toLatin1());
            if (bytes.size() <= 4) {
                if (callback)
                    callback(true, QString(), QString());
                return;
            }
            bytes = bytes.mid(4);
            static const unsigned char key[16] = {64, 71, 97, 119, 94, 50, 116, 71, 81, 54, 49, 45, 206, 210, 110, 105};
            for (int i = 0; i < bytes.size(); ++i)
                bytes[i] = static_cast<char>(bytes.at(i) ^ key[i % 16]);

            const QByteArray plain = Crypto::zlibInflate(bytes);
            const QString krc = QString::fromUtf8(plain);
            if (callback)
                callback(true, krcToLrc(krc), QString());
        });
    });
}

void KugouClient::resolveCovers(const QList<Track> &tracks,
                                std::function<void(const QHash<QString, QString> &)> callback)
{
    QJsonArray resource;
    QHash<QString, QString> hashToKey;

    for (const Track &track : tracks) {
        if (resource.size() >= 30)
            break;
        if (!track.coverUrl.isEmpty() || track.hash.isEmpty())
            continue;

        QJsonObject item;
        item[QStringLiteral("type")] = QStringLiteral("audio");
        item[QStringLiteral("page_id")] = 0;
        item[QStringLiteral("hash")] = track.hash;
        item[QStringLiteral("album_id")] = track.albumId.toLongLong();
        resource.append(item);
        hashToKey.insert(track.hash.toUpper(), trackKey(track));
    }

    if (resource.isEmpty()) {
        if (callback)
            callback({});
        return;
    }

    QJsonArray qualities;
    for (const char *quality : {"128", "320", "flac", "high"})
        qualities.append(QString::fromLatin1(quality));

    QJsonObject body;
    body[QStringLiteral("appid")] = 1005;
    body[QStringLiteral("area_code")] = 1;
    body[QStringLiteral("behavior")] = QStringLiteral("play");
    body[QStringLiteral("clientver")] = 20489;
    body[QStringLiteral("need_hash_offset")] = 1;
    body[QStringLiteral("relate")] = 1;
    body[QStringLiteral("support_verify")] = 1;
    body[QStringLiteral("resource")] = resource;
    body[QStringLiteral("qualities")] = qualities;

    Request request;
    request.path = QStringLiteral("/v2/get_res_privilege/lite");
    request.method = QStringLiteral("POST");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("media.store.kugou.com");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [callback, hashToKey, resource](bool ok, const QJsonObject &response, const QString &error) {
        QHash<QString, QString> covers;
        if (!ok) {
            Logger::debug(QStringLiteral("kugou cover lookup failed: %1").arg(error));
            if (callback)
                callback(covers);
            return;
        }

        QJsonArray entries = response.value(QStringLiteral("data")).toArray();
        if (entries.isEmpty()) {
            const QJsonObject data = response.value(QStringLiteral("data")).toObject();
            entries = data.value(QStringLiteral("info")).toArray();
            if (entries.isEmpty())
                entries = data.value(QStringLiteral("lists")).toArray();
        }

        for (const QJsonValue &value : entries) {
            const QJsonObject entry = value.toObject();
            const QString hash = stringField(entry, {QStringLiteral("hash"), QStringLiteral("Hash")});
            QString image = stringField(entry, {QStringLiteral("image"), QStringLiteral("img"),
                                                QStringLiteral("album_image"), QStringLiteral("union_cover")});
            if (image.isEmpty()) {
                const QJsonObject trans = entry.value(QStringLiteral("trans_param")).toObject();
                image = stringField(trans, {QStringLiteral("union_cover")});
            }
            if (image.isEmpty()) {
                const QJsonObject album = entry.value(QStringLiteral("album_info")).toObject();
                image = stringField(album, {QStringLiteral("cover"), QStringLiteral("sizable_cover"),
                                            QStringLiteral("image")});
            }
            if (image.isEmpty())
                continue;

            const QString key = hashToKey.value(hash.toUpper());
            if (!key.isEmpty())
                covers.insert(key, normalizeKugouCover(image));
        }

        Logger::info(QStringLiteral("kugou cover lookup: %1 of %2 resolved")
                         .arg(covers.size())
                         .arg(resource.size()));
        if (!covers.isEmpty())
            Logger::debug(QStringLiteral("kugou cover sample: %1").arg(covers.constBegin().value()));
        if (callback)
            callback(covers);
    });
}

void KugouClient::registerDevice(std::function<void(bool)> callback)
{
    QJsonObject dataMap;
    dataMap[QStringLiteral("availableRamSize")] = 4983533568.0;
    dataMap[QStringLiteral("availableRomSize")] = 48114719.0;
    dataMap[QStringLiteral("availableSDSize")] = 48114717.0;
    dataMap[QStringLiteral("basebandVer")] = QString();
    dataMap[QStringLiteral("batteryLevel")] = 100;
    dataMap[QStringLiteral("batteryStatus")] = 3;
    dataMap[QStringLiteral("brand")] = QStringLiteral("Redmi");
    dataMap[QStringLiteral("buildSerial")] = QStringLiteral("unknown");
    dataMap[QStringLiteral("device")] = QStringLiteral("marble");
    dataMap[QStringLiteral("imei")] = Fingerprint::instance().kugouGuid();
    dataMap[QStringLiteral("imsi")] = QString();
    dataMap[QStringLiteral("manufacturer")] = QStringLiteral("Xiaomi");
    dataMap[QStringLiteral("uuid")] = Fingerprint::instance().kugouGuid();
    dataMap[QStringLiteral("accelerometer")] = false;
    dataMap[QStringLiteral("accelerometerValue")] = QString();
    dataMap[QStringLiteral("gravity")] = false;
    dataMap[QStringLiteral("gravityValue")] = QString();
    dataMap[QStringLiteral("gyroscope")] = false;
    dataMap[QStringLiteral("gyroscopeValue")] = QString();
    dataMap[QStringLiteral("light")] = false;
    dataMap[QStringLiteral("lightValue")] = QString();
    dataMap[QStringLiteral("magnetic")] = false;
    dataMap[QStringLiteral("magneticValue")] = QString();
    dataMap[QStringLiteral("orientation")] = false;
    dataMap[QStringLiteral("orientationValue")] = QString();
    dataMap[QStringLiteral("pressure")] = false;
    dataMap[QStringLiteral("pressureValue")] = QString();
    dataMap[QStringLiteral("step_counter")] = false;
    dataMap[QStringLiteral("step_counterValue")] = QString();
    dataMap[QStringLiteral("temperature")] = false;
    dataMap[QStringLiteral("temperatureValue")] = QString();

    QString aesKey;
    const QJsonObject encrypted = playlistAesEncrypt(dataMap, &aesKey);

    QJsonObject rsaPayload;
    rsaPayload[QStringLiteral("aes")] = aesKey;
    rsaPayload[QStringLiteral("uid")] = m_cookies.value(QStringLiteral("userid"), QStringLiteral("0")).toLongLong();
    rsaPayload[QStringLiteral("token")] = m_cookies.value(QStringLiteral("token"));
    const QByteArray rsaCipher = Crypto::rsaEncryptPkcs1v15Pem(
        QByteArray(kPublicKeyPem), QJsonDocument(rsaPayload).toJson(QJsonDocument::Compact));

    Request request;
    request.baseUrl = QStringLiteral("https://userservice.kugou.com");
    request.path = QStringLiteral("/risk/v2/r_register_dev");
    request.method = QStringLiteral("POST");
    request.params[QStringLiteral("part")] = QStringLiteral("1");
    request.params[QStringLiteral("platid")] = QStringLiteral("1");
    request.params[QStringLiteral("p")] = Crypto::hexLower(rsaCipher);
    request.body = encrypted.value(QStringLiteral("str")).toString().toUtf8();

    sendRaw(request, [this, callback, aesKey](bool transportOk, const QByteArray &payload, const QString &transportError) {
        QString dfid;
        if (transportOk) {
            // The response body is AES-CBC encrypted and base64 encoded.
            const QJsonObject decrypted = playlistAesDecrypt(QString::fromLatin1(payload.toBase64()), aesKey);
            const QJsonObject data = decrypted.value(QStringLiteral("data")).toObject();
            dfid = stringField(data, {QStringLiteral("dfid")});
            if (dfid.isEmpty())
                dfid = stringField(decrypted, {QStringLiteral("dfid")});
            if (!dfid.isEmpty()) {
                m_cookies[QStringLiteral("dfid")] = dfid;
                emit cookiesChanged();
                Logger::info(QStringLiteral("kugou device registered, dfid=%1").arg(dfid));
            }
        } else {
            Logger::warn(QStringLiteral("kugou register_dev failed: %1").arg(transportError));
        }
        if (callback)
            callback(!dfid.isEmpty());
    });
}

void KugouClient::qrKey(JsonCallback callback)
{
    Request request;
    request.baseUrl = QStringLiteral("https://login-user.kugou.com");
    request.path = QStringLiteral("/v2/qrcode");
    request.method = QStringLiteral("GET");
    request.encryptType = QStringLiteral("web");
    request.params[QStringLiteral("appid")] = QStringLiteral("1001");
    request.params[QStringLiteral("type")] = QStringLiteral("1");
    request.params[QStringLiteral("plat")] = QStringLiteral("4");
    request.params[QStringLiteral("qrcode_txt")] =
        QStringLiteral("https://h5.kugou.com/apps/loginQRCode/html/index.html?appid=1005&");
    request.params[QStringLiteral("srcappid")] = QStringLiteral("2919");

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        const QJsonObject data = body.value(QStringLiteral("data")).toObject();
        QString key = stringField(data, {QStringLiteral("qrcode"), QStringLiteral("key")});
        if (key.isEmpty())
            key = stringField(body, {QStringLiteral("qrcode"), QStringLiteral("key")});

        QJsonObject out;
        out[QStringLiteral("key")] = key;
        out[QStringLiteral("url")] = QStringLiteral("https://h5.kugou.com/apps/loginQRCode/html/index.html?qrcode=%1").arg(key);
        if (callback)
            callback(!key.isEmpty(), out, key.isEmpty() ? QStringLiteral("获取二维码失败") : QString());
    });
}

void KugouClient::qrCheck(const QString &key, JsonCallback callback)
{
    Request request;
    request.baseUrl = QStringLiteral("https://login-user.kugou.com");
    request.path = QStringLiteral("/v2/get_userinfo_qrcode");
    request.method = QStringLiteral("GET");
    request.encryptType = QStringLiteral("web");
    request.params[QStringLiteral("plat")] = QStringLiteral("4");
    request.params[QStringLiteral("appid")] = QStringLiteral("1005");
    request.params[QStringLiteral("srcappid")] = QStringLiteral("2919");
    request.params[QStringLiteral("qrcode")] = key;
    request.params[QStringLiteral("dev")] = Fingerprint::instance().kugouDev();

    send(request, [this, callback](bool ok, const QJsonObject &body, const QString &error) {
        Q_UNUSED(error)
        if (!ok) {
            if (callback)
                callback(false, {}, body.value(QStringLiteral("error")).toString());
            return;
        }
        const QJsonObject data = body.value(QStringLiteral("data")).toObject();
        const int status = data.value(QStringLiteral("status")).toInt(0);
        if (status == 4) {
            const QString token = stringField(data, {QStringLiteral("token")});
            const QString userid = stringField(data, {QStringLiteral("userid"), QStringLiteral("uid")});
            if (!token.isEmpty()) {
                m_cookies[QStringLiteral("token")] = token;
                m_cookies[QStringLiteral("userid")] = userid;
                emit cookiesChanged();
                emit loginStateChanged();
                userDetail(nullptr);
            }
        }
        QJsonObject out = data;
        out[QStringLiteral("status")] = status;
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::loginByPassword(const QString &username, const QString &password, JsonCallback callback)
{
    const qint64 dateNow = QDateTime::currentMSecsSinceEpoch();

    QJsonObject plain;
    plain[QStringLiteral("pwd")] = password;
    plain[QStringLiteral("code")] = QString();
    plain[QStringLiteral("clienttime_ms")] = static_cast<double>(dateNow);

    QString aesKey;
    const QString cipherHex = kugouAesEncrypt(QJsonDocument(plain).toJson(QJsonDocument::Compact), randomPoolString(16).toLower(), &aesKey);

    QJsonObject pkPayload;
    pkPayload[QStringLiteral("clienttime_ms")] = static_cast<double>(dateNow);
    pkPayload[QStringLiteral("key")] = aesKey;
    const QString pk = Crypto::hexUpper(
        Crypto::rsaEncryptRawPem(QByteArray(kPublicKeyPem), QJsonDocument(pkPayload).toJson(QJsonDocument::Compact)));

    QJsonObject body;
    body[QStringLiteral("plat")] = 1;
    body[QStringLiteral("support_multi")] = 1;
    body[QStringLiteral("clienttime_ms")] = static_cast<double>(dateNow);
    body[QStringLiteral("t1")] = QStringLiteral(
        "562a6f12a6e803453647d16a08f5f0c2ff7eee692cba2ab74cc4c8ab47fc467561a7c6b586ce7dc46a63613b246737c0"
        "3a1dc8f8d162d8ce1d2c71893d19f1d4b797685a4c6d3d81341cbde65e488c4829a9b4d42ef2df470eb102979fa5adc"
        "dd9b4eecfea8b909ff7599abeb49867640f10c3c70fc444effca9d15db44a9a6c907731e2bb0f22cd9b353638016999"
        "5693e5f0e2424e3378097d3813186e3fe96bbe7023808a0981b4e2b6135a76faac");
    body[QStringLiteral("t2")] = QStringLiteral(
        "31c4daf4cf480169ccea1cb7d4a209295865a9d2b788510301694db229b87807469ea0d41b4d4b9173c2151da7294a"
        "eebfc9738df154bbdf11a4e117bb5dff6a3af8ce5ce333e681c1f29a44038f27567d58992eb81283e080778ac77db14"
        "00fdf49b7cf7e26be2e5af4da7830cc3be4");
    body[QStringLiteral("t3")] = QStringLiteral("MCwwLDAsMCwwLDAsMCwwLDA=");
    body[QStringLiteral("dev")] = Fingerprint::instance().kugouDev();
    body[QStringLiteral("username")] = username;
    body[QStringLiteral("params")] = cipherHex;
    body[QStringLiteral("pk")] = pk;

    Request request;
    request.path = QStringLiteral("/v9/login_by_pwd");
    request.method = QStringLiteral("POST");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("login.user.kugou.com");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [this, callback, aesKey](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, body, error);
            return;
        }

        QJsonObject data = body.value(QStringLiteral("data")).toObject();
        const QString secured = data.value(QStringLiteral("secu_params")).toString();
        if (!secured.isEmpty()) {
            const QByteArray plain = kugouAesDecrypt(secured, aesKey);
            const QJsonDocument inner = QJsonDocument::fromJson(plain);
            if (inner.isObject()) {
                const QJsonObject extra = inner.object();
                for (auto it = extra.constBegin(); it != extra.constEnd(); ++it)
                    data[it.key()] = it.value();
            } else {
                data[QStringLiteral("token")] = QString::fromUtf8(plain);
            }
        }

        const QString token = stringField(data, {QStringLiteral("token")});
        if (!token.isEmpty()) {
            m_cookies[QStringLiteral("token")] = token;
            m_cookies[QStringLiteral("userid")] = stringField(data, {QStringLiteral("userid")});
            m_cookies[QStringLiteral("vip_type")] = stringField(data, {QStringLiteral("vip_type")});
            m_cookies[QStringLiteral("vip_token")] = stringField(data, {QStringLiteral("vip_token")});
            m_cookies[QStringLiteral("t1")] = stringField(data, {QStringLiteral("t1")});
            emit cookiesChanged();
            emit loginStateChanged();
            userDetail(nullptr);
        }

        QJsonObject out;
        out[QStringLiteral("success")] = !token.isEmpty();
        out[QStringLiteral("userid")] = m_cookies.value(QStringLiteral("userid"));
        if (callback)
            callback(!token.isEmpty(), out, token.isEmpty() ? QStringLiteral("登录失败") : QString());
    });
}

void KugouClient::userDetail(JsonCallback callback)
{
    const QString token = m_cookies.value(QStringLiteral("token"));
    const QString userid = m_cookies.value(QStringLiteral("userid"));
    if (token.isEmpty()) {
        if (callback)
            callback(false, {}, QStringLiteral("未登录"));
        return;
    }

    const qint64 seconds = QDateTime::currentSecsSinceEpoch();
    QJsonObject pkPayload;
    pkPayload[QStringLiteral("token")] = token;
    pkPayload[QStringLiteral("clienttime")] = static_cast<double>(seconds);
    const QString pk = Crypto::hexUpper(
        Crypto::rsaEncryptRawPem(QByteArray(kPublicKeyPem), QJsonDocument(pkPayload).toJson(QJsonDocument::Compact)));

    QJsonObject body;
    body[QStringLiteral("visit_time")] = static_cast<double>(seconds);
    body[QStringLiteral("usertype")] = 1;
    body[QStringLiteral("p")] = pk;
    body[QStringLiteral("userid")] = userid.toLongLong();

    Request request;
    request.path = QStringLiteral("/v3/get_my_info");
    request.method = QStringLiteral("POST");
    request.params[QStringLiteral("plat")] = QStringLiteral("1");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("usercenter.kugou.com");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [this, callback](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, body, error);
            return;
        }
        const QJsonObject data = body.value(QStringLiteral("data")).toObject();
        QJsonObject profile;
        profile[QStringLiteral("nickname")] = stringField(data, {QStringLiteral("nickname"),
                                                                 QStringLiteral("user_name")});
        QString avatar = stringField(data, {QStringLiteral("pic"), QStringLiteral("avatar"),
                                            QStringLiteral("img")});
        if (!avatar.isEmpty() && !avatar.startsWith(QLatin1String("http")))
            avatar = QStringLiteral("https://imge.kugou.com/kugouicon/150/") + avatar;
        profile[QStringLiteral("avatarUrl")] = avatar;
        profile[QStringLiteral("userId")] = m_cookies.value(QStringLiteral("userid"));
        if (!profile.value(QStringLiteral("nickname")).toString().isEmpty()) {
            m_profile = profile;
            emit loginStateChanged();
        }
        if (callback)
            callback(true, profile, QString());
    });
}

// ---------------------------------------------------------------------------
// Catalogue browsing (TARGET.md §10.19 / §10.20)
// ---------------------------------------------------------------------------
namespace {

Album parseKugouAlbum(const QJsonObject &object)
{
    Album album;
    album.source = QStringLiteral("kugou");
    album.id = stringField(object, {QStringLiteral("album_id"), QStringLiteral("albumid"),
                                    QStringLiteral("AlbumID")});
    album.name = stringField(object, {QStringLiteral("album_name"), QStringLiteral("albumname"),
                                      QStringLiteral("AlbumName")});
    QString cover = stringField(object, {QStringLiteral("sizable_cover"), QStringLiteral("img"),
                                         QStringLiteral("imgurl"), QStringLiteral("ImgUrl"),
                                         QStringLiteral("banner")});
    album.coverUrl = cover.isEmpty() ? QString() : normalizeKugouCover(cover);

    // `/kmr/v2/albums` returns `authors`, the search API returns `singers`.
    QJsonArray authors = object.value(QStringLiteral("authors")).toArray();
    if (authors.isEmpty())
        authors = object.value(QStringLiteral("singers")).toArray();
    if (!authors.isEmpty()) {
        const QJsonObject author = authors.first().toObject();
        album.artist = stringField(author, {QStringLiteral("author_name"), QStringLiteral("name")});
        album.artistId = stringField(author, {QStringLiteral("author_id"), QStringLiteral("id")});
    }
    if (album.artist.isEmpty())
        album.artist = stringField(object, {QStringLiteral("singer"), QStringLiteral("singername"),
                                            QStringLiteral("author_name")});
    if (album.artistId.isEmpty()) {
        const QJsonArray singerIds = object.value(QStringLiteral("singerids")).toArray();
        if (!singerIds.isEmpty())
            album.artistId = QString::number(static_cast<qint64>(singerIds.first().toDouble()));
    }

    album.publishDate = stringField(object, {QStringLiteral("publish_date"), QStringLiteral("publish_time"),
                                             QStringLiteral("publishtime")});
    if (album.publishDate.contains(QLatin1Char(' ')))
        album.publishDate = album.publishDate.section(QLatin1Char(' '), 0, 0);
    album.company = stringField(object, {QStringLiteral("company")});
    album.description = stringField(object, {QStringLiteral("intro"), QStringLiteral("short_intro")});
    album.songCount = static_cast<int>(intField(object, {QStringLiteral("song_count"),
                                                         QStringLiteral("songcount")}));
    return album;
}

Mv parseKugouMv(const QJsonObject &object)
{
    Mv mv;
    mv.source = QStringLiteral("kugou");
    // search API: MvID / MvHash, artist page: hash / id
    mv.id = stringField(object, {QStringLiteral("MvID"), QStringLiteral("id"), QStringLiteral("hash"),
                                 QStringLiteral("MvHash"), QStringLiteral("video_hash"),
                                 QStringLiteral("mkvhash")});
    mv.videoHash = stringField(object, {QStringLiteral("MvHash"), QStringLiteral("FileHash"),
                                        QStringLiteral("video_hash")});
    mv.name = stringField(object, {QStringLiteral("MvName"), QStringLiteral("filename"),
                                   QStringLiteral("video_name"), QStringLiteral("name")});
    mv.artist = stringField(object, {QStringLiteral("SingerName"), QStringLiteral("singername"),
                                     QStringLiteral("author_name")});
    QJsonArray singers = object.value(QStringLiteral("Singers")).toArray();
    if (singers.isEmpty())
        singers = object.value(QStringLiteral("singers")).toArray();
    if (!singers.isEmpty()) {
        const QJsonObject singer = singers.first().toObject();
        if (mv.artist.isEmpty())
            mv.artist = stringField(singer, {QStringLiteral("name")});
        mv.artistId = stringField(singer, {QStringLiteral("id")});
    }
    if (mv.artistId.isEmpty()) {
        const QJsonArray singerIds = object.value(QStringLiteral("SingerID")).toArray();
        if (!singerIds.isEmpty())
            mv.artistId = QString::number(static_cast<qint64>(singerIds.first().toDouble()));
    }
    const QString cover = stringField(object, {QStringLiteral("imgurl"), QStringLiteral("cover"),
                                               QStringLiteral("Pic")});
    if (!cover.isEmpty()) {
        // The MV search API only returns the picture file name; the public
        // CDN path is /mvhdpic/<size>/<file>.
        mv.coverUrl = cover.startsWith(QLatin1String("http"))
            ? normalizeKugouCover(cover)
            : QStringLiteral("https://imge.kugou.com/mvhdpic/240/%1").arg(cover);
    }
    qint64 seconds = intField(object, {QStringLiteral("Duration"), QStringLiteral("duration"),
                                       QStringLiteral("timelength")});
    if (seconds > 100000)
        seconds /= 1000;
    mv.durationMs = seconds * 1000;
    mv.playCount = intField(object, {QStringLiteral("HistoryHeat"), QStringLiteral("MvHot"),
                                     QStringLiteral("play_count"), QStringLiteral("playcount")});
    mv.publishTime = stringField(object, {QStringLiteral("PublishDate"), QStringLiteral("publish_time"),
                                          QStringLiteral("addtime")});
    if (mv.publishTime.contains(QLatin1Char(' ')))
        mv.publishTime = mv.publishTime.section(QLatin1Char(' '), 0, 0);
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

Track makeTrack(const QJsonObject &item)
{
    Track track;
    track.source = QStringLiteral("kugou");
    track.hash = stringField(item, {QStringLiteral("FileHash"), QStringLiteral("hash")});
    track.id = track.hash;
    track.albumAudioId = stringField(item, {QStringLiteral("AlbumAudioID"), QStringLiteral("album_audio_id"),
                                            QStringLiteral("audio_id"), QStringLiteral("Audioid")});
    track.albumId = stringField(item, {QStringLiteral("AlbumID"), QStringLiteral("album_id")});
    track.title = stringField(item, {QStringLiteral("SongName"), QStringLiteral("songname"),
                                     QStringLiteral("filename"), QStringLiteral("audio_name")});
    track.artists = stringField(item, {QStringLiteral("SingerName"), QStringLiteral("singername"),
                                       QStringLiteral("author_name")});
    track.album = stringField(item, {QStringLiteral("AlbumName"), QStringLiteral("album_name")});
    {
        QJsonArray singers = item.value(QStringLiteral("Singers")).toArray();
        if (singers.isEmpty())
            singers = item.value(QStringLiteral("singers")).toArray();
        if (!singers.isEmpty())
            track.artistId = stringField(singers.first().toObject(), {QStringLiteral("id")});
        if (track.artistId.isEmpty()) {
            const QJsonArray singerIds = item.value(QStringLiteral("SingerID")).toArray();
            if (!singerIds.isEmpty())
                track.artistId = QString::number(static_cast<qint64>(singerIds.first().toDouble()));
        }
    }
    track.coverUrl = stringField(item, {QStringLiteral("Image"), QStringLiteral("img"),
                                        QStringLiteral("album_image")});
    qint64 duration = intField(item, {QStringLiteral("Duration"), QStringLiteral("timelen")});
    if (duration > 0 && duration < 100000)
        duration *= 1000;
    track.durationMs = duration;
    track.title = cleanKugouTitle(track.title, track.artists);
    if (track.coverUrl.isEmpty()) {
        const QJsonObject trans = item.value(QStringLiteral("trans_param")).toObject();
        track.coverUrl = stringField(trans, {QStringLiteral("union_cover")});
    }
    track.coverUrl = normalizeKugouCover(track.coverUrl);
    track.fee = static_cast<int>(intField(item, {QStringLiteral("Privilege")}, 0));
    return track;
}

QJsonArray objectList(const QJsonObject &body)
{
    QJsonArray list = body.value(QStringLiteral("data")).toArray();
    if (!list.isEmpty())
        return list;
    const QJsonObject data = body.value(QStringLiteral("data")).toObject();
    for (const char *key : {"lists", "info", "list", "album_list", "audios", "songs", "videos"}) {
        const QJsonArray candidate = data.value(QLatin1String(key)).toArray();
        if (!candidate.isEmpty())
            return candidate;
    }
    return QJsonArray();
}

} // namespace

void KugouClient::searchAlbums(const QString &keywords, int page, int pageSize, JsonCallback callback)
{
    Request request;
    request.path = QStringLiteral("/v1/search/album");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("complexsearch.kugou.com");
    request.params[QStringLiteral("albumhide")] = QStringLiteral("0");
    request.params[QStringLiteral("iscorrection")] = QStringLiteral("1");
    request.params[QStringLiteral("keyword")] = keywords;
    request.params[QStringLiteral("nocollect")] = QStringLiteral("0");
    request.params[QStringLiteral("page")] = QString::number(page);
    request.params[QStringLiteral("pagesize")] = QString::number(pageSize);
    request.params[QStringLiteral("platform")] = QStringLiteral("AndroidFilter");

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QList<Album> albums;
        for (const QJsonValue &value : objectList(body))
            albums.append(parseKugouAlbum(value.toObject()));
        QJsonObject out;
        out[QStringLiteral("albums")] = albumsToJson(albums);
        out[QStringLiteral("total")] = body.value(QStringLiteral("data"))
                                           .toObject()
                                           .value(QStringLiteral("total"))
                                           .toInt(albums.size());
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::searchMvs(const QString &keywords, int page, int pageSize, JsonCallback callback)
{
    Request request;
    request.path = QStringLiteral("/v1/search/mv");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("complexsearch.kugou.com");
    request.params[QStringLiteral("albumhide")] = QStringLiteral("0");
    request.params[QStringLiteral("iscorrection")] = QStringLiteral("1");
    request.params[QStringLiteral("keyword")] = keywords;
    request.params[QStringLiteral("nocollect")] = QStringLiteral("0");
    request.params[QStringLiteral("page")] = QString::number(page);
    request.params[QStringLiteral("pagesize")] = QString::number(pageSize);
    request.params[QStringLiteral("platform")] = QStringLiteral("AndroidFilter");

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QList<Mv> mvs;
        for (const QJsonValue &value : objectList(body))
            mvs.append(parseKugouMv(value.toObject()));
        QJsonObject out;
        out[QStringLiteral("mvs")] = mvsToJson(mvs);
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::albumDetail(const QString &albumId, JsonCallback callback)
{
    QJsonObject entry;
    entry[QStringLiteral("album_id")] = albumId;

    QJsonObject body;
    body[QStringLiteral("data")] = QJsonArray{entry};
    body[QStringLiteral("is_buy")] = 0;
    body[QStringLiteral("fields")] = QStringLiteral(
        "album_id,album_name,publish_date,sizable_cover,intro,language,is_publish,heat,type,quality,"
        "authors,exclusive,author_name,trans_param,song_count");

    Request request;
    request.path = QStringLiteral("/kmr/v2/albums");
    request.method = QStringLiteral("POST");
    request.baseUrl = QStringLiteral("https://openapi.kugou.com");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("openapi.kugou.com");
    request.headers[QStringLiteral("kg-tid")] = QStringLiteral("255");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [callback](bool ok, const QJsonObject &response, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        const QJsonArray list = objectList(response);
        Album album;
        if (!list.isEmpty())
            album = parseKugouAlbum(list.first().toObject());
        QJsonObject out;
        out[QStringLiteral("album")] = QJsonObject::fromVariantMap(album.toVariantMap());
        if (callback)
            callback(true, out, QString());
    });
}

namespace {

/// `/v1/album_audio/lite` returns a much richer item than the search API.
Track makeAlbumTrack(const QJsonObject &item)
{
    const QJsonObject base = item.value(QStringLiteral("base")).toObject();
    const QJsonObject audio = item.value(QStringLiteral("audio_info")).toObject();
    const QJsonObject albumInfo = item.value(QStringLiteral("album_info")).toObject();

    Track track;
    track.source = QStringLiteral("kugou");
    track.hash = stringField(audio, {QStringLiteral("hash")});
    track.id = track.hash;
    track.albumAudioId = stringField(base, {QStringLiteral("album_audio_id")});
    track.albumId = stringField(base, {QStringLiteral("album_id")});
    track.title = stringField(base, {QStringLiteral("audio_name")});
    track.artists = stringField(base, {QStringLiteral("author_name")});
    const QJsonArray authors = item.value(QStringLiteral("authors")).toArray();
    if (!authors.isEmpty()) {
        if (track.artists.isEmpty())
            track.artists = stringField(authors.first().toObject(), {QStringLiteral("author_name")});
        track.artistId = stringField(authors.first().toObject(), {QStringLiteral("author_id")});
    }
    track.album = stringField(albumInfo, {QStringLiteral("album_name")});
    track.coverUrl = stringField(albumInfo, {QStringLiteral("cover")});
    if (track.coverUrl.isEmpty())
        track.coverUrl = stringField(item.value(QStringLiteral("trans_param")).toObject(),
                                     {QStringLiteral("union_cover")});
    track.coverUrl = normalizeKugouCover(track.coverUrl);
    track.durationMs = intField(audio, {QStringLiteral("duration")});
    track.fee = static_cast<int>(intField(item.value(QStringLiteral("copyright")).toObject(),
                                          {QStringLiteral("privilege")}));
    track.title = cleanKugouTitle(track.title, track.artists);
    return track;
}

} // namespace

void KugouClient::albumSongs(const QString &albumId, int page, int pageSize, JsonCallback callback)
{
    QJsonObject body;
    body[QStringLiteral("album_id")] = albumId;
    body[QStringLiteral("page")] = page;
    body[QStringLiteral("pagesize")] = qMin(pageSize, 50);

    Request request;
    request.path = QStringLiteral("/v1/album_audio/lite");
    request.method = QStringLiteral("POST");
    request.baseUrl = QStringLiteral("https://openapi.kugou.com");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("openapi.kugou.com");
    request.headers[QStringLiteral("kg-tid")] = QStringLiteral("255");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [callback](bool ok, const QJsonObject &response, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QJsonArray tracks;
        for (const QJsonValue &value : objectList(response)) {
            const Track track = makeAlbumTrack(value.toObject());
            if (track.title.isEmpty() || track.hash.isEmpty())
                continue;
            tracks.append(QJsonValue(track.toJson()));
        }
        QJsonObject out;
        out[QStringLiteral("tracks")] = tracks;
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::artistDetail(const QString &artistId, JsonCallback callback)
{
    QJsonObject body;
    body[QStringLiteral("author_id")] = artistId;

    Request request;
    request.path = QStringLiteral("/kmr/v3/author");
    request.method = QStringLiteral("POST");
    request.baseUrl = QStringLiteral("https://openapi.kugou.com");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("openapi.kugou.com");
    request.headers[QStringLiteral("kg-tid")] = QStringLiteral("36");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [callback, artistId](bool ok, const QJsonObject &response, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QJsonObject object = response.value(QStringLiteral("data")).toObject();
        if (object.isEmpty())
            object = response;

        Artist artist;
        artist.source = QStringLiteral("kugou");
        artist.id = stringField(object, {QStringLiteral("author_id"), QStringLiteral("singerid")});
        if (artist.id.isEmpty())
            artist.id = artistId;
        artist.name = stringField(object, {QStringLiteral("author_name"), QStringLiteral("singername")});
        const QString avatar = stringField(object, {QStringLiteral("sizable_avatar"),
                                                    QStringLiteral("avatar"), QStringLiteral("imgurl")});
        artist.avatarUrl = avatar.isEmpty() ? QString() : normalizeKugouCover(avatar);
        artist.description = stringField(object, {QStringLiteral("intro"), QStringLiteral("profile")});
        artist.songCount = stringField(object, {QStringLiteral("song_count"), QStringLiteral("audio_num")}).toInt();
        artist.albumCount = stringField(object, {QStringLiteral("album_count"), QStringLiteral("album_num")}).toInt();
        artist.mvCount = stringField(object, {QStringLiteral("video_count"), QStringLiteral("mv_num")}).toInt();
        artist.fansCount = stringField(object, {QStringLiteral("fansnums"), QStringLiteral("fans_count")}).toLongLong();

        QJsonObject out;
        out[QStringLiteral("artist")] = QJsonObject::fromVariantMap(artist.toVariantMap());
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::artistSongs(const QString &artistId, int page, int pageSize, bool hot,
                              JsonCallback callback)
{
    const qint64 clientTime = QDateTime::currentSecsSinceEpoch();
    QJsonObject body;
    body[QStringLiteral("appid")] = 1005;
    body[QStringLiteral("clientver")] = 20489;
    body[QStringLiteral("mid")] = Fingerprint::instance().kugouMid();
    body[QStringLiteral("clienttime")] = clientTime;
    body[QStringLiteral("key")] = Crypto::md5Hex(
        QStringLiteral("1005OIlwieks28dk2k092lksi2UIkp20489%1").arg(clientTime).toUtf8());
    body[QStringLiteral("author_id")] = artistId;
    body[QStringLiteral("pagesize")] = pageSize;
    body[QStringLiteral("page")] = page;
    body[QStringLiteral("sort")] = hot ? 1 : 2;
    body[QStringLiteral("area_code")] = QStringLiteral("all");

    Request request;
    request.path = QStringLiteral("/kmr/v1/audio_group/author");
    request.method = QStringLiteral("POST");
    request.baseUrl = QStringLiteral("https://openapi.kugou.com");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("openapi.kugou.com");
    request.headers[QStringLiteral("kg-tid")] = QStringLiteral("220");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [callback](bool ok, const QJsonObject &response, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QJsonArray tracks;
        for (const QJsonValue &value : objectList(response))
            tracks.append(QJsonValue(makeTrack(value.toObject()).toJson()));
        QJsonObject out;
        out[QStringLiteral("tracks")] = tracks;
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::artistAlbums(const QString &artistId, int page, int pageSize, bool hot,
                               JsonCallback callback)
{
    QJsonObject body;
    body[QStringLiteral("author_id")] = artistId;
    body[QStringLiteral("pagesize")] = pageSize;
    body[QStringLiteral("page")] = page;
    body[QStringLiteral("sort")] = hot ? 3 : 1;
    body[QStringLiteral("category")] = 1;
    body[QStringLiteral("area_code")] = QStringLiteral("all");

    Request request;
    request.path = QStringLiteral("/kmr/v1/author/albums");
    request.method = QStringLiteral("POST");
    request.baseUrl = QStringLiteral("https://openapi.kugou.com");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("openapi.kugou.com");
    request.headers[QStringLiteral("kg-tid")] = QStringLiteral("36");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);

    send(request, [callback](bool ok, const QJsonObject &response, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QList<Album> albums;
        for (const QJsonValue &value : objectList(response))
            albums.append(parseKugouAlbum(value.toObject()));
        QJsonObject out;
        out[QStringLiteral("albums")] = albumsToJson(albums);
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::artistMvs(const QString &artistId, int page, int pageSize, JsonCallback callback)
{
    Request request;
    request.path = QStringLiteral("/kmr/v1/author/videos");
    request.baseUrl = QStringLiteral("https://openapicdn.kugou.com");
    request.params[QStringLiteral("author_id")] = artistId;
    request.params[QStringLiteral("is_fanmade")] = QString();
    request.params[QStringLiteral("tag_idx")] = QString();
    request.params[QStringLiteral("pagesize")] = QString::number(pageSize);
    request.params[QStringLiteral("page")] = QString::number(page);

    send(request, [callback](bool ok, const QJsonObject &response, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QList<Mv> mvs;
        for (const QJsonValue &value : objectList(response))
            mvs.append(parseKugouMv(value.toObject()));
        QJsonObject out;
        out[QStringLiteral("mvs")] = mvsToJson(mvs);
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::videoUrl(const QString &hash, JsonCallback callback)
{
    Request request;
    request.path = QStringLiteral("/v2/interface/index");
    request.baseUrl = QStringLiteral("https://trackermv.kugou.com");
    request.headers[QStringLiteral("x-router")] = QStringLiteral("trackermv.kugou.com");
    request.params[QStringLiteral("backupdomain")] = QStringLiteral("1");
    request.params[QStringLiteral("cmd")] = QStringLiteral("123");
    request.params[QStringLiteral("ext")] = QStringLiteral("mp4");
    request.params[QStringLiteral("ismp3")] = QStringLiteral("0");
    request.params[QStringLiteral("hash")] = hash;
    request.params[QStringLiteral("pid")] = QStringLiteral("1");
    request.params[QStringLiteral("type")] = QStringLiteral("1");
    request.encryptKey = true;

    send(request, [callback](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        // {"data": {"<hash>": {"downurl": "...", "backupdownurl": ["..."]}}}
        const QJsonObject data = body.value(QStringLiteral("data")).toObject();
        QJsonObject entry = data;
        if (!data.contains(QStringLiteral("url")) && !data.contains(QStringLiteral("downurl"))) {
            for (auto it = data.constBegin(); it != data.constEnd(); ++it) {
                if (it.value().isObject()) {
                    entry = it.value().toObject();
                    break;
                }
            }
        }
        QString url = stringField(entry, {QStringLiteral("url"), QStringLiteral("downurl"),
                                          QStringLiteral("play_url"), QStringLiteral("mp4")});
        if (url.isEmpty()) {
            const QJsonArray backups = entry.value(QStringLiteral("backupdownurl")).toArray();
            if (!backups.isEmpty())
                url = backups.first().toString();
        }
        if (url.isEmpty())
            url = stringField(body, {QStringLiteral("url")});
        QJsonObject out;
        out[QStringLiteral("url")] = url;
        if (url.isEmpty()) {
            if (callback)
                callback(false, out, QStringLiteral("未取到 MV 播放地址"));
            return;
        }
        if (callback)
            callback(true, out, QString());
    });
}

void KugouClient::likedVideos(int pageSize, JsonCallback callback)
{
    Request request;
    request.path = QStringLiteral("/m.comment.service/v1/get_user_like_video");
    request.baseUrl = QStringLiteral("https://gateway.kugou.com");
    const QString uid = userId();
    request.params[QStringLiteral("kugouid")] = uid.isEmpty() ? QStringLiteral("0") : uid;
    request.params[QStringLiteral("pagesize")] = QString::number(pageSize);
    request.params[QStringLiteral("load_video_info")] = QStringLiteral("1");
    request.params[QStringLiteral("p")] = QStringLiteral("1");
    request.params[QStringLiteral("plat")] = QStringLiteral("1");

    send(request, [callback](bool ok, const QJsonObject &response, const QString &error) {
        if (!ok) {
            if (callback)
                callback(false, {}, error);
            return;
        }
        QList<Mv> mvs;
        for (const QJsonValue &value : objectList(response))
            mvs.append(parseKugouMv(value.toObject()));
        QJsonObject out;
        out[QStringLiteral("mvs")] = mvsToJson(mvs);
        if (callback)
            callback(true, out, QString());
    });
}

} // namespace mr
