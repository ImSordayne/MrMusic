#include "core/Crypto.h"

#include "core/Inflate.h"

#include <QCryptographicHash>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QVector>

#include <cstring>

namespace mr {
namespace {

// ---------------------------------------------------------------------------
// AES (FIPS-197), byte oriented reference implementation.
// ---------------------------------------------------------------------------

const unsigned char kSbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76,
    0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0, 0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0,
    0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75,
    0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0, 0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84,
    0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8,
    0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5, 0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2,
    0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb,
    0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c, 0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79,
    0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a,
    0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e, 0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e,
    0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};

const unsigned char kRsbox[256] = {
    0x52, 0x09, 0x6a, 0xd5, 0x30, 0x36, 0xa5, 0x38, 0xbf, 0x40, 0xa3, 0x9e, 0x81, 0xf3, 0xd7, 0xfb,
    0x7c, 0xe3, 0x39, 0x82, 0x9b, 0x2f, 0xff, 0x87, 0x34, 0x8e, 0x43, 0x44, 0xc4, 0xde, 0xe9, 0xcb,
    0x54, 0x7b, 0x94, 0x32, 0xa6, 0xc2, 0x23, 0x3d, 0xee, 0x4c, 0x95, 0x0b, 0x42, 0xfa, 0xc3, 0x4e,
    0x08, 0x2e, 0xa1, 0x66, 0x28, 0xd9, 0x24, 0xb2, 0x76, 0x5b, 0xa2, 0x49, 0x6d, 0x8b, 0xd1, 0x25,
    0x72, 0xf8, 0xf6, 0x64, 0x86, 0x68, 0x98, 0x16, 0xd4, 0xa4, 0x5c, 0xcc, 0x5d, 0x65, 0xb6, 0x92,
    0x6c, 0x70, 0x48, 0x50, 0xfd, 0xed, 0xb9, 0xda, 0x5e, 0x15, 0x46, 0x57, 0xa7, 0x8d, 0x9d, 0x84,
    0x90, 0xd8, 0xab, 0x00, 0x8c, 0xbc, 0xd3, 0x0a, 0xf7, 0xe4, 0x58, 0x05, 0xb8, 0xb3, 0x45, 0x06,
    0xd0, 0x2c, 0x1e, 0x8f, 0xca, 0x3f, 0x0f, 0x02, 0xc1, 0xaf, 0xbd, 0x03, 0x01, 0x13, 0x8a, 0x6b,
    0x3a, 0x91, 0x11, 0x41, 0x4f, 0x67, 0xdc, 0xea, 0x97, 0xf2, 0xcf, 0xce, 0xf0, 0xb4, 0xe6, 0x73,
    0x96, 0xac, 0x74, 0x22, 0xe7, 0xad, 0x35, 0x85, 0xe2, 0xf9, 0x37, 0xe8, 0x1c, 0x75, 0xdf, 0x6e,
    0x47, 0xf1, 0x1a, 0x71, 0x1d, 0x29, 0xc5, 0x89, 0x6f, 0xb7, 0x62, 0x0e, 0xaa, 0x18, 0xbe, 0x1b,
    0xfc, 0x56, 0x3e, 0x4b, 0xc6, 0xd2, 0x79, 0x20, 0x9a, 0xdb, 0xc0, 0xfe, 0x78, 0xcd, 0x5a, 0xf4,
    0x1f, 0xdd, 0xa8, 0x33, 0x88, 0x07, 0xc7, 0x31, 0xb1, 0x12, 0x10, 0x59, 0x27, 0x80, 0xec, 0x5f,
    0x60, 0x51, 0x7f, 0xa9, 0x19, 0xb5, 0x4a, 0x0d, 0x2d, 0xe5, 0x7a, 0x9f, 0x93, 0xc9, 0x9c, 0xef,
    0xa0, 0xe0, 0x3b, 0x4d, 0xae, 0x2a, 0xf5, 0xb0, 0xc8, 0xeb, 0xbb, 0x3c, 0x83, 0x53, 0x99, 0x61,
    0x17, 0x2b, 0x04, 0x7e, 0xba, 0x77, 0xd6, 0x26, 0xe1, 0x69, 0x14, 0x63, 0x55, 0x21, 0x0c, 0x7d};

const unsigned char kRcon[11] = {0x00, 0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x1b, 0x36};

inline unsigned char xtime(unsigned char x)
{
    return static_cast<unsigned char>((x << 1) ^ (0x1b & static_cast<unsigned char>(-(x >> 7))));
}

inline unsigned char gmul(unsigned char a, unsigned char b)
{
    unsigned char result = 0;
    while (b) {
        if (b & 1)
            result ^= a;
        a = xtime(a);
        b >>= 1;
    }
    return result;
}

struct AesSchedule
{
    int rounds = 10;
    unsigned char roundKey[16 * 15] = {};
};

bool aesExpandKey(const QByteArray &key, AesSchedule &schedule)
{
    const int keyLen = key.size();
    if (keyLen != 16 && keyLen != 24 && keyLen != 32)
        return false;

    const int nk = keyLen / 4;
    const int nr = nk + 6;
    schedule.rounds = nr;

    const int totalWords = 4 * (nr + 1);
    unsigned char w[240] = {}; // 60 words * 4 bytes (AES-256 upper bound)
    std::memcpy(w, key.constData(), static_cast<size_t>(keyLen));

    unsigned char temp[4];
    for (int i = nk; i < totalWords; ++i) {
        temp[0] = w[(i - 1) * 4 + 0];
        temp[1] = w[(i - 1) * 4 + 1];
        temp[2] = w[(i - 1) * 4 + 2];
        temp[3] = w[(i - 1) * 4 + 3];
        if (i % nk == 0) {
            // RotWord + SubWord + Rcon
            const unsigned char t = temp[0];
            temp[0] = static_cast<unsigned char>(kSbox[temp[1]] ^ kRcon[i / nk]);
            temp[1] = kSbox[temp[2]];
            temp[2] = kSbox[temp[3]];
            temp[3] = kSbox[t];
        } else if (nk > 6 && i % nk == 4) {
            temp[0] = kSbox[temp[0]];
            temp[1] = kSbox[temp[1]];
            temp[2] = kSbox[temp[2]];
            temp[3] = kSbox[temp[3]];
        }
        w[i * 4 + 0] = static_cast<unsigned char>(w[(i - nk) * 4 + 0] ^ temp[0]);
        w[i * 4 + 1] = static_cast<unsigned char>(w[(i - nk) * 4 + 1] ^ temp[1]);
        w[i * 4 + 2] = static_cast<unsigned char>(w[(i - nk) * 4 + 2] ^ temp[2]);
        w[i * 4 + 3] = static_cast<unsigned char>(w[(i - nk) * 4 + 3] ^ temp[3]);
    }

    std::memcpy(schedule.roundKey, w, static_cast<size_t>(16 * (nr + 1)));
    return true;
}

inline void addRoundKey(unsigned char *state, const unsigned char *roundKey)
{
    for (int i = 0; i < 16; ++i)
        state[i] ^= roundKey[i];
}

inline void subBytes(unsigned char *state)
{
    for (int i = 0; i < 16; ++i)
        state[i] = kSbox[state[i]];
}

inline void invSubBytes(unsigned char *state)
{
    for (int i = 0; i < 16; ++i)
        state[i] = kRsbox[state[i]];
}

// state index = row + 4 * column
inline void shiftRows(unsigned char *state)
{
    unsigned char tmp[16];
    std::memcpy(tmp, state, 16);
    for (int row = 1; row < 4; ++row) {
        for (int col = 0; col < 4; ++col)
            state[row + 4 * col] = tmp[row + 4 * ((col + row) % 4)];
    }
}

inline void invShiftRows(unsigned char *state)
{
    unsigned char tmp[16];
    std::memcpy(tmp, state, 16);
    for (int row = 1; row < 4; ++row) {
        for (int col = 0; col < 4; ++col)
            state[row + 4 * col] = tmp[row + 4 * ((col - row + 4) % 4)];
    }
}

inline void mixColumns(unsigned char *state)
{
    for (int col = 0; col < 4; ++col) {
        unsigned char *c = state + 4 * col;
        const unsigned char a0 = c[0], a1 = c[1], a2 = c[2], a3 = c[3];
        c[0] = static_cast<unsigned char>(xtime(a0) ^ (xtime(a1) ^ a1) ^ a2 ^ a3);
        c[1] = static_cast<unsigned char>(a0 ^ xtime(a1) ^ (xtime(a2) ^ a2) ^ a3);
        c[2] = static_cast<unsigned char>(a0 ^ a1 ^ xtime(a2) ^ (xtime(a3) ^ a3));
        c[3] = static_cast<unsigned char>((xtime(a0) ^ a0) ^ a1 ^ a2 ^ xtime(a3));
    }
}

inline void invMixColumns(unsigned char *state)
{
    for (int col = 0; col < 4; ++col) {
        unsigned char *c = state + 4 * col;
        const unsigned char a0 = c[0], a1 = c[1], a2 = c[2], a3 = c[3];
        c[0] = static_cast<unsigned char>(gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9));
        c[1] = static_cast<unsigned char>(gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13));
        c[2] = static_cast<unsigned char>(gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11));
        c[3] = static_cast<unsigned char>(gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14));
    }
}

