#pragma once

#include <QObject>
#include <QUrl>

class QAudioOutput;
class QMediaPlayer;
class QVideoSink;

namespace mr {

/// Thin wrapper around Qt Multimedia.
///
/// Qt 6 renders audio through its **FFmpeg** media backend (the only backend
/// shipped with the official Qt 6.11 builds for Windows/macOS/Linux/Android),
/// which keeps the decoding path identical on every platform as required by
/// TARGET.md §2.3. `main()` pins the backend explicitly via
/// `QT_MEDIA_BACKEND=ffmpeg`.
class AudioEngine : public QObject
{
    Q_OBJECT

public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    void setSource(const QUrl &url);
    void clearSource();

    /// Attaches a QML `VideoOutput` (or detaches with nullptr) so MV playback
    /// can render video frames (TARGET.md §10.22).
    void attachVideoOutput(QObject *videoOutput);
    bool hasVideo() const;

    void play();
    void pause();
    void stop();

    void setVolume(double volume);
    double volume() const;

    void setPosition(qint64 positionMs);
    qint64 position() const;
    qint64 duration() const;
    bool isPlaying() const;

signals:
    void playingChanged(bool playing);
    void positionChanged(qint64 positionMs);
    void durationChanged(qint64 durationMs);
    void finished();
    void errorOccurred(const QString &message);
    void videoChanged(bool hasVideo);

private:
    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_output = nullptr;
    QVideoSink *m_videoSink = nullptr;
};

} // namespace mr
