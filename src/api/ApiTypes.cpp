#include "api/ApiTypes.h"

#include <QObject>
#include <QStringList>

#include "core/Settings.h"

#include <QJsonDocument>
#include <QStringList>

namespace mr {

QString Track::subtitle() const
{
    QStringList parts;
    parts << (artists.isEmpty() ? QStringLiteral("未知艺术家") : artists);
    if (!album.isEmpty())
        parts << album;
    parts << sourceName();
    return parts.join(QStringLiteral(" - "));
}

QString Track::durationText() const
{
    const qint64 seconds = durationMs / 1000;
    return QStringLiteral("%1:%2")
        .arg(seconds / 60)
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

QVariantMap Track::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("source")] = source;
    map[QStringLiteral("sourceName")] = sourceName();
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("hash")] = hash;
    map[QStringLiteral("albumAudioId")] = albumAudioId;
    map[QStringLiteral("albumId")] = albumId;
    map[QStringLiteral("artistId")] = artistId;
    map[QStringLiteral("title")] = title;
    map[QStringLiteral("artists")] = artists;
    map[QStringLiteral("album")] = album;
    map[QStringLiteral("coverUrl")] = coverUrl;
    map[QStringLiteral("durationMs")] = durationMs;
    map[QStringLiteral("durationText")] = durationText();
    map[QStringLiteral("lyricId")] = lyricId;
    map[QStringLiteral("lyricAccessKey")] = lyricAccessKey;
    map[QStringLiteral("fee")] = fee;
    map[QStringLiteral("vip")] = vip;
    map[QStringLiteral("subtitle")] = subtitle();
    map[QStringLiteral("extra")] = extra;
    return map;
}

Track Track::fromVariantMap(const QVariantMap &map)
{
    Track track;
    track.source = map.value(QStringLiteral("source")).toString();
    track.id = map.value(QStringLiteral("id")).toString();
    track.hash = map.value(QStringLiteral("hash")).toString();
    track.albumAudioId = map.value(QStringLiteral("albumAudioId")).toString();
    track.albumId = map.value(QStringLiteral("albumId")).toString();
    track.artistId = map.value(QStringLiteral("artistId")).toString();
    track.title = map.value(QStringLiteral("title")).toString();
    track.artists = map.value(QStringLiteral("artists")).toString();
    track.album = map.value(QStringLiteral("album")).toString();
    track.coverUrl = map.value(QStringLiteral("coverUrl")).toString();
    track.durationMs = map.value(QStringLiteral("durationMs")).toLongLong();
    track.lyricId = map.value(QStringLiteral("lyricId")).toString();
    track.lyricAccessKey = map.value(QStringLiteral("lyricAccessKey")).toString();
    track.fee = map.value(QStringLiteral("fee")).toInt();
    track.vip = map.value(QStringLiteral("vip")).toBool();
    track.extra = map.value(QStringLiteral("extra")).toString();
    return track;
}

QJsonObject Track::toJson() const
{
    QJsonObject object;
    object[QStringLiteral("source")] = source;
    object[QStringLiteral("id")] = id;
    object[QStringLiteral("hash")] = hash;
    object[QStringLiteral("albumAudioId")] = albumAudioId;
    object[QStringLiteral("albumId")] = albumId;
    object[QStringLiteral("artistId")] = artistId;
    object[QStringLiteral("title")] = title;
    object[QStringLiteral("artists")] = artists;
    object[QStringLiteral("album")] = album;
    object[QStringLiteral("coverUrl")] = coverUrl;
    object[QStringLiteral("durationMs")] = static_cast<double>(durationMs);
    object[QStringLiteral("lyricId")] = lyricId;
    object[QStringLiteral("lyricAccessKey")] = lyricAccessKey;
    object[QStringLiteral("fee")] = fee;
    object[QStringLiteral("vip")] = vip;
    object[QStringLiteral("extra")] = extra;
    return object;
}

Track Track::fromJson(const QJsonObject &object)
{
    Track track;
    track.source = object.value(QStringLiteral("source")).toString();
    track.id = object.value(QStringLiteral("id")).toString();
    track.hash = object.value(QStringLiteral("hash")).toString();
    track.albumAudioId = object.value(QStringLiteral("albumAudioId")).toString();
    track.albumId = object.value(QStringLiteral("albumId")).toString();
    track.artistId = object.value(QStringLiteral("artistId")).toString();
    track.title = object.value(QStringLiteral("title")).toString();
    track.artists = object.value(QStringLiteral("artists")).toString();
    track.album = object.value(QStringLiteral("album")).toString();
    track.coverUrl = object.value(QStringLiteral("coverUrl")).toString();
    track.durationMs = static_cast<qint64>(object.value(QStringLiteral("durationMs")).toDouble());
    track.lyricId = object.value(QStringLiteral("lyricId")).toString();
    track.lyricAccessKey = object.value(QStringLiteral("lyricAccessKey")).toString();
    track.fee = object.value(QStringLiteral("fee")).toInt();
    track.vip = object.value(QStringLiteral("vip")).toBool();
    track.extra = object.value(QStringLiteral("extra")).toString();
    return track;
}

