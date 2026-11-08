#include "core/SaltStore.h"

#include "core/AppPaths.h"
#include "core/Crypto.h"
#include "core/Logger.h"

#include <QDir>
#include <QFile>
#include <QSaveFile>

namespace mr {

QString SaltStore::defaultPath()
{
    return QDir(AppPaths::dataDir()).absoluteFilePath(QStringLiteral(".salt"));
}

QByteArray SaltStore::loadOrCreate(const QString &path)
{
    const QString file = path.isEmpty() ? defaultPath() : path;

    QFile existing(file);
    if (existing.exists() && existing.open(QIODevice::ReadOnly)) {
        const QByteArray salt = existing.readAll();
        existing.close();
        if (salt.size() >= SaltSize)
            return salt.left(SaltSize);
        Logger::warn(QStringLiteral("salt file is too short, regenerating"));
    }

    const QByteArray salt = Crypto::randomBytes(SaltSize);
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly)) {
        Logger::warn(QStringLiteral("cannot create salt file %1: %2").arg(file, out.errorString()));
        return QByteArray();
    }
    out.write(salt);
    if (!out.commit()) {
        Logger::warn(QStringLiteral("cannot commit salt file %1: %2").arg(file, out.errorString()));
        return QByteArray();
    }
    return salt;
}

} // namespace mr