void aesEncryptBlock(const AesSchedule &schedule, unsigned char *block)
{
    addRoundKey(block, schedule.roundKey);
    for (int round = 1; round < schedule.rounds; ++round) {
        subBytes(block);
        shiftRows(block);
        mixColumns(block);
        addRoundKey(block, schedule.roundKey + 16 * round);
    }
    subBytes(block);
    shiftRows(block);
    addRoundKey(block, schedule.roundKey + 16 * schedule.rounds);
}

void aesDecryptBlock(const AesSchedule &schedule, unsigned char *block)
{
    addRoundKey(block, schedule.roundKey + 16 * schedule.rounds);
    for (int round = schedule.rounds - 1; round >= 1; --round) {
        invShiftRows(block);
        invSubBytes(block);
        addRoundKey(block, schedule.roundKey + 16 * round);
        invMixColumns(block);
    }
    invShiftRows(block);
    invSubBytes(block);
    addRoundKey(block, schedule.roundKey);
}

QByteArray pkcs7Pad(const QByteArray &data, int blockSize = 16)
{
    const int pad = blockSize - (data.size() % blockSize);
    QByteArray result = data;
    for (int i = 0; i < pad; ++i)
        result.append(static_cast<char>(pad));
    return result;
}

QByteArray pkcs7Unpad(const QByteArray &data)
{
    if (data.isEmpty())
        return data;
    const int pad = static_cast<unsigned char>(data.at(data.size() - 1));
    if (pad <= 0 || pad > 16 || pad > data.size())
        return QByteArray();
    QByteArray result = data.left(data.size() - pad);
    return result;
}

