#pragma once

#include <QString>

namespace mr {

/// Centralised file locations.
///
/// Per TARGET.md §5.2 the Windows (and Linux) build keeps all application data
/// in `<executable dir>/data`. macOS and Android use the platform standard
/// application data location.
class AppPaths
{
public:
    /// Resolves and creates the directories. Must be called once at startup.
    static void initialize();

    static QString dataDir();
    static QString cacheDir();
    static QString logDir();
    static QString coverCacheDir();
    static QString audioCacheDir();
    static QString defaultMusicDir();

    static QString configFile();
    static QString libraryFile();
    static QString accountsFile();
    static QString logFile();

    /// True when data is stored next to the executable (Windows / Linux).
    static bool isPortableLayout();

private:
    static QString s_dataDir;
};

} // namespace mr
