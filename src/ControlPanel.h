#pragma once

#include <QWidget>

class QLabel;
class QComboBox;
class QStackedWidget;

/// Main control panel with mode switching (Local / Relay) and QR codes.
class ControlPanel : public QWidget {
    Q_OBJECT
public:
    enum class Mode { Local, Relay };

    explicit ControlPanel(QWidget *parent = nullptr);

    // ── Local mode ────────────────────────────────────────────────────
    void setIPv6Url(const QString &url);
    void setIPv4Url(const QString &url);
    void setIPv6Available(bool avail);
    void setIPv4Available(bool avail);

    // ── Relay mode ────────────────────────────────────────────────────
    void setRelayUrl(const QString &url);
    void setRelayConnected(bool connected);

    // ── General ───────────────────────────────────────────────────────
    void setMode(Mode mode);
    Mode mode() const { return m_mode; }
    void setQrCodesVisible(bool visible);
    void setQuitting(bool v) { m_quitting = v; }

signals:
    void windowHidden();
    void modeChanged(ControlPanel::Mode mode);

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void setupUi();
    void updateQRLabelSizes();
    QPixmap createPlaceholder(const QString &text, int size);

    // Mode selector
    QComboBox *m_modeCombo = nullptr;

    // Local mode widgets
    QLabel *m_ipv6Header = nullptr;
    QLabel *m_ipv4Header = nullptr;
    QLabel *m_ipv6QrLabel  = nullptr;
    QLabel *m_ipv4QrLabel  = nullptr;
    QLabel *m_ipv6UrlLabel = nullptr;
    QLabel *m_ipv4UrlLabel = nullptr;
    QLabel *m_ipv6Status   = nullptr;
    QLabel *m_ipv4Status   = nullptr;
    QWidget *m_ipv6Group   = nullptr;
    QWidget *m_ipv4Group   = nullptr;
    QWidget *m_localPanel  = nullptr;

    // Relay mode widgets
    QWidget *m_relayPanel  = nullptr;
    QLabel  *m_relayHeader   = nullptr;
    QLabel  *m_relayQrLabel   = nullptr;
    QLabel  *m_relayUrlLabel  = nullptr;
    QLabel  *m_relayStatus    = nullptr;

    QStackedWidget *m_stack = nullptr;

    Mode m_mode       = Mode::Local;
    bool m_quitting   = false;
    bool m_updatingSizes = false;
};
