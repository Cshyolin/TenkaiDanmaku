#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QFont>
#include <QHostAddress>
#include <QUuid>

#include "Logger.h"
#include "NetworkHelper.h"
#include "SimpleHttpServer.h"
#include "WsServer.h"
#include "DanmakuWindow.h"
#include "ControlPanel.h"
#include "ConfigDialog.h"
#include "TrayManager.h"
#include "RelayClient.h"

// ── Single-instance guard ───────────────────────────────────────────────

static bool checkSingleInstance()
{
    static QSharedMemory shm("TenkaiDanmaku_Instance_6e3f1a");
    if (!shm.create(1) && shm.error() == QSharedMemory::AlreadyExists) {
        QMessageBox::warning(nullptr, "TenkaiDanmaku",
                             "程序已在运行中，请检查系统托盘。");
        return false;
    }
    return true;
}

// ── Helpers ─────────────────────────────────────────────────────────────

static QString ipType(const QString &ip)
{
    QHostAddress addr(ip);
    return (addr.protocol() == QAbstractSocket::IPv6Protocol)
        ? QStringLiteral("弹幕 IPv6") : QStringLiteral("弹幕 IPv4");
}

static QString generateToken()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

// ── Entry point ─────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TenkaiDanmaku");
    app.setOrganizationName("TenkaiDanmaku");
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon(":/icons/icon.ico"));

    if (!checkSingleInstance())
        return 0;

    // ── Settings ────────────────────────────────────────────────────────
    QSettings settings("TenkaiDanmaku", "TenkaiDanmaku");

    const QString fontFamily   = settings.value("fontFamily", "Microsoft YaHei").toString();
    const int     fontSize     = settings.value("fontSize", 24).toInt();
    const quint16 baseHttpPort = static_cast<quint16>(settings.value("httpPort", 8080).toUInt());
    const double  trackRatio   = settings.value("trackAreaRatio", 0.3).toDouble();
    const int     rateLimit    = settings.value("rateLimit", 3).toInt();
    const int     animMinMs    = settings.value("animMinMs", 5000).toInt();
    const int     animMaxMs    = settings.value("animMaxMs", 8000).toInt();
    const QString logDir       = settings.value("logDir", "./logs").toString();

    // Relay settings
    const QString relayUrl     = settings.value("relayUrl", "http://localhost:3000").toString();
    const int     defaultMode  = settings.value("defaultMode", 0).toInt();
    const bool    closeLocalInRelay = settings.value("closeLocalInRelay", false).toBool();

    // ── Logger ──────────────────────────────────────────────────────────
    Logger::instance()->init(logDir);
    Logger::instance()->logEvent("程序启动");

    // ── Network IPs ─────────────────────────────────────────────────────
    const QString ipv6 = NetworkHelper::getGlobalIPv6();
    const QString ipv4 = NetworkHelper::getLocalIPv4();

    if (ipv6.isEmpty()) Logger::instance()->logEvent("未检测到公网 IPv6 地址");
    if (ipv4.isEmpty()) Logger::instance()->logEvent("未检测到局域网 IPv4 地址");

    // ── Port pair ───────────────────────────────────────────────────────
    quint16 httpPort = baseHttpPort;
    quint16 wsPort   = baseHttpPort + 1;
    if (!NetworkHelper::findAvailablePorts(httpPort, wsPort)) {
        QMessageBox::critical(nullptr, "TenkaiDanmaku",
            QString("无法找到可用端口对 (尝试了 %1 → %2)。\n请检查端口占用情况或修改起始端口。")
                .arg(baseHttpPort).arg(httpPort + 2));
        Logger::instance()->logEvent("启动失败：无可用端口");
        return 1;
    }
    if (httpPort != baseHttpPort)
        Logger::instance()->logEvent(
            QString("端口冲突，已自动递增至 HTTP=%1 WS=%2").arg(httpPort).arg(wsPort));

    // ── HTTP server ─────────────────────────────────────────────────────
    SimpleHttpServer httpServer;
    if (!httpServer.start(httpPort, wsPort)) {
        QMessageBox::critical(nullptr, "TenkaiDanmaku",
            QString("HTTP 服务启动失败（端口 %1）").arg(httpPort));
        Logger::instance()->logEvent("HTTP 服务启动失败");
        return 1;
    }

    // ── WebSocket server ────────────────────────────────────────────────
    WsServer wsServer;
    wsServer.setRateLimit(rateLimit);
    if (!wsServer.start(wsPort)) {
        QMessageBox::critical(nullptr, "TenkaiDanmaku",
            QString("WebSocket 服务启动失败（端口 %1）").arg(wsPort));
        Logger::instance()->logEvent("WebSocket 服务启动失败");
        return 1;
    }

    // ── Danmaku window ──────────────────────────────────────────────────
    DanmakuWindow danmakuWin;
    danmakuWin.setDanmakuFont(QFont(fontFamily, fontSize));
    danmakuWin.setTrackAreaRatio(trackRatio);
    danmakuWin.setSpeedRange(animMinMs, animMaxMs);
    danmakuWin.connectToServer(wsPort);
    danmakuWin.show();

    // ── Relay client ────────────────────────────────────────────────────
    RelayClient relayClient;
    QString relayToken;

    // Relay → Danmaku display
    QObject::connect(&relayClient, &RelayClient::danmakuReceived,
                     &danmakuWin, &DanmakuWindow::addDanmaku);

    // Relay → Logger
    QObject::connect(&relayClient, &RelayClient::danmakuReceived,
        [](const QString &text, const QString &senderInfo) {
            Logger::instance()->logDanmaku("弹幕 RELAY", "中继", text);
        });

    // ── Control panel ───────────────────────────────────────────────────
    ControlPanel controlPanel;

    // Local mode setup
    if (!ipv6.isEmpty()) {
        controlPanel.setIPv6Url(NetworkHelper::buildUrl(ipv6, httpPort));
        controlPanel.setIPv6Available(true);
    } else {
        controlPanel.setIPv6Available(false);
    }
    if (!ipv4.isEmpty()) {
        controlPanel.setIPv4Url(NetworkHelper::buildUrl(ipv4, httpPort));
        controlPanel.setIPv4Available(true);
    } else {
        controlPanel.setIPv4Available(false);
    }

    // ── Config dialog ───────────────────────────────────────────────────
    ConfigDialog configDialog;

    // ── Tray manager ────────────────────────────────────────────────────
    TrayManager trayManager;

    // ── Signal / slot wiring ────────────────────────────────────────────

    // WS → Logger (local danmaku)
    QObject::connect(&wsServer, &WsServer::newMessage,
        [](const QString &text, const QString &ip) {
            Logger::instance()->logDanmaku(ipType(ip), ip, text);
        });

    QObject::connect(&wsServer, &WsServer::rateLimited,
        [](const QString &ip) { Logger::instance()->logRateLimit(ip); });

    // Relay connection state → ControlPanel status
    QObject::connect(&relayClient, &RelayClient::connectionStateChanged,
        [&controlPanel](bool connected) {
            controlPanel.setRelayConnected(connected);
        });

    // Relay errors → Logger
    QObject::connect(&relayClient, &RelayClient::errorOccurred,
        [](const QString &msg) {
            Logger::instance()->logEvent("中继错误: " + msg);
        });

    // ── Mode switching ──────────────────────────────────────────────────

    // User changes mode in ControlPanel
    QObject::connect(&controlPanel, &ControlPanel::modeChanged,
        [&](ControlPanel::Mode mode) {
            if (mode == ControlPanel::Mode::Relay) {
                // Generate token & connect
                relayToken = generateToken();
                const QString fullUrl = relayUrl + "?token=" + relayToken;
                controlPanel.setRelayUrl(fullUrl);
                relayClient.connectToServer(relayUrl, relayToken);
                Logger::instance()->logEvent("切换到中继模式 Token=" + relayToken);

                if (closeLocalInRelay) {
                    wsServer.close();
                    httpServer.close();
                    Logger::instance()->logEvent("已关闭本地 HTTP/WS 服务");
                }
            } else {
                // Switch back to local
                relayClient.disconnect();
                controlPanel.setRelayConnected(false);
                Logger::instance()->logEvent("切换到局域网直连模式");

                if (closeLocalInRelay) {
                    // Restart local services
                    if (!httpServer.start(httpPort, wsPort))
                        Logger::instance()->logEvent("HTTP 服务重启失败");
                    wsServer.setRateLimit(rateLimit);
                    if (!wsServer.start(wsPort))
                        Logger::instance()->logEvent("WS 服务重启失败");
                    danmakuWin.connectToServer(wsPort);
                    Logger::instance()->logEvent("已恢复本地 HTTP/WS 服务");
                }
            }
        });

    // ── Tray actions ────────────────────────────────────────────────────

    QObject::connect(&trayManager, &TrayManager::toggleControlPanel, [&]() {
        if (controlPanel.isVisible()) {
            controlPanel.hide();
            trayManager.setQrCodesChecked(false);
        } else {
            controlPanel.show();
            trayManager.setQrCodesChecked(true);
        }
    });

    QObject::connect(&trayManager, &TrayManager::toggleDanmakuWindow,
                     [&](bool checked) { danmakuWin.setVisible(checked); });

    QObject::connect(&trayManager, &TrayManager::openConfig, [&]() {
        configDialog.loadSettings();
        configDialog.show();
        configDialog.raise();
        configDialog.activateWindow();
    });

    QObject::connect(&trayManager, &TrayManager::quitApp, [&]() {
        Logger::instance()->logEvent("用户退出");
        relayClient.disconnect();
        controlPanel.setQuitting(true);
        danmakuWin.setQuitting(true);
        app.quit();
    });

    // ── Config → apply instant settings ─────────────────────────────────

    QObject::connect(&configDialog, &ConfigDialog::settingsChanged, [&]() {
        QSettings s("TenkaiDanmaku", "TenkaiDanmaku");
        danmakuWin.setDanmakuFont(QFont(
            s.value("fontFamily", "Microsoft YaHei").toString(),
            s.value("fontSize", 24).toInt()));
        danmakuWin.setTrackAreaRatio(s.value("trackAreaRatio", 0.3).toDouble());
        danmakuWin.setSpeedRange(
            s.value("animMinMs", 5000).toInt(),
            s.value("animMaxMs", 8000).toInt());
        wsServer.setRateLimit(s.value("rateLimit", 3).toInt());
        Logger::instance()->logEvent("配置已更新并应用");
    });

    QObject::connect(&controlPanel, &ControlPanel::windowHidden,
                     [&]() { trayManager.setQrCodesChecked(false); });

    // ── Network change timer (local mode only) ──────────────────────────

    QString lastIPv6 = ipv6;
    QString lastIPv4 = ipv4;
    QTimer netTimer;
    QObject::connect(&netTimer, &QTimer::timeout, [&]() {
        const QString newIPv6 = NetworkHelper::getGlobalIPv6();
        const QString newIPv4 = NetworkHelper::getLocalIPv4();

        if (newIPv6 != lastIPv6) {
            lastIPv6 = newIPv6;
            if (!newIPv6.isEmpty()) {
                controlPanel.setIPv6Url(NetworkHelper::buildUrl(newIPv6, httpPort));
                controlPanel.setIPv6Available(true);
                Logger::instance()->logEvent("IPv6 地址变更: " + newIPv6);
            } else {
                controlPanel.setIPv6Available(false);
                Logger::instance()->logEvent("IPv6 地址已断开");
            }
        }
        if (newIPv4 != lastIPv4) {
            lastIPv4 = newIPv4;
            if (!newIPv4.isEmpty()) {
                controlPanel.setIPv4Url(NetworkHelper::buildUrl(newIPv4, httpPort));
                controlPanel.setIPv4Available(true);
                Logger::instance()->logEvent("IPv4 地址变更: " + newIPv4);
            } else {
                controlPanel.setIPv4Available(false);
                Logger::instance()->logEvent("IPv4 地址已断开");
            }
        }
    });
    netTimer.start(10000);

    // ── Startup: apply default mode ─────────────────────────────────────

    if (defaultMode == 1) {
        controlPanel.setMode(ControlPanel::Mode::Relay);
        relayToken = generateToken();
        const QString fullUrl = relayUrl + "?token=" + relayToken;
        controlPanel.setRelayUrl(fullUrl);
        relayClient.connectToServer(relayUrl, relayToken);
        Logger::instance()->logEvent("默认中继模式 Token=" + relayToken);

        if (closeLocalInRelay) {
            wsServer.close();
            httpServer.close();
        }
    }

    controlPanel.show();
    trayManager.show();

    Logger::instance()->logEvent(
        QString("服务已启动  IPv6=%1  IPv4=%2  HTTP=%3  WS=%4  Mode=%5")
            .arg(ipv6.isEmpty() ? "无" : NetworkHelper::buildUrl(ipv6, httpPort),
                 ipv4.isEmpty() ? "无" : NetworkHelper::buildUrl(ipv4, httpPort))
            .arg(httpPort).arg(wsPort)
            .arg(defaultMode == 1 ? "中继" : "局域网"));

    return app.exec();
}
