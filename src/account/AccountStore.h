#pragma once

#include "core/JsonStore.h"

#include <QObject>
#include <QJsonObject>

namespace mr {

/// Persists the (encrypted) login state of both platforms in
/// `<data>/accounts.json` (TARGET.md §3).
///
/// The key is derived from the machine fingerprint and a per installation salt
/// (`Fingerprint::deriveStorageKey`), unless the user opts into plain text
/// storage in the settings.
class AccountStore : public QObject
{
    Q_OBJECT

public:
    explicit AccountStore(QObject *parent = nullptr);

    void load();
    bool save();

    QJsonObject netease() const;
    void setNetease(const QJsonObject &value);

    QJsonObject kugou() const;
    void setKugou(const QJsonObject &value);

    QString anonymousToken() const;
    void setAnonymousToken(const QString &token);

    bool corrupted() const { return m_corrupted; }
    QString lastError() const { return m_store.lastError(); }

    /// Switches between encrypted (default) and plain text storage.
    void setPlaintext(bool plaintext);
    bool plaintext() const { return m_store.plaintext(); }

    /// Deletes the stored credentials (used when the file cannot be decrypted).
    void reset();

signals:
    void corruptedChanged();

private:
    JsonStore m_store;
    QJsonObject m_root;
    bool m_corrupted = false;
};

} // namespace mr
