#include "ui/BrowseController.h"

#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/Logger.h"
#include "ui/CatalogModel.h"
#include "ui/TrackListModel.h"

namespace mr {

namespace {

QList<Track> tracksFromJson(const QJsonArray &array)
{
    QList<Track> tracks;
    tracks.reserve(array.size());
    for (const QJsonValue &value : array)
        tracks.append(Track::fromJson(value.toObject()));
    return tracks;
}

} // namespace

BrowseController::BrowseController(NeteaseClient *netease, KugouClient *kugou, QObject *parent)
    : QObject(parent)
    , m_netease(netease)
    , m_kugou(kugou)
    , m_artistTracks(new TrackListModel(this))
    , m_albumTracks(new TrackListModel(this))
    , m_artistAlbums(new CatalogModel(this))
    , m_artistMvs(new CatalogModel(this))
{
}

void BrowseController::setView(const QString &view)
{
    if (m_view == view)
        return;
    m_view = view;
    emit viewChanged();
}

void BrowseController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void BrowseController::setMessage(const QString &message)
{
    if (m_message == message)
        return;
    m_message = message;
    emit messageChanged();
}

void BrowseController::setArtist(const QVariantMap &artist)
{
    m_artist = artist;
    emit artistChanged();
}

void BrowseController::setAlbum(const QVariantMap &album)
{
    m_album = album;
    emit albumChanged();
}

void BrowseController::taskStarted()
{
    ++m_pending;
    setLoading(true);
}

void BrowseController::taskFinished()
{
    if (m_pending > 0)
        --m_pending;
    if (m_pending == 0)
        setLoading(false);
}

void BrowseController::clearMessage()
{
    setMessage(QString());
}

void BrowseController::close()
{
    m_pending = 0;
    setLoading(false);
    setView(QString());
    setArtist({});
    setAlbum({});
    m_artistTracks->clear();
    m_albumTracks->clear();
    m_artistAlbums->clear();
    m_artistMvs->clear();
    setMessage(QString());
}

void BrowseController::artistRequestFailed(const QString &error)
{
    Logger::warn(QStringLiteral("artist page failed: %1").arg(error));
    setMessage(tr("歌手信息加载失败：%1").arg(error));
    taskFinished();
}

void BrowseController::albumRequestFailed(const QString &error)
{
    Logger::warn(QStringLiteral("album page failed: %1").arg(error));
    setMessage(tr("专辑信息加载失败：%1").arg(error));
    taskFinished();
}

void BrowseController::openArtist(const QString &source, const QString &id)
{
    if (id.isEmpty())
        return;

    m_artistSource = source;
    m_artistId = id;
    m_artistTracks->clear();
    m_artistAlbums->clear();
    m_artistMvs->clear();
    setMessage(QString());
    setArtist({});
    setView(QStringLiteral("artist"));

    // three independent lists: detail + songs + albums + mvs
    m_pending = 4;
    setLoading(true);

    const Source platform = sourceFromString(source);
    if (platform == Source::Netease) {
        m_netease->artistDetail(id, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                artistRequestFailed(error);
                return;
            }
            setArtist(body.value(QStringLiteral("artist")).toObject().toVariantMap());
            taskFinished();
        });
        m_netease->artistSongs(id, 50, 0, true, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok)
                Logger::warn(QStringLiteral("artist songs failed: %1").arg(error));
            else
                m_artistTracks->setTracks(tracksFromJson(body.value(QStringLiteral("tracks")).toArray()));
            taskFinished();
        });
        m_netease->artistAlbums(id, 30, 0, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok)
                Logger::warn(QStringLiteral("artist albums failed: %1").arg(error));
            else
                m_artistAlbums->setItems(body.value(QStringLiteral("albums")).toArray());
            taskFinished();
        });
        m_netease->artistMvs(id, 30, 0, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok)
                Logger::warn(QStringLiteral("artist mvs failed: %1").arg(error));
            else
                m_artistMvs->setItems(body.value(QStringLiteral("mvs")).toArray());
            taskFinished();
        });
        return;
    }

    if (platform == Source::Kugou) {
        m_kugou->artistDetail(id, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                artistRequestFailed(error);
                return;
            }
            setArtist(body.value(QStringLiteral("artist")).toObject().toVariantMap());
            taskFinished();
        });
        m_kugou->artistSongs(id, 1, 50, true, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok)
                Logger::warn(QStringLiteral("kugou artist songs failed: %1").arg(error));
            else
                m_artistTracks->setTracks(tracksFromJson(body.value(QStringLiteral("tracks")).toArray()));
            taskFinished();
        });
        m_kugou->artistAlbums(id, 1, 30, true, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok)
                Logger::warn(QStringLiteral("kugou artist albums failed: %1").arg(error));
            else
                m_artistAlbums->setItems(body.value(QStringLiteral("albums")).toArray());
            taskFinished();
        });
        m_kugou->artistMvs(id, 1, 30, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok)
                Logger::warn(QStringLiteral("kugou artist mvs failed: %1").arg(error));
            else
                m_artistMvs->setItems(body.value(QStringLiteral("mvs")).toArray());
            taskFinished();
        });
        return;
    }

    m_pending = 0;
    setLoading(false);
    setMessage(tr("未知的平台来源"));
}

