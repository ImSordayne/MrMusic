#include "download/TagWriter.h"

#include "core/Logger.h"

#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QSaveFile>
#include <QVector>

namespace mr {
namespace {

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

void appendU32Be(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>((value >> 24) & 0xff));
    out.append(static_cast<char>((value >> 16) & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
    out.append(static_cast<char>(value & 0xff));
}

void appendU32Le(QByteArray &out, quint32 value)
{
    out.append(static_cast<char>(value & 0xff));
    out.append(static_cast<char>((value >> 8) & 0xff));
    out.append(static_cast<char>((value >> 16) & 0xff));
    out.append(static_cast<char>((value >> 24) & 0xff));
}

quint32 readU32Be(const QByteArray &data, int offset)
{
    return (quint32(quint8(data.at(offset))) << 24) | (quint32(quint8(data.at(offset + 1))) << 16)
        | (quint32(quint8(data.at(offset + 2))) << 8) | quint32(quint8(data.at(offset + 3)));
}

quint32 readU32Le(const QByteArray &data, int offset)
{
    return quint32(quint8(data.at(offset))) | (quint32(quint8(data.at(offset + 1))) << 8)
        | (quint32(quint8(data.at(offset + 2))) << 16) | (quint32(quint8(data.at(offset + 3))) << 24);
}

void setU32Be(QByteArray &data, int offset, quint32 value)
{
    data[offset] = static_cast<char>((value >> 24) & 0xff);
    data[offset + 1] = static_cast<char>((value >> 16) & 0xff);
    data[offset + 2] = static_cast<char>((value >> 8) & 0xff);
    data[offset + 3] = static_cast<char>(value & 0xff);
}

QByteArray utf16WithBom(const QString &text)
{
    QByteArray out;
    out.append(static_cast<char>(0xff));
    out.append(static_cast<char>(0xfe));
    // QString stores UTF-16, which is what ID3v2 encoding 1 expects.
    const char16_t *units = reinterpret_cast<const char16_t *>(text.utf16());
    out.append(reinterpret_cast<const char *>(units), text.size() * 2);
    out.append('\0');
    out.append('\0');
    return out;
}

QByteArray id3TextFrame(const char *id, const QString &text)
{
    QByteArray payload;
    payload.append(static_cast<char>(0x01)); // UTF-16 with BOM
    payload.append(utf16WithBom(text));

    QByteArray frame;
    frame.append(id, 4);
    appendU32Be(frame, static_cast<quint32>(payload.size()));
    frame.append('\0');
    frame.append('\0');
    frame.append(payload);
    return frame;
}

QByteArray id3LyricsFrame(const QString &lyrics)
{
    QByteArray payload;
    payload.append(static_cast<char>(0x01)); // UTF-16
    payload.append("eng");
    payload.append(utf16WithBom(QString())); // empty content descriptor
    payload.append(utf16WithBom(lyrics));

    QByteArray frame;
    frame.append("USLT", 4);
    appendU32Be(frame, static_cast<quint32>(payload.size()));
    frame.append('\0');
    frame.append('\0');
    frame.append(payload);
    return frame;
}

QByteArray id3PictureFrame(const QByteArray &image, const QByteArray &mime)
{
    QByteArray payload;
    payload.append(static_cast<char>(0x00)); // ISO-8859-1
    payload.append(mime.isEmpty() ? QByteArrayLiteral("image/jpeg") : mime);
    payload.append('\0');
    payload.append(static_cast<char>(0x03)); // front cover
    payload.append('\0');                    // empty description
    payload.append(image);

    QByteArray frame;
    frame.append("APIC", 4);
    appendU32Be(frame, static_cast<quint32>(payload.size()));
    frame.append('\0');
    frame.append('\0');
    frame.append(payload);
    return frame;
}

QByteArray syncSafe(quint32 value)
{
    QByteArray out(4, '\0');
    out[0] = static_cast<char>((value >> 21) & 0x7f);
    out[1] = static_cast<char>((value >> 14) & 0x7f);
    out[2] = static_cast<char>((value >> 7) & 0x7f);
    out[3] = static_cast<char>(value & 0x7f);
    return out;
}

// ---------------------------------------------------------------------------
// MP3 / ID3v2.3
// ---------------------------------------------------------------------------

bool writeId3v2(const QString &path, const TagInfo &info, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray original = file.readAll();
    file.close();

    int audioOffset = 0;
    if (original.size() > 10 && original.startsWith("ID3")) {
        const quint32 size = (quint32(quint8(original.at(6))) << 21) | (quint32(quint8(original.at(7))) << 14)
            | (quint32(quint8(original.at(8))) << 7) | quint32(quint8(original.at(9)));
        audioOffset = 10 + int(size);
        if (original.at(5) & 0x10) // footer present
            audioOffset += 10;
        if (audioOffset > original.size())
            audioOffset = 0;
    }

    QByteArray frames;
    if (!info.title.isEmpty())
        frames.append(id3TextFrame("TIT2", info.title));
    if (!info.artists.isEmpty())
        frames.append(id3TextFrame("TPE1", info.artists));
    if (!info.album.isEmpty())
        frames.append(id3TextFrame("TALB", info.album));
    if (info.year > 0)
        frames.append(id3TextFrame("TYER", QString::number(info.year)));
    frames.append(id3TextFrame("TSSE", QStringLiteral("MrMusic")));
    if (!info.lyrics.isEmpty())
        frames.append(id3LyricsFrame(info.lyrics));
    if (!info.cover.isEmpty())
        frames.append(id3PictureFrame(info.cover, info.coverMime));

    QByteArray tag;
    tag.append("ID3", 3);
    tag.append(static_cast<char>(0x03)); // version 2.3
    tag.append('\0');
    tag.append('\0'); // flags
    tag.append(syncSafe(static_cast<quint32>(frames.size())));
    tag.append(frames);

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = out.errorString();
        return false;
    }
    out.write(tag);
    if (audioOffset < original.size())
        out.write(original.mid(audioOffset));
    if (!out.commit()) {
        if (error)
            *error = out.errorString();
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// FLAC: VORBIS_COMMENT + PICTURE
// ---------------------------------------------------------------------------

QByteArray flacCommentBlock(const TagInfo &info)
{
    QByteArray payload;
    const QByteArray vendor = QByteArrayLiteral("MrMusic");
    appendU32Le(payload, static_cast<quint32>(vendor.size()));
    payload.append(vendor);

    QVector<QPair<QByteArray, QByteArray>> fields;
    if (!info.title.isEmpty())
        fields.append({QByteArrayLiteral("TITLE"), info.title.toUtf8()});
    if (!info.artists.isEmpty())
        fields.append({QByteArrayLiteral("ARTIST"), info.artists.toUtf8()});
    if (!info.album.isEmpty())
        fields.append({QByteArrayLiteral("ALBUM"), info.album.toUtf8()});
    if (!info.sourceName.isEmpty())
        fields.append({QByteArrayLiteral("SOURCE"), info.sourceName.toUtf8()});
    if (info.year > 0)
        fields.append({QByteArrayLiteral("DATE"), QByteArray::number(info.year)});
    if (!info.lyrics.isEmpty())
        fields.append({QByteArrayLiteral("LYRICS"), info.lyrics.toUtf8()});

    appendU32Le(payload, static_cast<quint32>(fields.size()));
    for (const auto &field : fields) {
        const QByteArray entry = field.first + '=' + field.second;
        appendU32Le(payload, static_cast<quint32>(entry.size()));
        payload.append(entry);
    }
    return payload;
}

QByteArray flacPictureBlock(const TagInfo &info)
{
    QByteArray payload;
    appendU32Be(payload, 3); // front cover
    const QByteArray mime = info.coverMime.isEmpty() ? QByteArrayLiteral("image/jpeg") : info.coverMime;
    appendU32Be(payload, static_cast<quint32>(mime.size()));
    payload.append(mime);
    appendU32Be(payload, 0); // description length

    const QImage image = QImage::fromData(info.cover);
    appendU32Be(payload, static_cast<quint32>(image.width()));
    appendU32Be(payload, static_cast<quint32>(image.height()));
    appendU32Be(payload, image.isNull() ? 24 : static_cast<quint32>(image.depth()));
    appendU32Be(payload, 0);
    appendU32Be(payload, static_cast<quint32>(info.cover.size()));
    payload.append(info.cover);
    return payload;
}

bool writeFlac(const QString &path, const TagInfo &info, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    const QByteArray data = file.readAll();
    file.close();

    if (!data.startsWith("fLaC")) {
        if (error)
            *error = QStringLiteral("not a FLAC file");
        return false;
    }

    int offset = 4;
    QVector<QPair<int, QByteArray>> keptBlocks; // type + raw block (header included)
    while (offset + 4 <= data.size()) {
        const quint8 header = quint8(data.at(offset));
        const int type = header & 0x7f;
        const int length = (quint8(data.at(offset + 1)) << 16) | (quint8(data.at(offset + 2)) << 8)
            | quint8(data.at(offset + 3));
        if (offset + 4 + length > data.size())
            break;
        const QByteArray raw = data.mid(offset, 4 + length);
        if (type != 4 && type != 6) // drop existing comments/pictures
            keptBlocks.append({type, raw});
        offset += 4 + length;
        if (header & 0x80)
            break;
    }
    const int audioStart = offset;

    QVector<QByteArray> blocks;
    QVector<int> types;

    // STREAMINFO must stay first.
    for (const auto &block : keptBlocks) {
        blocks.append(block.second);
        types.append(block.first);
        if (block.first == 0)
            break;
    }
    types.append(4);
    blocks.append(QByteArray());
    if (!info.cover.isEmpty()) {
        types.append(6);
        blocks.append(QByteArray());
    }

    // rebuild the block payloads
    QByteArray output = "fLaC";
    for (int i = 0; i < blocks.size(); ++i) {
        QByteArray payload;
        if (types.at(i) == 4)
            payload = flacCommentBlock(info);
        else if (types.at(i) == 6)
            payload = flacPictureBlock(info);
        else
            payload = blocks.at(i).mid(4); // strip old header

        const bool last = (i == blocks.size() - 1);
        const quint32 length = static_cast<quint32>(payload.size());
        output.append(static_cast<char>((last ? 0x80 : 0x00) | (types.at(i) & 0x7f)));
        output.append(static_cast<char>((length >> 16) & 0xff));
        output.append(static_cast<char>((length >> 8) & 0xff));
        output.append(static_cast<char>(length & 0xff));
        output.append(payload);
    }

    if (audioStart < data.size())
        output.append(data.mid(audioStart));

    QSaveFile out(path);
    if (!out.open(QIODevice::WriteOnly)) {
        if (error)
            *error = out.errorString();
        return false;
    }
    out.write(output);
    if (!out.commit()) {
        if (error)
            *error = out.errorString();
        return false;
    }
    return true;
}

} // namespace

QString TagWriter::containerForExtension(const QString &extension)
{
    const QString ext = extension.toLower();
    if (ext == QLatin1String("mp3"))
        return QStringLiteral("mp3");
    if (ext == QLatin1String("flac"))
        return QStringLiteral("flac");
    if (ext == QLatin1String("m4a") || ext == QLatin1String("mp4") || ext == QLatin1String("aac"))
        return QStringLiteral("m4a");
    return ext;
}

bool TagWriter::write(const QString &filePath, const TagInfo &info, QString *errorMessage,
                      const QString &containerHint)
{
    const QString extension = containerHint.isEmpty() ? QFileInfo(filePath).suffix().toLower()
                                                      : containerHint.toLower();
    const QString container = containerForExtension(extension);

    if (container == QLatin1String("mp3"))
        return writeId3v2(filePath, info, errorMessage);
    if (container == QLatin1String("flac"))
        return writeFlac(filePath, info, errorMessage);

    Logger::info(QStringLiteral("tag writing not supported for '.%1', skipping").arg(extension));
    return true;
}

} // namespace mr
