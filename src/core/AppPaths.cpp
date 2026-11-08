#include "core/AppPaths.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

namespace mr {

QString AppPaths::s_dataDir;

void AppPaths::initialize()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    s_dataDir = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath(QStringLiteral("data"));
#else
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::home().absoluteFilePath(QStringLiteral(".mrmusic"));
    s_dataDir = base;
#endif

    QDir().mkpath(s_dataDir);
    QDir().mkpath(cacheDir());
    QDir().mkpath(logDir());
    QDir().mkpath(coverCacheDir());
    QDir().mkpath(audioCacheDir());
}

bool AppPaths::isPortableLayout()
{
#if defined(Q_OS_WIN) || defined(Q_OS_LINUX)
    return true;
#else
    return false;
#endif
}

QString AppPaths::dataDir()
{
    if (s_dataDir.isEmpty())
        initialize();
    return s_dataDir;
}

QString AppPaths::cacheDir()
{
    return QDir(dataDir()).absoluteFilePath(QStringLiteral("cache"));
}

QString AppPaths::logDir()
{
    return QDir(dataDir()).absoluteFilePath(QStringLiteral("logs"));
}

QString AppPaths::coverCacheDir()
{
    return QDir(cacheDir()).absoluteFilePath(QStringLiteral("covers"));
}

QString AppPaths::audioCacheDir()
{
    return QDir(cacheDir()).absoluteFilePath(QStringLiteral("audio"));
}

QString AppPaths::defaultMusicDir()
{
    QString music = QStandardPaths::writableLocation(QStandardPaths::MusicLocation);
    if (music.isEmpty())
        music = QDir::home().absoluteFilePath(QStringLiteral("Music"));
    return music;
}

QString AppPaths::configFile()
{
    return QDir(dataDir()).absoluteFilePath(QStringLiteral("config.json"));
}

QString AppPaths::libraryFile()
{
    return QDir(dataDir()).absoluteFilePath(QStringLiteral("library.json"));
}

QString AppPaths::accountsFile()
{
    return QDir(dataDir()).absoluteFilePath(QStringLiteral("accounts.json"));
}

QString AppPaths::logFile()
{
    return QDir(logDir()).absoluteFilePath(QStringLiteral("mrmusic.log"));
}

} // namespace mr
