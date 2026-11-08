#include "core/Fingerprint.h"

#include "core/Crypto.h"

#include <QStringList>
#include <QSysInfo>

namespace mr {
namespace {

QString hexUpperGrouped(const QByteArray &bytes)
{
    QStringList parts;
    parts.reserve(bytes.size());
    for (char byte : bytes)
        parts.append(QStringLiteral("%1").arg(static_cast<unsigned char>(byte), 2, 16, QLatin1Char('0')).toUpper());
    return parts.join(QLatin1Char(':'));
}

} // namespace

Fingerprint::Fingerprint()
{
    QByteArray machine = QSysInfo::machineUniqueId();
    if (machine.isEmpty()) {
        machine = (QSysInfo::machineHostName() + QLatin1Char('/') + QSysInfo::productType() + QLatin1Char('/')
                   + QSysInfo::kernelVersion() + QLatin1Char('/') + QSysInfo::prettyProductName())
                      .toUtf8();
    }
    m_machineId = Crypto::sha256Hex(machine);

    m_seed = Crypto::sha256("MrMusic/" + machine + "/" + QSysInfo::productType().toUtf8());

    m_storageSecret = Crypto::sha256(m_seed + ":storage-secret");

    m_kugouGuid = Crypto::md5Hex(m_seed + ":kugou-guid");

    // KuGou `mid` == decimal representation of md5(guid) (see upstream calculateMid).
    m_kugouMid = Crypto::decimalFromHex(Crypto::md5Hex(m_kugouGuid.toUtf8()));

    static const char alphabet[] = "1234567890ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const QByteArray devSeed = Crypto::sha256(m_seed + ":kugou-dev");
    QString dev;
    dev.reserve(10);
    for (int i = 0; i < 10; ++i)
        dev.append(QLatin1Char(alphabet[static_cast<unsigned char>(devSeed.at(i)) % 36]));
    m_kugouDev = dev;

    const QByteArray macSeed = Crypto::sha256(m_seed + ":kugou-mac");
    QByteArray macBytes = macSeed.left(6);
    macBytes[0] = static_cast<char>(0x02); // locally administered unicast prefix, like upstream's default
    m_kugouMac = hexUpperGrouped(macBytes);

    const QByteArray webglSeed = Crypto::sha256(m_seed + ":kugou-webgl").left(8);
    m_kugouWebgl = Crypto::decimalFromBytes(webglSeed);

    const QByteArray neteaseSeed = Crypto::sha256(m_seed + ":netease-device");
    QString neteaseId;
    neteaseId.reserve(52);
    for (int i = 0; i < 52; ++i)
        neteaseId.append(QStringLiteral("%1").arg(static_cast<unsigned char>(neteaseSeed.at(i % neteaseSeed.size())) % 16, 1, 16).toUpper());
    m_neteaseDeviceId = neteaseId;
}

const Fingerprint &Fingerprint::instance()
{
    static const Fingerprint fingerprint;
    return fingerprint;
}

QString Fingerprint::machineId() const
{
    return m_machineId;
}

QByteArray Fingerprint::deriveStorageKey(const QByteArray &salt) const
{
    // 120 000 PBKDF2 rounds: expensive enough to make brute forcing the derived
    // key unpleasant, cheap enough to stay below ~100 ms on desktop hardware.
    return Crypto::pbkdf2Sha256(m_storageSecret, salt, 120000, 32);
}

QString Fingerprint::kugouGuid() const
{
    return m_kugouGuid;
}

QString Fingerprint::kugouMid() const
{
    return m_kugouMid;
}

QString Fingerprint::kugouDev() const
{
    return m_kugouDev;
}

QString Fingerprint::kugouMac() const
{
    return m_kugouMac;
}

QString Fingerprint::kugouWebgl() const
{
    return m_kugouWebgl;
}

QString Fingerprint::neteaseDeviceId() const
{
    return m_neteaseDeviceId;
}

} // namespace mr
