#pragma once

// Allocators for data kept in PSRAM on the device (large JSON documents,
// calendar events), so the internal heap stays free for TLS and Wi-Fi; the
// normal heap on the PC and on boards without PSRAM.

#include <ArduinoJson.h>

#include <cstddef>

namespace core {

ArduinoJson::Allocator* jsonAllocator();

// PSRAM if available, else the internal heap; aborts if both are full
// (like a failed new).
void* psramAllocate(size_t size);
void psramFree(void* ptr);

// Allocator for standard containers and strings, e.g.
// std::vector<Event, core::PsramAllocator<Event>>.
template <typename T>
struct PsramAllocator {
  using value_type = T;
  PsramAllocator() = default;
  template <typename U>
  PsramAllocator(const PsramAllocator<U>&) {}  // NOLINT: implicit like std::allocator
  T* allocate(size_t n) { return static_cast<T*>(psramAllocate(n * sizeof(T))); }
  void deallocate(T* p, size_t) { psramFree(p); }
};

template <typename T, typename U>
bool operator==(const PsramAllocator<T>&, const PsramAllocator<U>&) {
  return true;
}
template <typename T, typename U>
bool operator!=(const PsramAllocator<T>&, const PsramAllocator<U>&) {
  return false;
}

}  // namespace core
