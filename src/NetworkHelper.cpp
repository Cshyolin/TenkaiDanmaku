#include "NetworkHelper.h"

#include <QNetworkInterface>
#include <QTcpServer>

namespace NetworkHelper {

QString getGlobalIPv6()
{
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;

        const QString name = iface.humanReadableName().toLower();
        if (name.contains("virtual") || name.contains("vmnet")
            || name.contains("vbox") || name.contains("docker"))
            continue;

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() != QAbstractSocket::IPv6Protocol)
                continue;
            if (ip.isLoopback() || ip.isLinkLocal() || ip.isMulticast())
                continue;
            if (ip.isUniqueLocalUnicast())   // fc00::/7
                continue;
            if (ip.scopeId().isEmpty())
                return ip.toString();        // e.g. 240e:...
        }
    }
    return {};
}

QString getLocalIPv4()
{
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface &iface : ifaces) {
        if (!(iface.flags() & QNetworkInterface::IsUp)
            || (iface.flags() & QNetworkInterface::IsLoopBack))
            continue;

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            const QHostAddress ip = entry.ip();
            if (ip.protocol() == QAbstractSocket::IPv4Protocol
                && !ip.isLoopback()
                && ip != QHostAddress::LocalHost)
                return ip.toString();
        }
    }
    return {};
}

QString buildUrl(const QString &ip, quint16 port)
{
    QHostAddress addr(ip);
    const bool isV6 = (addr.protocol() == QAbstractSocket::IPv6Protocol);
    if (isV6)
        return QString("http://[%1]:%2").arg(ip).arg(port);
    else
        return QString("http://%1:%2").arg(ip).arg(port);
}

bool isPortAvailable(quint16 port)
{
    QTcpServer server;
    return server.listen(QHostAddress::Any, port);
    // server goes out of scope → socket closed → port released
}

bool findAvailablePorts(quint16 &httpPort, quint16 &wsPort, int maxAttempts)
{
    for (int i = 0; i < maxAttempts; ++i) {
        if (isPortAvailable(httpPort) && isPortAvailable(wsPort))
            return true;
        httpPort += 2;
        wsPort  += 2;
    }
    return false;
}

} // namespace NetworkHelper