QString neteaseLevelForQuality(Quality quality)
{
    switch (quality) {
    case Quality::Standard:
        return QStringLiteral("standard");
    case Quality::High:
        return QStringLiteral("exhigh");
    case Quality::Lossless:
        return QStringLiteral("lossless");
    case Quality::HiRes:
        return QStringLiteral("hires");
    }
    return QStringLiteral("exhigh");
}

QString kugouQualityForQuality(Quality quality)
{
    switch (quality) {
    case Quality::Standard:
        return QStringLiteral("128");
    case Quality::High:
        return QStringLiteral("320");
    case Quality::Lossless:
        return QStringLiteral("flac");
    case Quality::HiRes:
        return QStringLiteral("high");
    }
    return QStringLiteral("320");
}

QString Album::subtitle() const
{
    QStringList parts;
    if (!artist.isEmpty())
        parts << artist;
    if (!publishDate.isEmpty())
        parts << publishDate;
    if (songCount > 0)
        parts << QStringLiteral("%1 %2").arg(songCount).arg(QObject::tr("首"));
    if (parts.isEmpty())
        parts << sourceName();
    return parts.join(QStringLiteral(" · "));
}

QVariantMap Album::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("source")] = source;
    map[QStringLiteral("sourceName")] = sourceName();
    map[QStringLiteral("key")] = albumKey(*this);
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("name")] = name;
    map[QStringLiteral("artist")] = artist;
    map[QStringLiteral("artistId")] = artistId;
    map[QStringLiteral("coverUrl")] = coverUrl;
    map[QStringLiteral("publishDate")] = publishDate;
    map[QStringLiteral("company")] = company;
    map[QStringLiteral("description")] = description;
    map[QStringLiteral("songCount")] = songCount;
    map[QStringLiteral("subscribed")] = subscribed;
    map[QStringLiteral("subtitle")] = subtitle();
    return map;
}

QString Mv::subtitle() const
{
    QStringList parts;
    if (!artist.isEmpty())
        parts << artist;
    const QString time = durationText();
    if (!time.isEmpty())
        parts << time;
    parts << sourceName();
    return parts.join(QStringLiteral(" · "));
}

QString Mv::durationText() const
{
    if (durationMs <= 0)
        return QString();
    const qint64 totalSeconds = durationMs / 1000;
    return QStringLiteral("%1:%2")
        .arg(totalSeconds / 60)
        .arg(totalSeconds % 60, 2, 10, QLatin1Char('0'));
}

QVariantMap Mv::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("source")] = source;
    map[QStringLiteral("sourceName")] = sourceName();
    map[QStringLiteral("key")] = mvKey(*this);
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("name")] = name;
    map[QStringLiteral("artist")] = artist;
    map[QStringLiteral("artistId")] = artistId;
    map[QStringLiteral("coverUrl")] = coverUrl;
    map[QStringLiteral("durationMs")] = durationMs;
    map[QStringLiteral("durationText")] = durationText();
    map[QStringLiteral("videoHash")] = videoHash;
    map[QStringLiteral("playCount")] = playCount;
    map[QStringLiteral("publishTime")] = publishTime;
    map[QStringLiteral("subscribed")] = subscribed;
    map[QStringLiteral("subtitle")] = subtitle();
    return map;
}

QVariantMap Artist::toVariantMap() const
{
    QVariantMap map;
    map[QStringLiteral("source")] = source;
    map[QStringLiteral("sourceName")] = sourceName();
    map[QStringLiteral("key")] = artistKey(*this);
    map[QStringLiteral("id")] = id;
    map[QStringLiteral("name")] = name;
    map[QStringLiteral("avatarUrl")] = avatarUrl;
    map[QStringLiteral("description")] = description;
    map[QStringLiteral("songCount")] = songCount;
    map[QStringLiteral("albumCount")] = albumCount;
    map[QStringLiteral("mvCount")] = mvCount;
    map[QStringLiteral("fansCount")] = fansCount;
    map[QStringLiteral("followed")] = followed;
    return map;
}

} // namespace mr
