#include "TrayManager.h"

#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QPainter>
#include <QPixmap>
#include <QApplication>
#include <QStyle>

TrayManager::TrayManager(QObject *parent)
    : QObject(parent)
{
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(createTrayIcon());
    m_tray->setToolTip("TenkaiDanmaku");

    setupMenu();

    // Double-click toggles control panel
    connect(m_tray, &QSystemTrayIcon::activated, this,
            [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick)
            emit toggleControlPanel();
    });
}

void TrayManager::setupMenu()
{
    m_menu = new QMenu;

    m_showQrAction = m_menu->addAction("显示二维码");
    m_showQrAction->setCheckable(true);
    m_showQrAction->setChecked(true);
    connect(m_showQrAction, &QAction::toggled, this, [this](bool checked) {
        emit toggleControlPanel();
    });

    m_showDanmakuAction = m_menu->addAction("显示弹幕");
    m_showDanmakuAction->setCheckable(true);
    m_showDanmakuAction->setChecked(true);
    connect(m_showDanmakuAction, &QAction::toggled, this, [this](bool checked) {
        emit toggleDanmakuWindow(checked);
    });

    m_menu->addSeparator();

    QAction *configAction = m_menu->addAction("配置...");
    connect(configAction, &QAction::triggered, this, &TrayManager::openConfig);

    m_menu->addSeparator();

    QAction *quitAction = m_menu->addAction("退出");
    connect(quitAction, &QAction::triggered, this, &TrayManager::quitApp);

    m_tray->setContextMenu(m_menu);
}

void TrayManager::show()
{
    m_tray->show();
}

void TrayManager::setQrCodesChecked(bool checked)
{
    m_showQrAction->setChecked(checked);
}

void TrayManager::setDanmakuChecked(bool checked)
{
    m_showDanmakuAction->setChecked(checked);
}

// ── Programmatic icon generation ─────────────────────────────────────────
// Draws a speech-bubble shape with "弹" — no external image file required.

QIcon TrayManager::createTrayIcon()
{
    const int size = 64;  // will be scaled by the system

    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);

    // Background circle
    p.setBrush(QColor(0xe9, 0x45, 0x60));  // accent red
    p.setPen(Qt::NoPen);
    p.drawEllipse(4, 4, size - 8, size - 8);

    // Text
    p.setPen(Qt::white);
    QFont f = p.font();
    f.setPixelSize(static_cast<int>(size * 0.55));
    f.setBold(true);
    p.setFont(f);
    p.drawText(pm.rect(), Qt::AlignCenter, "弹");

    p.end();
    return QIcon(pm);
}
