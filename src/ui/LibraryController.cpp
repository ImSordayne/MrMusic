#include "ui/LibraryController.h"

#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/AppPaths.h"
#include "core/Logger.h"
#include "ui/CatalogModel.h"
#include "ui/TrackListModel.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QVector>

namespace mr {
namespace {
constexpr int kMaxHistory = 200;
}

LibraryController::LibraryController(NeteaseClient *netease, KugouClient *kugou, QObject *parent)
    : QObject(parent)
    , m_netease(netease)
    , m_kugou(kugou)
    , m_favorites(new TrackListModel(this))
    , m_history(new TrackListModel(this))
    , m_favoriteAlbums(new CatalogModel(this))
    , m_favoriteMvs(new CatalogModel(this))
{
    connect(m_favorites, &TrackListModel::countChanged, this, [this]() {
        emit favoritesChanged();
        save();
    });
    connect(m_history, &TrackListModel::countChanged, this, [this]() {
        emit historyChanged();
        save();
    });
}

void LibraryController::load()
{
    QFile file(AppPaths::libraryFile());
    if (!file.open(QIODevice::ReadOnly))
        return;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    file.close();

    QList<Track> favorites;
    for (const QJsonValue &value : root.value(QStringLiteral("favorites")).toArray()) {
        const Track track = Track::fromJson(value.toObject());
        if (track.isValid())
            favorites.append(track);
    }

    QList<Track> history;
    for (const QJsonValue &value : root.value(QStringLiteral("history")).toArray()) {
        const Track track = Track::fromJson(value.toObject());
        if (track.isValid())
            history.append(track);
    }

    m_favorites->setTracks(favorites);
    m_history->setTracks(history);
    save();
}

void LibraryController::save() const
{
    QJsonArray favorites;
    for (const Track &track : m_favorites->tracks())
        favorites.append(track.toJson());

    QJsonArray history;
    for (const Track &track : m_history->tracks())
        history.append(track.toJson());

    QJsonObject root;
    root[QStringLiteral("favorites")] = favorites;
    root[QStringLiteral("history")] = history;

    QSaveFile file(AppPaths::libraryFile());
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::warn(QStringLiteral("cannot write library: %1").arg(file.errorString()));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (!file.commit())
        Logger::warn(QStringLiteral("cannot commit library: %1").arg(file.errorString()));
}

QSet<QString> LibraryController::favoriteKeys() const
{
    QSet<QString> keys;
    for (const Track &track : m_favorites->tracks())
        keys.insert(trackKey(track));
    return keys;
}

QStringList LibraryController::favoriteKeyList() const
{
    QStringList keys;
    for (const Track &track : m_favorites->tracks())
        keys.append(trackKey(track));
    return keys;
}

QSet<QString> LibraryController::historyKeys() const
{
    QSet<QString> keys;
    for (const Track &track : m_history->tracks())
        keys.insert(trackKey(track));
    return keys;
}

bool LibraryController::isFavorite(const QVariantMap &track) const
{
    const QString key = trackKey(Track::fromVariantMap(track));
    for (const Track &entry : m_favorites->tracks()) {
        if (trackKey(entry) == key)
            return true;
    }
    return false;
}

void LibraryController::toggleFavorite(const QVariantMap &track)
{
    const Track t = Track::fromVariantMap(track);
    if (!t.isValid())
        return;

    const bool favorite = !isFavorite(track);
    applyLocalFavorite(t, favorite);

    // Mirror the change into the account favourite list (TARGET.md §10.21).
    if (t.sourceEnum() == Source::Netease && m_netease) {
        if (!m_netease->loggedIn()) {
            setRemoteMessage(tr("未登录网易云：收藏仅保存在本机"));
            return;
        }
        m_netease->likeSong(t.id, favorite, [this](bool ok, const QJsonObject &, const QString &error) {
            if (!ok)
                setRemoteMessage(tr("同步到账号收藏失败：%1").arg(error));
            else
                setRemoteMessage(QString());
        });
        return;
    }
    if (t.sourceEnum() == Source::Kugou)
        setRemoteMessage(tr("酷狗收藏歌曲接口尚未接入，当前仅保存在本机"));
}

void LibraryController::applyLocalFavorite(const Track &track, bool favorite)
{
    const QString key = trackKey(track);
    const QList<Track> current = m_favorites->tracks();
    for (int row = 0; row < current.size(); ++row) {
        if (trackKey(current.at(row)) == key) {
            if (!favorite)
                m_favorites->removeAt(row);
            return;
        }
    }
    if (favorite)
        m_favorites->append(track);
}

void LibraryController::setRemoteLoading(bool loading)
{
    if (m_remoteLoading == loading)
        return;
    m_remoteLoading = loading;
    emit remoteLoadingChanged();
}

void LibraryController::setRemoteMessage(const QString &message)
{
    if (m_remoteMessage == message)
        return;
    m_remoteMessage = message;
    emit remoteMessageChanged();
}

void LibraryController::remoteTaskFinished()
{
    if (m_remotePending > 0)
        --m_remotePending;
    if (m_remotePending == 0)
        setRemoteLoading(false);
}

void LibraryController::mergeRemoteTracks(const QList<Track> &remote)
{
    // Account favourites come first, then anything that only exists locally
    // (offline favourites made while signed out).
    QList<Track> merged = remote;
    QSet<QString> keys;
    for (const Track &track : remote)
        keys.insert(trackKey(track));
    for (const Track &track : m_favorites->tracks()) {
        if (!keys.contains(trackKey(track)))
            merged.append(track);
    }
    m_favorites->setTracks(merged);
}

void LibraryController::refreshRemote()
{
    if (!m_netease) {
        setRemoteMessage(tr("收藏接口不可用"));
        return;
    }
    if (!m_netease->loggedIn()) {
        m_favoriteAlbums->clear();
        m_favoriteMvs->clear();
        setRemoteMessage(tr("登录网易云后可查看账号收藏的专辑与 MV"));
        return;
    }

    setRemoteMessage(QString());
    m_remotePending = 3;
    setRemoteLoading(true);

    m_netease->albumSubList(50, 0, [this](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok)
            setRemoteMessage(tr("收藏专辑加载失败：%1").arg(error));
        else
            m_favoriteAlbums->setItems(body.value(QStringLiteral("albums")).toArray());
        remoteTaskFinished();
    });

