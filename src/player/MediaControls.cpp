#include "player/MediaControls.h"

#include "core/Logger.h"

#if defined(Q_OS_WIN) && !defined(MRMUSIC_NO_WINRT)
#  include <windows.h>

#  include <winrt/Windows.Foundation.h>
#  include <winrt/Windows.Media.h>
#  include <winrt/base.h>

#  include <SystemMediaTransportControlsInterop.h>

#  include <QWindow>
#endif

#if defined(MRMUSIC_HAVE_DBUS)
#  include <QDBusConnection>
#  include <QDBusMessage>
#endif

#include <QTimer>

#if defined(Q_OS_ANDROID)
#  include <QCoreApplication>
#  include <QJniObject>
#  include <QJniEnvironment>
#endif

namespace mr {

MediaControls::MediaControls(QObject *parent)
    : QObject(parent)
{
}

MediaControls::~MediaControls() = default;

void MediaControls::attachToWindow(QWindow *window)
{
    Q_UNUSED(window)
}

void MediaControls::setMetadata(const QString &title, const QString &artist, const QString &album,
                                const QUrl &coverUrl)
{
    Q_UNUSED(title)
    Q_UNUSED(artist)
    Q_UNUSED(album)
    Q_UNUSED(coverUrl)
}

void MediaControls::setPlaybackState(bool playing)
{
    Q_UNUSED(playing)
}

void MediaControls::setCanSkip(bool canGoNext, bool canGoPrevious)
{
    Q_UNUSED(canGoNext)
    Q_UNUSED(canGoPrevious)
}

void MediaControls::setTimeline(qint64 positionMs, qint64 durationMs)
{
    Q_UNUSED(positionMs)
    Q_UNUSED(durationMs)
}

#if defined(Q_OS_WIN) && !defined(MRMUSIC_NO_WINRT)

// ---------------------------------------------------------------------------
// Windows: SystemMediaTransportControls
// ---------------------------------------------------------------------------
class WindowsMediaControls final : public MediaControls
{
public:
    explicit WindowsMediaControls(QObject *parent = nullptr)
        : MediaControls(parent)
    {
    }

    ~WindowsMediaControls() override
    {
        try {
            if (m_controls) {
                m_controls.IsEnabled(false);
                m_controls = nullptr;
            }
        } catch (...) {
        }
        if (m_apartmentInitialized) {
            try {
                winrt::uninit_apartment();
            } catch (...) {
            }
        }
    }

    bool isAvailable() const override { return m_available; }

    void attachToWindow(QWindow *window) override
    {
        if (m_available || !window)
            return;

        try {
            winrt::init_apartment(winrt::apartment_type::single_threaded);
            m_apartmentInitialized = true;
        } catch (...) {
            // COM may already be initialised in a different mode; that is fine.
        }

        try {
            const auto interop = winrt::get_activation_factory<winrt::Windows::Media::SystemMediaTransportControls,
                                                               ISystemMediaTransportControlsInterop>();
            const HRESULT hr =
                interop->GetForWindow(reinterpret_cast<HWND>(window->winId()),
                                      winrt::guid_of<winrt::Windows::Media::SystemMediaTransportControls>(),
                                      winrt::put_abi(m_controls));
            if (FAILED(hr) || !m_controls) {
                Logger::warn(QStringLiteral("SMTC: GetForWindow failed (0x%1)").arg(quint32(hr), 8, 16, QLatin1Char('0')));
                return;
            }

            m_controls.IsEnabled(true);
            m_controls.IsPlayEnabled(true);
            m_controls.IsPauseEnabled(true);
            m_controls.IsStopEnabled(true);
            m_controls.IsNextEnabled(true);
            m_controls.IsPreviousEnabled(true);
            m_controls.PlaybackStatus(winrt::Windows::Media::MediaPlaybackStatus::Closed);

            m_controls.ButtonPressed([this](const winrt::Windows::Media::SystemMediaTransportControls &,
                                            const winrt::Windows::Media::SystemMediaTransportControlsButtonPressedEventArgs &args) {
                const auto button = args.Button();
                QMetaObject::invokeMethod(this, [this, button]() {
                    using Button = winrt::Windows::Media::SystemMediaTransportControlsButton;
                    switch (button) {
                    case Button::Play:
                    case Button::Pause:
                        emit togglePlayRequested();
                        break;
                    case Button::Stop:
                        emit stopRequested();
                        break;
                    case Button::Next:
                        emit nextRequested();
                        break;
                    case Button::Previous:
                        emit previousRequested();
                        break;
                    default:
                        break;
                    }
                }, Qt::QueuedConnection);
            });

            m_available = true;
            Logger::info(QStringLiteral("SMTC initialised"));
            applyMetadata();
            applyState();
        } catch (const winrt::hresult_error &error) {
            Logger::warn(QStringLiteral("SMTC unavailable: %1").arg(QString::fromWCharArray(error.message().c_str())));
        } catch (...) {
            Logger::warn(QStringLiteral("SMTC unavailable: unknown error"));
        }
    }

