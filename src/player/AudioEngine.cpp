#include "player/AudioEngine.h"

#include "core/Logger.h"

#include <QAudioOutput>
#include <QVariant>
#include <QVideoSink>
#include <QMediaPlayer>

namespace mr {

AudioEngine::AudioEngine(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_output(new QAudioOutput(this))
{
    m_player->setAudioOutput(m_output);
    m_videoSink = new QVideoSink(this);
    m_player->setVideoSink(m_videoSink);

    connect(m_player, &QMediaPlayer::playbackStateChanged, this, [this](QMediaPlayer::PlaybackState state) {
        emit playingChanged(state == QMediaPlayer::PlayingState);
    });

    connect(m_player, &QMediaPlayer::positionChanged, this, &AudioEngine::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &AudioEngine::durationChanged);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia)
            emit finished();
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia
            || status == QMediaPlayer::BufferingMedia)
            emit videoChanged(hasVideo());
    });

    connect(m_player, &QMediaPlayer::hasVideoChanged, this, [this](bool hasVideo) {
        emit videoChanged(hasVideo);
    });

    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString &errorString) {
                if (error == QMediaPlayer::NoError)
                    return;
                Logger::warn(QStringLiteral("audio engine error: %1 (%2)").arg(errorString).arg(int(error)));
                emit errorOccurred(errorString);
            });
}

AudioEngine::~AudioEngine() = default;

void AudioEngine::setSource(const QUrl &url)
{
    m_player->setSource(url);
}

void AudioEngine::clearSource()
{
    m_player->stop();
    m_player->setSource(QUrl());
}

void AudioEngine::attachVideoOutput(QObject *videoOutput)
{
    if (!videoOutput) {
        m_player->setVideoSink(m_videoSink);
        return;
    }
    const QVariant value = videoOutput->property("videoSink");
    auto *sink = value.value<QVideoSink *>();
    if (!sink) {
        Logger::warn(QStringLiteral("video output has no videoSink property"));
        return;
    }
    m_player->setVideoSink(sink);
}

bool AudioEngine::hasVideo() const
{
    return m_player->hasVideo();
}

void AudioEngine::play()
{
    m_player->play();
}

void AudioEngine::pause()
{
    m_player->pause();
}

void AudioEngine::stop()
{
    m_player->stop();
}

void AudioEngine::setVolume(double volume)
{
    m_output->setVolume(qBound(0.0, volume, 1.0));
}

double AudioEngine::volume() const
{
    return m_output->volume();
}

void AudioEngine::setPosition(qint64 positionMs)
{
    m_player->setPosition(positionMs);
}

qint64 AudioEngine::position() const
{
    return m_player->position();
}

qint64 AudioEngine::duration() const
{
    return m_player->duration();
}

bool AudioEngine::isPlaying() const
{
    return m_player->playbackState() == QMediaPlayer::PlayingState;
}

} // namespace mr
