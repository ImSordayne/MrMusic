// Live probe for the NetEase / KuGou direct API clients.
//
// Developer tool: performs real requests against the upstream services to
// verify signatures, encryption and response parsing. It never starts a local
// HTTP server; every request goes directly to the upstream host.
//
//   mrmusic_apiprobe [keyword]

#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/AppPaths.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QTimer>

using namespace mr;

namespace {

QString g_keyword = QStringLiteral("周杰伦");
bool g_dumpRaw = false;

void dump(const QString &title, const QJsonObject &object, int maxLength = 1200)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    qInfo().noquote() << QStringLiteral("── %1 ──").arg(title);
    qInfo().noquote() << (json.size() > maxLength ? json.left(maxLength) + "..." : json);
}

// ---------------------------------------------------------------------------
// KuGou chain: register device -> search -> url -> lyric -> daily
// ---------------------------------------------------------------------------
void kugouSearchChain(KugouClient &kugou, std::function<void()> done);
void kugouChain(KugouClient &kugou, std::function<void()> done)
{
    kugou.registerDevice([&kugou, done](bool registered) {
        qInfo().noquote() << "kugou register_dev:" << registered
                          << "dfid=" << kugou.cookies().value(QStringLiteral("dfid")).toString();
        kugouSearchChain(kugou, done);
    });
}

void kugouSearchChain(KugouClient &kugou, std::function<void()> done)
{
    kugou.search(g_keyword, 1, 3, [&kugou, done](bool ok, const QJsonObject &result, const QString &error) {
        qInfo().noquote() << "kugou search:" << ok << error;
        if (!ok) {
            done();
            return;
        }
        const QJsonArray tracks = result.value(QStringLiteral("tracks")).toArray();
        qInfo().noquote() << "  tracks:" << tracks.size();
        if (tracks.isEmpty()) {
            done();
            return;
        }
        for (int i = 0; i < tracks.size(); ++i)
            dump(QStringLiteral("kugou track %1").arg(i), tracks.at(i).toObject(), 500);

        const Track track = Track::fromJson(tracks.first().toObject());
        kugou.songUrl(track, Quality::Lossless, [&kugou, track, done](bool ok2, const AudioStream &stream,
                                                                     const QString &error2) {
            qInfo().noquote() << "kugou url:" << ok2 << error2 << stream.url.left(100) << stream.extension
                              << stream.bitrate << stream.size;
            kugou.lyric(track, [&kugou, done](bool ok3, const QString &lyric, const QString &error3) {
                qInfo().noquote() << "kugou lyric:" << ok3 << error3 << "chars=" << lyric.size();
                qInfo().noquote() << lyric.left(300);
                kugou.dailyRecommend([done](bool ok4, const QJsonObject &daily, const QString &error4) {
                    qInfo().noquote() << "kugou daily:" << ok4 << error4
                                      << "count=" << daily.value(QStringLiteral("tracks")).toArray().size();
                    done();
                });
            });
        });
    });
}

// ---------------------------------------------------------------------------
// NetEase chain: anonymous -> search -> url (free track) -> lyric -> qr key
// ---------------------------------------------------------------------------
void neteaseChain(NeteaseClient &netease, std::function<void()> done)
{
    netease.registerAnonymous([&netease, done](bool ok, const QJsonObject &body, const QString &error) {
        qInfo().noquote() << "netease anonymous:" << ok << error << "token=" << netease.anonymousToken().size();
        if (g_dumpRaw)
            dump("anonymous body", body, 300);

        netease.search(g_keyword, 10, 0, [&netease, done](bool ok2, const QJsonObject &result,
                                                         const QString &error2) {
            qInfo().noquote() << "netease search:" << ok2 << error2;
            if (!ok2) {
                done();
                return;
            }
            const QJsonArray tracks = result.value(QStringLiteral("tracks")).toArray();
            qInfo().noquote() << "  tracks:" << tracks.size();
            if (tracks.isEmpty()) {
                done();
                return;
            }
            for (int i = 0; i < qMin(3, tracks.size()); ++i)
                dump(QStringLiteral("netease track %1").arg(i), tracks.at(i).toObject(), 500);

            // prefer a non VIP track for the URL probe
            Track chosen = Track::fromJson(tracks.first().toObject());
            for (const QJsonValue &value : tracks) {
                const Track candidate = Track::fromJson(value.toObject());
                if (!candidate.vip) {
                    chosen = candidate;
                    break;
                }
            }
            qInfo().noquote() << "  probing url for:" << chosen.title;

            netease.songUrl(chosen, Quality::High, [&netease, chosen, done](bool ok3, const AudioStream &stream,
                                                                            const QString &error3) {
                qInfo().noquote() << "netease url:" << ok3 << error3 << stream.url.left(100) << stream.extension
                                  << stream.bitrate;
                netease.lyric(chosen, [&netease, done](bool ok4, const QString &lyric, const QString &error4) {
                    qInfo().noquote() << "netease lyric:" << ok4 << error4 << "chars=" << lyric.size();
                    netease.qrKey([done](bool ok5, const QJsonObject &key, const QString &error5) {
                        qInfo().noquote() << "netease qr key:" << ok5 << error5 << key.value(QStringLiteral("key")).toString();
                        qInfo().noquote() << "  qr url:" << key.value(QStringLiteral("url")).toString();
                        done();
                    });
                });
            });
        });
    });
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MrMusic"));
    AppPaths::initialize();
    Logger::initialize();

    for (int i = 1; i < argc; ++i) {
        const QString arg = QString::fromLocal8Bit(argv[i]);
        if (arg == QLatin1String("--raw"))
            g_dumpRaw = true;
        else
            g_keyword = arg;
    }

    qInfo().noquote() << "MrMusic API probe, keyword =" << g_keyword;

    NeteaseClient netease;
    KugouClient kugou;
    netease.setProxy(QString());
    kugou.setProxy(QString());

    if (g_dumpRaw) {
        QObject::connect(&kugou, &KugouClient::rawResponse, [](const QString &endpoint, const QByteArray &body) {
            qInfo().noquote() << "[raw-kugou]" << endpoint << body.left(2500);
        });
        QObject::connect(&netease, &NeteaseClient::rawResponse,
                         [](const QString &endpoint, int status, const QByteArray &body) {
                             qInfo().noquote() << "[raw-netease]" << endpoint << status << body.left(2500);
                         });
    }

    QTimer::singleShot(90000, &app, []() {
        qWarning() << "probe timeout";
        QCoreApplication::exit(3);
    });

    auto finish = [&app]() { QCoreApplication::quit(); };

    kugouChain(kugou, [&netease, finish]() {
        neteaseChain(netease, [finish]() { finish(); });
    });

    return app.exec();
}
