#include "SimpleHttpServer.h"

#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QCoreApplication>
#include <QDir>

// ── Helpers ──────────────────────────────────────────────────────────────

static QByteArray loadHtmlTemplate()
{
    // 1) Try external file next to the executable (hot-reload, no rebuild needed)
    const QString exeDir = QCoreApplication::applicationDirPath();
    const QString externalPath = exeDir + "/assets/index.html";
    if (QFile::exists(externalPath)) {
        QFile f(externalPath);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text))
            return f.readAll();
    }

    // 2) Fall back to Qt resource (embedded at build time)
    QFile f(":/index.html");
    if (f.open(QIODevice::ReadOnly | QIODevice::Text))
        return f.readAll();

    // 3) Ultimate fallback (should never happen if qrc is set up correctly)
    return QByteArrayLiteral("<html><body><h1>Error: HTML template not found</h1></body></html>");
}

// ── Server implementation ───────────────────────────────────────────────

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
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void SimpleHttpServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            const QByteArray req = sock->readAll();
            if (req.startsWith("GET /")) {
                const QByteArray resp = buildResponse();
                sock->write(resp);
            }
            sock->disconnectFromHost();
            sock->deleteLater();
        });
    }
}

QByteArray SimpleHttpServer::buildResponse() const
{
    QByteArray html = loadHtmlTemplate();
    html.replace("{{WS_PORT}}", QByteArray::number(m_wsPort));

    const QByteArray header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + QByteArray::number(html.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n";

    return header + html;
}
