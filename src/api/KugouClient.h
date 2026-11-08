#pragma once

#include "api/ApiTypes.h"
#include "api/HttpClient.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

#include <functional>

namespace mr {

struct HttpResponse;

/// Direct client for the KuGou Music upstream API.
///
/// Reproduces the request signature (`OIlwieks28dk2k092lksi2UIkp` salt), the
/// `x-router` header routing, the device identifiers (dfid/mid/uuid) and the
/// AES/RSA encrypted login payloads from the reference project.
class KugouClient : public QObject
{
    Q_OBJECT

public:
    explicit KugouClient(QObject *parent = nullptr);

    void setProxy(const QString &proxyUrl);

    // --- session ------------------------------------------------------
    QJsonObject cookies() const;
    void setCookies(const QJsonObject &cookies);
    bool loggedIn() const { return !m_cookies.value(QStringLiteral("token")).isEmpty(); }
    QString userId() const { return m_cookies.value(QStringLiteral("userid")); }
    QString nickname() const;
    QString avatarUrl() const;
    QJsonObject profile() const { return m_profile; }
    void setProfile(const QJsonObject &profile);
    void clearSession();

    /// Registers a device (`/risk/v2/r_register_dev`) to obtain the `dfid`
    /// cookie. Failures are not fatal: the API also accepts the `-` placeholder.
    void registerDevice(std::function<void(bool)> callback = {});
    bool deviceRegistered() const { return !m_cookies.value(QStringLiteral("dfid")).isEmpty(); }

    // --- endpoints ----------------------------------------------------
    void search(const QString &keywords, int page, int pageSize, JsonCallback callback);
    void dailyRecommend(JsonCallback callback);
    void songUrl(const Track &track, Quality quality, StreamCallback callback);
    void lyric(const Track &track, TextCallback callback);

    // --- catalogue / browsing (TARGET.md §10.19 / §10.20) --------------
    void searchAlbums(const QString &keywords, int page, int pageSize, JsonCallback callback);
    void searchMvs(const QString &keywords, int page, int pageSize, JsonCallback callback);
    void albumDetail(const QString &albumId, JsonCallback callback);
    void albumSongs(const QString &albumId, int page, int pageSize, JsonCallback callback);
    void artistDetail(const QString &artistId, JsonCallback callback);
    void artistSongs(const QString &artistId, int page, int pageSize, bool hot, JsonCallback callback);
    void artistAlbums(const QString &artistId, int page, int pageSize, bool hot, JsonCallback callback);
    void artistMvs(const QString &artistId, int page, int pageSize, JsonCallback callback);
    /// Videos the account marked as favourite (needs a logged in session).
    void likedVideos(int pageSize, JsonCallback callback);
    /// Resolves the playable mp4 url of an MV (`/v2/interface/index`).
    void videoUrl(const QString &hash, JsonCallback callback);

    /// Fills in missing cover URLs (daily recommendations do not ship one) by
    /// querying `/v2/get_res_privilege/lite` in a single batch request.
    /// The callback receives `trackKey -> coverUrl` for the resolved entries.
    void resolveCovers(const QList<Track> &tracks,
                       std::function<void(const QHash<QString, QString> &)> callback);

    void qrKey(JsonCallback callback);
    void qrCheck(const QString &key, JsonCallback callback);
    void loginByPassword(const QString &username, const QString &password, JsonCallback callback);
    void userDetail(JsonCallback callback);

signals:
    void cookiesChanged();
    void loginStateChanged();
    void rawResponse(const QString &endpoint, const QByteArray &body);

private:
    struct Request
    {
        QString baseUrl = QStringLiteral("https://gateway.kugou.com");
        QString path;
        QString method = QStringLiteral("GET");
        QMap<QString, QString> params;
        QByteArray body;
        QString encryptType = QStringLiteral("android");
        QMap<QString, QString> headers;
        bool encryptKey = false;
        bool notSign = false;
        bool clearDefaultParams = false;
        QString signKeyHash;
    };

    struct Request;

    HttpRequest buildHttp(const Request &request);
    void songUrlV5(const Track &track, Quality quality, StreamCallback callback);
    void songUrlV6(const Track &track, Quality quality, StreamCallback callback);
    void send(Request request, JsonCallback callback);
    void sendRaw(Request request, std::function<void(bool, const QByteArray &, const QString &)> callback);
    void mergeCookies(const HttpResponse &response);
    QString defaultDfid() const;

    HttpClient *m_http = nullptr;
    QMap<QString, QString> m_cookies;
    QJsonObject m_profile;
};

} // namespace mr
