#include "account/AccountStore.h"

#include "core/AppPaths.h"
#include "core/Fingerprint.h"
#include "core/Logger.h"
#include "core/SaltStore.h"

#include <QFile>

namespace mr {

AccountStore::AccountStore(QObject *parent)
    : QObject(parent)
    , m_store(AppPaths::accountsFile())
{
    const QByteArray salt = SaltStore::loadOrCreate();
    m_store.setKeyMaterial(Fingerprint::instance().deriveStorageKey(salt));
}

void AccountStore::load()
{
    m_root = m_store.load();
    const bool corrupted = m_store.corrupted();
    if (corrupted) {
        Logger::warn(QStringLiteral("credential store unusable: %1").arg(m_store.lastError()));
        m_root = QJsonObject();
    }
    if (m_corrupted != corrupted) {
        m_corrupted = corrupted;
        emit corruptedChanged();
    }
}

bool AccountStore::save()
{
    if (m_root.isEmpty())
        return true;
    const bool ok = m_store.save(m_root);
    if (!ok)
        Logger::warn(QStringLiteral("cannot persist credentials: %1").arg(m_store.lastError()));
    return ok;
}

QJsonObject AccountStore::netease() const
{
    return m_root.value(QStringLiteral("netease")).toObject();
}

void AccountStore::setNetease(const QJsonObject &value)
{
    m_root[QStringLiteral("netease")] = value;
}

QJsonObject AccountStore::kugou() const
{
    return m_root.value(QStringLiteral("kugou")).toObject();
}

void AccountStore::setKugou(const QJsonObject &value)
{
    m_root[QStringLiteral("kugou")] = value;
}

QString AccountStore::anonymousToken() const
{
    return m_root.value(QStringLiteral("neteaseAnonymousToken")).toString();
}

void AccountStore::setAnonymousToken(const QString &token)
{
    m_root[QStringLiteral("neteaseAnonymousToken")] = token;
}

void AccountStore::setPlaintext(bool plaintext)
{
    if (m_store.plaintext() == plaintext)
        return;
    m_store.setPlaintext(plaintext);
    save();
}

void AccountStore::reset()
{
    m_root = QJsonObject();
    QFile::remove(AppPaths::accountsFile());
    if (m_corrupted) {
        m_corrupted = false;
        emit corruptedChanged();
    }
}

} // namespace mr
