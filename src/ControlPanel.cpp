#include "ControlPanel.h"
#include "QRCodeGenerator.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QCloseEvent>
#include <QPainter>
#include <QFrame>
#include <QApplication>
#include <QScreen>

ControlPanel::ControlPanel(QWidget *parent)
    : QWidget(parent)
{
    setWindowTitle("TenkaiDanmaku");
    setMinimumSize(580, 340);

    // Centre on primary screen
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

    // ── Horizontal QR panel ────────────────────────────────────────────
    auto *qrRow = new QHBoxLayout;
    qrRow->setSpacing(20);

    const int qrSize = 250;

    // ── IPv6 group ──
    m_ipv6Group = new QWidget;
    auto *v6Layout = new QVBoxLayout(m_ipv6Group);
    v6Layout->setContentsMargins(0, 0, 0, 0);
    v6Layout->setSpacing(4);

    auto *v6Header = new QLabel("IPv6 公网");
    v6Header->setStyleSheet("font-weight: bold; color: #4caf50;");
    v6Header->setAlignment(Qt::AlignCenter);
    v6Layout->addWidget(v6Header);

    m_ipv6QrLabel = new QLabel;
    m_ipv6QrLabel->setFixedSize(qrSize, qrSize);
    m_ipv6QrLabel->setAlignment(Qt::AlignCenter);
    m_ipv6QrLabel->setStyleSheet("border: 2px solid #4caf50; border-radius: 4px;");
    v6Layout->addWidget(m_ipv6QrLabel, 0, Qt::AlignCenter);

    m_ipv6UrlLabel = new QLabel;
    m_ipv6UrlLabel->setWordWrap(true);
    m_ipv6UrlLabel->setAlignment(Qt::AlignCenter);
    m_ipv6UrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ipv6UrlLabel->setStyleSheet("color: #ccc; font-size: 11px;");
    v6Layout->addWidget(m_ipv6UrlLabel);

    m_ipv6Status = new QLabel;
    m_ipv6Status->setAlignment(Qt::AlignCenter);
    m_ipv6Status->setStyleSheet("color: #888; font-size: 11px;");
    v6Layout->addWidget(m_ipv6Status);

    qrRow->addWidget(m_ipv6Group);

    // ── IPv4 group ──
    m_ipv4Group = new QWidget;
    auto *v4Layout = new QVBoxLayout(m_ipv4Group);
    v4Layout->setContentsMargins(0, 0, 0, 0);
    v4Layout->setSpacing(4);

    auto *v4Header = new QLabel("IPv4 局域网");
    v4Header->setStyleSheet("font-weight: bold; color: #2196f3;");
    v4Header->setAlignment(Qt::AlignCenter);
    v4Layout->addWidget(v4Header);

    m_ipv4QrLabel = new QLabel;
    m_ipv4QrLabel->setFixedSize(qrSize, qrSize);
    m_ipv4QrLabel->setAlignment(Qt::AlignCenter);
    m_ipv4QrLabel->setStyleSheet("border: 2px solid #2196f3; border-radius: 4px;");
    v4Layout->addWidget(m_ipv4QrLabel, 0, Qt::AlignCenter);

    m_ipv4UrlLabel = new QLabel;
    m_ipv4UrlLabel->setWordWrap(true);
    m_ipv4UrlLabel->setAlignment(Qt::AlignCenter);
    m_ipv4UrlLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_ipv4UrlLabel->setStyleSheet("color: #ccc; font-size: 11px;");
    v4Layout->addWidget(m_ipv4UrlLabel);

    m_ipv4Status = new QLabel;
    m_ipv4Status->setAlignment(Qt::AlignCenter);
    m_ipv4Status->setStyleSheet("color: #888; font-size: 11px;");
    v4Layout->addWidget(m_ipv4Status);

    qrRow->addWidget(m_ipv4Group);

    mainLayout->addLayout(qrRow);
    mainLayout->addStretch();

    // ── Hint ──
    auto *hint = new QLabel("点击 ✕ 将最小化到系统托盘");
    hint->setAlignment(Qt::AlignCenter);
    hint->setStyleSheet("color: #666; font-size: 11px;");
    mainLayout->addWidget(hint);

    // Initial placeholders
    setIPv6Available(false);
    setIPv4Available(false);
}

// ── Setters ──────────────────────────────────────────────────────────────

void ControlPanel::setIPv6Url(const QString &url)
{
    m_ipv6UrlLabel->setText(url);
    QPixmap qr = QRCodeGenerator::generate(url, 250);
    if (!qr.isNull())
        m_ipv6QrLabel->setPixmap(qr);
}

void ControlPanel::setIPv4Url(const QString &url)
{
    m_ipv4UrlLabel->setText(url);
    QPixmap qr = QRCodeGenerator::generate(url, 250);
    if (!qr.isNull())
        m_ipv4QrLabel->setPixmap(qr);
}

void ControlPanel::setQrCodesVisible(bool visible)
{
    m_ipv6Group->setVisible(visible);
    m_ipv4Group->setVisible(visible);
    adjustSize();
}

void ControlPanel::setIPv6Available(bool avail)
{
    if (!avail) {
        m_ipv6QrLabel->setPixmap(createPlaceholder("当前网络不支持\nIPv6", 250));
        m_ipv6UrlLabel->setText("不可用");
        m_ipv6Status->setText("当前网络不支持");
    } else {
        m_ipv6Status->setText("已就绪");
    }
}

void ControlPanel::setIPv4Available(bool avail)
{
    if (!avail) {
        m_ipv4QrLabel->setPixmap(createPlaceholder("当前网络不支持\nIPv4", 250));
        m_ipv4UrlLabel->setText("不可用");
        m_ipv4Status->setText("当前网络不支持");
    } else {
        m_ipv4Status->setText("已就绪");
    }
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

// ── Close → hide to tray ─────────────────────────────────────────────────

void ControlPanel::closeEvent(QCloseEvent *event)
{
    hide();
    emit windowHidden();
    event->ignore();
}
