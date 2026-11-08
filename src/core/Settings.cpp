#include "core/Settings.h"

#include "core/AppPaths.h"
#include "core/Logger.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace mr {

Settings *Settings::s_instance = nullptr;

Settings::Settings(QObject *parent)
    : QObject(parent)
{
    m_downloadDir = AppPaths::defaultMusicDir();
    s_instance = this;
}

Settings *Settings::instance()
{
    return s_instance;
}

void Settings::load()
{
    m_loading = true;
    QFile file(AppPaths::configFile());
    if (file.open(QIODevice::ReadOnly)) {
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        m_playbackQuality = root.value(QStringLiteral("playbackQuality")).toInt(m_playbackQuality);
        m_downloadDir = root.value(QStringLiteral("downloadDir")).toString(m_downloadDir);
        m_plaintextStorage = root.value(QStringLiteral("plaintextStorage")).toBool(m_plaintextStorage);
        m_proxyEnabled = root.value(QStringLiteral("proxyEnabled")).toBool(m_proxyEnabled);
        m_proxyUrl = root.value(QStringLiteral("proxyUrl")).toString(m_proxyUrl);
        m_volume = root.value(QStringLiteral("volume")).toDouble(m_volume);
        m_playMode = root.value(QStringLiteral("playMode")).toInt(m_playMode);
        m_themeMode = root.value(QStringLiteral("themeMode")).toInt(m_themeMode);
        m_locale = root.value(QStringLiteral("locale")).toString(m_locale);
        m_closeAction = root.value(QStringLiteral("closeAction")).toInt(m_closeAction);
        file.close();
    }
    if (m_downloadDir.isEmpty())
        m_downloadDir = AppPaths::defaultMusicDir();
    m_loading = false;
    emit changed();
}

void Settings::save() const
{
    QJsonObject root;
    root[QStringLiteral("playbackQuality")] = m_playbackQuality;
    root[QStringLiteral("downloadDir")] = m_downloadDir;
    root[QStringLiteral("plaintextStorage")] = m_plaintextStorage;
    root[QStringLiteral("proxyEnabled")] = m_proxyEnabled;
    root[QStringLiteral("proxyUrl")] = m_proxyUrl;
    root[QStringLiteral("volume")] = m_volume;
    root[QStringLiteral("playMode")] = m_playMode;
    root[QStringLiteral("themeMode")] = m_themeMode;
    root[QStringLiteral("locale")] = m_locale;
    root[QStringLiteral("closeAction")] = m_closeAction;

    QSaveFile file(AppPaths::configFile());
    if (!file.open(QIODevice::WriteOnly)) {
        Logger::warn(QStringLiteral("cannot write settings: %1").arg(file.errorString()));
        return;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit())
        Logger::warn(QStringLiteral("cannot commit settings: %1").arg(file.errorString()));
}

QString Settings::qualityName(int quality)
{
    switch (quality) {
    case QualityStandard:
        return QObject::tr("标准");
    case QualityHigh:
        return QObject::tr("高品");
    case QualityLossless:
        return QObject::tr("无损");
    case QualityHiRes:
        return QObject::tr("Hi-Res");
    default:
        break;
    }
    return QObject::tr("未知");
}

QString Settings::qualityFileNameSuffix(int quality)
{
    switch (quality) {
    case QualityStandard:
        return QStringLiteral("128K");
    case QualityHigh:
        return QStringLiteral("320K");
    case QualityLossless:
        return QStringLiteral("FLAC");
    case QualityHiRes:
        return QStringLiteral("HiRes");
    default:
        break;
    }
    return QStringLiteral("unknown");
}

void Settings::setPlaybackQuality(int value)
{
    value = qBound(0, value, 3);
    if (m_playbackQuality == value)
        return;
    m_playbackQuality = value;
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setDownloadDir(const QString &value)
{
    if (m_downloadDir == value)
        return;
    m_downloadDir = value;
    emit downloadDirChanged();
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setPlaintextStorage(bool value)
{
    if (m_plaintextStorage == value)
        return;
    m_plaintextStorage = value;
    emit plaintextStorageChanged();
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setProxyEnabled(bool value)
{
    if (m_proxyEnabled == value)
        return;
    m_proxyEnabled = value;
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setProxyUrl(const QString &value)
{
    if (m_proxyUrl == value)
        return;
    m_proxyUrl = value;
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setVolume(double value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_volume, value))
        return;
    m_volume = value;
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setPlayMode(int value)
{
    value = qBound(0, value, 3);
    if (m_playMode == value)
        return;
    m_playMode = value;
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setThemeMode(int value)
{
    value = qBound(0, value, 2);
    if (m_themeMode == value)
        return;
    m_themeMode = value;
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setCloseAction(int value)
{
    value = qBound(0, value, 1);
    if (m_closeAction == value)
        return;
    m_closeAction = value;
    emit changed();
    if (!m_loading)
        save();
}

void Settings::setLocale(const QString &value)
{
    if (m_locale == value)
        return;
    m_locale = value;
    emit changed();
    if (!m_loading)
        save();
}

} // namespace mr
