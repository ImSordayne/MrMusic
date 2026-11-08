#pragma once

#include "api/ApiTypes.h"
#include "ui/CatalogModel.h"
#include "ui/TrackListModel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QVariantMap>

namespace mr {

class KugouClient;
class NeteaseClient;

/// Backing store for the artist / album detail pages (TARGET.md §10.20).
///
/// `view` is empty when no detail page is open, otherwise "artist" or "album".
/// The artist page loads tracks, albums and MVs in parallel; the album page
/// loads the album header plus its track list.
class BrowseController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString view READ view NOTIFY viewChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(QString message READ message NOTIFY messageChanged)
    Q_PROPERTY(QVariantMap artist READ artist NOTIFY artistChanged)
    Q_PROPERTY(QVariantMap album READ album NOTIFY albumChanged)
    Q_PROPERTY(mr::TrackListModel *artistTracks READ artistTracks CONSTANT)
    Q_PROPERTY(mr::TrackListModel *albumTracks READ albumTracks CONSTANT)
    Q_PROPERTY(mr::CatalogModel *artistAlbums READ artistAlbums CONSTANT)
    Q_PROPERTY(mr::CatalogModel *artistMvs READ artistMvs CONSTANT)

public:
    BrowseController(NeteaseClient *netease, KugouClient *kugou, QObject *parent = nullptr);

    QString view() const { return m_view; }
    bool loading() const { return m_loading; }
    QString message() const { return m_message; }
    QVariantMap artist() const { return m_artist; }
    QVariantMap album() const { return m_album; }
    TrackListModel *artistTracks() const { return m_artistTracks; }
    TrackListModel *albumTracks() const { return m_albumTracks; }
    CatalogModel *artistAlbums() const { return m_artistAlbums; }
    CatalogModel *artistMvs() const { return m_artistMvs; }

    /// `source` is "netease" or "kugou"; `id` is the platform artist / album id.
    Q_INVOKABLE void openArtist(const QString &source, const QString &id);
    Q_INVOKABLE void openAlbum(const QString &source, const QString &id);
    Q_INVOKABLE void close();

    /// Follow / unfollow the open artist (needs a signed in account).
    Q_INVOKABLE void toggleFollowArtist();
    /// Subscribe / unsubscribe the open album (needs a signed in account).
    Q_INVOKABLE void toggleSubscribeAlbum();

    Q_INVOKABLE void clearMessage();

signals:
    void viewChanged();
    void loadingChanged();
    void messageChanged();
    void artistChanged();
    void albumChanged();

private:
    void setView(const QString &view);
    void setLoading(bool loading);
    void setMessage(const QString &message);
    void setArtist(const QVariantMap &artist);
    void setAlbum(const QVariantMap &album);
    void taskStarted();
    void taskFinished();
    void artistRequestFailed(const QString &error);
    void albumRequestFailed(const QString &error);

    NeteaseClient *m_netease = nullptr;
    KugouClient *m_kugou = nullptr;

    QString m_view;
    bool m_loading = false;
    QString m_message;
    QVariantMap m_artist;
    QVariantMap m_album;
    QString m_artistSource;
    QString m_artistId;
    QString m_albumSource;
    QString m_albumId;
    int m_pending = 0;

    TrackListModel *m_artistTracks = nullptr;
    TrackListModel *m_albumTracks = nullptr;
    CatalogModel *m_artistAlbums = nullptr;
    CatalogModel *m_artistMvs = nullptr;
};

} // namespace mr
