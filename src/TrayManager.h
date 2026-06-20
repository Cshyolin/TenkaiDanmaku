#pragma once

#include <QObject>

class QSystemTrayIcon;
class QMenu;
class QAction;

/// Manages the system-tray icon and its context menu.
class TrayManager : public QObject {
    Q_OBJECT
public:
    explicit TrayManager(QObject *parent = nullptr);

    /// Show the tray icon.
    void show();

    /// Update check-state of the "show QR codes" menu action.
    void setQrCodesChecked(bool checked);

    /// Update check-state of the "show danmaku" menu action.
    void setDanmakuChecked(bool checked);

signals:
    void showControlPanel();
    void toggleControlPanel();
    void toggleDanmakuWindow(bool visible);
    void openConfig();
    void quitApp();

private:
    void setupMenu();
    QIcon createTrayIcon();

    QSystemTrayIcon *m_tray = nullptr;
    QMenu   *m_menu          = nullptr;
    QAction *m_showQrAction  = nullptr;
    QAction *m_showDanmakuAction = nullptr;
};