// ---------------------------------------------------------------------------
// Big unsigned integers (little endian 32 bit limbs) used by the RSA helpers.
// ---------------------------------------------------------------------------

using Limbs = QVector<quint32>;

Limbs limbsFromBigEndian(const QByteArray &bytes, int limbCount)
{
    Limbs out(limbCount, 0u);
    int byteIndex = bytes.size() - 1;
    int limb = 0;
    while (byteIndex >= 0 && limb < limbCount) {
        quint32 value = 0;
        for (int shift = 0; shift < 32 && byteIndex >= 0; shift += 8, --byteIndex)
            value |= static_cast<quint32>(static_cast<unsigned char>(bytes.at(byteIndex))) << shift;
        out[limb++] = value;
    }
    return out;
}

QByteArray limbsToBigEndian(const Limbs &value, int byteCount)
{
    QByteArray out(byteCount, '\0');
    for (int i = 0; i < byteCount; ++i) {
        const int limbIndex = i / 4;
        const int byteInLimb = i % 4;
        unsigned char byte = 0;
        if (limbIndex < value.size())
            byte = static_cast<unsigned char>((value[limbIndex] >> (8 * byteInLimb)) & 0xff);
        out[byteCount - 1 - i] = static_cast<char>(byte);
    }
    return out;
}

