#include "WsServer.h"

#include <QWebSocketServer>
#include <QWebSocket>
#include <QDateTime>

WsServer::WsServer(QObject *parent)
    : QObject(parent)
{
}

WsServer::~WsServer()
{
    close();
}

bool WsServer::start(quint16 port)
{
    if (m_server) close();

    m_server = new QWebSocketServer("TenkaiDanmaku",
                                     QWebSocketServer::NonSecureMode, this);
    if (!m_server->listen(QHostAddress::Any, port))
        return false;

    m_port = port;

    connect(m_server, &QWebSocketServer::newConnection,
            this, &WsServer::onNewConnection);
    return true;
}

void WsServer::broadcast(const QString &message)
{
    for (QWebSocket *c : m_clients) {
        if (c->state() == QAbstractSocket::ConnectedState)
            c->sendTextMessage(message);
    }
}

void WsServer::close()
{
    if (m_server) {
        m_server->close();
        qDeleteAll(m_clients);
        m_clients.clear();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

// ── Slots ────────────────────────────────────────────────────────────────

void WsServer::onNewConnection()
{
    while (QWebSocket *sock = m_server->nextPendingConnection()) {
        connect(sock, &QWebSocket::textMessageReceived,
                this, &WsServer::onTextMessage);
        connect(sock, &QWebSocket::disconnected,
                this, &WsServer::onDisconnected);
        m_clients.append(sock);
    }
}

void WsServer::onTextMessage(const QString &message)
{
    QWebSocket *sender = qobject_cast<QWebSocket*>(QObject::sender());
    if (!sender) return;

    const QString ip = sender->peerAddress().toString();

    if (!checkRateLimit(ip)) {
        emit rateLimited(ip);
        return;
    }

    emit newMessage(message, ip);
    broadcast(message);
}

void WsServer::onDisconnected()
{
    QWebSocket *sock = qobject_cast<QWebSocket*>(QObject::sender());
    if (sock) {
        m_clients.removeAll(sock);
        sock->deleteLater();
    }
}

// ── Rate limiting ────────────────────────────────────────────────────────

bool WsServer::checkRateLimit(const QString &ip)
{
    if (m_rateLimit <= 0)
        return true;  // unlimited

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 window = 1000;  // 1-second sliding window

    QList<qint64> &timestamps = m_rateLimitMap[ip];

    // Remove timestamps older than the window
    while (!timestamps.isEmpty() && (now - timestamps.first() > window))
        timestamps.removeFirst();

    // Clean up empty entries periodically (when map gets large)
    // Not strictly necessary for event-scale usage but keeps memory bounded.
    if (timestamps.isEmpty())
        m_rateLimitMap.remove(ip);

    if (timestamps.size() >= m_rateLimit)
        return false;  // over limit

    timestamps.append(now);
    return true;
}
