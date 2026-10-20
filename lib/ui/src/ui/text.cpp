#include "ui/text.h"

#include <algorithm>
#include <vector>

namespace ui {

namespace {

constexpr uint32_t kReplacement = 0xFFFD;

// Removes spaces at both ends.
std::string trimmed(const std::string& text) {
  const size_t first = text.find_first_not_of(' ');
  if (first == std::string::npos) return "";
  const size_t last = text.find_last_not_of(' ');
  return text.substr(first, last - first + 1);
}

// First `count` codepoints of a text.
std::string prefix(std::string_view text, size_t count) {
  size_t pos = 0;
  for (size_t i = 0; i < count && pos < text.size(); i++) decodeUtf8(text, pos);
  return std::string(text.substr(0, pos));
}

}  // namespace

const Glyph& findGlyph(const Font& font, uint32_t codepoint) {
  size_t low = 0;
  size_t high = font.glyph_count;
  while (low < high) {
    const size_t mid = (low + high) / 2;
    if (font.glyphs[mid].codepoint < codepoint) {
      low = mid + 1;
    } else {
      high = mid;
    }
  }
  if (low < font.glyph_count && font.glyphs[low].codepoint == codepoint) return font.glyphs[low];
  return codepoint == '?' ? font.glyphs[0] : findGlyph(font, '?');
}

uint32_t decodeUtf8(std::string_view text, size_t& pos) {
  const uint8_t first = static_cast<uint8_t>(text[pos]);
  int extra;
  uint32_t cp;
  if (first < 0x80) {
    pos++;
    return first;
  } else if ((first & 0xE0) == 0xC0) {
    extra = 1;
    cp = first & 0x1F;
  } else if ((first & 0xF0) == 0xE0) {
    extra = 2;
    cp = first & 0x0F;
  } else if ((first & 0xF8) == 0xF0) {
    extra = 3;
    cp = first & 0x07;
  } else {
    pos++;
    return kReplacement;
  }
  if (pos + extra >= text.size()) {
    pos++;
    return kReplacement;
  }
  for (int i = 1; i <= extra; i++) {
    const uint8_t c = static_cast<uint8_t>(text[pos + i]);
    if ((c & 0xC0) != 0x80) {
      pos++;
      return kReplacement;
    }
    cp = (cp << 6) | (c & 0x3F);
  }
  pos += extra + 1;
  return cp;
}

size_t utf8Length(std::string_view text) {
  size_t count = 0;
  for (size_t pos = 0; pos < text.size(); count++) decodeUtf8(text, pos);
  return count;
}

int textWidth(const Font& font, std::string_view text) {
  int x8 = 0;
  for (size_t pos = 0; pos < text.size();) x8 += findGlyph(font, decodeUtf8(text, pos)).advance_x8;
  return (x8 + 4) / 8;
}

int drawText(Canvas& canvas, const Font& font, int x, int baseline, std::string_view text,
             Color color, Align align) {
  if (align != Align::kLeft) {
    const int w = textWidth(font, text);
    x -= align == Align::kCenter ? w / 2 : w;
  }
  int x8 = x * 8;
  for (size_t pos = 0; pos < text.size();) {
    const Glyph& g = findGlyph(font, decodeUtf8(text, pos));
    const int gx = (x8 + 4) / 8 + g.left;
    const int gy = baseline + g.top;
    uint32_t bit = g.offset * 8;
    for (int row = 0; row < g.height; row++) {
      for (int col = 0; col < g.width; col++, bit++) {
        if (font.bitmaps[bit / 8] & (0x80 >> (bit % 8))) canvas.setPixel(gx + col, gy + row, color);
      }
    }
    x8 += g.advance_x8;
  }
  return (x8 + 4) / 8;
}

std::string fitText(const Font& font, std::string_view text, int max_width) {
  if (textWidth(font, text) <= max_width) return std::string(text);
  size_t length = utf8Length(text);
  std::string result(text);
  while (length > 1 && textWidth(font, result + ".") > max_width) {
    length--;
    result = prefix(text, length);
  }
  return trimmed(result) + ".";
}

std::string fitEvenly(const Font& font, std::string_view text, int max_width) {
  if (textWidth(font, text) <= max_width) return std::string(text);

  constexpr size_t kMaxParts = 4;
  std::vector<std::string> parts;
  size_t start = 0;
  while (parts.size() < kMaxParts) {
    const size_t slash = text.find('/', start);
    if (slash == std::string_view::npos || parts.size() == kMaxParts - 1) {
      parts.emplace_back(text.substr(start));
      break;
    }
    parts.emplace_back(text.substr(start, slash - start));
    start = slash + 1;
  }

  size_t max_length = 0;
  for (const std::string& p : parts) max_length = std::max(max_length, utf8Length(p));

  for (size_t k = max_length > 1 ? max_length - 1 : 0; k >= 1; k--) {
    std::string result;
    for (size_t i = 0; i < parts.size(); i++) {
      std::string p = parts[i];
      if (utf8Length(p) > k) p = trimmed(prefix(p, k)) + ".";
      if (i > 0) result += "/";
      result += p;
    }
    if (textWidth(font, result) <= max_width) return result;
    if (k == 1) break;
  }
  return fitText(font, text, max_width);
}

}  // namespace ui
