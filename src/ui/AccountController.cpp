#include "ui/AccountController.h"

#include "account/AccountStore.h"
#include "api/KugouClient.h"
#include "api/NeteaseClient.h"
#include "core/Fingerprint.h"
#include "core/Logger.h"
#include "ui/QrCode.h"

#include <QTimer>

namespace mr {

AccountController::AccountController(NeteaseClient *netease, KugouClient *kugou, AccountStore *store,
                                     QObject *parent)
    : QObject(parent)
    , m_netease(netease)
    , m_kugou(kugou)
    , m_store(store)
    , m_pollTimer(new QTimer(this))
{
    m_pollTimer->setInterval(2500);
    connect(m_pollTimer, &QTimer::timeout, this, &AccountController::pollQr);

    connect(m_netease, &NeteaseClient::loginStateChanged, this, [this]() {
        emit stateChanged();
        persist();
    });
    connect(m_kugou, &KugouClient::loginStateChanged, this, [this]() {
        emit stateChanged();
        persist();
    });
    connect(m_netease, &NeteaseClient::cookiesChanged, this, &AccountController::persist);
    connect(m_kugou, &KugouClient::cookiesChanged, this, &AccountController::persist);

    connect(m_store, &AccountStore::corruptedChanged, this, &AccountController::updateStorageWarning);
}

bool AccountController::neteaseLoggedIn() const
{
    return m_netease->loggedIn();
}

QString AccountController::neteaseNickname() const
{
    return m_netease->nickname();
}

QString AccountController::neteaseAvatar() const
{
    return m_netease->avatarUrl();
}

bool AccountController::kugouLoggedIn() const
{
    return m_kugou->loggedIn();
}

QString AccountController::kugouNickname() const
{
    return m_kugou->nickname();
}

QString AccountController::kugouAvatar() const
{
    return m_kugou->avatarUrl();
}

void AccountController::updateStorageWarning()
{
    if (m_store->corrupted()) {
        m_storageWarning = tr("本地登录数据已损坏或与本机不匹配，请重新登录。");
        Logger::warn(m_storageWarning);
    } else {
        m_storageWarning.clear();
    }
    emit storageWarningChanged();
}

QString AccountController::deviceFingerprint() const
{
    return Fingerprint::instance().machineId().left(16);
}

void AccountController::applyPlaintextStorage(bool plaintext)
{
    m_store->setPlaintext(plaintext);
    persist();
}

void AccountController::dismissStorageWarning()
{
    m_store->reset();
    m_storageWarning.clear();
    emit storageWarningChanged();
}

void AccountController::restore()
{
    m_store->load();
    updateStorageWarning();

    const QJsonObject netease = m_store->netease();
    if (!netease.isEmpty()) {
        m_netease->setCookies(netease.value(QStringLiteral("cookies")).toObject());
        m_netease->setProfile(netease.value(QStringLiteral("profile")).toObject());
        const QString token = netease.value(QStringLiteral("anonymousToken")).toString();
        if (!token.isEmpty())
            m_netease->setAnonymousToken(token);
    }
    if (m_netease->anonymousToken().isEmpty()) {
        const QString stored = m_store->anonymousToken();
        if (!stored.isEmpty())
            m_netease->setAnonymousToken(stored);
    }

    const QJsonObject kugou = m_store->kugou();
    if (!kugou.isEmpty()) {
        m_kugou->setCookies(kugou.value(QStringLiteral("cookies")).toObject());
        m_kugou->setProfile(kugou.value(QStringLiteral("profile")).toObject());
    }

    if (!m_kugou->deviceRegistered())
        m_kugou->registerDevice({});

    if (m_netease->loggedIn())
        m_netease->accountInfo(nullptr);
    else if (!m_triedAnonymous) {
        m_triedAnonymous = true;
        m_netease->registerAnonymous([this](bool ok, const QJsonObject &, const QString &) {
            if (ok)
                persist();
        });
    }

    emit stateChanged();
}

void AccountController::persist()
{
    if (!m_store)
        return;

    QJsonObject netease;
    netease[QStringLiteral("cookies")] = m_netease->cookies();
    netease[QStringLiteral("profile")] = m_netease->profile();
    netease[QStringLiteral("anonymousToken")] = m_netease->anonymousToken();
    m_store->setNetease(netease);

    QJsonObject kugou;
    kugou[QStringLiteral("cookies")] = m_kugou->cookies();
    kugou[QStringLiteral("profile")] = m_kugou->profile();
    m_store->setKugou(kugou);

    m_store->setAnonymousToken(m_netease->anonymousToken());
    m_store->save();
}

void AccountController::setLoginState(const QString &status, const QString &message)
{
    m_loginStatus = status;
    m_loginMessage = message;
    m_busy = status == QLatin1String("waiting") || status == QLatin1String("scanned");
    emit loginStateChanged();
}

void AccountController::startQrLogin(const QString &source)
{
    cancelLogin();
    m_loginSource = source;
    m_qrImage.clear();
    setLoginState(QStringLiteral("waiting"), tr("正在获取二维码…"));

    if (source == QLatin1String("netease")) {
        m_netease->qrKey([this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                setLoginState(QStringLiteral("error"), error.isEmpty() ? tr("获取二维码失败") : error);
                return;
            }
            m_qrKey = body.value(QStringLiteral("key")).toString();
            m_qrImage = QrCodeUtil::toDataUrl(body.value(QStringLiteral("url")).toString());
            m_pollCount = 0;
            setLoginState(QStringLiteral("waiting"), tr("请使用网易云音乐 App 扫描二维码"));
            m_pollTimer->start();
        });
    } else {
        m_kugou->qrKey([this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                setLoginState(QStringLiteral("error"), error.isEmpty() ? tr("获取二维码失败") : error);
                return;
            }
            m_qrKey = body.value(QStringLiteral("key")).toString();
            m_qrImage = QrCodeUtil::toDataUrl(body.value(QStringLiteral("url")).toString());
            m_pollCount = 0;
            setLoginState(QStringLiteral("waiting"), tr("请使用酷狗音乐 App 扫描二维码"));
            m_pollTimer->start();
        });
    }
}

