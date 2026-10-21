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

void appendUtf8(std::string& out, uint32_t cp) {
  if (cp < 0x80) {
    out += static_cast<char>(cp);
  } else if (cp < 0x800) {
    out += static_cast<char>(0xC0 | (cp >> 6));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else if (cp < 0x10000) {
    out += static_cast<char>(0xE0 | (cp >> 12));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  } else {
    out += static_cast<char>(0xF0 | (cp >> 18));
    out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    out += static_cast<char>(0x80 | (cp & 0x3F));
  }
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

int trackedTextWidth(const Font& font, std::string_view text, int spacing_x8) {
  int x8 = 0;
  size_t count = 0;
  for (size_t pos = 0; pos < text.size(); count++) {
    x8 += findGlyph(font, decodeUtf8(text, pos)).advance_x8;
  }
  if (count > 1) x8 += static_cast<int>(count - 1) * spacing_x8;
  return (x8 + 4) / 8;
}

int drawTrackedText(Canvas& canvas, const Font& font, int x, int baseline, std::string_view text,
                    int spacing_x8, Color color, Align align) {
  if (text.empty()) return x;
  if (align != Align::kLeft) {
    const int w = trackedTextWidth(font, text, spacing_x8);
    x -= align == Align::kCenter ? w / 2 : w;
  }
  int x8 = x * 8;
  for (size_t pos = 0; pos < text.size();) {
    const size_t start = pos;
    const uint32_t cp = decodeUtf8(text, pos);
    drawText(canvas, font, (x8 + 4) / 8, baseline, text.substr(start, pos - start), color);
    x8 += findGlyph(font, cp).advance_x8 + spacing_x8;
  }
  return (x8 - spacing_x8 + 4) / 8;
}

std::string toUpper(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  for (size_t pos = 0; pos < text.size();) {
    uint32_t cp = decodeUtf8(text, pos);
    if (cp >= 'a' && cp <= 'z') {
      cp -= 0x20;
    } else if (cp == 0xDF) {  // ß
      out += "SS";
      continue;
    } else if (cp >= 0xE0 && cp <= 0xFE && cp != 0xF7) {
      cp -= 0x20;
    } else if (cp == 0xFF) {
      cp = 0x178;
    } else if ((cp >= 0x100 && cp <= 0x137) || (cp >= 0x14A && cp <= 0x177)) {
      cp &= ~1u;  // pairs: even = upper case
    } else if ((cp >= 0x139 && cp <= 0x148) || (cp >= 0x179 && cp <= 0x17E)) {
      if ((cp & 1u) == 0) cp -= 1;  // pairs: odd = upper case
    }
    appendUtf8(out, cp);
  }
  return out;
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

namespace {

constexpr char kEllipsis[] = "\u2026";

// Removes spaces and ",.-" at the end (before an ellipsis).
std::string trimmedForEllipsis(std::string text) {
  while (!text.empty() &&
         (text.back() == ' ' || text.back() == ',' || text.back() == '.' || text.back() == '-')) {
    text.pop_back();
  }
  return text;
}

std::vector<std::string_view> words(std::string_view text) {
  std::vector<std::string_view> result;
  size_t pos = 0;
  while (pos < text.size()) {
    while (pos < text.size() && text[pos] == ' ') pos++;
    const size_t start = pos;
    while (pos < text.size() && text[pos] != ' ') pos++;
    if (pos > start) result.push_back(text.substr(start, pos - start));
  }
  return result;
}

}  // namespace

std::string fitTextEllipsis(const Font& font, std::string_view text, int max_width) {
  if (textWidth(font, text) <= max_width) return std::string(text);
  size_t length = utf8Length(text);
  std::string result = trimmedForEllipsis(std::string(text));
  while (length > 0 && textWidth(font, result + kEllipsis) > max_width) {
    length--;
    result = trimmedForEllipsis(prefix(text, length));
  }
  return result + kEllipsis;
}

std::vector<std::string> wrapText(const Font& font, std::string_view text, int max_width,
                                  int max_lines, bool* complete) {
  if (complete != nullptr) *complete = true;
  const std::vector<std::string_view> list = words(text);
  std::vector<std::string> lines;
  std::string line;
  size_t i = 0;
  for (; i < list.size(); i++) {
    const std::string candidate =
        line.empty() ? std::string(list[i]) : line + " " + std::string(list[i]);
    if (textWidth(font, candidate) <= max_width) {
      line = candidate;
      continue;
    }
    if (line.empty()) {  // a single word wider than the line
      line = fitTextEllipsis(font, list[i], max_width);
      continue;
    }
    if (max_lines > 0 && static_cast<int>(lines.size()) + 1 == max_lines) break;
    lines.push_back(line);
    line = std::string(list[i]);
    if (textWidth(font, line) > max_width) line = fitTextEllipsis(font, line, max_width);
  }
  if (i < list.size()) {
    // Text left over: the last line ends with "…" after a whole word.
    if (complete != nullptr) *complete = false;
    std::vector<std::string_view> last = words(line);
    std::string shortened;
    while (!last.empty()) {
      shortened.clear();
      for (std::string_view w : last) shortened += (shortened.empty() ? "" : " ") + std::string(w);
      shortened = trimmedForEllipsis(shortened);
      if (textWidth(font, shortened + kEllipsis) <= max_width) break;
      last.pop_back();
    }
    line = last.empty() ? fitTextEllipsis(font, line + " " + std::string(list[i]), max_width)
                        : shortened + kEllipsis;
  }
  if (!line.empty()) lines.push_back(line);
  return lines;
}

}  // namespace ui
