#include "DanmakuWindow.h"

#include <QGraphicsScene>
#include <QGraphicsTextItem>
#include <QPropertyAnimation>
#include <QVariantAnimation>
#include <QRandomGenerator>
#include <QScreen>
#include <QGuiApplication>
#include <QFontMetrics>
#include <QResizeEvent>
#include <QWebSocket>
#include <QTimer>

// ── Constructor / Destructor ─────────────────────────────────────────────

DanmakuWindow::DanmakuWindow(QWidget *parent)
    : QGraphicsView(parent)
{
    setWindowFlags(Qt::FramelessWindowHint
                   | Qt::WindowStaysOnTopHint
                   | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setStyleSheet("background: transparent; border: none;");
    setRenderHint(QPainter::Antialiasing, true);

    m_scene = new QGraphicsScene(this);
    setScene(m_scene);

    m_font = QFont("Microsoft YaHei", 24);
    m_client = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);

    connect(m_client, &QWebSocket::connected,
            this, &DanmakuWindow::onConnected);
    connect(m_client, &QWebSocket::disconnected,
            this, &DanmakuWindow::onDisconnected);
    connect(m_client, &QWebSocket::textMessageReceived,
            this, &DanmakuWindow::onWsMessage);

    // Monitor changes
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens) {
        connect(s, &QScreen::geometryChanged,
                this, &DanmakuWindow::onScreenGeometryChanged);
    }
    QGuiApplication *app = qobject_cast<QGuiApplication*>(QGuiApplication::instance());
    if (app) {
        connect(app, &QGuiApplication::screenAdded,
                this, [this](QScreen *s) {
            connect(s, &QScreen::geometryChanged,
                    this, &DanmakuWindow::onScreenGeometryChanged);
            onScreenGeometryChanged();
        });
        connect(app, &QGuiApplication::screenRemoved,
                this, [this](QScreen *) { onScreenGeometryChanged(); });
    }

    recalcScene();
    recalcTracks();
}

DanmakuWindow::~DanmakuWindow()
{
    if (m_client->state() != QAbstractSocket::UnconnectedState)
        m_client->close();
}

// ── Public interface ─────────────────────────────────────────────────────

void DanmakuWindow::connectToServer(quint16 wsPort)
{
    m_wsPort = wsPort;
    const QString url = QString("ws://127.0.0.1:%1").arg(wsPort);
    m_client->open(QUrl(url));
}

void DanmakuWindow::setDanmakuFont(const QFont &font)
{
    m_font = font;
    recalcTracks();
}

void DanmakuWindow::setSpeedRange(int minMs, int maxMs)
{
    m_minAnimMs = minMs;
    m_maxAnimMs = maxMs;
}

void DanmakuWindow::setTrackAreaRatio(double ratio)
{
    m_trackAreaRatio = qBound(0.1, ratio, 0.5);
    recalcTracks();
}

// ── Screen geometry ─────────────────────────────────────────────────────

void DanmakuWindow::recalcScene()
{
    QRect total;
    const auto screens = QGuiApplication::screens();
    for (QScreen *s : screens)
        total = total.united(s->geometry());

    setGeometry(total);
    m_scene->setSceneRect(0, 0, total.width(), total.height());
}

void DanmakuWindow::recalcTracks()
{
    const int areaHeight = static_cast<int>(m_scene->height() * m_trackAreaRatio);
    const int trackH = QFontMetrics(m_font).height() + m_margin;
    m_trackCount = qMax(1, areaHeight / trackH);
    m_trackLastUsed.clear();
    m_trackLastUsed.reserve(m_trackCount);
    for (int i = 0; i < m_trackCount; ++i)
        m_trackLastUsed.append(3); // all available initially
}

void DanmakuWindow::onScreenGeometryChanged()
{
    recalcScene();
    recalcTracks();
}

void DanmakuWindow::resizeEvent(QResizeEvent *event)
{
    QGraphicsView::resizeEvent(event);
    recalcTracks();
}

// ── Track selection ──────────────────────────────────────────────────────

int DanmakuWindow::selectTrack()
{
    QList<int> candidates;
    for (int i = 0; i < m_trackCount; ++i) {
        if (m_trackLastUsed[i] > 2)
            candidates.append(i);
    }
    if (candidates.isEmpty()) {
        for (int i = 0; i < m_trackCount; ++i)
            candidates.append(i);
    }

    const int idx = candidates[QRandomGenerator::global()->bounded(candidates.size())];

    // Update usage counters
    for (int i = 0; i < m_trackCount; ++i)
        m_trackLastUsed[i] = qMin(m_trackLastUsed[i] + 1, 3);
    m_trackLastUsed[idx] = 0;

    return idx;
}

// ── Colour parsing ──────────────────────────────────────────────────────

QPair<QString, QColor> DanmakuWindow::parseDanmaku(const QString &raw)
{
    // Check for trailing #RRGGBB
    if (raw.length() >= 7 && raw.at(raw.length() - 7) == QChar('#')) {
        const QString hex = raw.right(7);
        const QColor color(hex);
        if (color.isValid()) {
            const QString text = raw.left(raw.length() - 7).trimmed();
            if (!text.isEmpty())
                return {text, color};
        }
    }
    // Default: white
    return {raw, QColor(255, 255, 255)};
}

// ── Danmaku animation ────────────────────────────────────────────────────

void DanmakuWindow::addDanmaku(const QString &text, const QString & /*senderIp*/)
{
    auto [displayText, color] = parseDanmaku(text);

    auto *item = new QGraphicsTextItem(displayText);
    item->setFont(m_font);
    item->setDefaultTextColor(color);

    const int track = selectTrack();
    const int trackH = QFontMetrics(m_font).height() + m_margin;
    const int y = track * trackH;

    item->setPos(m_scene->width(), y);
    m_scene->addItem(item);

    const int duration = m_minAnimMs
        + QRandomGenerator::global()->bounded(m_maxAnimMs - m_minAnimMs + 1);

    // Animate "pos" property (QPointF) — only x changes, y is constant
    auto *anim = new QPropertyAnimation(item, "pos");
    anim->setDuration(duration);
    anim->setStartValue(QPointF(m_scene->width(), y));
    anim->setEndValue(QPointF(-item->boundingRect().width(), y));
    connect(anim, &QPropertyAnimation::finished,
            item, &QGraphicsObject::deleteLater);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

// ── WebSocket client slots ───────────────────────────────────────────────

void DanmakuWindow::onConnected()
{
    // Optional: log reconnection
}

void DanmakuWindow::onDisconnected()
{
    // Auto-reconnect after 3 seconds
    QTimer::singleShot(3000, this, [this]() {
        if (m_client && m_client->state() == QAbstractSocket::UnconnectedState)
            connectToServer(m_wsPort);
    });
}

void DanmakuWindow::onWsMessage(const QString &message)
{
    addDanmaku(message, QString());
}
