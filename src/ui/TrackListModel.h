#pragma once

#include "api/ApiTypes.h"

#include <QAbstractListModel>
#include <QList>
#include <QSet>

namespace mr {

/// Generic list model for tracks (search results, daily recommendations,
/// favourites, play queue, download list).
class TrackListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        TitleRole = Qt::UserRole + 1,
        ArtistsRole,
        AlbumRole,
        SourceRole,
        SourceNameRole,
        CoverRole,
        DurationRole,
        DurationMsRole,
        SubtitleRole,
        IdRole,
        ArtistIdRole,
        AlbumIdRole,
        VipRole,
        FavoriteRole,
        PlayingRole,
        IndexRole,
        TrackRole
    };
    Q_ENUM(Roles)

    explicit TrackListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return static_cast<int>(m_tracks.size()); }
    QList<Track> tracks() const { return m_tracks; }
    Track trackAt(int row) const;
    Q_INVOKABLE QVariantMap at(int row) const;
    Q_INVOKABLE QVariantList toVariantList() const;

    void setTracks(const QList<Track> &tracks);
    void appendTracks(const QList<Track> &tracks);
    void append(const Track &track);
    void insertAt(int row, const Track &track);
    void removeAt(int row);
    void clear();

    void setFavoriteKeys(const QSet<QString> &keys);
    void setCurrentKey(const QString &key);
    QString currentKey() const { return m_currentKey; }

signals:
    void countChanged();

private:
    QList<Track> m_tracks;
    QSet<QString> m_favorites;
    QString m_currentKey;
};

} // namespace mr
