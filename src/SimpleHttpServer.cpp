#include "SimpleHttpServer.h"

#include <QTcpServer>
#include <QTcpSocket>

// ── Embedded HTML template ──────────────────────────────────────────────
// {{WS_PORT}} is replaced at serve-time with the actual WebSocket port.
static const char HTML_TEMPLATE[] = R"html(<!DOCTYPE html>
<html lang="zh">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>弹幕发送</title>
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body { font-family: -apple-system, "Microsoft YaHei", sans-serif;
         background: #1a1a2e; color: #eee; padding: 20px; min-height: 100vh; }
  h3 { margin-bottom: 12px; color: #e94560; }
  h4 { margin: 16px 0 8px; }
  #status { display: flex; align-items: center; gap: 6px; font-size: 13px;
            margin-bottom: 14px; padding: 6px 10px; background: #16213e;
            border-radius: 6px; }
  #status-dot { width: 10px; height: 10px; border-radius: 50%; background: #e74c3c;
                display: inline-block; flex-shrink: 0; }
  #status-dot.connected { background: #4caf50; }
  #toast { display: none; background: #e74c3c; color: #fff; padding: 8px 14px;
           border-radius: 6px; font-size: 14px; margin-bottom: 10px; }
  #input-box { display: flex; gap: 8px; margin-bottom: 4px; }
  #msg { flex: 1; padding: 10px 12px; font-size: 16px; border: 1px solid #0f3460;
         border-radius: 8px; background: #16213e; color: #eee; outline: none; }
  #msg:focus { border-color: #e94560; }
  button { padding: 10px 20px; font-size: 16px; border: none; border-radius: 8px;
           background: #e94560; color: #fff; cursor: pointer; font-weight: bold; }
  button:active { background: #c23152; }
  #history { border: 1px solid #0f3460; border-radius: 8px; padding: 10px;
             height: 220px; overflow-y: auto; background: #16213e; }
  .danmaku-item { margin: 4px 0; padding: 4px 8px; color: #ddd;
                  border-bottom: 1px solid #0f3460; word-break: break-all; }
</style>
</head>
<body>
  <h3>发送弹幕</h3>
  <div id="status">
    <span id="status-dot"></span>
    <span id="status-text">连接中...</span>
  </div>
  <div id="toast"></div>
  <div id="input-box">
    <input type="text" id="msg" placeholder="输入弹幕..." maxlength="60" autofocus>
    <button onclick="send()">发送</button>
  </div>
  <h4>最近弹幕</h4>
  <div id="history"></div>

  <script>
    const wsUrl = 'ws://' + window.location.hostname + ':{{WS_PORT}}';
    var ws;
    var historyDiv = document.getElementById('history');
    var statusDot = document.getElementById('status-dot');
    var statusText = document.getElementById('status-text');
    var toast = document.getElementById('toast');
    var historyMax = 10;
    var historyList = [];

    function showToast(msg) {
      toast.textContent = msg;
      toast.style.display = 'block';
      setTimeout(function() { toast.style.display = 'none'; }, 3000);
    }

    function setConnected(ok) {
      if (ok) {
        statusDot.className = 'connected';
        statusText.textContent = '已连接';
      } else {
        statusDot.className = '';
        statusText.textContent = '已断开，正在重连...';
      }
    }

    function connect() {
      ws = new WebSocket(wsUrl);
      ws.onopen = function() { setConnected(true); };
      ws.onmessage = function(event) { addHistory(event.data); };
      ws.onclose = function() {
        setConnected(false);
        setTimeout(connect, 3000);
      };
      ws.onerror = function() {};
    }

    function send() {
      var input = document.getElementById('msg');
      var text = input.value.trim();
      if (!text) return;
      if (!ws || ws.readyState !== WebSocket.OPEN) {
        showToast('连接已断开，请稍候或刷新页面');
        return;
      }
      ws.send(text);
      input.value = '';
      input.focus();
    }

    function addHistory(text) {
      historyList.push(text);
      if (historyList.length > historyMax) historyList.shift();
      historyDiv.innerHTML = historyList.map(function(t) {
        return '<div class="danmaku-item">' + escapeHtml(t) + '</div>';
      }).join('');
      historyDiv.scrollTop = historyDiv.scrollHeight;
    }

    function escapeHtml(s) {
      return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
    }

    document.getElementById('msg').addEventListener('keydown', function(e) {
      if (e.key === 'Enter') send();
    });

    connect();
  </script>
</body>
</html>
)html";

// ── Server implementation ───────────────────────────────────────────────

SimpleHttpServer::SimpleHttpServer(QObject *parent)
    : QObject(parent)
{
}

SimpleHttpServer::~SimpleHttpServer()
{
    close();
}

bool SimpleHttpServer::start(quint16 httpPort, quint16 wsPort)
{
    if (m_server) close();

    m_server = new QTcpServer(this);
    if (!m_server->listen(QHostAddress::Any, httpPort))
        return false;

    m_httpPort = httpPort;
    m_wsPort   = wsPort;

    connect(m_server, &QTcpServer::newConnection,
            this, &SimpleHttpServer::onNewConnection);
    return true;
}

void SimpleHttpServer::close()
{
    if (m_server) {
        m_server->close();
        m_server->deleteLater();
        m_server = nullptr;
    }
}

void SimpleHttpServer::onNewConnection()
{
    while (QTcpSocket *sock = m_server->nextPendingConnection()) {
        connect(sock, &QTcpSocket::readyRead, this, [this, sock]() {
            // We only care about GET /
            const QByteArray req = sock->readAll();
            if (req.startsWith("GET /")) {
                const QByteArray resp = buildResponse();
                sock->write(resp);
            }
            sock->disconnectFromHost();
            sock->deleteLater();
        });
    }
}

QByteArray SimpleHttpServer::buildResponse() const
{
    QByteArray html = HTML_TEMPLATE;
    html.replace("{{WS_PORT}}", QByteArray::number(m_wsPort));

    const QByteArray header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html; charset=utf-8\r\n"
        "Content-Length: " + QByteArray::number(html.size()) + "\r\n"
        "Connection: close\r\n"
        "\r\n";

    return header + html;
}
