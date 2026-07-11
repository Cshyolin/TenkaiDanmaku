#include "ControlPanel.h"
#include "QRCodeGenerator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QStackedWidget>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QApplication>
#include <QScreen>
#include <QTimer>

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("TenkaiDanmaku");
    setMinimumSize(580, 340);

    if (QScreen *s = QApplication::primaryScreen()) {
        const QRect g = s->availableGeometry();
        move(g.center() - rect().center());
    }

    setupUi();
}

void ControlPanel::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(16, 12, 16, 12);

    // Title
    auto *title = new QLabel("Tenkai Danmaku");
    QFont titleFont = title->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    title->setFont(titleFont);
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    // ── Mode selector ────────────────────────────────────────────────
    auto *modeRow = new QHBoxLayout;
    modeRow->addStretch();
    auto *modeLabel = new QLabel("模式:");
    modeLabel->setStyleSheet("color: var(--text-secondary);");
    m_modeCombo = new QComboBox;
    m_modeCombo->addItem("局域网直连");
    m_modeCombo->addItem("公网中继");
    m_modeCombo->setFixedWidth(140);
    modeRow->addWidget(modeLabel);
    modeRow->addWidget(m_modeCombo);
    modeRow->addStretch();
    mainLayout->addLayout(modeRow);

    connect(m_modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        Mode newMode = (idx == 1) ? Mode::Relay : Mode::Local;
        if (newMode != m_mode) {
            m_mode = newMode;
            m_stack->setCurrentIndex(idx);
            emit modeChanged(m_mode);
        }
    });

    // ── Stacked widget: Local | Relay ────────────────────────────────
    m_stack = new QStackedWidget;

    // ── Page 0: Local (dual QR) ──────────────────────────────────────
    m_localPanel = new QWidget;
    auto *localRow = new QHBoxLayout(m_localPanel);
    localRow->setSpacing(20);
    localRow->setContentsMargins(0, 0, 0, 0);

    const int qrSize = 2048;

    // IPv6
    m_ipv6Group = new QWidget;
    auto *v6Lay = new QVBoxLayout(m_ipv6Group);
    v6Lay->setContentsMargins(0, 0, 0, 0);
    v6Lay->setSpacing(4);
    auto *v6Hdr = new QLabel("IPv6 公网");
    m_ipv6Header = v6Hdr;
    v6Hdr->setStyleSheet("font-weight: bold; color: #4caf50;");
    v6Hdr->setAlignment(Qt::AlignCenter);
    v6Lay->addWidget(v6Hdr);
    m_ipv6QrLabel = new QLabel;
    m_ipv6QrLabel->setFixedSize(200, 200);
    m_ipv6QrLabel->setScaledContents(true);
    m_ipv6QrLabel->setAlignment(Qt::AlignCenter);
    m_ipv6QrLabel->setStyleSheet("border: 2px solid #4caf50; border-radius: 4px;");
    v6Lay->addWidget(m_ipv6QrLabel, 0, Qt::AlignCenter);
    m_ipv6UrlLabel = new QLabel;
    m_ipv6UrlLabel->setWordWrap(true);
    m_ipv6UrlLabel->setMaximumHeight(36);
    m_ipv6UrlLabel->setAlignment(Qt::AlignCenter);
    m_ipv6UrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ipv6UrlLabel->setStyleSheet("color: #ccc; font-size: 11px;");
    v6Lay->addWidget(m_ipv6UrlLabel);
    m_ipv6Status = new QLabel;
    m_ipv6Status->setAlignment(Qt::AlignCenter);
    m_ipv6Status->setStyleSheet("color: #888; font-size: 11px;");
    v6Lay->addWidget(m_ipv6Status);
    localRow->addWidget(m_ipv6Group);

    // IPv4
    m_ipv4Group = new QWidget;
    auto *v4Lay = new QVBoxLayout(m_ipv4Group);
    v4Lay->setContentsMargins(0, 0, 0, 0);
    v4Lay->setSpacing(4);
    auto *v4Hdr = new QLabel("IPv4 局域网");
    m_ipv4Header = v4Hdr;
    v4Hdr->setStyleSheet("font-weight: bold; color: #2196f3;");
    v4Hdr->setAlignment(Qt::AlignCenter);
    v4Lay->addWidget(v4Hdr);
    m_ipv4QrLabel = new QLabel;
    m_ipv4QrLabel->setFixedSize(200, 200);
    m_ipv4QrLabel->setScaledContents(true);
    m_ipv4QrLabel->setAlignment(Qt::AlignCenter);
    m_ipv4QrLabel->setStyleSheet("border: 2px solid #2196f3; border-radius: 4px;");
    v4Lay->addWidget(m_ipv4QrLabel, 0, Qt::AlignCenter);
    m_ipv4UrlLabel = new QLabel;
    m_ipv4UrlLabel->setWordWrap(true);
    m_ipv4UrlLabel->setMaximumHeight(36);
    m_ipv4UrlLabel->setAlignment(Qt::AlignCenter);
    m_ipv4UrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ipv4UrlLabel->setStyleSheet("color: #ccc; font-size: 11px;");
    v4Lay->addWidget(m_ipv4UrlLabel);
    m_ipv4Status = new QLabel;
    m_ipv4Status->setAlignment(Qt::AlignCenter);
    m_ipv4Status->setStyleSheet("color: #888; font-size: 11px;");
    v4Lay->addWidget(m_ipv4Status);
    localRow->addWidget(m_ipv4Group);

    m_stack->addWidget(m_localPanel); // index 0

    // ── Page 1: Relay (single QR) ────────────────────────────────────
    m_relayPanel = new QWidget;
    auto *relayLay = new QVBoxLayout(m_relayPanel);
    relayLay->setAlignment(Qt::AlignCenter);

    auto *relayHdr = new QLabel("公网中继");
    m_relayHeader = relayHdr;
    relayHdr->setStyleSheet("font-weight: bold; color: #e94560; font-size: 14px;");
    relayHdr->setAlignment(Qt::AlignCenter);
    relayLay->addWidget(relayHdr);

    m_relayQrLabel = new QLabel;
    m_relayQrLabel->setFixedSize(200, 200);
    m_relayQrLabel->setScaledContents(true);
    m_relayQrLabel->setAlignment(Qt::AlignCenter);
    m_relayQrLabel->setStyleSheet("border: 2px solid #e94560; border-radius: 4px;");
    relayLay->addWidget(m_relayQrLabel, 0, Qt::AlignCenter);

    m_relayUrlLabel = new QLabel;
    m_relayUrlLabel->setWordWrap(true);
    m_relayUrlLabel->setMaximumHeight(36);
    m_relayUrlLabel->setAlignment(Qt::AlignCenter);
    m_relayUrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_relayUrlLabel->setStyleSheet("color: #ccc; font-size: 11px;");
    relayLay->addWidget(m_relayUrlLabel);

    m_relayStatus = new QLabel;
    m_relayStatus->setAlignment(Qt::AlignCenter);
    m_relayStatus->setStyleSheet("color: #888; font-size: 11px;");
    relayLay->addWidget(m_relayStatus);

    // Default placeholder
    m_relayQrLabel->setPixmap(createPlaceholder("请先配置中继\n服务器地址", qrSize));

    m_stack->addWidget(m_relayPanel); // index 1

    mainLayout->addWidget(m_stack, 1);
    mainLayout->addStretch();

    // Hint
    auto *hint = new QLabel("点击 ✕ 将最小化到系统托盘");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #666; font-size: 11px;");
    mainLayout->addWidget(hint);

    // Init
    setIPv6Available(false);
    setIPv4Available(false);
}

