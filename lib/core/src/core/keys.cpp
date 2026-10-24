#include "core/keys.h"

namespace core {

namespace {

struct KeyFunctions {
  KeyAction short_press;
  KeyAction long_press;
};

// KEY1: refresh. KEY2: free (planned: system log). KEY3: settings page.
// No key has a function on a long press yet.
constexpr KeyFunctions kKeys[kKeyCount] = {
    {KeyAction::kRefresh, KeyAction::kNone},
    {KeyAction::kNone, KeyAction::kNone},
    {KeyAction::kSettings, KeyAction::kNone},
};

}  // namespace

KeyAction keyAction(size_t key, ButtonEvent event) {
  if (key >= kKeyCount) return KeyAction::kNone;
  switch (event) {
    case ButtonEvent::kShort:
      return kKeys[key].short_press;
    case ButtonEvent::kLong:
      return kKeys[key].long_press;
    case ButtonEvent::kNone:
      break;
  }
  return KeyAction::kNone;
}

bool keyHasLongPress(size_t key) {
  return key < kKeyCount && kKeys[key].long_press != KeyAction::kNone;
}

const char* keyActionName(KeyAction action) {
  switch (action) {
    case KeyAction::kNone:
      return "none";
    case KeyAction::kRefresh:
      return "refresh";
    case KeyAction::kSettings:
      return "settings page";
  }
  return "";
}

}  // namespace core
