#pragma once

#include <QObject>
#include <QList>
#include <QHash>
#include <QElapsedTimer>

class QWebSocketServer;
class QWebSocket;

/// WebSocket server that broadcasts messages to all connected clients.
/// Includes per-IP rate limiting (default 3 messages/second).
class WsServer : public QObject {
    Q_OBJECT
public:
    explicit WsServer(QObject *parent = nullptr);
    ~WsServer() override;

    /// Start listening on @p port.  Returns false on failure.
    bool start(quint16 port);

    /// Broadcast a text message to all connected clients.
    void broadcast(const QString &message);

    /// Set the per-IP rate limit (messages per second).  0 = unlimited.
    void setRateLimit(int maxPerSecond) { m_rateLimit = maxPerSecond; }

    int  rateLimit() const { return m_rateLimit; }

    /// The port this server is listening on.
    quint16 port() const { return m_port; }

    /// Stop the server and disconnect all clients.
    void close();

signals:
    /// Emitted when a valid (non-rate-limited) message is received.
    void newMessage(const QString &text, const QString &senderIp);

    /// Emitted when a message is dropped due to rate limiting.
    void rateLimited(const QString &ip);

private slots:
    void onNewConnection();
    void onTextMessage(const QString &message);
    void onDisconnected();

private:
    bool checkRateLimit(const QString &ip);

    QWebSocketServer *m_server = nullptr;
    QList<QWebSocket*> m_clients;
    quint16 m_port = 0;
    int     m_rateLimit = 3;

    // Rate-limit state:  IP → list of message timestamps (ms)
    QHash<QString, QList<qint64>> m_rateLimitMap;
};
