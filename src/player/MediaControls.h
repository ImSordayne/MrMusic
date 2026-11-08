#pragma once

#include <QObject>
#include <QString>
#include <QUrl>

class QWindow;

namespace mr {

/// Native "now playing" integration (TARGET.md §2.3).
///
/// The concrete implementation is created with `create()`:
///   * Windows  -> SystemMediaTransportControls (SMTC, WinRT)
///   * Linux    -> MPRIS (D-Bus `org.mpris.MediaPlayer2`)
///   * Android  -> MediaSession via JNI (Java helper in `android/src`)
///   * macOS    -> MPNowPlayingInfoCenter / MPRemoteCommandCenter
class MediaControls : public QObject
{
    Q_OBJECT

public:
    explicit MediaControls(QObject *parent = nullptr);
    ~MediaControls() override;

    static MediaControls *create(QObject *parent = nullptr);

    /// Must be called once the QML window exists (needed on Windows).
    virtual void attachToWindow(QWindow *window);
    virtual bool isAvailable() const { return false; }

    virtual void setMetadata(const QString &title, const QString &artist, const QString &album,
                             const QUrl &coverUrl);
    virtual void setPlaybackState(bool playing);
    virtual void setCanSkip(bool canGoNext, bool canGoPrevious);
    virtual void setTimeline(qint64 positionMs, qint64 durationMs);

signals:
    void playRequested();
    void pauseRequested();
    void togglePlayRequested();
    void nextRequested();
    void previousRequested();
    void stopRequested();
    void seekRequested(qint64 positionMs);
};

} // namespace mr
