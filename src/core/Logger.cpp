#include "core/Logger.h"

#include "core/AppPaths.h"

#include <QDateTime>
#include <QFile>
#include <QMessageLogContext>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

namespace mr {
namespace {

QMutex g_mutex;
bool g_debugEnabled = false;
QString g_logPath;
bool g_initialized = false;

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    Q_UNUSED(context)
    QString level;
    switch (type) {
    case QtDebugMsg:
        if (!g_debugEnabled)
            return;
        level = QStringLiteral("DEBUG");
        break;
    case QtInfoMsg:
        level = QStringLiteral("INFO");
        break;
    case QtWarningMsg:
        level = QStringLiteral("WARN");
        break;
    case QtCriticalMsg:
    case QtFatalMsg:
        level = QStringLiteral("ERROR");
        break;
    }
    Logger::write(level, message);
}

} // namespace

void Logger::initialize()
{
    if (g_initialized)
        return;
    g_initialized = true;
    g_logPath = AppPaths::logFile();
    qInstallMessageHandler(messageHandler);
}

void Logger::setDebugEnabled(bool enabled)
{
    g_debugEnabled = enabled;
}

void Logger::write(const QString &level, const QString &message)
{
    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
    const QString line = QStringLiteral("%1 [%2] %3").arg(stamp, level, message);

    QMutexLocker locker(&g_mutex);

    QTextStream console(stdout);
    console << line << Qt::endl;

    const QString path = g_logPath.isEmpty() ? AppPaths::logFile() : g_logPath;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << line << Qt::endl;
        file.close();
    }
}

} // namespace mr
