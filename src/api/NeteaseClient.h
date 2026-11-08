#pragma once

#include "api/ApiTypes.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QMap>
#include <QObject>
#include <QString>

namespace mr {

class HttpClient;
struct HttpResponse;

/// Direct client for the NetEase Cloud Music upstream API.
///
/// Only the endpoints used by the official clients are called
/// (`/weapi/...` and `/eapi/...`); the Node.js proxy of the reference project is
/// never involved. Everything (weapi AES+RSA, eapi AES-ECB, cookie maintenance,
/// anonymous token) is implemented in C++.
class NeteaseClient : public QObject
{
    Q_OBJECT

public:
    explicit NeteaseClient(QObject *parent = nullptr);

    void setProxy(const QString &proxyUrl);

    // --- session ------------------------------------------------------
    QJsonObject cookies() const;
    void setCookies(const QJsonObject &cookies);
    QString anonymousToken() const { return m_anonymousToken; }
    void setAnonymousToken(const QString &token);

    QJsonObject profile() const { return m_profile; }
    void setProfile(const QJsonObject &profile);

    bool loggedIn() const { return m_cookies.contains(QStringLiteral("MUSIC_U")); }
    QString userId() const;
    QString nickname() const;
    QString avatarUrl() const;
    void clearSession();

    // --- endpoints ----------------------------------------------------
    void search(const QString &keywords, int limit, int offset, JsonCallback callback);
    void dailyRecommend(JsonCallback callback);
    void songUrl(const Track &track, Quality quality, StreamCallback callback);
    void lyric(const Track &track, TextCallback callback);

    void registerAnonymous(JsonCallback callback);
    void qrKey(JsonCallback callback);
    void qrCheck(const QString &key, JsonCallback callback);
    void loginCellphone(const QString &phone, const QString &password, const QString &captcha,
                        JsonCallback callback);
    void accountInfo(JsonCallback callback);
    void logout(JsonCallback callback);

    // --- catalogue / browsing (TARGET.md §10) --------------------------
    void searchAlbums(const QString &keywords, int limit, int offset, JsonCallback callback);
    void searchMvs(const QString &keywords, int limit, int offset, JsonCallback callback);
    void songDetail(const QList<QString> &ids, JsonCallback callback);
    void albumDetail(const QString &albumId, JsonCallback callback);
    void artistDetail(const QString &artistId, JsonCallback callback);
    void artistSongs(const QString &artistId, int limit, int offset, bool hot, JsonCallback callback);
    void artistAlbums(const QString &artistId, int limit, int offset, JsonCallback callback);
    void artistMvs(const QString &artistId, int limit, int offset, JsonCallback callback);

    // --- account actions (favourites / subscriptions) -------------------
    void likeSong(const QString &trackId, bool like, JsonCallback callback);
    void likedSongs(const QString &uid, JsonCallback callback);
    void albumSubscribe(const QString &albumId, bool subscribe, JsonCallback callback);
    void albumSubList(int limit, int offset, JsonCallback callback);
    void mvSubscribe(const QString &mvId, bool subscribe, JsonCallback callback);
    void mvSubList(int limit, int offset, JsonCallback callback);
    void artistSubscribe(const QString &artistId, bool follow, JsonCallback callback);
    void artistSubList(int limit, int offset, JsonCallback callback);
    void mvUrl(const QString &mvId, int resolution, JsonCallback callback);

signals:
    void cookiesChanged();
    void loginStateChanged();
    void rawResponse(const QString &endpoint, int status, const QByteArray &body);

private:
    enum class Mode { Eapi, Weapi };

    void request(const QString &uri, QJsonObject data, Mode mode, JsonCallback callback);
    void mergeCookies(const HttpResponse &response);

    QMap<QString, QString> processedCookies(const QString &uri) const;
    static QString cookieObjectToString(const QMap<QString, QString> &cookies);
    static QJsonObject weapiEncrypt(const QJsonObject &data);
    static QString eapiEncrypt(const QString &uri, const QJsonObject &data);
    static QString formEncode(const QJsonObject &fields);

    static QList<Track> parseSongs(const QJsonArray &songs);

    HttpClient *m_http = nullptr;
    QMap<QString, QString> m_cookies;
    QString m_anonymousToken;
    QJsonObject m_profile;
};

} // namespace mr
