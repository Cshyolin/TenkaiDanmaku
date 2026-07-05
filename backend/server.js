const http = require('http');
const fs = require('fs');
const path = require('path');
const { WebSocketServer } = require('ws');

// ── Config ────────────────────────────────────────────────────────────────
const PORT              = parseInt(process.env.PORT || '3000', 10);
const PATH_PREFIX       = process.env.PATH_PREFIX || '';
const AUTH_TIMEOUT_MS   = parseInt(process.env.AUTH_TIMEOUT_MS || '5000', 10);
const RATE_LIMIT_MS     = parseInt(process.env.RATE_LIMIT_MS || '1000', 10);
const MAX_CONNS_PER_IP  = parseInt(process.env.MAX_CONNS_PER_IP || '5', 10);
const PING_INTERVAL_MS  = parseInt(process.env.PING_INTERVAL_MS || '30000', 10);
const GROUP_TTL_MS      = parseInt(process.env.GROUP_TTL_MS || '60000', 10);

// ── State ─────────────────────────────────────────────────────────────────
const groups = new Map(); // token → Set<Client>
let groupTimers = new Map(); // token → setTimeout for cleanup

// ── Helpers ──────────────────────────────────────────────────────────────

function parseColor(text) {
    const m = text.match(/^(.*?)#([0-9A-Fa-f]{6})$/);
    if (m) return { text: m[1].trim(), color: '#' + m[2].toUpperCase() };
    return { text, color: '#FFFFFF' };
}

function sendJson(ws, obj) {
    if (ws.readyState === 1) ws.send(JSON.stringify(obj));
}

function ipCount(ip) {
    let n = 0;
    for (const [, set] of groups) {
        for (const c of set) { if (c.ip === ip) n++; }
    }
    return n;
}

function removeClient(client) {
    const set = groups.get(client.token);
    if (!set) return;
    set.delete(client);
    if (set.size === 0) {
        // Delayed cleanup
        groupTimers.set(client.token, setTimeout(() => {
            const s = groups.get(client.token);
            if (!s || s.size === 0) {
                groups.delete(client.token);
                groupTimers.delete(client.token);
                console.log(`[cleanup] group ${client.token.slice(0,8)}... removed`);
            }
        }, GROUP_TTL_MS));
    }
}

function broadcast(token, msg) {
    const set = groups.get(token);
    if (!set) return;
    const payload = JSON.stringify(msg);
    for (const c of set) {
        if (c.ws.readyState === 1) c.ws.send(payload);
    }
}

// ── HTTP server (static files only) ──────────────────────────────────────

const MIME = {
    '.html': 'text/html; charset=utf-8',
    '.css': 'text/css',
    '.js': 'application/javascript',
    '.png': 'image/png',
    '.svg': 'image/svg+xml',
    '.ico': 'image/x-icon',
};

const server = http.createServer((req, res) => {
    if (req.method !== 'GET') { res.writeHead(405); res.end(); return; }

    // Strip platform path prefix if set, then query string
    let urlPath = req.url.split('?')[0];
    if (PATH_PREFIX && urlPath.startsWith(PATH_PREFIX))
        urlPath = urlPath.slice(PATH_PREFIX.length) || '/';

    // Let WebSocket upgrade requests bypass the static file handler
    if (urlPath === '/ws' || urlPath.endsWith('/ws')) return;

    if (urlPath === '/' || urlPath === '') urlPath = '/index.html';

    let filePath = path.join(__dirname, 'public', urlPath);

    // Prevent directory traversal
    if (!filePath.startsWith(path.join(__dirname, 'public'))) {
        res.writeHead(403); res.end(); return;
    }

    const ext = path.extname(filePath);
    fs.readFile(filePath, (err, data) => {
        if (err) {
            res.writeHead(404, { 'Content-Type': 'text/html; charset=utf-8' });
            res.end('<html><body><h1>404 Not Found</h1></body></html>');
        } else {
            res.writeHead(200, { 'Content-Type': MIME[ext] || 'application/octet-stream' });
            res.end(data);
        }
    });
});

// ── WebSocket ────────────────────────────────────────────────────────────

const wss = new WebSocketServer({ server });

wss.on('connection', (ws, req) => {
    const ip = req.headers['x-forwarded-for']?.split(',')[0].trim()
            || req.socket.remoteAddress;

    // IP connection limit
    if (ipCount(ip) >= MAX_CONNS_PER_IP) {
        sendJson(ws, { type: 'error', message: 'too many connections' });
        ws.close(4000);
        return;
    }

    const client = { ws, ip, token: null, lastMsg: 0, authed: false };
    let authTimer = null;

    // Auth timeout
    authTimer = setTimeout(() => {
        if (!client.authed) {
            sendJson(ws, { type: 'error', message: 'auth timeout' });
            ws.close(4001);
        }
    }, AUTH_TIMEOUT_MS);

    ws.on('message', (raw) => {
        let msg;
        try { msg = JSON.parse(raw.toString()); }
        catch { return; }

        // ── Auth ──────────────────────────────────────────────────────
        if (msg.type === 'auth') {
            if (client.authed) return;

            const token = msg.token;
            // Must be a valid UUID v4 format
            if (!token || !/^[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}$/i.test(token)) {
                sendJson(ws, { type: 'error', message: 'invalid token' });
                ws.close(4002);
                return;
            }

            client.token = token;
            client.authed = true;
            clearTimeout(authTimer);

            // Cancel delayed cleanup for this group
            const t = groupTimers.get(token);
            if (t) { clearTimeout(t); groupTimers.delete(token); }

            let set = groups.get(token);
            if (!set) {
                set = new Set();
                groups.set(token, set);
                console.log(`[group] ${token.slice(0,8)}... created`);
            }
            set.add(client);

            sendJson(ws, { type: 'auth_ok' });
            console.log(`[auth] ${token.slice(0,8)}... from ${ip} (group size: ${set.size})`);
            return;
        }

        // ── Not authed ────────────────────────────────────────────────
        if (!client.authed) {
            sendJson(ws, { type: 'error', message: 'not authenticated' });
            return;
        }

        // ── Danmaku ───────────────────────────────────────────────────
        if (msg.type === 'danmaku') {
            const now = Date.now();
            if (now - client.lastMsg < RATE_LIMIT_MS) return;
            client.lastMsg = now;

            let text = String(msg.text || '').slice(0, 200);
            const { text: cleanText, color } = parseColor(text);

            broadcast(client.token, {
                type: 'danmaku',
                text: cleanText,
                color: color,
            });
        }
    });

    ws.on('close', () => {
        clearTimeout(authTimer);
        if (client.token) removeClient(client);
    });

    ws.on('error', () => {});
});

// ── Ping ─────────────────────────────────────────────────────────────────

const pingInterval = setInterval(() => {
    for (const [, set] of groups) {
        for (const c of set) {
            if (c.ws.readyState === 1) c.ws.ping();
        }
    }
}, PING_INTERVAL_MS);

wss.on('close', () => clearInterval(pingInterval));

// ── Start ────────────────────────────────────────────────────────────────

server.listen(PORT, () => {
    console.log(`[start] TenkaiDanmaku relay on http://0.0.0.0:${PORT}`);
    console.log(`[start] WebSocket at ws://0.0.0.0:${PORT}/ws`);
});
