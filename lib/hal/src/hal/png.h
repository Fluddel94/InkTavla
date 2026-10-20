#pragma once

// Minimal PNG writer for 1-bit frames (PC only: previews, tests and the
// simulated display). The image data is stored uncompressed, so the output
// is deterministic and can be compared byte by byte with a reference file.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>

namespace hal::png {

inline uint32_t crc32(const uint8_t* data, size_t length, uint32_t crc = 0) {
  crc = ~crc;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

inline void put32(std::string& out, uint32_t v) {
  for (int shift = 24; shift >= 0; shift -= 8) out += static_cast<char>((v >> shift) & 0xFF);
}

inline void chunk(std::string& out, const char* type, const std::string& data) {
  put32(out, static_cast<uint32_t>(data.size()));
  std::string body = std::string(type, 4) + data;
  out += body;
  put32(out, crc32(reinterpret_cast<const uint8_t*>(body.data()), body.size()));
}

// PNG (greyscale, 1 bit, white = 1) of a 1-bit frame (set bit = black).
inline std::string encode(const uint8_t* frame, uint16_t width, uint16_t height, size_t stride) {
  const size_t row_bytes = (width + 7u) / 8u;
  // Raw scanlines: filter byte 0 + row, inverted (PNG: 0 = black).
  std::string raw;
  raw.reserve(height * (row_bytes + 1));
  for (int y = 0; y < height; y++) {
    raw += '\0';
    const uint8_t* row = frame + y * stride;
    for (size_t i = 0; i < row_bytes; i++) raw += static_cast<char>(~row[i]);
  }
  // zlib stream with stored (uncompressed) deflate blocks.
  std::string z = "\x78\x01";
  for (size_t pos = 0; pos < raw.size() || pos == 0;) {
    const size_t n = std::min<size_t>(65535, raw.size() - pos);
    const bool last = pos + n >= raw.size();
    z += static_cast<char>(last ? 1 : 0);
    z += static_cast<char>(n & 0xFF);
    z += static_cast<char>(n >> 8);
    z += static_cast<char>(~n & 0xFF);
    z += static_cast<char>((~n >> 8) & 0xFF);
    z += raw.substr(pos, n);
    pos += n;
    if (last) break;
  }
  uint32_t a = 1, b = 0;
  for (unsigned char c : raw) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  put32(z, (b << 16) | a);

  std::string png = "\x89PNG\r\n\x1a\n";
  std::string header;
  put32(header, width);
  put32(header, height);
  header += std::string("\x01\x00\x00\x00\x00", 5);  // bit depth 1, greyscale
  chunk(png, "IHDR", header);
  chunk(png, "IDAT", z);
  chunk(png, "IEND", "");
  return png;
}

// The PNG with only its critical chunks (IHDR, IDAT, IEND, ...). Tools on
// the PC may add ancillary chunks to a file (e.g. content credentials)
// without changing the image; comparing only the critical chunks keeps a
// byte-for-byte check meaningful.
inline std::string criticalChunks(const std::string& png) {
  if (png.size() < 8) return png;
  std::string out = png.substr(0, 8);
  size_t pos = 8;
  while (pos + 12 <= png.size()) {
    const uint32_t length = (static_cast<uint32_t>(static_cast<uint8_t>(png[pos])) << 24) |
                            (static_cast<uint32_t>(static_cast<uint8_t>(png[pos + 1])) << 16) |
                            (static_cast<uint32_t>(static_cast<uint8_t>(png[pos + 2])) << 8) |
                            static_cast<uint32_t>(static_cast<uint8_t>(png[pos + 3]));
    const size_t total = 12 + static_cast<size_t>(length);
    if (pos + total > png.size()) return png;          // broken file: compare as is
    const bool critical = (png[pos + 4] & 0x20) == 0;  // upper-case first letter
    if (critical) out += png.substr(pos, total);
    pos += total;
  }
  return out;
}

}  // namespace hal::png