    void setMetadata(const QString &title, const QString &artist, const QString &album,
                     const QUrl &coverUrl) override
    {
        m_title = title;
        m_artist = artist;
        m_album = album;
        m_cover = coverUrl;
        applyMetadata();
    }

    void setPlaybackState(bool playing) override
    {
        m_playing = playing;
        applyState();
    }

    void setCanSkip(bool canGoNext, bool canGoPrevious) override
    {
        m_canNext = canGoNext;
        m_canPrevious = canGoPrevious;
        if (!m_available)
            return;
        try {
            m_controls.IsNextEnabled(m_canNext);
            m_controls.IsPreviousEnabled(m_canPrevious);
        } catch (...) {
        }
    }

private:
    void applyMetadata()
    {
        if (!m_available)
            return;
        try {
            auto updater = m_controls.DisplayUpdater();
            updater.Type(winrt::Windows::Media::MediaPlaybackType::Music);
            auto music = updater.MusicProperties();
            music.Title(winrt::hstring(m_title.toStdWString()));
            music.Artist(winrt::hstring(m_artist.toStdWString()));
            music.AlbumTitle(winrt::hstring(m_album.toStdWString()));
            updater.Update();
        } catch (...) {
        }
    }

    void applyState()
    {
        if (!m_available)
            return;
        try {
            using winrt::Windows::Media::MediaPlaybackStatus;
            m_controls.PlaybackStatus(m_playing ? MediaPlaybackStatus::Playing : MediaPlaybackStatus::Paused);
        } catch (...) {
        }
    }

    winrt::Windows::Media::SystemMediaTransportControls m_controls{nullptr};
    bool m_available = false;
    bool m_apartmentInitialized = false;
    bool m_playing = false;
    bool m_canNext = true;
    bool m_canPrevious = true;
    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_cover;
};

#elif defined(MRMUSIC_HAVE_DBUS)

// ---------------------------------------------------------------------------
// Linux: MPRIS2 (`org.mpris.MediaPlayer2.MrMusic`)
// ---------------------------------------------------------------------------
class MprisMediaControls final : public MediaControls
{
public:
    explicit MprisMediaControls(QObject *parent = nullptr)
        : MediaControls(parent)
    {
        QDBusConnection bus = QDBusConnection::sessionBus();
        if (!bus.isConnected()) {
            Logger::warn(QStringLiteral("MPRIS: no session bus"));
            return;
        }
        bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this,
                           QDBusConnection::ExportAllSlots);
        QDBusMessage reply = bus.call(QDBusMessage::createMethodCall(
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("/org/freedesktop/DBus"),
            QStringLiteral("org.freedesktop.DBus"), QStringLiteral("RequestName")),
            QDBus::Block, 2000);
        m_available = true;
        Logger::info(QStringLiteral("MPRIS object exported"));
    }

    bool isAvailable() const override { return m_available; }

