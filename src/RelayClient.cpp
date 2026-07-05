#include "RelayClient.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QUrl>

RelayClient::RelayClient(QObject *parent)
    : QObject(parent)
{
    m_socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_socket, &QWebSocket::connected,
            this, &RelayClient::onConnected);
    connect(m_socket, &QWebSocket::disconnected,
            this, &RelayClient::onDisconnected);
    connect(m_socket, &QWebSocket::textMessageReceived,
            this, &RelayClient::onTextMessage);
    connect(m_socket, &QWebSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError err) {
        qDebug() << "[RelayClient] socket error:" << err
                 << m_socket->errorString();
    });

    m_authTimer = new QTimer(this);
    m_authTimer->setSingleShot(true);
    connect(m_authTimer, &QTimer::timeout, this, &RelayClient::onAuthTimeout);

    m_reconnectTimer = new QTimer(this);
    m_reconnectTimer->setSingleShot(true);
    connect(m_reconnectTimer, &QTimer::timeout, this, &RelayClient::onReconnectTick);
}

RelayClient::~RelayClient()
{
    m_intentional = true;
    m_reconnectTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();
}

// ── Public ──────────────────────────────────────────────────────────────

void RelayClient::connectToServer(const QString &serverUrl, const QString &token)
{
    // Clean up any previous pending connection/reconnect
    m_reconnectTimer->stop();
    m_authTimer->stop();
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();

    m_intentional = false;
    m_token = token;
    m_authed = false;
    resetReconnectDelay();

    // Derive WebSocket URL from HTTP base URL.
    // Input:  http://localhost:3000  or  https://example.com
    // Output: ws://localhost:3000/ws  or  wss://example.com/ws
    QUrl url(serverUrl);
    if (url.scheme() == "http")
        url.setScheme("ws");
    else if (url.scheme() == "https")
        url.setScheme("wss");

    // Safely append /ws path, avoiding double slashes
    QString path = url.path();
    if (path.isEmpty() || path == "/")
        path = "/ws";
    else if (path.endsWith('/'))
        path += "ws";
    else
        path += "/ws";
    url.setPath(path);

    m_serverUrl = url.toString();
    qDebug() << "[RelayClient] connecting to" << m_serverUrl
             << "token:" << m_token.left(8) << "...";

    m_socket->open(url);
}

void RelayClient::disconnect()
{
    m_intentional = true;
    m_reconnectTimer->stop();
    m_authTimer->stop();
    m_authed = false;
    if (m_socket->state() != QAbstractSocket::UnconnectedState)
        m_socket->close();

    emit connectionStateChanged(false);
    qDebug() << "[RelayClient] disconnected intentionally";
}

// ── Auth ────────────────────────────────────────────────────────────────

void RelayClient::sendAuth()
{
    QJsonObject auth;
    auth["type"]  = QStringLiteral("auth");
    auth["token"] = m_token;

    m_socket->sendTextMessage(
        QString::fromUtf8(QJsonDocument(auth).toJson(QJsonDocument::Compact)));

    qDebug() << "[RelayClient] auth sent, token:" << m_token.left(8) << "...";
    m_authTimer->start(5000);
}

void RelayClient::onAuthTimeout()
{
    qDebug() << "[RelayClient] auth timeout";
    m_socket->close();
}

// ── Slots ───────────────────────────────────────────────────────────────

void RelayClient::onConnected()
{
    qDebug() << "[RelayClient] socket connected";
    sendAuth();
}

void RelayClient::onDisconnected()
{
    qDebug() << "[RelayClient] socket disconnected, wasAuthed:" << m_authed;
    m_authTimer->stop();
    const bool wasAuthed = m_authed;
    m_authed = false;

    if (wasAuthed)
        emit connectionStateChanged(false);

    if (!m_intentional)
        scheduleReconnect();
}

void RelayClient::onTextMessage(const QString &message)
{
    qDebug() << "[RelayClient] received:" << message.left(100);

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(message.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError) {
        qDebug() << "[RelayClient] JSON parse error:" << err.errorString();
        return;
    }

    const QJsonObject obj = doc.object();
    const QString type = obj["type"].toString();

    if (type == "auth_ok") {
        qDebug() << "[RelayClient] auth OK";
        m_authTimer->stop();
        m_authed = true;
        resetReconnectDelay();
        emit connectionStateChanged(true);
        return;
    }

    if (type == "error") {
        m_authTimer->stop();
        const QString msg = obj["message"].toString();
        qDebug() << "[RelayClient] auth error:" << msg;
        emit errorOccurred(msg);
        m_socket->close();
        return;
    }

    if (type == "danmaku") {
        const QString text  = obj["text"].toString();
        const QString color = obj["color"].toString("#FFFFFF");

        // Reconstruct in the format that parseDanmaku() expects
        QString formatted = text;
        if (!color.isEmpty() && color != "#FFFFFF")
            formatted = text + color;

        qDebug() << "[RelayClient] danmaku:" << formatted;
        emit danmakuReceived(formatted, QStringLiteral("[RELAY]"));
    }
}

// ── Reconnect ───────────────────────────────────────────────────────────

void RelayClient::scheduleReconnect()
{
    qDebug() << "[RelayClient] scheduling reconnect in" << m_reconnectDelayMs << "ms";
    m_reconnectTimer->start(m_reconnectDelayMs);
    m_reconnectDelayMs = qMin(m_reconnectDelayMs * 2, 30000);
}

void RelayClient::onReconnectTick()
{
    if (m_intentional) return;
    qDebug() << "[RelayClient] reconnecting...";
    m_socket->open(QUrl(m_serverUrl));
}

void RelayClient::resetReconnectDelay()
{
    m_reconnectDelayMs = 1000;
}
