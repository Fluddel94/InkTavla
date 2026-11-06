#include "ui/emoji.h"

#include "ui/text.h"

namespace ui {

namespace {

// Icon of an emoji codepoint, or kCount.
EmojiIcon iconFor(uint32_t cp) {
  using I = EmojiIcon;
  if (cp >= 0x1F550 && cp <= 0x1F567) return I::kClock;  // clock faces
  // clang-format off
  switch (cp) {
    case 0x2764: case 0x2665: case 0x2661: case 0x2763: case 0x1F9E1: case 0x1F49B:
    case 0x1F49A: case 0x1F499: case 0x1F49C: case 0x1F5A4: case 0x1F90D: case 0x1F90E:
    case 0x1F498: case 0x1F49D: case 0x1F496: case 0x1F497: case 0x1F493: case 0x1F495:
    case 0x1F49E: case 0x1F49F: case 0x1FA75: case 0x1FA76: case 0x1FA77:
      return I::kHeart;
    case 0x1F602: case 0x1F923: case 0x1F600: case 0x1F603: case 0x1F604: case 0x1F601:
    case 0x1F606: case 0x1F605:
      return I::kLaughingFace;
    case 0x1F60A: case 0x263A: case 0x1F642: case 0x1F60C: case 0x1F607:
      return I::kSmilingFace;
    case 0x1F60D: case 0x1F970: case 0x1F618: case 0x1F929: case 0x1F61A: case 0x1F619:
      return I::kLovingFace;
    case 0x1F62D: case 0x1F622:
      return I::kCryingFace;
    case 0x1F609: case 0x1F61C:
      return I::kWinkingFace;
    case 0x1F44D:
      return I::kThumbsUp;
    case 0x1F389: case 0x1F38A: case 0x1F973:
      return I::kParty;
    case 0x1F382: case 0x1F370: case 0x1F9C1:
      return I::kCake;
    case 0x1F381:
      return I::kGift;
    case 0x2B50: case 0x1F31F: case 0x2605: case 0x2606: case 0x2728:
      return I::kStar;
    case 0x2714: case 0x2705: case 0x2611: case 0x2713:
      return I::kCheck;
    case 0x26A0: case 0x1F6A8:
      return I::kWarning;
    case 0x2757: case 0x2755: case 0x203C: case 0x2049:
      return I::kExclamation;
    case 0x1F525:
      return I::kFire;
    case 0x1F4C5: case 0x1F4C6: case 0x1F5D3:
      return I::kCalendar;
    case 0x23F0: case 0x23F1: case 0x23F2: case 0x231A:
      return I::kClock;
    case 0x1F4DE: case 0x260E: case 0x1F4F1: case 0x1F4F2:
      return I::kPhone;
    case 0x1F4BC:
      return I::kBriefcase;
    case 0x1F3E0: case 0x1F3E1:
      return I::kHouse;
    case 0x1FA7A: case 0x2695: case 0x1F489: case 0x1F3E5: case 0x1F48A:
      return I::kMedical;
    case 0x1F9B7:
      return I::kTooth;
    case 0x1F6D2: case 0x1F6CD:
      return I::kCart;
    case 0x2708: case 0x1F6EB: case 0x1F6EC: case 0x1F6E9:
      return I::kAirplane;
    case 0x1F697: case 0x1F695: case 0x1F699: case 0x1F698:
      return I::kCar;
    case 0x1F686: case 0x1F687: case 0x1F688: case 0x1F685: case 0x1F684: case 0x1F683:
    case 0x1F682: case 0x1F68B: case 0x1F68A:
      return I::kTrain;
    case 0x1F37D: case 0x1F374: case 0x1F355: case 0x1F354: case 0x1F35D: case 0x1F35C:
    case 0x1F957: case 0x1F363:
      return I::kDining;
    case 0x2615: case 0x1F375:
      return I::kCoffee;
    case 0x26BD:
      return I::kSoccer;
    default:
      return I::kCount;
  }
  // clang-format on
}

// Emoji without an icon and the characters that belong to emoji: variation
// selectors, zero width joiner, keycap mark, skin tones, regional indicators
// (flags) and tag characters. None of them is in the fonts.
bool isRemoved(uint32_t cp) {
  return cp == 0xFE0E || cp == 0xFE0F || cp == 0x200D || cp == 0x20E3 || cp == 0x3030 ||
         cp == 0x303D || cp == 0x3297 || cp == 0x3299 || (cp >= 0x2300 && cp <= 0x23FF) ||
         (cp >= 0x25A0 && cp <= 0x25FF) || (cp >= 0x2600 && cp <= 0x27BF) ||
         (cp >= 0x2900 && cp <= 0x297F) || (cp >= 0x2B00 && cp <= 0x2BFF) ||
         (cp >= 0x1F000 && cp <= 0x1FAFF) || (cp >= 0xE0020 && cp <= 0xE007F);
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

std::string mapEmoji(std::string_view text) {
  bool ascii = true;
  for (char c : text) ascii = ascii && static_cast<unsigned char>(c) < 0x80;
  if (ascii) return std::string(text);

  std::string out;
  out.reserve(text.size());
  bool removed = false;
  for (size_t pos = 0; pos < text.size();) {
    const uint32_t cp = decodeUtf8(text, pos);
    const EmojiIcon icon = iconFor(cp);
    if (icon != EmojiIcon::kCount) {
      appendUtf8(out, emojiCodepoint(icon));
    } else if (isRemoved(cp) || (cp >= 0x1F3FB && cp <= 0x1F3FF) ||
               (cp >= 0x1F1E6 && cp <= 0x1F1FF)) {
      removed = true;
    } else {
      appendUtf8(out, cp);
    }
  }
  if (!removed) return out;

  // Merge the spaces around removed emoji.
  std::string merged;
  merged.reserve(out.size());
  for (char c : out) {
    if (c == ' ' && (merged.empty() || merged.back() == ' ')) continue;
    merged += c;
  }
  while (!merged.empty() && merged.back() == ' ') merged.pop_back();
  return merged;
}

}  // namespace ui
