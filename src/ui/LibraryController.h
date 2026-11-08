#pragma once

#include "api/ApiTypes.h"
#include "ui/TrackListModel.h"

#include <QObject>
#include <QSet>
#include <QStringList>

namespace mr {

class CatalogModel;
class KugouClient;
class NeteaseClient;
class TrackListModel;

/// Favourites and play history.
///
/// Tracks are kept in a local JSON file (offline fallback + history), while the
/// account favourite lists (songs / albums / MVs) are read from the platform
/// API when the user is signed in (TARGET.md §10.21).
class LibraryController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(mr::TrackListModel *favorites READ favorites CONSTANT)
    Q_PROPERTY(mr::TrackListModel *history READ history CONSTANT)
    Q_PROPERTY(mr::CatalogModel *favoriteAlbums READ favoriteAlbums CONSTANT)
    Q_PROPERTY(mr::CatalogModel *favoriteMvs READ favoriteMvs CONSTANT)
    Q_PROPERTY(int favoriteCount READ favoriteCount NOTIFY favoritesChanged)
    Q_PROPERTY(bool remoteLoading READ remoteLoading NOTIFY remoteLoadingChanged)
    Q_PROPERTY(QString remoteMessage READ remoteMessage NOTIFY remoteMessageChanged)

public:
    explicit LibraryController(NeteaseClient *netease = nullptr, KugouClient *kugou = nullptr,
                               QObject *parent = nullptr);

    TrackListModel *favorites() const { return m_favorites; }
    TrackListModel *history() const { return m_history; }
    CatalogModel *favoriteAlbums() const { return m_favoriteAlbums; }
    CatalogModel *favoriteMvs() const { return m_favoriteMvs; }
    int favoriteCount() const { return m_favorites->count(); }
    bool remoteLoading() const { return m_remoteLoading; }
    QString remoteMessage() const { return m_remoteMessage; }

    void load();
    void save() const;

    QSet<QString> favoriteKeys() const;
    Q_INVOKABLE QStringList favoriteKeyList() const;
    QSet<QString> historyKeys() const;

public slots:
    bool isFavorite(const QVariantMap &track) const;
    void toggleFavorite(const QVariantMap &track);
    void addHistory(const QVariantMap &track);
    void clearHistory();

    /// Reloads the account favourites (songs / albums / MVs) from the API.
    Q_INVOKABLE void refreshRemote();
    Q_INVOKABLE void removeFavoriteAlbum(const QString &id);
    Q_INVOKABLE void removeFavoriteMv(const QString &id);

signals:
    void favoritesChanged();
    void historyChanged();
    void remoteLoadingChanged();
    void remoteMessageChanged();

private:
    void setRemoteLoading(bool loading);
    void setRemoteMessage(const QString &message);
    void applyLocalFavorite(const Track &track, bool favorite);
    void mergeRemoteTracks(const QList<Track> &remote);
    void remoteTaskFinished();

    NeteaseClient *m_netease = nullptr;
    KugouClient *m_kugou = nullptr;

    TrackListModel *m_favorites = nullptr;
    TrackListModel *m_history = nullptr;
    CatalogModel *m_favoriteAlbums = nullptr;
    CatalogModel *m_favoriteMvs = nullptr;

    bool m_remoteLoading = false;
    int m_remotePending = 0;
    QString m_remoteMessage;
};

} // namespace mr
