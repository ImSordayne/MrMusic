#include "download/DownloadManager.h"

#include "api/HttpClient.h"
#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/AppPaths.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "download/TagWriter.h"

#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

namespace mr {
namespace {

QString qualityLabel(int quality)
{
    return Settings::qualityFileNameSuffix(quality);
}

} // namespace

DownloadManager::DownloadManager(NeteaseClient *netease, KugouClient *kugou, Settings *settings,
                                 QObject *parent)
    : QObject(parent)
    , m_netease(netease)
    , m_kugou(kugou)
    , m_settings(settings)
    , m_http(new HttpClient(this))
{
    if (settings && settings->proxyEnabled())
        m_http->setProxy(settings->proxyUrl());
}

QString DownloadManager::targetDir() const
{
    if (m_settings && !m_settings->downloadDir().isEmpty())
        return m_settings->downloadDir();
    return AppPaths::defaultMusicDir();
}

void DownloadManager::setTargetDir(const QString &dir)
{
    if (!m_settings || dir.isEmpty())
        return;
    m_settings->setDownloadDir(dir);
    emit targetDirChanged();
}

QString DownloadManager::sanitize(const QString &text)
{
    QString out = text.trimmed();
    static const QRegularExpression invalid(QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1f]"));
    out.replace(invalid, QStringLiteral("_"));
    out.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    if (out.size() > 120)
        out = out.left(120);
    if (out.isEmpty())
        out = QStringLiteral("unknown");
    return out;
}


void DownloadManager::rebuildItems()
{
    m_items.clear();
    for (const Item &item : m_queue) {
        QVariantMap map;
        map[QStringLiteral("title")] = item.track.title;
        map[QStringLiteral("artists")] = item.track.artists;
        map[QStringLiteral("album")] = item.track.album;
        map[QStringLiteral("sourceName")] = item.track.sourceName();
        map[QStringLiteral("coverUrl")] = item.track.coverUrl;
        map[QStringLiteral("state")] = item.state;
        map[QStringLiteral("message")] = item.message;
        map[QStringLiteral("progress")] = item.progress;
        map[QStringLiteral("filePath")] = item.filePath;
        map[QStringLiteral("quality")] = item.qualityText;
        m_items.append(map);
    }
    int active = 0;
    for (const Item &item : m_queue) {
        if (item.state == QLatin1String("resolving") || item.state == QLatin1String("downloading")
            || item.state == QLatin1String("tagging"))
            ++active;
    }
    m_activeCount = active;
    emit itemsChanged();
}

int DownloadManager::activeForSource(const QString &source) const
{
    int count = 0;
    for (const Item &item : m_queue) {
        if (item.track.source == source
            && (item.state == QLatin1String("resolving") || item.state == QLatin1String("downloading")
                || item.state == QLatin1String("tagging")))
            ++count;
    }
    return count;
}

void DownloadManager::updateItem(int index, const QString &state, const QString &message, double progress)
{
    if (index < 0 || index >= m_queue.size())
        return;
    Item &item = m_queue[index];
    if (!state.isEmpty())
        item.state = state;
    if (!message.isNull())
        item.message = message;
    if (progress >= 0)
        item.progress = progress;

    rebuildItems();

    if (state == QLatin1String("done") || state == QLatin1String("error"))
        pump();
}

void DownloadManager::enqueue(const QVariantMap &track)
{
    const Track t = Track::fromVariantMap(track);
    if (!t.isValid())
        return;

    const QString key = trackKey(t);
    for (const Item &item : m_queue) {
        if (trackKey(item.track) == key && item.state != QLatin1String("error"))
            return; // already queued
    }

    Item item;
    item.track = t;
    item.state = QStringLiteral("queued");
    item.qualityText = qualityLabel(m_settings ? m_settings->playbackQuality() : 2);
    m_queue.append(item);
    rebuildItems();
    pump();
}

void DownloadManager::enqueueList(const QVariantList &tracks)
{
    for (const QVariant &entry : tracks)
        enqueue(entry.toMap());
}

void DownloadManager::retry(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;
    m_queue[index].state = QStringLiteral("queued");
    m_queue[index].message.clear();
    m_queue[index].progress = 0.0;
    rebuildItems();
    pump();
}

void DownloadManager::remove(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;
    if (m_queue.at(index).state == QLatin1String("downloading"))
        return;
    m_queue.removeAt(index);
    rebuildItems();
}

void DownloadManager::clearFinished()
{
    for (int i = m_queue.size() - 1; i >= 0; --i) {
        const QString state = m_queue.at(i).state;
        if (state == QLatin1String("done") || state == QLatin1String("error"))
            m_queue.removeAt(i);
    }
    rebuildItems();
}

void DownloadManager::openFolder() const
{
    const QString dir = targetDir();
    QDir().mkpath(dir);
    QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
}

void DownloadManager::pump()
{
    const int quality = m_settings ? m_settings->playbackQuality() : 2;
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).state != QLatin1String("queued"))
            continue;
        if (activeForSource(m_queue.at(i).track.source) >= maxPerSource())
            continue;
        m_queue[i].qualityText = qualityLabel(quality);
        startItem(i);
    }
}

void DownloadManager::startItem(int index)
{
    updateItem(index, QStringLiteral("resolving"), QString(), 0.0);
    resolveAndDownload(index);
}

