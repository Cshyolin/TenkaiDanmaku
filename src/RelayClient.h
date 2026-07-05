#pragma once

#include <QObject>
#include <QWebSocket>
#include <QTimer>
#include <QJsonObject>

/// WebSocket client for the public relay backend.
/// Handles JSON auth handshake, receives danmaku broadcasts,
/// and forwards them formatted for DanmakuWindow::addDanmaku.
class RelayClient : public QObject {
    Q_OBJECT
public:
    explicit RelayClient(QObject *parent = nullptr);
    ~RelayClient() override;

    /// Connect to the relay server and authenticate with the given token.
    void connectToServer(const QString &serverUrl, const QString &token);

    /// Gracefully disconnect (no auto-reconnect).
    void disconnect();

    bool isConnected() const { return m_authed; }
    QString token() const { return m_token; }

signals:
    /// Emitted for each danmaku broadcast from the relay.
    /// @param text  "message#RRGGBB" formatted for parseDanmaku compatibility.
    /// @param senderInfo  "[RELAY]" ident string.
    void danmakuReceived(const QString &text, const QString &senderInfo);

    /// Connection state changed (authenticated → true, disconnected → false).
    void connectionStateChanged(bool connected);

    /// Relay-reported error.
    void errorOccurred(const QString &message);

private slots:
    void onConnected();
    void onDisconnected();
    void onTextMessage(const QString &message);
    void onAuthTimeout();
    void onReconnectTick();

private:
    void sendAuth();
    void scheduleReconnect();
    void resetReconnectDelay();

    QWebSocket *m_socket    = nullptr;
    QTimer     *m_authTimer = nullptr;
    QTimer     *m_reconnectTimer = nullptr;

    QString m_serverUrl;
    QString m_token;
    int     m_reconnectDelayMs = 1000;
    bool    m_authed           = false;
    bool    m_intentional      = false;
};
