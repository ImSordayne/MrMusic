#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QString>

namespace mr {

/// JSON document persisted on disk with optional encryption and atomic writes.
///
/// Layout of an encrypted file:
/// ```json
/// { "version": 1, "encrypted": true, "iv": "<b64>", "data": "<b64>", "mac": "<b64>" }
/// ```
/// `iv` is 16 random bytes, `data` is AES-256-CBC(keyMaterial, iv) over the
/// compact JSON of the payload and `mac` is HMAC-SHA256 over (iv || data).
///
/// When plain text storage is selected (TARGET.md §3.3) the payload is written
/// as a plain JSON object instead.
class JsonStore
{
public:
    explicit JsonStore(QString path);

    void setKeyMaterial(const QByteArray &key);
    void setPlaintext(bool plaintext);
    bool plaintext() const { return m_plaintext; }

    /// Reads the file. An empty result is returned when the file does not
    /// exist. When the payload is corrupted or the key does not match,
    /// `corrupted()` becomes true and the caller must ask the user to log in
    /// again (TARGET.md §3.5).
    QJsonObject load();

    bool save(const QJsonObject &object);

    bool corrupted() const { return m_corrupted; }
    QString lastError() const { return m_lastError; }
    QString path() const { return m_path; }

private:
    QString m_path;
    QByteArray m_key;
    bool m_plaintext = false;
    bool m_corrupted = false;
    QString m_lastError;
};

} // namespace mr
