#pragma once

// What the buttons of the device do. One table, so the assignment can be
// changed in one place (and later from the configuration).

#include <cstddef>

#include "core/buttons.h"

namespace core {

enum class KeyAction {
  kNone,
  kRefresh,   // fetch everything now and refresh the whole panel
  kSettings,  // open the settings page (or close it when open)
};

// Number of buttons of the reference hardware (KEY1..KEY3 of the EE04).
inline constexpr size_t kKeyCount = 3;

// Action of a short or long press of key 0..kKeyCount-1.
KeyAction keyAction(size_t key, ButtonEvent event);

// True if the key has a function on a long press (then a short press is
// reported on release, otherwise at once).
bool keyHasLongPress(size_t key);

// Name of an action for the log.
const char* keyActionName(KeyAction action);

}  // namespace core