int compareLimbs(const Limbs &a, const Limbs &b)
{
    for (int i = a.size() - 1; i >= 0; --i) {
        if (a[i] != b[i])
            return a[i] > b[i] ? 1 : -1;
    }
    return 0;
}

void subInPlace(Limbs &a, const Limbs &b)
{
    quint64 borrow = 0;
    for (int i = 0; i < a.size(); ++i) {
        const quint64 subtrahend = static_cast<quint64>(b[i]) + borrow;
        const quint64 minuend = a[i];
        a[i] = static_cast<quint32>(minuend - subtrahend);
        borrow = (minuend < subtrahend) ? 1 : 0;
    }
}

void addModInPlace(Limbs &a, const Limbs &b, const Limbs &n)
{
    quint64 carry = 0;
    for (int i = 0; i < a.size(); ++i) {
        const quint64 sum = static_cast<quint64>(a[i]) + b[i] + carry;
        a[i] = static_cast<quint32>(sum);
        carry = sum >> 32;
    }
    if (carry != 0 || compareLimbs(a, n) >= 0)
        subInPlace(a, n);
}

int bitLength(const Limbs &value)
{
    for (int i = value.size() - 1; i >= 0; --i) {
        if (value[i] != 0) {
            int bits = 0;
            quint32 v = value[i];
            while (v) {
                v >>= 1;
                ++bits;
            }
            return i * 32 + bits;
        }
    }
    return 0;
}

bool testBit(const Limbs &value, int bit)
{
    const int limb = bit / 32;
    if (limb < 0 || limb >= value.size())
        return false;
    return ((value[limb] >> (bit % 32)) & 1u) != 0;
}

Limbs multiplyMod(const Limbs &a, const Limbs &b, const Limbs &n)
{
    Limbs result(n.size(), 0u);
    const int bits = bitLength(b);
    for (int bit = bits - 1; bit >= 0; --bit) {
        // result = (result * 2) mod n
        addModInPlace(result, result, n);
        if (testBit(b, bit))
            addModInPlace(result, a, n);
    }
    return result;
}

Limbs exponentiateMod(const Limbs &base, const Limbs &exponent, const Limbs &modulus)
{
    const int limbCount = modulus.size();
    Limbs result(limbCount, 0u);
    // 1 mod n (modulus >= 2 here)
    result[0] = 1u;
    Limbs baseCopy = base;
    const int bits = bitLength(exponent);
    for (int bit = bits - 1; bit >= 0; --bit) {
        result = multiplyMod(result, result, modulus);
        if (testBit(exponent, bit))
            result = multiplyMod(result, baseCopy, modulus);
    }
    return result;
}

QByteArray modPowBytes(const QByteArray &base, const QByteArray &exponent, const QByteArray &modulus)
{
    const int byteCount = modulus.size();
    int limbCount = (byteCount + 3) / 4;
    const Limbs mod = limbsFromBigEndian(modulus, limbCount);
    const Limbs baseLimbs = limbsFromBigEndian(base, limbCount);
    const Limbs expLimbs = limbsFromBigEndian(exponent, limbCount);
    const Limbs result = exponentiateMod(baseLimbs, expLimbs, mod);
    return limbsToBigEndian(result, byteCount);
}

// ---------------------------------------------------------------------------
// Minimal DER parsing for SubjectPublicKeyInfo / RSAPublicKey
// ---------------------------------------------------------------------------

bool derRead(const QByteArray &data, int &pos, unsigned char &tag, int &contentStart, int &contentLength)
{
    if (pos + 2 > data.size())
        return false;
    tag = static_cast<unsigned char>(data.at(pos++));
    unsigned char lengthByte = static_cast<unsigned char>(data.at(pos++));
    int length = 0;
    if (lengthByte & 0x80) {
        const int count = lengthByte & 0x7f;
        if (count <= 0 || count > 4 || pos + count > data.size())
            return false;
        for (int i = 0; i < count; ++i)
            length = (length << 8) | static_cast<unsigned char>(data.at(pos++));
    } else {
        length = lengthByte;
    }
    if (pos + length > data.size())
        return false;
    contentStart = pos;
    contentLength = length;
    return true;
}

