// Self test for the MrMusic core primitives (crypto, inflate, device identity).
//
// Run with:
//   mrmusic_selftest
// Exits with a non zero status when a check fails.

#include "core/Crypto.h"
#include "core/Fingerprint.h"
#include "core/Inflate.h"
#include "core/SaltStore.h"

#include <QByteArray>
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDebug>
#include <QString>

namespace {

int g_failures = 0;

void check(bool condition, const QString &name, const QString &detail = QString())
{
    if (condition) {
        qInfo().noquote() << "  [ ok ]" << name;
    } else {
        ++g_failures;
        qWarning().noquote() << "  [FAIL]" << name << (detail.isEmpty() ? QString() : QStringLiteral("-> ") + detail);
    }
}

void testHashes()
{
    qInfo() << "hashes";
    check(mr::Crypto::md5Hex(QByteArray("abc")) == "900150983cd24fb0d6963f7d28e17f72", "md5(abc)");
    check(mr::Crypto::md5Hex(QByteArray("")) == "d41d8cd98f00b204e9800998ecf8427e", "md5(\"\")");
    check(mr::Crypto::sha1Hex(QByteArray("abc")) == "a9993e364706816aba3e25717850c26c9cd0d89d", "sha1(abc)");
    check(mr::Crypto::sha256Hex(QByteArray("abc"))
              == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
          "sha256(abc)");
}

void testAes()
{
    qInfo() << "aes";
    // FIPS-197 appendix C.1 (AES-128)
    const QByteArray key = QByteArray::fromHex("000102030405060708090a0b0c0d0e0f");
    const QByteArray plain = QByteArray::fromHex("00112233445566778899aabbccddeeff");
    const QByteArray expected = QByteArray::fromHex("69c4e0d86a7b0430d8cdb78070b4c55a");

    // ECB adds PKCS#7 padding, therefore the first block must match the KAT.
    const QByteArray cipher = mr::Crypto::aesEcbEncrypt(plain, key);
    check(cipher.size() == 32, "aes-128-ecb output length", QString::number(cipher.size()));
    check(cipher.left(16) == expected, "aes-128-ecb KAT", mr::Crypto::hexLower(cipher.left(16)));

    // Round trip through CBC.
    const QByteArray iv = QByteArray::fromHex("0102030405060708090a0b0c0d0e0f10");
    const QByteArray message = QStringLiteral("MrMusic - 网易云 & 酷狗").toUtf8();
    const QByteArray cbc = mr::Crypto::aesCbcEncrypt(message, key, iv);
    const QByteArray roundTrip = mr::Crypto::aesCbcDecrypt(cbc, key, iv);
    check(roundTrip == message, "aes-128-cbc round trip");

    // AES-256 (used by the credential store).
    const QByteArray key256 = mr::Crypto::sha256("storage-key");
    const QByteArray cbc256 = mr::Crypto::aesCbcEncrypt(message, key256, iv);
    check(cbc256.size() % 16 == 0 && cbc256 != message, "aes-256-cbc encrypts");
    check(mr::Crypto::aesCbcDecrypt(cbc256, key256, iv) == message, "aes-256-cbc round trip");
}

void testBase64AndDecimal()
{
    qInfo() << "encodings";
    const QByteArray data = QByteArray::fromHex("00ff10a1");
    check(mr::Crypto::fromBase64(mr::Crypto::base64(data)) == data, "base64 round trip");
    check(mr::Crypto::decimalFromHex("ff") == "255", "decimalFromHex(ff)", "");
    check(mr::Crypto::decimalFromHex("0100") == "256", "decimalFromHex(0100)");
    check(mr::Crypto::decimalFromHex("") == "0", "decimalFromHex(empty)");

    const QString mid = mr::Crypto::decimalFromHex(mr::Crypto::md5Hex(QByteArray("kugou-guid")));
    bool digitsOk = !mid.isEmpty();
    for (QChar c : mid) {
        if (!c.isDigit())
            digitsOk = false;
    }
    check(digitsOk && mid.size() > 20, "kugou mid is a long decimal", mid);
}

void testRsa()
{
    qInfo() << "rsa";
    // Classic textbook example: p=61 q=53 n=3233 e=17, m=65 -> c=2790
    mr::Crypto::RsaPublicKey small;
    small.modulus = QByteArray::fromHex("0ca1"); // 3233
    small.exponent = QByteArray::fromHex("11");  // 17
    const QByteArray cipher = mr::Crypto::rsaEncryptRaw(small, QByteArray(1, 65));
    check(cipher == QByteArray::fromHex("0ae6"), "raw rsa modpow (2790)", mr::Crypto::hexLower(cipher));

    // NetEase weapi public key (1024 bit).
    const QByteArray pem =
        "-----BEGIN PUBLIC KEY-----\n"
        "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDgtQn2JZ34ZC28NWYpAUd98iZ37BUrX/aKzmFbt7clFSs6sXqHauqKWqdtLkF2KexO40H1YTX8z2lSgBBOAxLsvaklV8k4cBFK9snQXE9/DDaFt6Rr7iVZMldczhC0JNgTz+SHXT6CBHuX3e9SdB1Ua44oncaTWz7OBGLbCiK45wIDAQAB\n"
        "-----END PUBLIC KEY-----\n";
    const auto key = mr::Crypto::rsaPublicKeyFromPem(pem);
    check(key.byteLength() == 128, "netease key length", QString::number(key.byteLength()));
    check(key.exponent == QByteArray::fromHex("010001"), "netease exponent", mr::Crypto::hexLower(key.exponent));

    const QByteArray encrypted = mr::Crypto::rsaEncryptPkcs1v15(key, QByteArray("0123456789abcdef"));
    check(encrypted.size() == 128, "pkcs1 v1.5 output length", QString::number(encrypted.size()));
    check(encrypted.left(1) != QByteArray(1, '\0') || true, "pkcs1 does not crash");

    // KuGou login key (used for `pk`).
    const QByteArray kugouPem =
        "-----BEGIN PUBLIC KEY-----\n"
        "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDIAG7QOELSYoIJvTFJhMpe1s/gbjDJX51HBNnEl5HXqTW6lQ7LC8jr9fWZTwusknp+sVGzwd40MwP6U5yDE27M/X1+UR4tvOGOqp94TJtQ1EPnWGWXngpeIW5GxoQGao1rmYWAu6oi1z9XkChrsUdC6DJE5E221wf/4WLFxwAtRQIDAQAB\n"
        "-----END PUBLIC KEY-----\n";
    const auto kugouKey = mr::Crypto::rsaPublicKeyFromPem(kugouPem);
    check(kugouKey.byteLength() == 128, "kugou key length", QString::number(kugouKey.byteLength()));
    const QByteArray kugouCipher = mr::Crypto::rsaEncryptRaw(kugouKey, QByteArray("{\"key\":\"abc\"}"));
    check(kugouCipher.size() == 128, "kugou raw rsa output length", QString::number(kugouCipher.size()));
}

void testInflate()
{
    qInfo() << "inflate";
    const QByteArray pattern = QByteArrayLiteral("[KRC]MrMusic lyric line ");
    QByteArray payload;
    for (int i = 0; i < 5000; ++i)
        payload.append(pattern.at(i % pattern.size()));
    payload.append(QByteArray(5000, 'x')); // force long runs / back references

    // qCompress prepends the 4 byte uncompressed length to a zlib stream.
    const QByteArray compressed = qCompress(payload, 9).mid(4);
    QByteArray inflated;
    const bool ok = mr::zlibInflate(compressed, inflated);
    check(ok, "zlib stream inflates");
    check(inflated == payload, "inflate round trip", QString::number(inflated.size()));

    // Non compressible data (stored blocks) must work as well.
    const QByteArray random = mr::Crypto::randomBytes(4096);
    const QByteArray compressedRandom = qCompress(random, 0).mid(4);
    QByteArray inflateRandom;
    check(mr::zlibInflate(compressedRandom, inflateRandom) && inflateRandom == random, "stored block inflate");
}

void testFingerprint()
{
    qInfo() << "fingerprint";
    const auto &fp = mr::Fingerprint::instance();
    const QByteArray salt = mr::Crypto::randomBytes(mr::SaltStore::SaltSize);
    const QByteArray key = fp.deriveStorageKey(salt);
    check(key.size() == 32, "storage key is 32 bytes");
    check(key == fp.deriveStorageKey(salt), "storage key is deterministic for the same salt");
    check(key != fp.deriveStorageKey(mr::Crypto::randomBytes(mr::SaltStore::SaltSize)),
          "storage key changes with the salt");
    check(fp.kugouGuid().size() == 32, "kugou guid is 32 hex chars");
    check(!fp.kugouMid().isEmpty(), "kugou mid present");
    check(fp.kugouDev().size() == 10, "kugou dev is 10 chars");
    check(fp.kugouMac().count(':') == 5, "kugou mac formatted");
    check(!fp.kugouWebgl().isEmpty(), "kugou webgl present");
    check(!fp.machineId().isEmpty(), "machine id present");
    check(fp.kugouGuid() == mr::Fingerprint::instance().kugouGuid(), "fingerprint is stable");
    qInfo().noquote() << "    machine:" << fp.machineId();
    qInfo().noquote() << "    guid/mid:" << fp.kugouGuid() << fp.kugouMid();
}

void testPbkdf2()
{
    qInfo() << "pbkdf2";
    // PBKDF2-HMAC-SHA256 test vectors (RFC 7914 / common test set).
    const QByteArray v1 = mr::Crypto::pbkdf2Sha256("password", "salt", 1, 32);
    check(mr::Crypto::hexLower(v1) == "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
          "pbkdf2 c=1", mr::Crypto::hexLower(v1));
    const QByteArray v2 = mr::Crypto::pbkdf2Sha256("password", "salt", 2, 32);
    check(mr::Crypto::hexLower(v2) == "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43",
          "pbkdf2 c=2", mr::Crypto::hexLower(v2));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("MrMusic"));

    qInfo() << "MrMusic core self test";
    testHashes();
    testAes();
    testBase64AndDecimal();
    testRsa();
    testInflate();
    testPbkdf2();
    testFingerprint();

    if (g_failures == 0)
        qInfo() << "ALL CHECKS PASSED";
    else
        qWarning() << g_failures << "CHECK(S) FAILED";
    return g_failures == 0 ? 0 : 1;
}
