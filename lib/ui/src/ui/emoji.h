#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ui {

// The UI fonts contain letters and punctuation only. Event titles may
// contain emoji, so a small set of them is drawn as simple pictures: own
// 1-bit drawings (tools/fontgen/emoji_icons.py) that fontgen adds to every
// text font as glyphs in the Unicode private use area, starting at
// kEmojiFirstCodepoint. The order here is the order in emoji_icons.py.
enum class EmojiIcon : uint8_t {
  kHeart,  // all heart colours
  kLaughingFace,
  kSmilingFace,
  kLovingFace,  // heart eyes, blowing a kiss
  kCryingFace,
  kWinkingFace,
  kThumbsUp,
  kParty,
  kCake,
  kGift,
  kStar,
  kCheck,
  kWarning,
  kExclamation,
  kFire,
  kCalendar,
  kClock,
  kPhone,
  kBriefcase,
  kHouse,
  kMedical,
  kTooth,
  kCart,
  kAirplane,
  kCar,
  kTrain,
  kDining,
  kCoffee,
  kSoccer,
  kTree,  // 🎄 🌲 (not 🌳, a deciduous tree)
  kCount,
};

inline constexpr uint32_t kEmojiFirstCodepoint = 0xE000;
inline constexpr size_t kEmojiIconCount = static_cast<size_t>(EmojiIcon::kCount);

// Codepoint of an icon in the fonts.
constexpr uint32_t emojiCodepoint(EmojiIcon icon) {
  return kEmojiFirstCodepoint + static_cast<uint32_t>(icon);
}

// Prepares a UTF-8 text for the fonts: every emoji with an icon becomes the
// codepoint of its icon (variants share one: all hearts, all laughing
// faces, ...), every other emoji, skin tone, flag and invisible emoji
// control character is removed, and the spaces that remain around removed
// emoji are merged. Text without emoji is returned unchanged.
std::string mapEmoji(std::string_view text);

}  // namespace ui