bool parseRsaInteger(const QByteArray &data, int &pos, QByteArray &value)
{
    unsigned char tag = 0;
    int start = 0;
    int length = 0;
    if (!derRead(data, pos, tag, start, length))
        return false;
    if (tag != 0x02) // INTEGER
        return false;
    QByteArray raw = data.mid(start, length);
    while (raw.size() > 1 && static_cast<unsigned char>(raw.at(0)) == 0x00)
        raw.remove(0, 1);
    value = raw;
    pos = start + length;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QByteArray Crypto::md5(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Md5);
}

QString Crypto::md5Hex(const QByteArray &data)
{
    return QString::fromLatin1(md5(data).toHex());
}

QString Crypto::sha1Hex(const QByteArray &data)
{
    return QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha1).toHex());
}

QByteArray Crypto::sha256(const QByteArray &data)
{
    return QCryptographicHash::hash(data, QCryptographicHash::Sha256);
}

QString Crypto::sha256Hex(const QByteArray &data)
{
    return QString::fromLatin1(sha256(data).toHex());
}

QByteArray Crypto::hmacSha256(const QByteArray &key, const QByteArray &message)
{
    return QMessageAuthenticationCode::hash(message, key, QCryptographicHash::Sha256);
}

QByteArray Crypto::pbkdf2Sha256(const QByteArray &password, const QByteArray &salt, int iterations, int keyLength)
{
    constexpr int hLen = 32;
    if (iterations < 1 || keyLength < 1)
        return QByteArray();

    // HMAC-SHA256 with pre-computed inner/outer padding: roughly 3x faster than
    // calling QMessageAuthenticationCode for every round, which matters because
    // the credential store derives its key during application start.
    QByteArray key(64, '\0'); // zero padded key block
    if (password.size() > 64) {
        const QByteArray digest = sha256(password);
        std::memcpy(key.data(), digest.constData(), static_cast<size_t>(digest.size()));
    } else if (!password.isEmpty()) {
        std::memcpy(key.data(), password.constData(), static_cast<size_t>(password.size()));
    }
    QByteArray ipad(64, '\0');
    QByteArray opad(64, '\0');
    for (int i = 0; i < 64; ++i) {
        ipad[i] = static_cast<char>(key.at(i) ^ 0x36);
        opad[i] = static_cast<char>(key.at(i) ^ 0x5c);
    }

    const auto hmac = [&ipad, &opad](const QByteArray &message) {
        QCryptographicHash inner(QCryptographicHash::Sha256);
        inner.addData(ipad);
        inner.addData(message);
        const QByteArray innerDigest = inner.result();

        QCryptographicHash outer(QCryptographicHash::Sha256);
        outer.addData(opad);
        outer.addData(innerDigest);
        return outer.result();
    };

    QByteArray derived;
    derived.reserve(keyLength + hLen);
    const int blockCount = (keyLength + hLen - 1) / hLen;

    for (int block = 1; block <= blockCount; ++block) {
        QByteArray index(4, '\0');
        index[0] = static_cast<char>((block >> 24) & 0xff);
        index[1] = static_cast<char>((block >> 16) & 0xff);
        index[2] = static_cast<char>((block >> 8) & 0xff);
        index[3] = static_cast<char>(block & 0xff);

        QByteArray u = hmac(salt + index);
        QByteArray accumulator = u;
        for (int round = 1; round < iterations; ++round) {
            u = hmac(u);
            for (int i = 0; i < accumulator.size(); ++i)
                accumulator[i] = static_cast<char>(accumulator.at(i) ^ u.at(i));
        }
        derived.append(accumulator);
    }

    return derived.left(keyLength);
}

QString Crypto::hexUpper(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex()).toUpper();
}

QString Crypto::hexLower(const QByteArray &data)
{
    return QString::fromLatin1(data.toHex());
}

QByteArray Crypto::fromHex(const QString &hex)
{
    return QByteArray::fromHex(hex.toLatin1());
}