// ── Mode ─────────────────────────────────────────────────────────────────

void ControlPanel::setMode(Mode mode)
{
    m_mode = mode;
    m_modeCombo->setCurrentIndex(mode == Mode::Relay ? 1 : 0);
    m_stack->setCurrentIndex(mode == Mode::Relay ? 1 : 0);
}

// ── Local setters ────────────────────────────────────────────────────────

void ControlPanel::setIPv6Url(const QString &url)
{
    m_ipv6UrlLabel->setText(url);
    QPixmap qr = QRCodeGenerator::generate(url, 1024);
    if (!qr.isNull()) m_ipv6QrLabel->setPixmap(qr);
}

void ControlPanel::setIPv4Url(const QString &url)
{
    m_ipv4UrlLabel->setText(url);
    QPixmap qr = QRCodeGenerator::generate(url, 1024);
    if (!qr.isNull()) m_ipv4QrLabel->setPixmap(qr);
}

void ControlPanel::setIPv6Available(bool avail)
{
    if (!avail) {
        m_ipv6QrLabel->setPixmap(createPlaceholder("当前网络不支持\nIPv6", 1024));
        m_ipv6UrlLabel->setText("不可用");
        m_ipv6Status->setText("当前网络不支持");
    } else {
        m_ipv6Status->setText("已就绪");
    }
}

