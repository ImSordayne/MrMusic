#pragma once

#include <QString>

namespace mr {

/// Very small logging facade: writes to `<data>/logs/mrmusic.log` and to the
/// console. Qt's own message handler is redirected here as well.
class Logger
{
public:
    static void initialize();
    static void write(const QString &level, const QString &message);
    static void setDebugEnabled(bool enabled);

    static void info(const QString &message) { write(QStringLiteral("INFO"), message); }
    static void warn(const QString &message) { write(QStringLiteral("WARN"), message); }
    static void error(const QString &message) { write(QStringLiteral("ERROR"), message); }
    static void debug(const QString &message) { write(QStringLiteral("DEBUG"), message); }
};

} // namespace mr
