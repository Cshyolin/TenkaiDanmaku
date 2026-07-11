#pragma once

#include <QPixmap>
#include <QString>

namespace QRCodeGenerator {

/// Generate a QR code pixmap for the given URL.
/// @param url   The text to encode.
/// @param size  Output pixmap width/height in pixels (square). Default 1024.
/// @return      A QPixmap containing the QR code, or a null pixmap on failure.
QPixmap generate(const QString &url, int size = 1024);

}
