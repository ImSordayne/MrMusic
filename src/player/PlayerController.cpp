#include "player/PlayerController.h"

#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/Logger.h"
#include "core/Settings.h"
#include "player/AudioEngine.h"
#include "player/MediaControls.h"
#include "ui/TrackListModel.h"

#include <QRandomGenerator>
#include <QWindow>

namespace mr {

PlayerController::PlayerController(NeteaseClient *netease, KugouClient *kugou, Settings *settings,
                                   QObject *parent)
    : QObject(parent)
    , m_netease(netease)
    , m_kugou(kugou)
    , m_settings(settings)
    , m_engine(new AudioEngine(this))
    , m_mediaControls(MediaControls::create(this))
    , m_queue(new TrackListModel(this))
{
    m_volume = settings ? settings->volume() : 0.8;
    m_playMode = settings ? settings->playMode() : 1;
    m_engine->setVolume(m_volume);

    connect(m_engine, &AudioEngine::videoChanged, this, [this](bool hasVideo) {
        if (m_videoAvailable == hasVideo)
            return;
        m_videoAvailable = hasVideo;
        emit videoAvailableChanged();
    });

    connect(m_engine, &AudioEngine::playingChanged, this, [this](bool playing) {
        if (m_playing == playing)
            return;
        m_playing = playing;
        m_mediaControls->setPlaybackState(playing);
        emit playingChanged();
    });

    connect(m_engine, &AudioEngine::positionChanged, this, [this](qint64 position) {
        m_position = position;
        emit positionChanged();
    });

    connect(m_engine, &AudioEngine::durationChanged, this, [this](qint64 duration) {
        m_duration = duration;
        emit durationChanged();
    });

    connect(m_engine, &AudioEngine::finished, this, &PlayerController::handleFinished);

    connect(m_engine, &AudioEngine::errorOccurred, this, [this](const QString &message) {
        setStatus(tr("播放失败：%1").arg(message));
        setLoading(false);
    });

    connect(m_mediaControls, &MediaControls::togglePlayRequested, this, &PlayerController::togglePlay);
    connect(m_mediaControls, &MediaControls::nextRequested, this, &PlayerController::next);
    connect(m_mediaControls, &MediaControls::previousRequested, this, &PlayerController::previous);
    connect(m_mediaControls, &MediaControls::stopRequested, this, [this]() {
        m_engine->stop();
        m_mediaControls->setPlaybackState(false);
    });
    connect(m_mediaControls, &MediaControls::seekRequested, this, &PlayerController::seek);
}

PlayerController::~PlayerController() = default;

void PlayerController::attachWindow(QWindow *window)
{
    if (m_mediaControls)
        m_mediaControls->attachToWindow(window);
    m_mediaControls->setCanSkip(m_queue->count() > 1, m_queue->count() > 1);
}

QVariantMap PlayerController::currentTrackMap() const
{
    return m_current.toVariantMap();
}

void PlayerController::setFavoriteKeys(const QStringList &keys)
{
    QSet<QString> set;
    for (const QString &key : keys)
        set.insert(key);
    m_queue->setFavoriteKeys(set);
}

QString PlayerController::formatTime(qint64 milliseconds)
{
    const qint64 seconds = qMax<qint64>(0, milliseconds) / 1000;
    return QStringLiteral("%1:%2").arg(seconds / 60).arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

void PlayerController::setStatus(const QString &message)
{
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
    if (!message.isEmpty())
        Logger::warn(QStringLiteral("player: %1").arg(message));
}

void PlayerController::clearStatusMessage()
{
    setStatus(QString());
}

void PlayerController::setLoading(bool loading)
{
    if (m_loading == loading)
        return;
    m_loading = loading;
    emit loadingChanged();
}

void PlayerController::clearVideoMode()
{
    if (!m_videoMode && !m_videoAvailable)
        return;
    m_videoMode = false;
    m_videoAvailable = false;
    emit videoModeChanged();
    emit videoAvailableChanged();
}

void PlayerController::playMv(const QVariantMap &mv)
{
    const QString source = mv.value(QStringLiteral("source")).toString();
    const QString id = mv.value(QStringLiteral("id")).toString();
    const QString hash = mv.value(QStringLiteral("videoHash")).toString();
    if (id.isEmpty() && hash.isEmpty())
        return;

    Track track;
    track.source = source;
    track.id = id.isEmpty() ? hash : id;
    track.hash = hash;
    track.title = mv.value(QStringLiteral("name")).toString();
    track.artists = mv.value(QStringLiteral("artist")).toString();
    track.album = tr("MV");
    track.coverUrl = mv.value(QStringLiteral("coverUrl")).toString();
    track.durationMs = static_cast<qint64>(mv.value(QStringLiteral("durationMs")).toDouble());

    m_current = track;
    m_currentRow = -1;
    m_position = 0;
    m_duration = track.durationMs;
    m_videoMode = true;
    m_videoAvailable = false;
    emit videoModeChanged();
    emit videoAvailableChanged();
    emit currentChanged();
    emit positionChanged();
    emit durationChanged();
    setLoading(true);
    setStatus(tr("正在获取 MV 地址…"));

    auto finish = [this](bool ok, const QString &url, const QString &error) {
        setLoading(false);
        if (!ok || url.isEmpty()) {
            setStatus(error.isEmpty() ? tr("无法播放该 MV") : error);
            return;
        }
        m_engine->setSource(QUrl(url));
        m_engine->play();
        m_playing = true;
        emit playingChanged();
        if (m_mediaControls) {
            m_mediaControls->setMetadata(m_current.title, m_current.artists, m_current.album,
                                         QUrl(m_current.coverUrl));
            m_mediaControls->setPlaybackState(true);
        }
        setStatus(tr("正在播放 MV：%1").arg(m_current.title));
    };

    if (track.sourceEnum() == Source::Netease) {
        m_netease->mvUrl(track.id, 1080,
                         [finish](bool ok, const QJsonObject &body, const QString &error) {
                             const QJsonObject data = body.value(QStringLiteral("data")).toObject();
                             QString url = data.value(QStringLiteral("url")).toString();
                             if (url.isEmpty())
                                 url = data.value(QStringLiteral("murl")).toString();
                             finish(ok, url, ok && url.isEmpty() ? QStringLiteral("该 MV 没有可用的播放地址")
                                                                 : error);
                         });
        return;
    }
    if (track.sourceEnum() == Source::Kugou) {
        m_kugou->videoUrl(track.hash.isEmpty() ? track.id : track.hash,
                          [finish](bool ok, const QJsonObject &body, const QString &error) {
                              finish(ok, body.value(QStringLiteral("url")).toString(), error);
                          });
        return;
    }
    finish(false, QString(), tr("未知的平台来源"));
}

void PlayerController::attachVideoOutput(QObject *output)
{
    m_engine->attachVideoOutput(output);
}

void PlayerController::playTracks(const QVariantList &tracks, int startIndex)
{
    clearVideoMode();
    QList<Track> list;
    list.reserve(tracks.size());
    for (const QVariant &entry : tracks) {
        const Track track = Track::fromVariantMap(entry.toMap());
        if (track.isValid())
            list.append(track);
    }
    if (list.isEmpty())
        return;

    m_queue->setTracks(list);
    playAt(qBound(0, startIndex, static_cast<int>(list.size()) - 1));
}

void PlayerController::playTrack(const QVariantMap &track)
{
    clearVideoMode();
    const Track t = Track::fromVariantMap(track);
    if (!t.isValid())
        return;

    for (int row = 0; row < m_queue->count(); ++row) {
        if (trackKey(m_queue->trackAt(row)) == trackKey(t)) {
            playAt(row);
            return;
        }
    }
    m_queue->append(t);
    playAt(m_queue->count() - 1);
}

void PlayerController::playAt(int row)
{
    if (row < 0 || row >= m_queue->count())
        return;
    m_currentRow = row;
    m_current = m_queue->trackAt(row);
    m_queue->setCurrentKey(trackKey(m_current));

    m_position = 0;
    m_duration = m_current.durationMs;
    emit positionChanged();
    emit durationChanged();

    m_mediaControls->setMetadata(m_current.title, m_current.artists, m_current.album,
                                 QUrl(m_current.coverUrl));
    m_mediaControls->setCanSkip(m_queue->count() > 1, m_queue->count() > 1);

    emit currentChanged();
    emit trackStarted(m_current.toVariantMap());

    loadCurrentAndPlay();
}

void PlayerController::loadCurrentAndPlay()
{
    if (!m_current.isValid())
        return;

    setLoading(true);
    setStatus(QString());

    const Quality quality = m_settings ? static_cast<Quality>(m_settings->playbackQuality()) : Quality::Lossless;

    auto onStream = [this](bool ok, const AudioStream &stream, const QString &error) {
        setLoading(false);
        if (!ok || stream.isEmpty()) {
            setStatus(error.isEmpty() ? tr("无版权") : error);
            m_mediaControls->setPlaybackState(false);
            return;
        }
        if (stream.trial)
            setStatus(tr("当前为试听片段（VIP 歌曲）"));
        m_engine->setSource(QUrl(stream.url));
        m_engine->play();
        m_mediaControls->setPlaybackState(true);
    };

    if (m_current.sourceEnum() == Source::Netease) {
        m_netease->songUrl(m_current, quality, onStream);
    } else if (m_current.sourceEnum() == Source::Kugou) {
        m_kugou->songUrl(m_current, quality, onStream);
    } else {
        setLoading(false);
        setStatus(tr("未知来源"));
    }
}

void PlayerController::enqueue(const QVariantMap &track)
{
    const Track t = Track::fromVariantMap(track);
    if (!t.isValid())
        return;
    m_queue->append(t);
    m_mediaControls->setCanSkip(m_queue->count() > 1, m_queue->count() > 1);
}

void PlayerController::enqueueNext(const QVariantMap &track)
{
    const Track t = Track::fromVariantMap(track);
    if (!t.isValid())
        return;
    const int row = m_currentRow >= 0 ? m_currentRow + 1 : m_queue->count();
    m_queue->insertAt(row, t);
    m_mediaControls->setCanSkip(m_queue->count() > 1, m_queue->count() > 1);
}

void PlayerController::togglePlay()
{
    if (!m_current.isValid()) {
        if (m_queue->count() > 0)
            playAt(0);
        return;
    }
    if (m_engine->isPlaying()) {
        m_engine->pause();
        m_mediaControls->setPlaybackState(false);
    } else {
        m_engine->play();
        m_mediaControls->setPlaybackState(true);
    }
}

void PlayerController::play()
{
    if (m_current.isValid())
        m_engine->play();
}

void PlayerController::pause()
{
    m_engine->pause();
}

int PlayerController::nextRow(int direction) const
{
    const int count = m_queue->count();
    if (count == 0)
        return -1;
    if (count == 1)
        return 0;

    if (m_playMode == Settings::Shuffle) {
        int candidate = m_currentRow;
        while (candidate == m_currentRow)
            candidate = QRandomGenerator::global()->bounded(count);
        return candidate;
    }

    int row = m_currentRow + direction;
    if (row >= count) {
        if (m_playMode == Settings::RepeatAll || m_playMode == Settings::RepeatOne)
            row = 0;
        else
            return -1;
    } else if (row < 0) {
        row = count - 1;
    }
    return row;
}

void PlayerController::next()
{
    const int row = nextRow(1);
    if (row >= 0)
        playAt(row);
}

void PlayerController::previous()
{
    if (m_position > 3000) {
        seek(0);
        return;
    }
    const int row = nextRow(-1);
    if (row >= 0)
        playAt(row);
}

void PlayerController::handleFinished()
{
    if (m_playMode == Settings::RepeatOne) {
        seek(0);
        m_engine->play();
        return;
    }
    const int row = nextRow(1);
    if (row < 0) {
        m_engine->stop();
        m_mediaControls->setPlaybackState(false);
        return;
    }
    playAt(row);
}

void PlayerController::seek(int positionMs)
{
    m_engine->setPosition(qMax(0, positionMs));
}

void PlayerController::setVolume(double volume)
{
    volume = qBound(0.0, volume, 1.0);
    if (qFuzzyCompare(m_volume, volume))
        return;
    m_volume = volume;
    m_engine->setVolume(volume);
    if (m_settings)
        m_settings->setVolume(volume);
    emit volumeChanged();
}

void PlayerController::setPlayMode(int mode)
{
    mode = qBound(0, mode, 3);
    if (m_playMode == mode)
        return;
    m_playMode = mode;
    if (m_settings)
        m_settings->setPlayMode(mode);
    emit playModeChanged();
}

void PlayerController::cyclePlayMode()
{
    setPlayMode((m_playMode + 1) % 4);
}

void PlayerController::clearQueue()
{
    m_queue->clear();
    m_current = Track();
    m_currentRow = -1;
    m_queue->setCurrentKey(QString());
    m_engine->clearSource();
    m_position = 0;
    m_duration = 0;
    emit currentChanged();
    emit positionChanged();
    emit durationChanged();
}

void PlayerController::removeFromQueue(int row)
{
    if (row < 0 || row >= m_queue->count())
        return;
    m_queue->removeAt(row);
    if (row == m_currentRow) {
        if (m_queue->count() == 0) {
            clearQueue();
            return;
        }
        playAt(qMin(row, m_queue->count() - 1));
    } else if (row < m_currentRow) {
        --m_currentRow;
    }
}

} // namespace mr
