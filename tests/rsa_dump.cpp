#include "core/Crypto.h"
#include <QCoreApplication>
#include <QDebug>
int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);
    const QByteArray pem =
        "-----BEGIN PUBLIC KEY-----\n"
        "MIGfMA0GCSqGSIb3DQEBAQUAA4GNADCBiQKBgQDgtQn2JZ34ZC28NWYpAUd98iZ37BUrX/aKzmFbt7clFSs6sXqHauqKWqdtLkF2KexO40H1YTX8z2lSgBBOAxLsvaklV8k4cBFK9snQXE9/DDaFt6Rr7iVZMldczhC0JNgTz+SHXT6CBHuX3e9SdB1Ua44oncaTWz7OBGLbCiK45wIDAQAB\n"
        "-----END PUBLIC KEY-----\n";
    const auto key = mr::Crypto::rsaPublicKeyFromPem(pem);
    qInfo().noquote() << "N" << mr::Crypto::hexLower(key.modulus);
    qInfo().noquote() << "E" << mr::Crypto::hexLower(key.exponent);
    const QByteArray raw = mr::Crypto::rsaEncryptRaw(key, QByteArray("0123456789abcdef"));
    qInfo().noquote() << "RAW" << mr::Crypto::hexLower(raw);
    return 0;
}