QString Crypto::base64(const QByteArray &data)
{
    return QString::fromLatin1(data.toBase64());
}

QByteArray Crypto::fromBase64(const QString &text)
{
    return QByteArray::fromBase64(text.toLatin1());
}

QString Crypto::decimalFromHex(const QString &hex)
{
    QByteArray bytes = QByteArray::fromHex(hex.toLatin1());
    // long division by 10, digit by digit
    QString digits;
    QByteArray current = bytes;
    while (!current.isEmpty()) {
        int remainder = 0;
        QByteArray quotient;
        quotient.reserve(current.size());
        for (int i = 0; i < current.size(); ++i) {
            const int value = remainder * 256 + static_cast<unsigned char>(current.at(i));
            const int q = value / 10;
            remainder = value % 10;
            if (!quotient.isEmpty() || q != 0)
                quotient.append(static_cast<char>(q));
        }
        digits.prepend(QChar('0' + remainder));
        current = quotient;
    }
    if (digits.isEmpty())
        digits = QStringLiteral("0");
    return digits;
}

QString Crypto::decimalFromBytes(const QByteArray &bytes)
{
    return decimalFromHex(QString::fromLatin1(bytes.toHex()));
}

QByteArray Crypto::randomBytes(int count)
{
    QByteArray out(count, '\0');
    auto *generator = QRandomGenerator::system();
    for (int i = 0; i < count; ++i)
        out[i] = static_cast<char>(generator->bounded(256));
    return out;
}

QString Crypto::randomBase62(int count)
{
    static const char alphabet[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    QString out;
    out.reserve(count);
    auto *generator = QRandomGenerator::system();
    for (int i = 0; i < count; ++i)
        out.append(QLatin1Char(alphabet[generator->bounded(62)]));
    return out;
}

QByteArray Crypto::aesCbcEncrypt(const QByteArray &plain, const QByteArray &key, const QByteArray &iv)
{
    AesSchedule schedule;
    if (!aesExpandKey(key, schedule) || iv.size() != 16)
        return QByteArray();

    const QByteArray padded = pkcs7Pad(plain);
    QByteArray out(padded.size(), '\0');
    unsigned char previous[16];
    std::memcpy(previous, iv.constData(), 16);

    for (int offset = 0; offset < padded.size(); offset += 16) {
        unsigned char block[16];
        std::memcpy(block, padded.constData() + offset, 16);
        for (int i = 0; i < 16; ++i)
            block[i] ^= previous[i];
        aesEncryptBlock(schedule, block);
        std::memcpy(out.data() + offset, block, 16);
        std::memcpy(previous, block, 16);
    }
    return out;
}

QByteArray Crypto::aesCbcDecrypt(const QByteArray &cipher, const QByteArray &key, const QByteArray &iv)
{
    AesSchedule schedule;
    if (!aesExpandKey(key, schedule) || iv.size() != 16 || cipher.size() % 16 != 0)
        return QByteArray();

    QByteArray out(cipher.size(), '\0');
    unsigned char previous[16];
    std::memcpy(previous, iv.constData(), 16);

    for (int offset = 0; offset < cipher.size(); offset += 16) {
        unsigned char block[16];
        std::memcpy(block, cipher.constData() + offset, 16);
        unsigned char cipherBlock[16];
        std::memcpy(cipherBlock, block, 16);
        aesDecryptBlock(schedule, block);
        for (int i = 0; i < 16; ++i)
            block[i] ^= previous[i];
        std::memcpy(out.data() + offset, block, 16);
        std::memcpy(previous, cipherBlock, 16);
    }
    return pkcs7Unpad(out);
}

QByteArray Crypto::aesEcbEncrypt(const QByteArray &plain, const QByteArray &key)
{
    AesSchedule schedule;
    if (!aesExpandKey(key, schedule))
        return QByteArray();

    const QByteArray padded = pkcs7Pad(plain);
    QByteArray out(padded.size(), '\0');
    for (int offset = 0; offset < padded.size(); offset += 16) {
        unsigned char block[16];
        std::memcpy(block, padded.constData() + offset, 16);
        aesEncryptBlock(schedule, block);
        std::memcpy(out.data() + offset, block, 16);
    }
    return out;
}

QByteArray Crypto::aesEcbDecrypt(const QByteArray &cipher, const QByteArray &key)
{
    AesSchedule schedule;
    if (!aesExpandKey(key, schedule) || cipher.size() % 16 != 0)
        return QByteArray();

    QByteArray out(cipher.size(), '\0');
    for (int offset = 0; offset < cipher.size(); offset += 16) {
        unsigned char block[16];
        std::memcpy(block, cipher.constData() + offset, 16);
        aesDecryptBlock(schedule, block);
        std::memcpy(out.data() + offset, block, 16);
    }
    return pkcs7Unpad(out);
}

Crypto::RsaPublicKey Crypto::rsaPublicKeyFromPem(const QByteArray &pem)
{
    RsaPublicKey key;

    QString text = QString::fromLatin1(pem);
    const QStringList lines = text.split(QLatin1Char('\n'));
    QByteArray base64;
    for (const QString &line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith(QLatin1String("-----")))
            continue;
        base64.append(trimmed.toLatin1());
    }
    const QByteArray der = QByteArray::fromBase64(base64);

    int pos = 0;
    unsigned char tag = 0;
    int start = 0;
    int length = 0;
    // outer SEQUENCE
    if (!derRead(der, pos, tag, start, length) || tag != 0x30)
        return key;
    int inner = start;
    // AlgorithmIdentifier SEQUENCE -> skip
    if (!derRead(der, inner, tag, start, length) || tag != 0x30)
        return key;
    inner = start + length;
    // BIT STRING containing RSAPublicKey
    if (!derRead(der, inner, tag, start, length) || tag != 0x03)
        return key;
    const QByteArray bitString = der.mid(start + 1, length - 1); // skip unused-bits byte

    int rsaPos = 0;
    if (!derRead(bitString, rsaPos, tag, start, length) || tag != 0x30)
        return key;
    rsaPos = start;
    if (!parseRsaInteger(bitString, rsaPos, key.modulus))
        return key;
    if (!parseRsaInteger(bitString, rsaPos, key.exponent))
        return key;
    return key;
}

