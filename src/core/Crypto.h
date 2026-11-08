#pragma once

#include <QByteArray>
#include <QString>

namespace mr {

/// Self contained cryptographic helpers.
///
/// Everything the upstream (Node.js) projects do with `crypto-js` / `node-forge`
/// is re-implemented here so that MrMusic never has to start a helper service
/// and never depends on OpenSSL being present on the target machine.
class Crypto
{
public:
    // ----- hashes / encodings -------------------------------------------
    static QByteArray md5(const QByteArray &data);
    static QString md5Hex(const QByteArray &data);
    static QString sha1Hex(const QByteArray &data);
    static QByteArray sha256(const QByteArray &data);
    static QString sha256Hex(const QByteArray &data);
    static QByteArray hmacSha256(const QByteArray &key, const QByteArray &message);
    /// PBKDF2-HMAC-SHA256, used to stretch the salted machine fingerprint into
    /// the credential storage key.
    static QByteArray pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations,
                                   int keyLength);

    static QString hexUpper(const QByteArray &data);
    static QString hexLower(const QByteArray &data);
    static QByteArray fromHex(const QString &hex);
    static QString base64(const QByteArray &data);
    static QByteArray fromBase64(const QString &text);
    /// Converts a hexadecimal string into its decimal representation
    /// (used by the KuGou "mid" device identifier).
    static QString decimalFromHex(const QString &hex);
    /// Converts a 64 bit unsigned value given as raw bytes into a decimal string.
    static QString decimalFromBytes(const QByteArray &bytes);

    static QByteArray randomBytes(int count);
    static QString randomBase62(int count);

    // ----- AES (CBC / ECB, PKCS#7) --------------------------------------
    static QByteArray aesCbcEncrypt(const QByteArray &plain, const QByteArray &key, const QByteArray &iv);
    static QByteArray aesCbcDecrypt(const QByteArray &cipher, const QByteArray &key, const QByteArray &iv);
    static QByteArray aesEcbEncrypt(const QByteArray &plain, const QByteArray &key);
    static QByteArray aesEcbDecrypt(const QByteArray &cipher, const QByteArray &key);

    // ----- RSA -----------------------------------------------------------
    struct RsaPublicKey
    {
        QByteArray modulus;   // big endian, no leading zeroes
        QByteArray exponent;  // big endian, no leading zeroes
        int byteLength() const { return modulus.size(); }
    };

    static RsaPublicKey rsaPublicKeyFromPem(const QByteArray &pem);
    /// RSAES-PKCS1-v1_5 encryption, returns `byteLength()` bytes (big endian).
    static QByteArray rsaEncryptPkcs1v15(const RsaPublicKey &key, const QByteArray &data);
    /// Raw ("textbook") RSA: the message is left padded with zeroes and exponentiated.
    static QByteArray rsaEncryptRaw(const RsaPublicKey &key, const QByteArray &data);
    static QByteArray rsaEncryptPkcs1v15Pem(const QByteArray &pem, const QByteArray &data);
    static QByteArray rsaEncryptRawPem(const QByteArray &pem, const QByteArray &data);

    // ----- misc ----------------------------------------------------------
    /// zlib decompression (used for KuGou KRC lyrics). Returns an empty array
    /// when the payload cannot be inflated.
    static QByteArray zlibInflate(const QByteArray &data);
};

} // namespace mr