    m_netease->mvSubList(50, 0, [this](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok)
            setRemoteMessage(tr("收藏 MV 加载失败：%1").arg(error));
        else
            m_favoriteMvs->setItems(body.value(QStringLiteral("mvs")).toArray());
        remoteTaskFinished();
    });

    m_netease->likedSongs(m_netease->userId(), [this](bool ok, const QJsonObject &body, const QString &error) {
        if (!ok) {
            setRemoteMessage(tr("收藏歌曲加载失败：%1").arg(error));
            remoteTaskFinished();
            return;
        }
        QList<QString> ids;
        for (const QJsonValue &value : body.value(QStringLiteral("ids")).toArray())
            ids.append(QString::number(static_cast<qint64>(value.toDouble())));
        if (ids.isEmpty()) {
            remoteTaskFinished();
            return;
        }
        // The like list only carries ids, so the details are fetched in pages of 200.
        for (int offset = 0; offset < ids.size() && offset < 400; offset += 200) {
            const QList<QString> slice = ids.mid(offset, 200);
            m_netease->songDetail(slice, [this](bool detailOk, const QJsonObject &detailBody,
                                                const QString &detailError) {
                if (!detailOk) {
                    setRemoteMessage(tr("收藏歌曲详情加载失败：%1").arg(detailError));
                    remoteTaskFinished();
                    return;
                }
                QList<Track> tracks;
                for (const QJsonValue &value : detailBody.value(QStringLiteral("tracks")).toArray())
                    tracks.append(Track::fromJson(value.toObject()));
                mergeRemoteTracks(tracks);
                remoteTaskFinished();
            });
        }
    });
}

void LibraryController::removeFavoriteAlbum(const QString &id)
{
    if (!m_netease || id.isEmpty())
        return;
    if (!m_netease->loggedIn()) {
        setRemoteMessage(tr("请先登录网易云账号"));
        return;
    }
    m_netease->albumSubscribe(id, false, [this, id](bool ok, const QJsonObject &, const QString &error) {
        if (!ok) {
            setRemoteMessage(tr("取消收藏失败：%1").arg(error));
            return;
        }
        QVector<QVariantMap> rows = m_favoriteAlbums->rows();
        for (int row = 0; row < rows.size(); ++row) {
            if (rows.at(row).value(QStringLiteral("id")).toString() == id) {
                rows.removeAt(row);
                break;
            }
        }
        m_favoriteAlbums->setItems(rows);
        setRemoteMessage(tr("已取消收藏"));
    });
}

void LibraryController::removeFavoriteMv(const QString &id)
{
    if (!m_netease || id.isEmpty())
        return;
    if (!m_netease->loggedIn()) {
        setRemoteMessage(tr("请先登录网易云账号"));
        return;
    }
    m_netease->mvSubscribe(id, false, [this, id](bool ok, const QJsonObject &, const QString &error) {
        if (!ok) {
            setRemoteMessage(tr("取消收藏失败：%1").arg(error));
            return;
        }
        QVector<QVariantMap> rows = m_favoriteMvs->rows();
        for (int row = 0; row < rows.size(); ++row) {
            if (rows.at(row).value(QStringLiteral("id")).toString() == id) {
                rows.removeAt(row);
                break;
            }
        }
        m_favoriteMvs->setItems(rows);
        setRemoteMessage(tr("已取消收藏"));
    });
}

void LibraryController::addHistory(const QVariantMap &track)
{
    const Track t = Track::fromVariantMap(track);
    if (!t.isValid())
        return;

    const QString key = trackKey(t);
    const QList<Track> current = m_history->tracks();

    QList<Track> updated;
    updated.append(t);
    for (const Track &entry : current) {
        if (trackKey(entry) == key)
            continue;
        updated.append(entry);
        if (updated.size() >= kMaxHistory)
            break;
    }
    m_history->setTracks(updated);
}

void LibraryController::clearHistory()
{
    m_history->clear();
}

} // namespace mr
