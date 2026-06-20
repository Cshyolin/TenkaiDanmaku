#include "QRCodeGenerator.h"

#include <QPainter>

extern "C" {
#include <qrencode.h>
}

namespace QRCodeGenerator {

QPixmap generate(const QString &url, int size)
{
    if (url.isEmpty() || size <= 0)
        return {};

    // QRcode *QRcode_encodeString(const char *str, int version,
    //     QRecLevel level, QRencodeMode hint, int casesensitive)
    const QByteArray utf8 = url.toUtf8();
    QRcode *qr = QRcode_encodeString(utf8.constData(), 0,
                                     QR_ECLEVEL_M, QR_MODE_8, 1);
    if (!qr)
        return {};

    const int modules = qr->width;
    const int margin   = 2;
    const int total    = modules + margin * 2;
    const double scale = static_cast<double>(size) / total;

    QPixmap pixmap(size, size);
    pixmap.fill(Qt::white);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, false);

    // Black modules
    painter.setBrush(Qt::black);
    painter.setPen(Qt::NoPen);
    for (int row = 0; row < modules; ++row) {
        for (int col = 0; col < modules; ++col) {
            if (qr->data[row * modules + col] & 0x01) {
                const int x = static_cast<int>((col + margin) * scale);
                const int y = static_cast<int>((row + margin) * scale);
                const int w = static_cast<int>(scale + 0.5);
                const int h = static_cast<int>(scale + 0.5);
                // Clamp to avoid overflow on last pixel
                painter.drawRect(x, y,
                    qMin(w, size - x),
                    qMin(h, size - y));
            }
        }
    }
    painter.end();
    QRcode_free(qr);
    return pixmap;
}

} // namespace QRCodeGenerator
