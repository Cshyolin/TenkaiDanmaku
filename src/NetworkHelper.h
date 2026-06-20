#pragma once

#include <QString>
#include <QHostAddress>

namespace NetworkHelper {

/// Return the first global unicast IPv6 address, or empty string.
QString getGlobalIPv6();

/// Return the first non-loopback LAN IPv4 address, or empty string.
QString getLocalIPv4();

/// Build an HTTP URL from IP and port, adding brackets for IPv6.
QString buildUrl(const QString &ip, quint16 port);

/// Check whether a TCP port is available for listening.
bool isPortAvailable(quint16 port);

/// Try to find a pair of adjacent available ports (http, http+1).
/// Returns true on success, false after exhausting attempts.
bool findAvailablePorts(quint16 &httpPort, quint16 &wsPort, int maxAttempts = 10);

}
