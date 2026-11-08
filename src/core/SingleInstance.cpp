#include "core/SingleInstance.h"

#include "core/Logger.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace mr {
namespace {
constexpr int kConnectTimeoutMs = 400;
}

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
    , m_key(key)
{
    QLocalSocket probe;
    probe.connectToServer(m_key);
    if (probe.waitForConnected(kConnectTimeoutMs)) {
        probe.disconnectFromServer();
        m_primary = false;
        return;
    }

    // No live instance: take ownership (removing a stale socket file first).
    QLocalServer::removeServer(m_key);
    m_server = new QLocalServer(this);
    m_server->setSocketOptions(QLocalServer::UserAccessOption);

    connect(m_server, &QLocalServer::newConnection, this, [this]() {
        while (QLocalSocket *socket = m_server->nextPendingConnection()) {
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
            connect(socket, &QLocalSocket::readyRead, socket, [this, socket]() {
                const QByteArray request = socket->readAll();
                if (request.startsWith("show")) {
                    Logger::info(QStringLiteral("another instance asked for the window"));
                    emit activationRequested();
                }
                socket->write("ok");
                socket->flush();
                socket->disconnectFromServer();
            });
        }
    });

    m_primary = m_server->listen(m_key);
    if (!m_primary)
        Logger::warn(QStringLiteral("single instance: cannot listen on %1: %2").arg(m_key, m_server->errorString()));
}

SingleInstance::~SingleInstance()
{
    if (m_server) {
        m_server->close();
        QLocalServer::removeServer(m_key);
    }
}

bool SingleInstance::notifyPrimary()
{
    QLocalSocket socket;
    socket.connectToServer(m_key);
    if (!socket.waitForConnected(kConnectTimeoutMs)) {
        Logger::warn(QStringLiteral("single instance: cannot reach %1").arg(m_key));
        return false;
    }
    socket.write("show");
    socket.flush();
    socket.waitForBytesWritten(kConnectTimeoutMs);
    socket.waitForReadyRead(kConnectTimeoutMs);
    socket.disconnectFromServer();
    return true;
}

} // namespace mr
