#include "ui/TrackListModel.h"

namespace mr {

TrackListModel::TrackListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TrackListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_tracks.size());
}

QVariant TrackListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return {};

    const Track &track = m_tracks.at(index.row());
    switch (role) {
    case TitleRole:
        return track.title;
    case ArtistsRole:
        return track.artists;
    case AlbumRole:
        return track.album;
    case SourceRole:
        return track.source;
    case SourceNameRole:
        return track.sourceName();
    case CoverRole:
        return track.coverUrl;
    case DurationRole:
        return track.durationText();
    case DurationMsRole:
        return QVariant::fromValue(track.durationMs);
    case SubtitleRole:
        return track.subtitle();
    case IdRole:
        return track.id;
    case ArtistIdRole:
        return track.artistId;
    case AlbumIdRole:
        return track.albumId;
    case VipRole:
        return track.vip;
    case FavoriteRole:
        return m_favorites.contains(trackKey(track));
    case PlayingRole:
        return !m_currentKey.isEmpty() && trackKey(track) == m_currentKey;
    case IndexRole:
        return index.row();
    case TrackRole:
        return track.toVariantMap();
    default:
        break;
    }
    return {};
}

QHash<int, QByteArray> TrackListModel::roleNames() const
{
    return {
        {TitleRole, "title"},         {ArtistsRole, "artists"},     {AlbumRole, "album"},
        {SourceRole, "source"},       {SourceNameRole, "sourceName"}, {CoverRole, "coverUrl"},
        {DurationRole, "durationText"}, {DurationMsRole, "durationMs"}, {SubtitleRole, "subtitle"},
        {IdRole, "trackId"},          {VipRole, "vip"},             {FavoriteRole, "favorite"},
        {ArtistIdRole, "artistId"},   {AlbumIdRole, "albumId"},
        {PlayingRole, "playing"},     {IndexRole, "index"},         {TrackRole, "track"},
    };
}

Track TrackListModel::trackAt(int row) const
{
    if (row < 0 || row >= m_tracks.size())
        return {};
    return m_tracks.at(row);
}

QVariantMap TrackListModel::at(int row) const
{
    return trackAt(row).toVariantMap();
}

QVariantList TrackListModel::toVariantList() const
{
    QVariantList list;
    list.reserve(m_tracks.size());
    for (const Track &track : m_tracks)
        list.append(track.toVariantMap());
    return list;
}

void TrackListModel::setTracks(const QList<Track> &tracks)
{
    beginResetModel();
    m_tracks = tracks;
    endResetModel();
    emit countChanged();
}

void TrackListModel::appendTracks(const QList<Track> &tracks)
{
    if (tracks.isEmpty())
        return;
    const int first = static_cast<int>(m_tracks.size());
    beginInsertRows(QModelIndex(), first, first + static_cast<int>(tracks.size()) - 1);
    m_tracks.append(tracks);
    endInsertRows();
    emit countChanged();
}

void TrackListModel::append(const Track &track)
{
    const int row = static_cast<int>(m_tracks.size());
    beginInsertRows(QModelIndex(), row, row);
    m_tracks.append(track);
    endInsertRows();
    emit countChanged();
}

void TrackListModel::insertAt(int row, const Track &track)
{
    row = qBound(0, row, static_cast<int>(m_tracks.size()));
    beginInsertRows(QModelIndex(), row, row);
    m_tracks.insert(row, track);
    endInsertRows();
    emit countChanged();
}

void TrackListModel::removeAt(int row)
{
    if (row < 0 || row >= m_tracks.size())
        return;
    beginRemoveRows(QModelIndex(), row, row);
    m_tracks.removeAt(row);
    endRemoveRows();
    emit countChanged();
}

void TrackListModel::clear()
{
    if (m_tracks.isEmpty())
        return;
    beginResetModel();
    m_tracks.clear();
    endResetModel();
    emit countChanged();
}

void TrackListModel::setFavoriteKeys(const QSet<QString> &keys)
{
    if (m_favorites == keys)
        return;
    m_favorites = keys;
    if (!m_tracks.isEmpty()) {
        emit dataChanged(index(0), index(static_cast<int>(m_tracks.size()) - 1), {FavoriteRole});
    }
}

void TrackListModel::setCurrentKey(const QString &key)
{
    if (m_currentKey == key)
        return;
    const QString previous = m_currentKey;
    m_currentKey = key;
    for (int row = 0; row < m_tracks.size(); ++row) {
        const QString rowKey = trackKey(m_tracks.at(row));
        if (rowKey == previous || rowKey == key)
            emit dataChanged(index(row), index(row), {PlayingRole});
    }
}

} // namespace mr