QByteArray Crypto::rsaEncryptPkcs1v15(const RsaPublicKey &key, const QByteArray &data)
{
    const int k = key.byteLength();
    if (k <= 11 || data.size() > k - 11)
        return QByteArray();

    QByteArray em(k, '\0');
    em[0] = 0x00;
    em[1] = 0x02;
    const int psLength = k - 3 - data.size();
    for (int i = 0; i < psLength; ++i) {
        unsigned char value = 0;
        while (value == 0)
            value = static_cast<unsigned char>(Crypto::randomBytes(1).at(0));
        em[2 + i] = static_cast<char>(value);
    }
    em[2 + psLength] = 0x00;
    std::memcpy(em.data() + 3 + psLength, data.constData(), static_cast<size_t>(data.size()));

    return modPowBytes(em, key.exponent, key.modulus);
}

QByteArray Crypto::rsaEncryptRaw(const RsaPublicKey &key, const QByteArray &data)
{
    const int k = key.byteLength();
    if (data.size() > k)
        return QByteArray();
    QByteArray padded(k, '\0');
    std::memcpy(padded.data() + (k - data.size()), data.constData(), static_cast<size_t>(data.size()));
    return modPowBytes(padded, key.exponent, key.modulus);
}

QByteArray Crypto::rsaEncryptPkcs1v15Pem(const QByteArray &pem, const QByteArray &data)
{
    return rsaEncryptPkcs1v15(rsaPublicKeyFromPem(pem), data);
}

QByteArray Crypto::rsaEncryptRawPem(const QByteArray &pem, const QByteArray &data)
{
    return rsaEncryptRaw(rsaPublicKeyFromPem(pem), data);
}

QByteArray Crypto::zlibInflate(const QByteArray &data)
{
    QByteArray out;
    if (!mr::zlibInflate(data, out))
        return QByteArray();
    return out;
}

} // namespace mr
