#pragma once

#include <QWidget>

class QLabel;

/// Main control panel showing two QR codes (IPv6 + IPv4) and status info.
/// Closing the window hides it to tray rather than quitting.
class ControlPanel : public QWidget {
    Q_OBJECT
public:
    explicit ControlPanel(QWidget *parent = nullptr);

    /// Set the IPv6 URL to encode in the QR code.
    void setIPv6Url(const QString &url);
    void setIPv4Url(const QString &url);

    /// Show / hide the QR code areas.
    void setQrCodesVisible(bool visible);

    /// Update availability status.
    void setIPv6Available(bool avail);
    void setIPv4Available(bool avail);

signals:
    /// Emitted when the window is hidden (so tray menu can update).
    void windowHidden();

public:
    /// Set to true before quitting so closeEvent accepts the close.
    void setQuitting(bool v) { m_quitting = v; }

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setupUi();
    QPixmap createPlaceholder(const QString &text, int size);

    QLabel *m_ipv6QrLabel  = nullptr;
    QLabel *m_ipv4QrLabel  = nullptr;
    QLabel *m_ipv6UrlLabel = nullptr;
    QLabel *m_ipv4UrlLabel = nullptr;
    QLabel *m_ipv6Status   = nullptr;
    QLabel *m_ipv4Status   = nullptr;
    QWidget *m_ipv6Group   = nullptr;
    QWidget *m_ipv4Group   = nullptr;
    bool     m_quitting    = false;
};
