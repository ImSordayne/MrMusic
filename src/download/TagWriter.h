#pragma once

#include <QByteArray>
#include <QString>

namespace mr {

/// Metadata written into a downloaded file.
struct TagInfo
{
    QString title;
    QString artists;
    QString album;
    QString sourceName;
    QString lyrics;
    QByteArray cover;
    QByteArray coverMime; // "image/jpeg" or "image/png"
    int year = 0;
    int trackNumber = 0;
};

/// Writes tags / lyrics / cover art into downloaded audio files.
///
/// Implemented for the container formats the two platforms actually serve:
///   * MP3  -> ID3v2.3 (TIT2/TPE1/TALB/TYER/USLT/APIC)
///   * FLAC -> VORBIS_COMMENT + PICTURE blocks
///   * M4A  -> iTunes `ilst` atoms (©nam/©ART/©alb/©lyr/covr)
///
/// Other containers are left untouched (the audio itself is still valid).
class TagWriter
{
public:
    static bool write(const QString &filePath, const TagInfo &info, QString *errorMessage = nullptr,
                      const QString &containerHint = QString());

    /// Extension (without dot) of the container for a downloaded file.
    static QString containerForExtension(const QString &extension);
};

} // namespace mr