void BrowseController::openAlbum(const QString &source, const QString &id)
{
    if (id.isEmpty())
        return;

    m_albumSource = source;
    m_albumId = id;
    m_albumTracks->clear();
    setMessage(QString());
    setAlbum({});
    setView(QStringLiteral("album"));

    m_pending = 2;
    setLoading(true);

    const Source platform = sourceFromString(source);
    if (platform == Source::Netease) {
        m_netease->albumDetail(id, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                albumRequestFailed(error);
                return;
            }
            setAlbum(body.value(QStringLiteral("album")).toObject().toVariantMap());
            m_albumTracks->setTracks(tracksFromJson(body.value(QStringLiteral("tracks")).toArray()));
            taskFinished();
            taskFinished();
        });
        return;
    }

    if (platform == Source::Kugou) {
        m_kugou->albumDetail(id, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                albumRequestFailed(error);
                return;
            }
            setAlbum(body.value(QStringLiteral("album")).toObject().toVariantMap());
            taskFinished();
        });
        m_kugou->albumSongs(id, 1, 50, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok)
                Logger::warn(QStringLiteral("kugou album songs failed: %1").arg(error));
            else
                m_albumTracks->setTracks(tracksFromJson(body.value(QStringLiteral("tracks")).toArray()));
            taskFinished();
        });
        return;
    }

    m_pending = 0;
    setLoading(false);
    setMessage(tr("未知的平台来源"));
}

void BrowseController::toggleFollowArtist()
{
    const QString id = m_artist.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;

    const bool followed = m_artist.value(QStringLiteral("followed")).toBool();
    const Source platform = sourceFromString(m_artistSource);

    auto apply = [this, followed](bool ok, const QJsonObject &, const QString &error) {
        if (!ok) {
            setMessage(tr("关注操作失败：%1").arg(error));
            return;
        }
        QVariantMap artist = m_artist;
        artist[QStringLiteral("followed")] = !followed;
        setArtist(artist);
        setMessage(followed ? tr("已取消关注") : tr("已关注歌手"));
    };

    if (platform == Source::Netease) {
        if (!m_netease->loggedIn()) {
            setMessage(tr("请先登录网易云账号后再关注歌手"));
            return;
        }
        m_netease->artistSubscribe(id, !followed, apply);
        return;
    }
    if (platform == Source::Kugou) {
        if (!m_kugou->loggedIn()) {
            setMessage(tr("请先登录酷狗账号后再关注歌手"));
            return;
        }
        // KuGou's follow endpoint needs the signed request parameters that the
        // client already adds; it is surfaced as unsupported until verified.
        setMessage(tr("酷狗关注歌手接口尚未验证，请稍后再试"));
        return;
    }
}

void BrowseController::toggleSubscribeAlbum()
{
    const QString id = m_album.value(QStringLiteral("id")).toString();
    if (id.isEmpty())
        return;

    const bool subscribed = m_album.value(QStringLiteral("subscribed")).toBool();
    const Source platform = sourceFromString(m_albumSource);
    if (platform != Source::Netease) {
        setMessage(tr("酷狗收藏专辑接口尚未验证，请稍后再试"));
        return;
    }
    if (!m_netease->loggedIn()) {
        setMessage(tr("请先登录网易云账号后再收藏专辑"));
        return;
    }

    m_netease->albumSubscribe(id, !subscribed,
                              [this, subscribed](bool ok, const QJsonObject &, const QString &error) {
                                  if (!ok) {
                                      setMessage(tr("收藏操作失败：%1").arg(error));
                                      return;
                                  }
                                  QVariantMap album = m_album;
                                  album[QStringLiteral("subscribed")] = !subscribed;
                                  setAlbum(album);
                                  setMessage(subscribed ? tr("已取消收藏") : tr("已收藏专辑"));
                              });
}

} // namespace mr
