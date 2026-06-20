#pragma once

#include <QObject>
#include <QHash>

class QTcpServer;
class QTcpSocket;

/// Minimal HTTP server that serves a single HTML page.
/// Buffers incoming data per-connection until a complete HTTP request is
/// received, correctly handling TCP fragmentation (crucial for mobile IPv6).
class SimpleHttpServer : public QObject {
    Q_OBJECT
public:
    explicit SimpleHttpServer(QObject *parent = nullptr);
    ~SimpleHttpServer() override;

    bool start(quint16 httpPort, quint16 wsPort);

    quint16 httpPort() const { return m_httpPort; }

    void close();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onSocketDisconnected();

private:
    QByteArray buildResponse() const;
    QByteArray build404Response() const;

    QTcpServer *m_server  = nullptr;
    quint16     m_httpPort = 0;
    quint16     m_wsPort   = 0;

    /// Per-socket receive buffers, keyed by socket pointer.
    /// Cleared automatically when the socket is destroyed.
    QHash<QTcpSocket*, QByteArray> m_buffers;
};
