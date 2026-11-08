#include "ui/QrCode.h"

#include "core/Logger.h"

#include "qrcodegen.hpp"

#include <QBuffer>
#include <QImage>

namespace mr {

QString QrCodeUtil::toDataUrl(const QString &text, int pixelSize, int border)
{
    if (text.isEmpty())
        return {};

    try {
        const qrcodegen::QrCode code = qrcodegen::QrCode::encodeText(text.toUtf8().constData(),
                                                                   qrcodegen::QrCode::Ecc::MEDIUM);
        const int modules = code.getSize();
        const int scale = qMax(1, pixelSize / (modules + border * 2));
        const int size = (modules + border * 2) * scale;

        QImage image(size, size, QImage::Format_ARGB32);
        image.fill(Qt::white);

        for (int y = 0; y < modules; ++y) {
            for (int x = 0; x < modules; ++x) {
                if (!code.getModule(x, y))
                    continue;
                const int px = (x + border) * scale;
                const int py = (y + border) * scale;
                for (int dy = 0; dy < scale; ++dy) {
                    auto *line = reinterpret_cast<QRgb *>(image.scanLine(py + dy));
                    for (int dx = 0; dx < scale; ++dx)
                        line[px + dx] = qRgb(0, 0, 0);
                }
            }
        }

        QByteArray png;
        QBuffer buffer(&png);
        buffer.open(QIODevice::WriteOnly);
        if (!image.save(&buffer, "PNG")) {
            Logger::warn(QStringLiteral("QR code PNG encoding failed"));
            return {};
        }
        return QStringLiteral("data:image/png;base64,") + QString::fromLatin1(png.toBase64());
    } catch (const std::exception &error) {
        Logger::warn(QStringLiteral("QR code generation failed: %1").arg(QString::fromUtf8(error.what())));
        return {};
    }
}

} // namespace mr
