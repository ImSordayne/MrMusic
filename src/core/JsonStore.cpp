#include "core/JsonStore.h"

#include "core/Crypto.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSaveFile>

namespace mr {

JsonStore::JsonStore(QString path)
    : m_path(std::move(path))
{
}

void JsonStore::setKeyMaterial(const QByteArray &key)
{
    m_key = Crypto::sha256(key);
}

void JsonStore::setPlaintext(bool plaintext)
{
    m_plaintext = plaintext;
}

QJsonObject JsonStore::load()
{
    m_corrupted = false;
    m_lastError.clear();

    QFile file(m_path);
    if (!file.exists())
        return {};
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastError = QStringLiteral("cannot open %1: %2").arg(m_path, file.errorString());
        m_corrupted = true;
        return {};
    }

    const QByteArray raw = file.readAll();
    file.close();
    if (raw.trimmed().isEmpty())
        return {};

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(raw, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        m_lastError = QStringLiteral("invalid json: %1").arg(parseError.errorString());
        m_corrupted = true;
        return {};
    }

    const QJsonObject root = document.object();
    if (!root.value(QStringLiteral("encrypted")).toBool(false))
        return root; // plain text storage

    const QByteArray iv = Crypto::fromBase64(root.value(QStringLiteral("iv")).toString());
    const QByteArray data = Crypto::fromBase64(root.value(QStringLiteral("data")).toString());
    const QByteArray mac = Crypto::fromBase64(root.value(QStringLiteral("mac")).toString());

    if (iv.size() != 16 || data.isEmpty() || mac.isEmpty()) {
        m_lastError = QStringLiteral("truncated encrypted payload");
        m_corrupted = true;
        return {};
    }

    QByteArray key = m_key;
    if (key.isEmpty())
        key = Crypto::sha256(QByteArrayLiteral("mrmusic-fallback-key"));

    const QByteArray expected = Crypto::hmacSha256(key, iv + data);
    if (expected != mac) {
        m_lastError = QStringLiteral("integrity check failed (key mismatch)");
        m_corrupted = true;
        return {};
    }

    const QByteArray plain = Crypto::aesCbcDecrypt(data, key, iv);
    QJsonParseError inner{};
    const QJsonDocument innerDocument = QJsonDocument::fromJson(plain, &inner);
    if (inner.error != QJsonParseError::NoError || !innerDocument.isObject()) {
        m_lastError = QStringLiteral("payload cannot be decrypted");
        m_corrupted = true;
        return {};
    }
    return innerDocument.object();
}

bool JsonStore::save(const QJsonObject &object)
{
    m_lastError.clear();

    QByteArray output;
    if (m_plaintext) {
        output = QJsonDocument(object).toJson(QJsonDocument::Indented);
    } else {
        QByteArray key = m_key;
        if (key.isEmpty())
            key = Crypto::sha256(QByteArrayLiteral("mrmusic-fallback-key"));

        const QByteArray iv = Crypto::randomBytes(16);
        const QByteArray payload = QJsonDocument(object).toJson(QJsonDocument::Compact);
        const QByteArray cipher = Crypto::aesCbcEncrypt(payload, key, iv);
        const QByteArray mac = Crypto::hmacSha256(key, iv + cipher);

        QJsonObject root;
        root[QStringLiteral("version")] = 1;
        root[QStringLiteral("encrypted")] = true;
        root[QStringLiteral("iv")] = Crypto::base64(iv);
        root[QStringLiteral("data")] = Crypto::base64(cipher);
        root[QStringLiteral("mac")] = Crypto::base64(mac);
        output = QJsonDocument(root).toJson(QJsonDocument::Indented);
    }

    QSaveFile file(m_path);
    if (!file.open(QIODevice::WriteOnly)) {
        m_lastError = QStringLiteral("cannot write %1: %2").arg(m_path, file.errorString());
        return false;
    }
    if (file.write(output) != output.size()) {
        m_lastError = QStringLiteral("short write to %1").arg(m_path);
        file.cancelWriting();
        return false;
    }
    // QSaveFile performs the atomic rename (temporary file + replace).
    if (!file.commit()) {
        m_lastError = QStringLiteral("cannot commit %1: %2").arg(m_path, file.errorString());
        return false;
    }
    return true;
}

} // namespace mr
