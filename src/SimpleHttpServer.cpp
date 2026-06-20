#include "SimpleHttpServer.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QCoreApplication>

// ── Helpers ──────────────────────────────────────────────────────────────

static QByteArray loadHtmlTemplate()
{
    // 1) External file next to executable (hot-reload, no rebuild needed)
    const QString externalPath = QCoreApplication::applicationDirPath() + "/assets/index.html";
    if (QFile::exists(externalPath)) {
        QFile f(externalPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            return f.readAll();
    }

    // 2) Qt resource (embedded at build time)
    QFile f(":/index.html");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return f.readAll();

    // 3) Ultimate fallback
    return QByteArrayLiteral(
        "<html><body><h1>Error: HTML template not found</h1></body></html>");
}

/// Build a minimal HTTP response (status line + headers + body).
static QByteArray makeResponse(const QByteArray &status,
                               const QByteArray &contentType,
                               const QByteArray &body)
{
    return "HTTP/1.1 " + status + "\r\n"
           "Content-Type: " + contentType + "\r\n"
           "Content-Length: " + QByteArray::number(body.size()) + "\r\n"
           "Connection: close\r\n"
           "\r\n"
           + body;
}

// ── Server ───────────────────────────────────────────────────────────────

SimpleHttpServer::SimpleHttpServer(QObject *parent)
    : QObject(parent)
{
}

SimpleHttpServer::~SimpleHttpServer()
{
    close();
}

bool SimpleHttpServer::start(quint16 httpPort, quint16 wsPort)
{
    if (m_server) close();

    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::Any, httpPort))
        return false;

    m_httpPort = httpPort;
    m_wsPort   = wsPort;

    connect(m_server, &QTcpServer::newConnection,
            this, &SimpleHttpServer::onNewConnection);
    return true;
}

void SimpleHttpServer::close()
{
    // Disconnect all clients cleanly
    for (auto it = m_buffers.keyValueBegin(); it != m_buffers.keyValueEnd(); ++it) {
        QTcpSocket *sock = it->first;
        sock->disconnectFromHost();
    }
    m_buffers.clear();

    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

// ── Slots ────────────────────────────────────────────────────────────────

void SimpleHttpServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        // Allocate a fresh buffer for this connection
        m_buffers.insert(sock, QByteArray());

        connect(sock, &QTcpSocket::readyRead,
                this, &SimpleHttpServer::onReadyRead);
        connect(sock, &QTcpSocket::disconnected,
                this, &SimpleHttpServer::onSocketDisconnected);
        // If the socket is destroyed without disconnecting (edge case),
        // the buffer is cleaned up in onSocketDisconnected via sender().
    }
}

void SimpleHttpServer::onReadyRead()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (!sock) return;

    QByteArray &buf = m_buffers[sock];
    buf.append(sock->readAll());

    // Wait until the full HTTP header has arrived.
    const int headerEnd = buf.indexOf("\r\n\r\n");
    if (headerEnd < 0) {
        // Still incomplete — wait for more data.
        return;
    }

    const QByteArray header = buf.left(headerEnd);
    const QByteArray firstLine = header.split('\r').first();

    // Check whether the socket sent more data than we need (pipelining / POST
    // body).  We ignore it — this is a minimal server that only serves GET /.
    Q_UNUSED(buf.mid(headerEnd + 4));

    QByteArray resp;

    if (firstLine.startsWith("GET / ")) {
        resp = buildResponse();
    } else {
        // Any other path → 404
        resp = build404Response();
    }

    sock->write(resp);
    sock->disconnectFromHost();
    // onSocketDisconnected() will fire next and remove the buffer.
}

void SimpleHttpServer::onSocketDisconnected()
{
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) {
        m_buffers.remove(sock);
        sock->deleteLater();
    }
}

// ── Responses ────────────────────────────────────────────────────────────

QByteArray SimpleHttpServer::buildResponse() const
{
    QByteArray html = loadHtmlTemplate();
    html.replace("{{WS_PORT}}", QByteArray::number(m_wsPort));

    return makeResponse("200 OK", "text/html; charset=utf-8", html);
}

QByteArray SimpleHttpServer::build404Response() const
{
    const QByteArray body = "<html><body><h1>404 Not Found</h1></body></html>";
    return makeResponse("404 Not Found", "text/html; charset=utf-8", body);
}
