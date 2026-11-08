#pragma once

#include <QObject>
#include <QString>

namespace mr {

/// User preferences (non sensitive data, stored as plain JSON).
class Settings : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int playbackQuality READ playbackQuality WRITE setPlaybackQuality NOTIFY changed)
    Q_PROPERTY(QString downloadDir READ downloadDir WRITE setDownloadDir NOTIFY changed)
    Q_PROPERTY(bool plaintextStorage READ plaintextStorage WRITE setPlaintextStorage NOTIFY changed)
    Q_PROPERTY(bool proxyEnabled READ proxyEnabled WRITE setProxyEnabled NOTIFY changed)
    Q_PROPERTY(QString proxyUrl READ proxyUrl WRITE setProxyUrl NOTIFY changed)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY changed)
    Q_PROPERTY(int playMode READ playMode WRITE setPlayMode NOTIFY changed)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY changed)
    Q_PROPERTY(QString locale READ locale WRITE setLocale NOTIFY changed)
    Q_PROPERTY(int closeAction READ closeAction WRITE setCloseAction NOTIFY changed)

public:
    enum Quality { QualityStandard = 0, QualityHigh = 1, QualityLossless = 2, QualityHiRes = 3 };
    Q_ENUM(Quality)

    enum PlayMode { Sequential = 0, RepeatAll = 1, RepeatOne = 2, Shuffle = 3 };
    Q_ENUM(PlayMode)

    /// What happens when the window is closed (TARGET.md §9.7).
    enum CloseAction { QuitOnClose = 0, MinimizeToTray = 1 };
    Q_ENUM(CloseAction)

    explicit Settings(QObject *parent = nullptr);

    static Settings *instance();

    void load();
    void save() const;

    int playbackQuality() const { return m_playbackQuality; }
    QString downloadDir() const { return m_downloadDir; }
    bool plaintextStorage() const { return m_plaintextStorage; }
    bool proxyEnabled() const { return m_proxyEnabled; }
    QString proxyUrl() const { return m_proxyUrl; }
    double volume() const { return m_volume; }
    int playMode() const { return m_playMode; }
    int themeMode() const { return m_themeMode; }
    QString locale() const { return m_locale; }
    int closeAction() const { return m_closeAction; }

    static QString qualityName(int quality);
    static QString qualityFileNameSuffix(int quality);

public slots:
    void setPlaybackQuality(int value);
    void setDownloadDir(const QString &value);
    void setPlaintextStorage(bool value);
    void setProxyEnabled(bool value);
    void setProxyUrl(const QString &value);
    void setVolume(double value);
    void setPlayMode(int value);
    void setThemeMode(int value);
    void setLocale(const QString &value);
    void setCloseAction(int value);

signals:
    void changed();
    void plaintextStorageChanged();
    void downloadDirChanged();

private:
    static Settings *s_instance;

    int m_playbackQuality = QualityLossless;
    QString m_downloadDir;
    bool m_plaintextStorage = false;
    bool m_proxyEnabled = false;
    QString m_proxyUrl;
    double m_volume = 0.8;
    int m_playMode = RepeatAll;
    int m_themeMode = 0;
    QString m_locale = QStringLiteral("zh_CN");
    int m_closeAction = MinimizeToTray;
    bool m_loading = false;
};

} // namespace mr