void ControlPanel::setIPv4Available(bool avail)
{
    if (!avail) {
        m_ipv4QrLabel->setPixmap(createPlaceholder("当前网络不支持\nIPv4", 1024));
        m_ipv4UrlLabel->setText("不可用");
        m_ipv4Status->setText("当前网络不支持");
    } else {
        m_ipv4Status->setText("已就绪");
    }
}

void ControlPanel::setQrCodesVisible(bool visible)
{
    m_localPanel->setVisible(visible && m_mode == Mode::Local);
    m_relayPanel->setVisible(visible && m_mode == Mode::Relay);
}

// ── Relay setters ────────────────────────────────────────────────────────

void ControlPanel::setRelayUrl(const QString &url)
{
    m_relayUrlLabel->setText(url);
    QPixmap qr = QRCodeGenerator::generate(url, 1024);
    if (!qr.isNull()) m_relayQrLabel->setPixmap(qr);
}

void ControlPanel::setRelayConnected(bool connected)
{
    if (connected)
        m_relayStatus->setText("已连接");
    else
        m_relayStatus->setText("未连接");
}

// ── Placeholder ──────────────────────────────────────────────────────────

QPixmap ControlPanel::createPlaceholder(const QString &text, int size)
{
    QPixmap pm(size, size);
    pm.fill(QColor(60, 60, 60));
    QPainter p(&pm);
    p.setPen(QColor(150, 150, 150));
    p.setFont(QFont("Microsoft YaHei", 12));
    p.drawText(pm.rect(), Qt::AlignCenter, text);
    p.end();
    return pm;
}

// ── Resize → QR scaling ────────────────────────────────────────────────────

void ControlPanel::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    // Defer to next event loop so QStackedWidget has laid out m_localPanel
    QTimer::singleShot(0, this, [this] { updateQRLabelSizes(); });
}

void ControlPanel::updateQRLabelSizes()
{
    if (m_updatingSizes) return;
    m_updatingSizes = true;

    const int margin  = 16;
    const int spacing = 20;
    const int innerSpacing = 4;
    const int maxQrSide = 1600;

    if (m_mode == Mode::Local) {
        int panelW = m_localPanel->width();
        int panelH = m_localPanel->height();

        int availW = (panelW - margin * 2 - spacing) / 2;
        if (availW < 80) availW = 80;

        int nonQrH = qMax(
            m_ipv6Header->height() + m_ipv6UrlLabel->height() + m_ipv6Status->height(),
            m_ipv4Header->height() + m_ipv4UrlLabel->height() + m_ipv4Status->height()
        ) + innerSpacing * 3;

        int availH = panelH - margin * 2 - nonQrH;
        if (availH < 80) availH = 80;

        int side = qMin(availW, availH);
        side = qMin(side, maxQrSide);

        m_ipv6QrLabel->setFixedSize(side, side);
        m_ipv4QrLabel->setFixedSize(side, side);

    } else {
        int panelW = m_relayPanel->width();
        int panelH = m_relayPanel->height();

        int nonQrH = m_relayHeader->height()
                   + m_relayUrlLabel->height()
                   + m_relayStatus->height()
                   + innerSpacing * 3;

        int availW = panelW - margin * 2;
        int availH = panelH - margin * 2 - nonQrH;
        if (availW < 80) availW = 80;
        if (availH < 80) availH = 80;

        int side = qMin(availW, availH);
        side = qMin(side, maxQrSide);

        m_relayQrLabel->setFixedSize(side, side);
    }

    m_updatingSizes = false;
}

// ── Close → hide to tray ─────────────────────────────────────────────────

void ControlPanel::closeEvent(QCloseEvent *event)
{
    if (m_quitting) { event->accept(); return; }
    hide();
    emit windowHidden();
    event->ignore();
}