void AccountController::pollQr()
{
    if (m_qrKey.isEmpty()) {
        m_pollTimer->stop();
        return;
    }
    if (++m_pollCount > 120) { // ~5 minutes
        m_pollTimer->stop();
        setLoginState(QStringLiteral("error"), tr("二维码已过期，请刷新"));
        return;
    }

    if (m_loginSource == QLatin1String("netease")) {
        m_netease->qrCheck(m_qrKey, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                setLoginState(QStringLiteral("error"), error);
                m_pollTimer->stop();
                return;
            }
            const int code = body.value(QStringLiteral("code")).toInt(0);
            switch (code) {
            case 800:
                m_pollTimer->stop();
                setLoginState(QStringLiteral("error"), tr("二维码已过期，请刷新"));
                break;
            case 801:
                setLoginState(QStringLiteral("waiting"), tr("等待扫码…"));
                break;
            case 802:
                setLoginState(QStringLiteral("scanned"), tr("已扫码，请在手机上确认"));
                break;
            case 803:
                m_pollTimer->stop();
                m_netease->accountInfo(nullptr);
                setLoginState(QStringLiteral("success"), tr("网易云登录成功"));
                persist();
                emit stateChanged();
                break;
            default:
                break;
            }
        });
    } else {
        m_kugou->qrCheck(m_qrKey, [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                setLoginState(QStringLiteral("error"), error);
                m_pollTimer->stop();
                return;
            }
            const int status = body.value(QStringLiteral("status")).toInt(0);
            switch (status) {
            case 0:
                m_pollTimer->stop();
                setLoginState(QStringLiteral("error"), tr("二维码已过期，请刷新"));
                break;
            case 1:
                setLoginState(QStringLiteral("waiting"), tr("等待扫码…"));
                break;
            case 2:
                setLoginState(QStringLiteral("scanned"), tr("已扫码，请在手机上确认"));
                break;
            case 4:
                m_pollTimer->stop();
                setLoginState(QStringLiteral("success"), tr("酷狗登录成功"));
                persist();
                emit stateChanged();
                break;
            default:
                break;
            }
        });
    }
}

void AccountController::loginWithPassword(const QString &source, const QString &account,
                                          const QString &password)
{
    cancelLogin();
    m_loginSource = source;
    setLoginState(QStringLiteral("waiting"), tr("正在登录…"));

    if (source == QLatin1String("netease")) {
        m_netease->loginCellphone(account, password, QString(),
                                  [this](bool ok, const QJsonObject &body, const QString &error) {
            if (!ok) {
                setLoginState(QStringLiteral("error"), error);
                return;
            }
            m_netease->accountInfo(nullptr);
            setLoginState(QStringLiteral("success"), tr("网易云登录成功"));
            persist();
            emit stateChanged();
        });
    } else {
        m_kugou->loginByPassword(account, password,
                                 [this](bool ok, const QJsonObject &, const QString &error) {
            if (!ok) {
                setLoginState(QStringLiteral("error"), error.isEmpty() ? tr("登录失败") : error);
                return;
            }
            setLoginState(QStringLiteral("success"), tr("酷狗登录成功"));
            persist();
            emit stateChanged();
        });
    }
}

void AccountController::cancelLogin()
{
    m_pollTimer->stop();
    m_qrKey.clear();
    m_qrImage.clear();
    if (m_loginStatus != QLatin1String("idle"))
        setLoginState(QStringLiteral("idle"), QString());
    m_loginSource.clear();
}

void AccountController::logout(const QString &source)
{
    if (source == QLatin1String("netease")) {
        m_netease->logout([this](bool, const QJsonObject &, const QString &) {
            persist();
            emit stateChanged();
        });
    } else {
        m_kugou->clearSession();
        persist();
        emit stateChanged();
    }
}

} // namespace mr
