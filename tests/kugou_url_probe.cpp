// Developer tool: finds the parameter set accepted by KuGou `/v5/url`.
//
//   mrmusic_kugouurl [hash] [album_id] [album_audio_id] [dfid]

#include "api/HttpClient.h"
#include "core/AppPaths.h"
#include "core/Crypto.h"
#include "core/Fingerprint.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QMap>
#include <QStringList>
#include <QUrlQuery>

#include <algorithm>
#include <functional>

using namespace mr;

namespace {

const char *const kSalt = "OIlwieks28dk2k092lksi2UIkp";
const char *const kSignKeySalt = "57ae12eb6890223e355ccfcb74edf70d";

QString g_hash = QStringLiteral("B3A52A7A958BF0AED0EBFBA2E9A818B7");
QString g_albumId = QStringLiteral("966846");
QString g_albumAudioId = QStringLiteral("20505418");
QString g_dfid = QStringLiteral("0123456789ABCDEFGHIJKL");
QString g_quality = QStringLiteral("flac");

QString paramString(const QMap<QString, QString> &params)
{
    QStringList items;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        items << it.key() + QLatin1Char('=') + it.value();
    std::sort(items.begin(), items.end());
    return items.join(QString());
}

QString signature(const QMap<QString, QString> &params)
{
    const QString salt = QString::fromLatin1(kSalt);
    return Crypto::md5Hex((salt + paramString(params) + salt).toUtf8());
}

QString signKey(const QString &hash, const QString &mid, const QString &appid)
{
    return Crypto::md5Hex((hash + QLatin1String(kSignKeySalt) + appid + mid + QStringLiteral("0")).toUtf8());
}

struct Variant
{
    QString name;
    QStringList extras;   // extra parameter names to include
    bool includeKey = false;
    bool signed_ = true;
    bool signWithoutKey = false;
    bool upperHashKey = false;
};

void runVariant(HttpClient &http, const QList<Variant> &variants, int index, std::function<void()> done)
{
    if (index >= variants.size()) {
        done();
        return;
    }
    const Variant variant = variants.at(index);
    const QString clientTime = QString::number(QDateTime::currentSecsSinceEpoch());
    const QString mid = Fingerprint::instance().kugouMid();

    QMap<QString, QString> params;
    params[QStringLiteral("dfid")] = g_dfid;
    params[QStringLiteral("mid")] = mid;
    params[QStringLiteral("uuid")] = QStringLiteral("-");
    params[QStringLiteral("appid")] = QStringLiteral("1005");
    params[QStringLiteral("clientver")] = QStringLiteral("20489");
    params[QStringLiteral("clienttime")] = clientTime;
    params[QStringLiteral("hash")] = g_hash;
    params[QStringLiteral("album_id")] = g_albumId;
    params[QStringLiteral("album_audio_id")] = g_albumAudioId;
    params[QStringLiteral("area_code")] = QStringLiteral("1");
    params[QStringLiteral("behavior")] = QStringLiteral("play");
    params[QStringLiteral("quality")] = g_quality;
    params[QStringLiteral("ssa_flag")] = QStringLiteral("is_fromtrack");
    params[QStringLiteral("version")] = QStringLiteral("11430");

    for (const QString &extra : variant.extras) {
        if (extra == QLatin1String("page_id"))
            params[extra] = QStringLiteral("151369488");
        else if (extra == QLatin1String("pid"))
            params[extra] = QStringLiteral("2");
        else if (extra == QLatin1String("cmd"))
            params[extra] = QStringLiteral("26");
        else if (extra == QLatin1String("pidversion"))
            params[extra] = QStringLiteral("3001");
        else if (extra == QLatin1String("IsFreePart"))
            params[extra] = QStringLiteral("0");
        else if (extra == QLatin1String("ppage_id"))
            params[extra] = QStringLiteral("463467626,350369493,788954147");
        else if (extra == QLatin1String("cdnBackup"))
            params[extra] = QStringLiteral("1");
        else if (extra == QLatin1String("module"))
            params[extra] = QString();
        else if (extra == QLatin1String("clientver11430"))
            params[QStringLiteral("clientver")] = QStringLiteral("11430");
    }

    QString keyValue;
    if (variant.includeKey) {
        keyValue = signKey(variant.upperHashKey ? g_hash : params.value(QStringLiteral("hash")), mid,
                           QStringLiteral("1005"));
    }
    if (variant.signed_) {
        QMap<QString, QString> forSignature = params;
        if (variant.includeKey && variant.signWithoutKey)
            forSignature.remove(QStringLiteral("key"));
        if (variant.includeKey && !variant.signWithoutKey) {
            forSignature[QStringLiteral("key")] = keyValue;
        }
        params[QStringLiteral("signature")] = signature(forSignature);
    }
    if (variant.includeKey)
        params[QStringLiteral("key")] = keyValue;

    QUrl url(QStringLiteral("https://gateway.kugou.com/v5/url"));
    QUrlQuery query;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it)
        query.addQueryItem(it.key(), it.value());
    url.setQuery(query);

    HttpRequest request;
    request.url = url;
    request.method = QByteArrayLiteral("GET");
    request.setHeader("User-Agent", "Android15-1070-11083-46-0-DiscoveryDRADProtocol-wifi");
    request.setHeader("dfid", g_dfid.toUtf8());
    request.setHeader("clienttime", clientTime.toUtf8());
    request.setHeader("mid", mid.toUtf8());
    request.setHeader("kg-rc", "1");
    request.setHeader("kg-thash", "5d816a0");
    request.setHeader("kg-rec", "1");
    request.setHeader("kg-rf", "B9EDA08A64250DEFFBCADDEE00F8F25F");
    request.setHeader("x-router", "trackercdn.kugou.com");

    http.send(request, [&http, variants, index, variant, done](const HttpResponse &response) {
        qInfo().noquote() << QStringLiteral("── %1 ──").arg(variant.name);
        qInfo().noquote() << response.status << response.body.left(300);
        runVariant(http, variants, index + 1, done);
    }, 0);
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MrMusic"));
    AppPaths::initialize();
    Logger::initialize();

    if (argc > 1)
        g_hash = QString::fromLocal8Bit(argv[1]);
    if (argc > 2)
        g_albumId = QString::fromLocal8Bit(argv[2]);
    if (argc > 3)
        g_albumAudioId = QString::fromLocal8Bit(argv[3]);
    if (argc > 4)
        g_dfid = QString::fromLocal8Bit(argv[4]);
    if (argc > 5)
        g_quality = QString::fromLocal8Bit(argv[5]);

    HttpClient http;
    http.setUserAgent(QByteArray("Android15-1070-11083-46-0-DiscoveryDRADProtocol-wifi"));

    const QStringList incremental = {QStringLiteral("page_id"), QStringLiteral("pid"), QStringLiteral("cmd"),
                                     QStringLiteral("pidversion"), QStringLiteral("IsFreePart"),
                                     QStringLiteral("ppage_id"), QStringLiteral("cdnBackup"),
                                     QStringLiteral("clientver11430"), QStringLiteral("module")};

    QList<Variant> variants;
    const QStringList extrasNoModule = incremental.mid(0, incremental.size() - 1);
    variants.append({QStringLiteral("B quality=%1 hash=%2").arg(g_quality, g_hash), extrasNoModule, true, true, false, false});

    runVariant(http, variants, 0, []() { QCoreApplication::quit(); });
    return app.exec();
}
