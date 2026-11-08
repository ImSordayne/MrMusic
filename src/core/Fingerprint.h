#pragma once

#include <QByteArray>
#include <QString>

namespace mr {

/// Device identity derived exclusively from the machine fingerprint.
///
/// TARGET.md §1.5 / §3 forbid the user from editing the device identity and the
/// storage key, therefore every value below is derived (deterministically) from
/// `QSysInfo::machineUniqueId()` plus a fixed application salt:
///
///   Windows  -> MachineGuid from the registry
///   macOS    -> IOPlatformUUID
///   Linux    -> /etc/machine-id
///   Android  -> ANDROID_ID
class Fingerprint
{
public:
    static const Fingerprint &instance();

    /// Raw machine fingerprint (hex).
    QString machineId() const;

    /// Key used by the encrypted credential store.
    ///
    /// Derived as PBKDF2-HMAC-SHA256(machine fingerprint seed, per install salt).
    /// The salt lives in `<data>/.salt`, so the key never depends on anything the
    /// user can type in (TARGET.md §3.2) while still being salted against
    /// pre-computation attacks.
    QByteArray deriveStorageKey(const QByteArray &salt) const;

    // ---- KuGou device parameters (equivalent of KUGOU_API_GUID/DEV/MAC/WEBGL)
    QString kugouGuid() const;
    QString kugouMid() const;
    QString kugouDev() const;
    QString kugouMac() const;
    QString kugouWebgl() const;

    /// Stable device id used for the NetEase `deviceId` cookie.
    QString neteaseDeviceId() const;

private:
    Fingerprint();

    QByteArray m_seed;
    QByteArray m_storageSecret;
    QString m_kugouGuid;
    QString m_kugouMid;
    QString m_kugouDev;
    QString m_kugouMac;
    QString m_kugouWebgl;
    QString m_neteaseDeviceId;
    QString m_machineId;
};

} // namespace mr
