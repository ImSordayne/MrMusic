#pragma once

#include <QByteArray>
#include <QString>

namespace mr {

/// Per installation random salt, stored next to the other application data.
///
/// The credential store key is `PBKDF2(fingerprint, salt)`: the fingerprint
/// makes the key machine bound (the user can never type it in) while the salt
/// keeps pre-computed tables useless and makes two installations of the same
/// binary on the same machine produce different keys.
class SaltStore
{
public:
    /// Returns the salt for `path`, creating it on first use.
    static QByteArray loadOrCreate(const QString &path = QString());

    /// Default location: `<data>/.salt`.
    static QString defaultPath();

    static constexpr int SaltSize = 32;
};

} // namespace mr
