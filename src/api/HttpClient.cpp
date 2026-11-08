#include "api/HttpClient.h"

#include "core/Logger.h"

#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QTimer>

namespace mr {

struct HttpClient::Task
{
    HttpRequest request;
    Callback callback;
    ProgressCallback progress;
    DownloadCallback downloadCallback;
    QString targetPath;
    QPointer<QFile> file;
    int attempt = 0;
    int maxRetries = 2;
    bool isDownload = false;
};

HttpClient::HttpClient(QObject *parent)
    : QObject(parent)
    , m_manager(new QNetworkAccessManager(this))
{
}

HttpClient::~HttpClient() = default;

void HttpClient::setProxy(const QString &proxyUrl)
{
    m_proxyUrl = proxyUrl;
    if (proxyUrl.isEmpty()) {
        m_manager->setProxy(QNetworkProxy(QNetworkProxy::NoProxy));
        return;
    }
    const QUrl url(proxyUrl.contains(QLatin1String("://")) ? proxyUrl : QStringLiteral("http://") + proxyUrl);
    QNetworkProxy proxy(QNetworkProxy::HttpProxy, url.host(), static_cast<quint16>(url.port(8080)));
    if (!url.userName().isEmpty()) {
        proxy.setUser(url.userName());
        proxy.setPassword(url.password());
    }
    m_manager->setProxy(proxy);
}

void HttpClient::setUserAgent(const QByteArray &userAgent)
{
    m_userAgent = userAgent;
}

void HttpClient::send(HttpRequest request, Callback callback, int maxRetries)
{
    auto *task = new Task;
    task->request = std::move(request);
    task->callback = std::move(callback);
    task->maxRetries = maxRetries;
    dispatch(*task);
}

void HttpClient::dispatch(Task &task)
{
    QNetworkRequest request(task.request.url);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setAttribute(QNetworkRequest::Http2AllowedAttribute, true);
    if (!m_userAgent.isEmpty())
        request.setRawHeader("User-Agent", m_userAgent);
    for (const auto &header : task.request.headers)
        request.setRawHeader(header.first, header.second);

    QNetworkReply *reply = nullptr;
    const QByteArray method = task.request.method.toUpper();
    if (method == "POST")
        reply = m_manager->post(request, task.request.body);
    else if (method == "PUT")
        reply = m_manager->put(request, task.request.body);
    else if (method == "HEAD")
        reply = m_manager->head(request);
    else
        reply = m_manager->get(request);

    auto *taskPtr = &task;
    auto buffer = std::make_shared<QByteArray>();

    auto *timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->setInterval(m_timeoutMs);
    connect(timer, &QTimer::timeout, reply, [reply]() {
        if (reply->isRunning())
            reply->abort();
    });
    timer->start();

    connect(reply, &QNetworkReply::readyRead, reply, [reply, buffer, taskPtr]() {
        const QByteArray chunk = reply->readAll();
        buffer->append(chunk);
        if (taskPtr->isDownload && taskPtr->file && taskPtr->file->isOpen()) {
            taskPtr->file->write(chunk);
            if (taskPtr->progress) {
                const QVariant total = reply->property("totalBytes");
                taskPtr->progress(buffer->size(), total.toLongLong());
            }
        }
    });

    connect(reply, &QNetworkReply::downloadProgress, reply, [reply, taskPtr](qint64 received, qint64 total) {
        reply->setProperty("totalBytes", total);
        if (taskPtr->isDownload && taskPtr->progress)
            taskPtr->progress(received, total);
    });

    connect(reply, &QNetworkReply::finished, this, [this, reply, taskPtr, buffer]() {
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        HttpResponse response;
        response.status = status;
        response.body = reply->readAll();
        if (!buffer->isEmpty() && response.body.isEmpty())
            response.body = *buffer;

        const auto headerPairs = reply->rawHeaderPairs();
        for (const auto &pair : headerPairs) {
            response.headers.append({pair.first, pair.second});
            if (pair.first.compare("Set-Cookie", Qt::CaseInsensitive) == 0)
                response.setCookies.append(pair.second);
        }

        const QNetworkReply::NetworkError error = reply->error();
        if (error != QNetworkReply::NoError) {
            // 429 / 5xx / transient network errors are retried with backoff.
            const bool retryable = status == 429 || status >= 500 || status == 0
                || error == QNetworkReply::TimeoutError || error == QNetworkReply::TemporaryNetworkFailureError
                || error == QNetworkReply::NetworkSessionFailedError || error == QNetworkReply::OperationCanceledError;
            if (retryable && taskPtr->attempt < taskPtr->maxRetries) {
                ++taskPtr->attempt;
                const int delay = 400 * (1 << taskPtr->attempt);
                Logger::debug(QStringLiteral("http retry %1 for %2 in %3 ms (%4)")
                                  .arg(taskPtr->attempt)
                                  .arg(taskPtr->request.url.toString())
                                  .arg(delay)
                                  .arg(reply->errorString()));
                QTimer::singleShot(delay, this, [this, taskPtr, buffer]() {
                    buffer->clear();
                    dispatch(*taskPtr);
                });
                reply->deleteLater();
                return;
            }
            response.error = reply->errorString();
        }

        finish(*taskPtr, response);
        reply->deleteLater();
    });
}

void HttpClient::finish(Task &task, const HttpResponse &response)
{
    if (task.isDownload) {
        if (task.file && task.file->isOpen()) {
            task.file->flush();
            task.file->close();
        }
        const bool ok = response.isOk() && response.body.size() > 0;
        if (!ok && task.file && !task.targetPath.isEmpty())
            QFile::remove(task.targetPath);
        const QString error = ok ? QString() : (response.error.isEmpty()
                                                    ? QStringLiteral("HTTP %1").arg(response.status)
                                                    : response.error);
        if (task.downloadCallback)
            task.downloadCallback(ok, error);
        delete &task;
        return;
    }

    if (task.callback)
        task.callback(response);
    delete &task;
}

void HttpClient::download(HttpRequest request, const QString &targetPath, ProgressCallback progress,
                          DownloadCallback finished)
{
    auto *task = new Task;
    task->request = std::move(request);
    task->targetPath = targetPath;
    task->progress = std::move(progress);
    task->downloadCallback = std::move(finished);
    task->isDownload = true;
    task->maxRetries = 1;

    auto *file = new QFile(targetPath, this);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        const QString error = file->errorString();
        delete file;
        if (task->downloadCallback)
            task->downloadCallback(false, error);
        delete task;
        return;
    }
    task->file = file;
    dispatch(*task);
}

} // namespace mr
