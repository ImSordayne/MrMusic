// Live probe for the catalogue endpoints added in TARGET.md §10
// (album / MV search, album detail, artist pages) on both platforms.
//
//   mrmusic_catalogprobe [keyword]

#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/AppPaths.h"
#include "core/Logger.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTimer>

using namespace mr;

namespace {

QString g_keyword = QStringLiteral("周杰伦");
bool g_dumpRaw = false;

void dump(const QString &title, const QJsonObject &object, int maxLength = 900)
{
    const QByteArray json = QJsonDocument(object).toJson(QJsonDocument::Compact);
    qInfo().noquote() << QStringLiteral("── %1 ──").arg(title);
    qInfo().noquote() << (json.size() > maxLength ? json.left(maxLength) + "..." : json);
}

void fail(const QString &stage, const QString &error)
{
    qInfo().noquote() << QStringLiteral("[FAIL] %1: %2").arg(stage, error);
}

// ---------------------------------------------------------------------------
// NetEase
// ---------------------------------------------------------------------------
void neteaseChain(NeteaseClient &client, std::function<void()> done)
{
    client.searchAlbums(g_keyword, 5, 0, [&client, done](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            fail("netease searchAlbums", error);
            done();
            return;
        }
        const QJsonArray albums = body.value(QStringLiteral("albums")).toArray();
        dump("netease albums", body, 600);
        const QString albumId = albums.isEmpty()
            ? QString()
            : albums.first().toObject().value(QStringLiteral("id")).toString();

        client.searchMvs(g_keyword, 5, 0, [&client, done, albumId](bool okMv, const QJsonObject &mvBody,
                                                                   const QString &errorMv) {
            if (!okMv)
                fail("netease searchMvs", errorMv);
            else
                dump("netease mvs", mvBody, 600);

            const QJsonArray mvList = mvBody.value(QStringLiteral("mvs")).toArray();
            if (!mvList.isEmpty()) {
                const QString mvId = mvList.first().toObject().value(QStringLiteral("id")).toString();
                client.mvUrl(mvId, 1080, [](bool okUrl, const QJsonObject &urlBody, const QString &urlError) {
                    const QJsonObject data = urlBody.value(QStringLiteral("data")).toObject();
                    qInfo().noquote() << "netease mv url:" << (okUrl ? data.value(QStringLiteral("url")).toString().left(90) : urlError);
                });
            }

            client.albumDetail(albumId, [&client, done](bool okAlbum, const QJsonObject &albumBody,
                                                        const QString &errorAlbum) {
                if (!okAlbum) {
                    fail("netease albumDetail", errorAlbum);
                    done();
                    return;
                }
                const QJsonObject album = albumBody.value(QStringLiteral("album")).toObject();
                const QJsonArray tracks = albumBody.value(QStringLiteral("tracks")).toArray();
                qInfo().noquote() << "album:" << album.value(QStringLiteral("name")).toString()
                                  << "| artist:" << album.value(QStringLiteral("artist")).toString()
                                  << "| artistId:" << album.value(QStringLiteral("artistId")).toString()
                                  << "| cover:" << album.value(QStringLiteral("coverUrl")).toString().left(60)
                                  << "| tracks:" << tracks.size();

                const QString artistId = album.value(QStringLiteral("artistId")).toString();
                client.artistDetail(artistId, [&client, done](bool okArtist, const QJsonObject &artistBody,
                                                              const QString &errorArtist) {
                    if (!okArtist) {
                        fail("netease artistDetail", errorArtist);
                        done();
                        return;
                    }
                    const QJsonObject artist = artistBody.value(QStringLiteral("artist")).toObject();
                    dump("netease artist", artistBody, 500);
                    const QString id = artist.value(QStringLiteral("id")).toString();

                    client.artistSongs(id, 5, 0, true, [&client, done, id](bool okSongs, const QJsonObject &songsBody,
                                                                          const QString &errorSongs) {
                        if (!okSongs)
                            fail("netease artistSongs", errorSongs);
                        else
                            qInfo().noquote() << "netease artist songs:"
                                              << songsBody.value(QStringLiteral("tracks")).toArray().size();

                        client.artistAlbums(id, 5, 0, [&client, done, id](bool okAlbums, const QJsonObject &albumsBody,
                                                                         const QString &errorAlbums) {
                            if (!okAlbums)
                                fail("netease artistAlbums", errorAlbums);
                            else
                                qInfo().noquote() << "netease artist albums:"
                                                  << albumsBody.value(QStringLiteral("albums")).toArray().size();

                            client.artistMvs(id, 5, 0, [done](bool okMvs, const QJsonObject &mvsBody,
                                                              const QString &errorMvs) {
                                if (!okMvs)
                                    fail("netease artistMvs", errorMvs);
                                else
                                    qInfo().noquote() << "netease artist mvs:"
                                                      << mvsBody.value(QStringLiteral("mvs")).toArray().size();
                                done();
                            });
                        });
                    });
                });
            });
        });
    });
}
// ---------------------------------------------------------------------------
// KuGou
// ---------------------------------------------------------------------------
void kugouChain(KugouClient &client, std::function<void()> done)
{
    client.registerDevice([&client, done](bool) {
        client.searchAlbums(g_keyword, 1, 5, [&client, done](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                fail("kugou searchAlbums", error);
                done();
                return;
            }
            const QJsonArray albums = body.value(QStringLiteral("albums")).toArray();
            dump("kugou albums", body, 700);
            if (g_dumpRaw)
                dump("kugou albums raw", body, 2500);
            const QString albumId = albums.isEmpty()
                ? QString()
                : albums.first().toObject().value(QStringLiteral("id")).toString();

            client.searchMvs(g_keyword, 1, 5, [&client, done, albumId](bool ok2, const QJsonObject &mvBody,
                                                                       const QString &error2) {
                if (!ok2) {
                    fail("kugou searchMvs", error2);
                } else {
                    dump("kugou mvs", mvBody, 700);
                    if (g_dumpRaw)
                        dump("kugou mvs raw", mvBody, 2500);
                }

                const QJsonArray mvList = mvBody.value(QStringLiteral("mvs")).toArray();
                if (!mvList.isEmpty()) {
                    const QJsonObject mv = mvList.first().toObject();
                    const QString hash = mv.value(QStringLiteral("videoHash")).toString();
                    client.videoUrl(hash, [](bool okUrl, const QJsonObject &urlBody, const QString &urlError) {
                        qInfo().noquote() << "kugou mv url:" << (okUrl ? urlBody.value(QStringLiteral("url")).toString().left(90) : urlError);
                    });
                }

                client.albumDetail(albumId, [&client, done](bool ok3, const QJsonObject &albumBody,
                                                            const QString &error3) {
                    if (!ok3) {
                        fail("kugou albumDetail", error3);
                        done();
                        return;
                    }
                    const QJsonObject album = albumBody.value(QStringLiteral("album")).toObject();
                    dump("kugou album", albumBody, 700);

                    const QString artistId = album.value(QStringLiteral("artistId")).toString();
                    client.albumSongs(album.value(QStringLiteral("id")).toString(), 1, 5,
                                      [&client, done, artistId](bool ok4, const QJsonObject &songsBody,
                                                                const QString &error4) {
                        if (!ok4) {
                            fail("kugou albumSongs", error4);
                            done();
                            return;
                        }
                        qInfo().noquote() << "kugou album songs:" << songsBody.value(QStringLiteral("tracks")).toArray().size();
                        if (g_dumpRaw)
                            dump("kugou albumSongs raw", songsBody, 2500);

                        client.artistDetail(artistId, [&client, done](bool ok5, const QJsonObject &artistBody,
                                                                      const QString &error5) {
                            if (!ok5) {
                                fail("kugou artistDetail", error5);
                                done();
                                return;
                            }
                            dump("kugou artist", artistBody, 700);
                            const QJsonObject artist = artistBody.value(QStringLiteral("artist")).toObject();
                            const QString id = artist.value(QStringLiteral("id")).toString();

                            client.artistSongs(id, 1, 5, true,
                                               [&client, done, id](bool ok6, const QJsonObject &songs,
                                                                   const QString &error6) {
                                if (!ok6) {
                                    fail("kugou artistSongs", error6);
                                } else {
                                    qInfo().noquote() << "kugou artist songs:"
                                                      << songs.value(QStringLiteral("tracks")).toArray().size();
                                }
                                client.artistAlbums(id, 1, 5, true,
                                                    [&client, done, id](bool ok7, const QJsonObject &albumsBody,
                                                                        const QString &error7) {
                                    if (!ok7) {
                                        fail("kugou artistAlbums", error7);
                                    } else {
                                        qInfo().noquote() << "kugou artist albums:"
                                                          << albumsBody.value(QStringLiteral("albums")).toArray().size();
                                    }
                                    client.artistMvs(id, 1, 5,
                                                     [done](bool ok8, const QJsonObject &mvBody,
                                                            const QString &error8) {
                                        if (!ok8)
                                            fail("kugou artistMvs", error8);
                                        else
                                            qInfo().noquote() << "kugou artist mvs:"
                                                              << mvBody.value(QStringLiteral("mvs")).toArray().size();
                                        done();
                                    });
                                });
                            });
                        });
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
    AppPaths::initialize();
    Logger::initialize();
    Logger::setDebugEnabled(qEnvironmentVariableIsSet("MRMUSIC_DEBUG"));

    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (args.at(i) == QLatin1String("--raw"))
            g_dumpRaw = true;
        else if (!args.at(i).startsWith(QLatin1String("--")))
            g_keyword = args.at(i);
    }
    qInfo().noquote() << "keyword:" << g_keyword;

    NeteaseClient netease;
    KugouClient kugou;

    QObject::connect(&kugou, &KugouClient::rawResponse,
                     [](const QString &endpoint, const QByteArray &payload) {
                         if (!g_dumpRaw)
                             return;
                         if (!endpoint.contains(QLatin1String("interface/index"))
                             && !endpoint.contains(QLatin1String("search/album"))
                             && !endpoint.contains(QLatin1String("search/mv"))
                             && !endpoint.contains(QLatin1String("album_audio"))
                             && !endpoint.contains(QLatin1String("author")))
                             return;
                         const QJsonObject raw = QJsonDocument::fromJson(payload).object();
                         const QJsonObject data = raw.value(QStringLiteral("data")).toObject();
                         QJsonArray items = data.value(QStringLiteral("lists")).toArray();
                         if (items.isEmpty())
                             items = data.value(QStringLiteral("songs")).toArray();
                         if (items.isEmpty())
                             items = data.value(QStringLiteral("info")).toArray();
                         if (items.isEmpty())
                             items = data.value(QStringLiteral("videos")).toArray();
                         qInfo().noquote() << QStringLiteral("── RAW %1 (first item) ──").arg(endpoint);
                         if (!items.isEmpty())
                             qInfo().noquote() << QString::fromUtf8(QJsonDocument(items.first().toObject()).toJson(QJsonDocument::Compact));
                         else
                             qInfo().noquote() << QString::fromUtf8(payload).left(1400);
                     });

    int pending = 2;
    auto done = [&pending, &app]() {
        if (--pending == 0)
            app.quit();
    };

    neteaseChain(netease, done);
    kugouChain(kugou, done);

    QTimer::singleShot(60000, &app, &QCoreApplication::quit);
    return app.exec();
}
