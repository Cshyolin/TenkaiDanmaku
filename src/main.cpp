#include <QApplication>
#include <QSharedMemory>
#include <QMessageBox>
#include <QSettings>
#include <QTimer>
#include <QFont>
#include <QHostAddress>

#include "Logger.h"
#include "NetworkHelper.h"
#include "SimpleHttpServer.h"
#include "WsServer.h"
#include "DanmakuWindow.h"
#include "ControlPanel.h"
#include "ConfigDialog.h"
#include "TrayManager.h"

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

// ── Speed mapping helper ─────────────────────────────────────────────────

static QPair<int,int> speedRangeFromIndex(int idx)
{
    switch (idx) {
    case 0: return {8000, 12000};  // slow
    case 2: return {3000, 5000};   // fast
    default: return {5000, 8000};  // medium
    }
}

// ── Helper: get IP type string ──────────────────────────────────────────

static QString ipType(const QString &ip)
{
    QHostAddress addr(ip);
    return (addr.protocol() == QAbstractSocket::IPv6Protocol)
        ? QStringLiteral("弹幕 IPv6") : QStringLiteral("弹幕 IPv4");
}

// ── Entry point ─────────────────────────────────────────────────────────

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("TenkaiDanmaku");
    app.setOrganizationName("TenkaiDanmaku");
    app.setQuitOnLastWindowClosed(false);  // tray persistence

    if (!checkSingleInstance())
        return 0;

    // ── Settings ────────────────────────────────────────────────────────
    QSettings settings("TenkaiDanmaku", "TenkaiDanmaku");

    const QString fontFamily  = settings.value("fontFamily", "Microsoft YaHei").toString();
    const int     fontSize    = settings.value("fontSize", 24).toInt();
    const quint16 baseHttpPort = static_cast<quint16>(
        settings.value("httpPort", 8080).toUInt());
    const double  trackRatio  = settings.value("trackAreaRatio", 0.3).toDouble();
    const int     rateLimit   = settings.value("rateLimit", 3).toInt();
    const int     animMinMs   = settings.value("animMinMs", 5000).toInt();
    const int     animMaxMs   = settings.value("animMaxMs", 8000).toInt();
    const QString logDir      = settings.value("logDir", "./logs").toString();

    // ── Logger ──────────────────────────────────────────────────────────
    Logger::instance()->init(logDir);
    Logger::instance()->logEvent("程序启动");

    // ── Network IPs ─────────────────────────────────────────────────────
    const QString ipv6 = NetworkHelper::getGlobalIPv6();
    const QString ipv4 = NetworkHelper::getLocalIPv4();

    if (ipv6.isEmpty())
        Logger::instance()->logEvent("未检测到公网 IPv6 地址");
    if (ipv4.isEmpty())
        Logger::instance()->logEvent("未检测到局域网 IPv4 地址");

    // ── Port pair ───────────────────────────────────────────────────────
    quint16 httpPort = baseHttpPort;
    quint16 wsPort   = baseHttpPort + 1;
    if (!NetworkHelper::findAvailablePorts(httpPort, wsPort)) {
        QMessageBox::critical(nullptr, "TenkaiDanmaku",
            QString("无法找到可用端口对 (尝试了 %1 → %2)。\n"
                    "请检查端口占用情况或修改起始端口。")
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

    // ── Control panel ───────────────────────────────────────────────────
    ControlPanel controlPanel;

    // IPv6
    if (!ipv6.isEmpty()) {
        const QString url = NetworkHelper::buildUrl(ipv6, httpPort);
        controlPanel.setIPv6Url(url);
        controlPanel.setIPv6Available(true);
    } else {
        controlPanel.setIPv6Available(false);
    }

    // IPv4
    if (!ipv4.isEmpty()) {
        const QString url = NetworkHelper::buildUrl(ipv4, httpPort);
        controlPanel.setIPv4Url(url);
        controlPanel.setIPv4Available(true);
    } else {
        controlPanel.setIPv4Available(false);
    }

    controlPanel.show();

    // ── Config dialog ───────────────────────────────────────────────────
    ConfigDialog configDialog;

    // ── Tray manager ────────────────────────────────────────────────────
    TrayManager trayManager;

    // ── Signal / slot wiring ────────────────────────────────────────────

    // WS → Danmaku display is via DanmakuWindow's internal WebSocket client
    // that receives broadcasts from WsServer.  No direct signal connection
    // needed here — that would double-display every message.

    // WS → Logger (danmaku messages)
    QObject::connect(&wsServer, &WsServer::newMessage,
        [](const QString &text, const QString &ip) {
            Logger::instance()->logDanmaku(ipType(ip), ip, text);
        });

    // WS rate-limit → Logger
    QObject::connect(&wsServer, &WsServer::rateLimited,
        [](const QString &ip) {
            Logger::instance()->logRateLimit(ip);
        });

    // Tray: toggle control panel
    QObject::connect(&trayManager, &TrayManager::toggleControlPanel,
        [&]() {
            if (controlPanel.isVisible()) {
                controlPanel.hide();
                trayManager.setQrCodesChecked(false);
            } else {
                controlPanel.show();
                trayManager.setQrCodesChecked(true);
            }
        });

    // Tray: toggle danmaku window
    QObject::connect(&trayManager, &TrayManager::toggleDanmakuWindow,
        [&](bool checked) {
            danmakuWin.setVisible(checked);
        });

    // Tray: open config
    QObject::connect(&trayManager, &TrayManager::openConfig,
        [&]() {
            configDialog.loadSettings();
            configDialog.show();
            configDialog.raise();
            configDialog.activateWindow();
        });

    // Tray: quit — let windows close cleanly, then shut down.
    QObject::connect(&trayManager, &TrayManager::quitApp,
        [&]() {
            Logger::instance()->logEvent("用户退出");
            controlPanel.setQuitting(true);
            danmakuWin.setQuitting(true);
            app.quit();
        });

    // Config → apply settings
    QObject::connect(&configDialog, &ConfigDialog::settingsChanged,
        [&]() {
            QSettings s("TenkaiDanmaku", "TenkaiDanmaku");
            const QString fam = s.value("fontFamily", "Microsoft YaHei").toString();
            const int     sz  = s.value("fontSize", 24).toInt();
            const double  tr  = s.value("trackAreaRatio", 0.3).toDouble();
            const int     amn = s.value("animMinMs", 5000).toInt();
            const int     amx = s.value("animMaxMs", 8000).toInt();
            const int     rl  = s.value("rateLimit", 3).toInt();

            danmakuWin.setDanmakuFont(QFont(fam, sz));
            danmakuWin.setTrackAreaRatio(tr);
            danmakuWin.setSpeedRange(amn, amx);
            wsServer.setRateLimit(rl);

            Logger::instance()->logEvent("配置已更新并应用");
        });

    // ControlPanel close → update tray check state
    QObject::connect(&controlPanel, &ControlPanel::windowHidden, [&]() {
        trayManager.setQrCodesChecked(false);
    });

    // ── Network change timer ────────────────────────────────────────────
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

    // ── Startup log ─────────────────────────────────────────────────────
    trayManager.show();

    Logger::instance()->logEvent(
        QString("服务已启动  IPv6=%1  IPv4=%2  HTTP=%3  WS=%4")
            .arg(ipv6.isEmpty() ? "无" : NetworkHelper::buildUrl(ipv6, httpPort),
                 ipv4.isEmpty() ? "无" : NetworkHelper::buildUrl(ipv4, httpPort))
            .arg(httpPort).arg(wsPort));

    return app.exec();
}