void DownloadManager::resolveAndDownload(int index)
{
    if (index < 0 || index >= m_queue.size())
        return;

    const Track track = m_queue.at(index).track;
    const Quality quality = m_settings ? static_cast<Quality>(m_settings->playbackQuality()) : Quality::Lossless;

    auto onStream = [this, index, track, quality](bool ok, const AudioStream &stream, const QString &error) {
        if (!ok || stream.isEmpty()) {
            updateItem(index, QStringLiteral("error"), error.isEmpty() ? tr("无法获取下载地址") : error, -1);
            emit failed(track.title, error);
            return;
        }

        QString extension = stream.extension.toLower();
        if (extension.isEmpty())
            extension = stream.bitrate > 320000 ? QStringLiteral("flac") : QStringLiteral("mp3");

        // The name must reflect the quality that was actually delivered, not the
        // requested one (TARGET.md §2.4: no silent downgrade, but also no lying).
        QString qualityText;
        if (extension == QLatin1String("flac"))
            qualityText = stream.bitrate >= 900000 ? QStringLiteral("HiRes") : QStringLiteral("FLAC");
        else if (extension == QLatin1String("m4a") || extension == QLatin1String("mp4"))
            qualityText = stream.bitrate >= 400000 ? QStringLiteral("HiRes") : QStringLiteral("M4A");
        else if (stream.bitrate >= 300000)
            qualityText = QStringLiteral("320K");
        else
            qualityText = QStringLiteral("128K");
        m_queue[index].qualityText = qualityText;
        Q_UNUSED(quality)

        const QString dir = targetDir();
        QDir().mkpath(dir);

        QString baseName = sanitize(track.artists) + QLatin1Char('-') + sanitize(track.album) + QLatin1Char('-')
            + sanitize(qualityText) + QLatin1Char('-') + sanitize(track.sourceName());
        QString fileName = baseName + QLatin1Char('.') + extension;
        QString fullPath = QDir(dir).absoluteFilePath(fileName);
        int suffix = 1;
        while (QFile::exists(fullPath)) {
            fileName = baseName + QStringLiteral(" (%1).").arg(suffix++) + extension;
            fullPath = QDir(dir).absoluteFilePath(fileName);
        }

        m_queue[index].filePath = fullPath;
        const QString partPath = fullPath + QStringLiteral(".part");

        updateItem(index, QStringLiteral("downloading"), QString(), 0.0);

        HttpRequest request;
        request.url = QUrl(stream.url);
        request.method = QByteArrayLiteral("GET");
        request.setHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");

        m_http->download(request, partPath,
                         [this, index](qint64 received, qint64 total) {
                             const double progress = total > 0 ? double(received) / double(total) : 0.0;
                             updateItem(index, QString(), QString(), qBound(0.0, progress, 1.0));
                         },
                         [this, index, partPath, fullPath, track](bool downloadOk, const QString &downloadError) {
                             if (!downloadOk) {
                                 updateItem(index, QStringLiteral("error"), downloadError, -1);
                                 emit failed(track.title, downloadError);
                                 return;
                             }
                             updateItem(index, QStringLiteral("tagging"), QString(), 1.0);
                             writeTagsAndFinish(index, partPath, fullPath, track);
                         });
    };

    if (track.sourceEnum() == Source::Netease)
        m_netease->songUrl(track, quality, onStream);
    else if (track.sourceEnum() == Source::Kugou)
        m_kugou->songUrl(track, quality, onStream);
    else
        updateItem(index, QStringLiteral("error"), tr("未知来源"), -1);
}

void DownloadManager::writeTagsAndFinish(int index, const QString &partPath, const QString &fullPath,
                                        const Track &track)
{
    auto finishWith = [this, index, partPath, fullPath, track](const QString &lyrics, const QByteArray &cover,
                                                               const QByteArray &coverMime) {
        TagInfo info;
        info.title = track.title;
        info.artists = track.artists;
        info.album = track.album;
        info.sourceName = track.sourceName();
        info.lyrics = lyrics;
        info.cover = cover;
        info.coverMime = coverMime;

        QString tagError;
        TagWriter::write(partPath, info, &tagError, QFileInfo(fullPath).suffix());

        QFile::remove(fullPath);
        if (!QFile::rename(partPath, fullPath)) {
            updateItem(index, QStringLiteral("error"), tr("无法写入文件"), -1);
            emit failed(track.title, tr("无法写入文件"));
            return;
        }

        m_queue[index].filePath = fullPath;
        updateItem(index, QStringLiteral("done"), QString(), 1.0);
        Logger::info(QStringLiteral("downloaded %1").arg(fullPath));
        emit finished(fullPath);
    };

    // lyrics
    auto fetchCover = [this, track, finishWith](const QString &lyrics) {
        if (track.coverUrl.isEmpty()) {
            finishWith(lyrics, QByteArray(), QByteArray());
            return;
        }
        HttpRequest coverRequest;
        coverRequest.url = QUrl(track.coverUrl);
        coverRequest.setHeader("User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
        m_http->send(coverRequest, [finishWith, lyrics](const HttpResponse &response) {
            if (response.isOk() && !response.body.isEmpty()) {
                QByteArray mime = response.header("Content-Type");
                if (mime.isEmpty())
                    mime = QByteArrayLiteral("image/jpeg");
                finishWith(lyrics, response.body, mime);
            } else {
                finishWith(lyrics, QByteArray(), QByteArray());
            }
        }, 1);
    };

    auto lyricsCallback = [fetchCover](bool ok, const QString &text, const QString &error) {
        Q_UNUSED(ok)
        Q_UNUSED(error)
        fetchCover(text);
    };

    if (track.sourceEnum() == Source::Netease)
        m_netease->lyric(track, lyricsCallback);
    else if (track.sourceEnum() == Source::Kugou)
        m_kugou->lyric(track, lyricsCallback);
    else
        fetchCover(QString());
}

} // namespace mr
