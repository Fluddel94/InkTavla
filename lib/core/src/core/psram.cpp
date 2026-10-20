#include "core/psram.h"

#include <cstdlib>

#if defined(ESP_PLATFORM)
#include <esp_heap_caps.h>
#endif

namespace core {

namespace {

class PsramAllocator : public ArduinoJson::Allocator {
 public:
  void* allocate(size_t size) override {
#if defined(ESP_PLATFORM)
    void* p = heap_caps_malloc(size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p != nullptr ? p : std::malloc(size);  // no PSRAM: internal heap
#else
    return std::malloc(size);
#endif
  }

  void deallocate(void* ptr) override { std::free(ptr); }

  void* reallocate(void* ptr, size_t new_size) override {
#if defined(ESP_PLATFORM)
    void* p = heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    return p != nullptr ? p : std::realloc(ptr, new_size);
#else
    return std::realloc(ptr, new_size);
#endif
  }
};

}  // namespace

ArduinoJson::Allocator* jsonAllocator() {
  static PsramAllocator allocator;
  return &allocator;
}

}  // namespace core
