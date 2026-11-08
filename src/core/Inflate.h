#pragma once

#include <QByteArray>

namespace mr {

/// Minimal zlib (RFC 1950/1951) decompressor.
///
/// Qt only exposes `qUncompress()` which expects Qt's own 4 byte length
/// prefix, so a compact inflate implementation is kept here instead of pulling
/// a third party dependency (or OpenSSL/zlib) into the build.
///
/// Supports stored, fixed-Huffman and dynamic-Huffman deflate blocks.
bool zlibInflate(const QByteArray &input, QByteArray &output);

} // namespace mr
