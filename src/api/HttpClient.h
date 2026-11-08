#pragma once

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QUrl>

#include <functional>
#include <memory>

class QNetworkAccessManager;
class QNetworkReply;

namespace mr {

struct HttpRequest
{
    QUrl url;
    QByteArray method = QByteArrayLiteral("GET");
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;

    void setHeader(const QByteArray &name, const QByteArray &value)
    {
        for (auto &header : headers) {
            if (header.first.compare(name, Qt::CaseInsensitive) == 0) {
                header.second = value;
                return;
            }
        }
        headers.append({name, value});
    }
};

struct HttpResponse
{
    int status = 0;
    QByteArray body;
    QList<QPair<QByteArray, QByteArray>> headers;
    QList<QByteArray> setCookies;
    QString error;

    bool isOk() const { return error.isEmpty() && status >= 200 && status < 300; }
    QByteArray header(const QByteArray &name) const
    {
        for (const auto &header : headers) {
            if (header.first.compare(name, Qt::CaseInsensitive) == 0)
                return header.second;
        }
        return QByteArray();
    }
};

/// Thin wrapper around QNetworkAccessManager adding timeouts, retries with
/// exponential backoff (TARGET.md §1.7) and file downloads with progress.
class HttpClient : public QObject
{
    Q_OBJECT

public:
    using Callback = std::function<void(const HttpResponse &)>;
    using ProgressCallback = std::function<void(qint64 received, qint64 total)>;
    using DownloadCallback = std::function<void(bool ok, const QString &error)>;

    explicit HttpClient(QObject *parent = nullptr);
    ~HttpClient() override;

    void setProxy(const QString &proxyUrl);
    void setUserAgent(const QByteArray &userAgent);
    void setTimeoutMs(int timeoutMs) { m_timeoutMs = timeoutMs; }

    void send(HttpRequest request, Callback callback, int maxRetries = 2);

    void download(HttpRequest request, const QString &targetPath, ProgressCallback progress,
                  DownloadCallback finished);

private:
    struct Task;

    void dispatch(Task &task);
    void finish(Task &task, const HttpResponse &response);

    QNetworkAccessManager *m_manager = nullptr;
    QByteArray m_userAgent;
    QString m_proxyUrl;
    int m_timeoutMs = 20000;
};

} // namespace mr
