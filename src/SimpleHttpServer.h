#pragma once

#include <QObject>

class QTcpServer;
class QTcpSocket;

/// Minimal HTTP server that serves a single HTML page.
/// The template placeholder {{WS_PORT}} is replaced with the actual
/// WebSocket port before the page is sent to the client.
class SimpleHttpServer : public QObject {
    Q_OBJECT
public:
    explicit SimpleHttpServer(QObject *parent = nullptr);
    ~SimpleHttpServer() override;

    /// Start listening.  Returns false if the port is unavailable.
    bool start(quint16 httpPort, quint16 wsPort);

    /// The actual port this server is listening on.
    quint16 httpPort() const { return m_httpPort; }

    /// Stop listening and disconnect all clients.
    void close();

private slots:
    void onNewConnection();

private:
    QByteArray buildResponse() const;

    QTcpServer *m_server  = nullptr;
    quint16     m_httpPort = 0;
    quint16     m_wsPort   = 0;
};
