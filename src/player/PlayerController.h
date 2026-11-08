#pragma once

#include "api/ApiTypes.h"
#include "ui/TrackListModel.h"

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>

class QWindow;

namespace mr {

class AudioEngine;
class KugouClient;
class MediaControls;
class NeteaseClient;
class Settings;
class TrackListModel;

/// Playback queue + playback state, driven by QML.
class PlayerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(mr::TrackListModel *queue READ queue CONSTANT)
    Q_PROPERTY(QString title READ title NOTIFY currentChanged)
    Q_PROPERTY(QString artists READ artists NOTIFY currentChanged)
    Q_PROPERTY(QString album READ album NOTIFY currentChanged)
    Q_PROPERTY(QString coverUrl READ coverUrl NOTIFY currentChanged)
    Q_PROPERTY(QString sourceName READ sourceName NOTIFY currentChanged)
    Q_PROPERTY(QString subtitle READ subtitle NOTIFY currentChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool loading READ loading NOTIFY loadingChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(QString positionText READ positionText NOTIFY positionChanged)
    Q_PROPERTY(QString durationText READ durationText NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(int playMode READ playMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)
    Q_PROPERTY(bool hasTrack READ hasTrack NOTIFY currentChanged)
    Q_PROPERTY(QString currentKey READ currentKey NOTIFY currentChanged)
    Q_PROPERTY(bool videoMode READ videoMode NOTIFY videoModeChanged)
    Q_PROPERTY(bool videoAvailable READ videoAvailable NOTIFY videoAvailableChanged)

public:
    PlayerController(NeteaseClient *netease, KugouClient *kugou, Settings *settings,
                     QObject *parent = nullptr);
    ~PlayerController() override;

    TrackListModel *queue() const { return m_queue; }

    QString title() const { return m_current.title; }
    QString artists() const { return m_current.artists; }
    QString album() const { return m_current.album; }
    QString coverUrl() const { return m_current.coverUrl; }
    QString sourceName() const { return m_current.sourceName(); }
    QString subtitle() const { return m_current.subtitle(); }
    bool playing() const { return m_playing; }
    bool loading() const { return m_loading; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    QString positionText() const { return formatTime(m_position); }
    QString durationText() const { return formatTime(m_duration); }
    double volume() const { return m_volume; }
    int playMode() const { return m_playMode; }
    QString statusMessage() const { return m_statusMessage; }
    bool hasTrack() const { return m_current.isValid(); }
    QString currentKey() const { return m_current.isValid() ? trackKey(m_current) : QString(); }

    const Track &currentTrack() const { return m_current; }
    bool videoMode() const { return m_videoMode; }
    bool videoAvailable() const { return m_videoAvailable; }

public slots:
    void attachWindow(QWindow *window);
    QVariantMap currentTrackMap() const;
    void setFavoriteKeys(const QStringList &keys);
    void playTracks(const QVariantList &tracks, int startIndex = 0);
    void playTrack(const QVariantMap &track);
    /// Plays a music video straight in the player (TARGET.md §10.22).
    void playMv(const QVariantMap &mv);
    void attachVideoOutput(QObject *output);
    void playAt(int row);
    void enqueue(const QVariantMap &track);
    void enqueueNext(const QVariantMap &track);
    void togglePlay();
    void play();
    void pause();
    void next();
    void previous();
    void seek(int positionMs);
    void setVolume(double volume);
    void setPlayMode(int mode);
    void cyclePlayMode();
    void clearQueue();
    void removeFromQueue(int row);
    void clearStatusMessage();

signals:
    void currentChanged();
    void playingChanged();
    void loadingChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void playModeChanged();
    void statusMessageChanged();
    void videoModeChanged();
    void videoAvailableChanged();
    void trackStarted(const QVariantMap &track);
    void requestDownload(const QVariantMap &track);

private:
    void setStatus(const QString &message);
    void clearVideoMode();
    void setLoading(bool loading);
    void loadCurrentAndPlay();
    void handleFinished();
    int nextRow(int direction) const;
    static QString formatTime(qint64 milliseconds);

    NeteaseClient *m_netease = nullptr;
    KugouClient *m_kugou = nullptr;
    Settings *m_settings = nullptr;
    AudioEngine *m_engine = nullptr;
    MediaControls *m_mediaControls = nullptr;
    TrackListModel *m_queue = nullptr;

    Track m_current;
    int m_currentRow = -1;
    bool m_playing = false;
    bool m_loading = false;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    double m_volume = 0.8;
    int m_playMode = 1;
    QString m_statusMessage;
    bool m_videoMode = false;
    bool m_videoAvailable = false;
};

} // namespace mr
