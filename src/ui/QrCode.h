#pragma once

#include <QString>

namespace mr {

/// Renders QR codes for the scan-to-login flows as a `data:` URL so QML can
/// show them directly (`Image { source: account.qrImage }`).
class QrCodeUtil
{
public:
    /// PNG data URL of `text`, or an empty string when generation fails.
    static QString toDataUrl(const QString &text, int pixelSize = 320, int border = 4);
};

} // namespace mr
