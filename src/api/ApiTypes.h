#pragma once

#include <QJsonObject>
#include <QString>
#include <QVariantMap>

#include <functional>

namespace mr {

enum class Source { Netease, Kugou, Unknown };

inline QString sourceToString(Source source)
{
    switch (source) {
    case Source::Netease:
        return QStringLiteral("netease");
    case Source::Kugou:
        return QStringLiteral("kugou");
    default:
        break;
    }
    return QStringLiteral("unknown");
}

inline Source sourceFromString(const QString &value)
{
    if (value == QLatin1String("netease"))
        return Source::Netease;
    if (value == QLatin1String("kugou"))
        return Source::Kugou;
    return Source::Unknown;
}

/// Human readable platform name shown in the UI ("来源").
inline QString sourceDisplayName(Source source)
{
    switch (source) {
    case Source::Netease:
        return QStringLiteral("网易云");
    case Source::Kugou:
        return QStringLiteral("酷狗");
    default:
        break;
    }
    return QStringLiteral("未知");
}

/// A song, normalised across both platforms.
struct Track
{
    QString source;          // "netease" / "kugou"
    QString id;              // platform track id (netease id, kugou file hash)
    QString hash;            // KuGou file hash (empty for netease)
    QString albumAudioId;    // KuGou album audio id (needed for /v5/url)
    QString albumId;         // album id (both platforms)
    QString artistId;        // primary artist id, used by the artist page
    QString title;
    QString artists;
    QString album;
    QString coverUrl;
    qint64 durationMs = 0;
    QString lyricId;         // KuGou lyric candidate id
    QString lyricAccessKey;  // KuGou lyric candidate access key
    int fee = 0;             // raw platform fee flag
    bool vip = false;        // true when the platform flagged the track as VIP
    QString extra;           // free form payload (e.g. netease privilege json)

    bool isValid() const { return !id.isEmpty() && !title.isEmpty(); }
    Source sourceEnum() const { return sourceFromString(source); }
    QString sourceName() const { return sourceDisplayName(sourceEnum()); }
    /// "作者 - 专辑 - 来源" as required by UiDesign/ListItem.png
    QString subtitle() const;
    /// "m:ss"
    QString durationText() const;

    QVariantMap toVariantMap() const;
    static Track fromVariantMap(const QVariantMap &map);

    QJsonObject toJson() const;
    static Track fromJson(const QJsonObject &object);
};

/// Stable identifier used by the library / favourites / queue.
inline QString trackKey(const Track &track)
{
    return track.source + QLatin1Char(':') + track.id;
}

/// Search result category (TARGET.md §10.19).
enum class SearchKind { Songs = 0, Albums = 1, Mvs = 2 };

/// An album, normalised across both platforms.
struct Album
{
    QString source;
    QString id;
    QString name;
    QString artist;
    QString artistId;
    QString coverUrl;
    QString publishDate;
    QString company;
    QString description;
    int songCount = 0;
    bool subscribed = false;

    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
    Source sourceEnum() const { return sourceFromString(source); }
    QString sourceName() const { return sourceDisplayName(sourceEnum()); }
    /// "歌手 · 2020-01-01 · 12 首"
    QString subtitle() const;
    QVariantMap toVariantMap() const;
};

inline QString albumKey(const Album &album)
{
    return album.source + QLatin1Char(':') + album.id;
}

/// A music video, normalised across both platforms.
struct Mv
{
    QString source;
    QString id;
    QString name;
    QString artist;
    QString artistId;
    QString coverUrl;
    QString videoHash;      // KuGou: hash used by the video url endpoint
    qint64 durationMs = 0;
    qint64 playCount = 0;
    QString publishTime;
    bool subscribed = false;

    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
    Source sourceEnum() const { return sourceFromString(source); }
    QString sourceName() const { return sourceDisplayName(sourceEnum()); }
    QString subtitle() const;
    QString durationText() const;
    QVariantMap toVariantMap() const;
};

inline QString mvKey(const Mv &mv)
{
    return mv.source + QLatin1Char(':') + mv.id;
}

/// An artist, normalised across both platforms.
struct Artist
{
    QString source;
    QString id;
    QString name;
    QString avatarUrl;
    QString description;
    int songCount = 0;
    int albumCount = 0;
    int mvCount = 0;
    qint64 fansCount = 0;
    bool followed = false;

    bool isValid() const { return !id.isEmpty() && !name.isEmpty(); }
    Source sourceEnum() const { return sourceFromString(source); }
    QString sourceName() const { return sourceDisplayName(sourceEnum()); }
    QVariantMap toVariantMap() const;
};

inline QString artistKey(const Artist &artist)
{
    return artist.source + QLatin1Char(':') + artist.id;
}

/// Unified audio quality selection (TARGET.md §2.3).
enum class Quality { Standard = 0, High = 1, Lossless = 2, HiRes = 3 };

QString neteaseLevelForQuality(Quality quality);
QString kugouQualityForQuality(Quality quality);

/// Result of a resolved playable / downloadable URL.
struct AudioStream
{
    QString url;
    QString extension;   // "mp3" / "flac" / "m4a"
    int bitrate = 0;
    qint64 size = 0;
    int durationMs = 0;
    bool trial = false;  // only a preview was returned (VIP restriction)
    QString message;     // platform message when the request failed

    bool isEmpty() const { return url.isEmpty(); }
};

// Async callbacks shared by the platform clients.
using JsonCallback = std::function<void(bool ok, const QJsonObject &body, const QString &error)>;
using StreamCallback = std::function<void(bool ok, const AudioStream &stream, const QString &error)>;
using TextCallback = std::function<void(bool ok, const QString &text, const QString &error)>;

} // namespace mr
