#pragma once

#include <QObject>
#include <QString>

class QTimer;

namespace mr {

class AccountStore;
class KugouClient;
class NeteaseClient;

/// Login flows for both platforms (TARGET.md §2.5): QR code scan and
/// account / password, each platform logged in independently.
class AccountController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool neteaseLoggedIn READ neteaseLoggedIn NOTIFY stateChanged)
    Q_PROPERTY(QString neteaseNickname READ neteaseNickname NOTIFY stateChanged)
    Q_PROPERTY(QString neteaseAvatar READ neteaseAvatar NOTIFY stateChanged)
    Q_PROPERTY(bool kugouLoggedIn READ kugouLoggedIn NOTIFY stateChanged)
    Q_PROPERTY(QString kugouNickname READ kugouNickname NOTIFY stateChanged)
    Q_PROPERTY(QString kugouAvatar READ kugouAvatar NOTIFY stateChanged)
    Q_PROPERTY(QString qrImage READ qrImage NOTIFY loginStateChanged)
    Q_PROPERTY(QString loginSource READ loginSource NOTIFY loginStateChanged)
    Q_PROPERTY(QString loginStatus READ loginStatus NOTIFY loginStateChanged)
    Q_PROPERTY(QString loginMessage READ loginMessage NOTIFY loginStateChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY loginStateChanged)
    Q_PROPERTY(QString storageWarning READ storageWarning NOTIFY storageWarningChanged)
    Q_PROPERTY(QString deviceFingerprint READ deviceFingerprint CONSTANT)

public:
    AccountController(NeteaseClient *netease, KugouClient *kugou, AccountStore *store,
                      QObject *parent = nullptr);

    bool neteaseLoggedIn() const;
    QString neteaseNickname() const;
    QString neteaseAvatar() const;
    bool kugouLoggedIn() const;
    QString kugouNickname() const;
    QString kugouAvatar() const;

    QString qrImage() const { return m_qrImage; }
    QString loginSource() const { return m_loginSource; }
    QString loginStatus() const { return m_loginStatus; }
    QString loginMessage() const { return m_loginMessage; }
    bool busy() const { return m_busy; }
    QString storageWarning() const { return m_storageWarning; }
    QString deviceFingerprint() const;

    /// Restores the persisted sessions (called once at startup).
    void restore();

public slots:
    void startQrLogin(const QString &source);
    void loginWithPassword(const QString &source, const QString &account, const QString &password);
    void cancelLogin();
    void logout(const QString &source);
    void dismissStorageWarning();
    void applyPlaintextStorage(bool plaintext);

signals:
    void stateChanged();
    void loginStateChanged();
    void storageWarningChanged();

private:
    void setLoginState(const QString &status, const QString &message);
    void pollQr();
    void persist();
    void updateStorageWarning();

    NeteaseClient *m_netease = nullptr;
    KugouClient *m_kugou = nullptr;
    AccountStore *m_store = nullptr;
    QTimer *m_pollTimer = nullptr;

    QString m_loginSource;
    QString m_loginStatus = QStringLiteral("idle");
    QString m_loginMessage;
    QString m_qrImage;
    QString m_qrKey;
    QString m_storageWarning;
    bool m_busy = false;
    int m_pollCount = 0;
    bool m_triedAnonymous = false;
};

} // namespace mr
