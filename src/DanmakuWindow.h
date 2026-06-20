#pragma once

#include <QGraphicsView>
#include <QFont>
#include <QList>
#include <QPair>
#include <QColor>
#include <QWebSocket>

class QGraphicsScene;

/// Transparent full-screen overlay that displays scrolling danmaku messages.
/// Connects as a WebSocket client to the local WsServer.
class DanmakuWindow : public QGraphicsView {
    Q_OBJECT
public:
    explicit DanmakuWindow(QWidget *parent = nullptr);
    ~DanmakuWindow() override;

    /// Connect to the local WebSocket server.
    void connectToServer(quint16 wsPort);

    /// Apply font settings (new danmaku only).
    void setDanmakuFont(const QFont &font);

    /// Set animation speed range in milliseconds.
    void setSpeedRange(int minMs, int maxMs);

    /// Set the ratio of screen height used for danmaku tracks (0.1 – 0.5).
    void setTrackAreaRatio(double ratio);

    /// Set to true before quitting so the window closes cleanly.
    void setQuitting(bool v) { m_quitting = v; }

public slots:
    /// Called when a message is received from the server.
    void addDanmaku(const QString &text, const QString &senderIp);

    /// Recalculate scene geometry and tracks (monitor hotplug / resize).
    void onScreenGeometryChanged();

protected:
    void resizeEvent(QResizeEvent *event) override;
    void closeEvent(QCloseEvent *event) override;

private:
    void recalcScene();
    void recalcTracks();
    int  selectTrack();
    static QPair<QString, QColor> parseDanmaku(const QString &raw);

    QGraphicsScene *m_scene   = nullptr;
    QWebSocket     *m_client  = nullptr;
    quint16         m_wsPort  = 0;
    QFont           m_font;
    int             m_margin  = 4;

    int     m_trackCount = 0;
    QList<int> m_trackLastUsed;

    double  m_trackAreaRatio  = 0.3;
    int     m_minAnimMs       = 5000;
    int     m_maxAnimMs       = 8000;
    bool    m_quitting        = false;

private slots:
    void onConnected();
    void onDisconnected();
    void onWsMessage(const QString &message);
};