    void setMetadata(const QString &title, const QString &artist, const QString &album,
                     const QUrl &coverUrl) override
    {
        m_title = title;
        m_artist = artist;
        m_album = album;
        m_cover = coverUrl;
    }

    void setPlaybackState(bool playing) override
    {
        m_playing = playing;
    }

    // NOTE: this class deliberately has no Q_OBJECT (it lives in a .cpp), so
    // it cannot declare Qt signals. D-Bus PropertiesChanged notifications are
    // not emitted by this skeleton implementation (see README §7).

private:
    bool m_available = false;
    bool m_playing = false;
    QString m_title;
    QString m_artist;
    QString m_album;
    QUrl m_cover;
};

#elif defined(Q_OS_ANDROID)

// ---------------------------------------------------------------------------
// Android: MediaSession + media style notification
// (Java side: android/src/io/sordayne/github/mrmusic/MediaSessionHelper.java)
// ---------------------------------------------------------------------------
class AndroidMediaControls final : public MediaControls
{
public:
    explicit AndroidMediaControls(QObject *parent = nullptr)
        : MediaControls(parent)
        , m_pollTimer(new QTimer(this))
    {
        QJniObject context = QNativeInterface::QAndroidApplication::context();
        if (!context.isValid()) {
            Logger::warn(QStringLiteral("Android media session: no android context"));
            return;
        }

        m_helper = QJniObject("io/sordayne/github/mrmusic/MediaSessionHelper",
                              "(Landroid/content/Context;)V", context.object());
        m_available = m_helper.isValid();
        if (!m_available) {
            Logger::warn(QStringLiteral("Android media session helper unavailable"));
            return;
        }

        m_pollTimer->setInterval(250);
        connect(m_pollTimer, &QTimer::timeout, this, [this]() { drainActions(); });
        m_pollTimer->start();
        Logger::info(QStringLiteral("Android media session initialised"));
    }

    ~AndroidMediaControls() override
    {
        if (m_available)
            m_helper.callMethod<void>("release");
    }

    bool isAvailable() const override { return m_available; }

    void setMetadata(const QString &title, const QString &artist, const QString &album,
                     const QUrl &coverUrl) override
    {
        Q_UNUSED(coverUrl)
        if (!m_available)
            return;
        m_helper.callMethod<void>("updateMetadata",
                                  "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V",
                                  QJniObject::fromString(title).object(),
                                  QJniObject::fromString(artist).object(),
                                  QJniObject::fromString(album).object());
    }

    void setPlaybackState(bool playing) override
    {
        if (!m_available)
            return;
        m_helper.callMethod<void>("setPlaybackState", "(Z)V", playing);
    }

private:
    void drainActions()
    {
        if (!m_available)
            return;
        for (;;) {
            const int action = QJniObject::callStaticMethod<jint>("io/sordayne/github/mrmusic/MediaSessionHelper",
                                                                  "pollAction", "()I");
            if (action < 0)
                break;
            switch (action) {
            case 0:
                emit togglePlayRequested();
                break;
            case 1:
                emit nextRequested();
                break;
            case 2:
                emit previousRequested();
                break;
            case 3:
                emit stopRequested();
                break;
            default:
                break;
            }
        }
    }

    QJniObject m_helper;
    QTimer *m_pollTimer = nullptr;
    bool m_available = false;
};

#else

class NullMediaControls final : public MediaControls
{
public:
    explicit NullMediaControls(QObject *parent = nullptr)
        : MediaControls(parent)
    {
        Logger::info(QStringLiteral("native media controls not implemented for this platform"));
    }
};

#endif

MediaControls *MediaControls::create(QObject *parent)
{
#if defined(Q_OS_WIN) && !defined(MRMUSIC_NO_WINRT)
    return new WindowsMediaControls(parent);
#elif defined(MRMUSIC_HAVE_DBUS)
    return new MprisMediaControls(parent);
#elif defined(Q_OS_ANDROID)
    return new AndroidMediaControls(parent);
#else
    return new NullMediaControls(parent);
#endif
}

} // namespace mr
