#if defined(ARDUINO)

#include "core/littlefs_store.h"

#include <LittleFS.h>

namespace core {

bool LittleFsStore::write(const char* path, const std::string& data) {
  File f = LittleFS.open(path, "w");
  if (!f) return false;
  const size_t n = f.write(reinterpret_cast<const uint8_t*>(data.data()), data.size());
  f.close();
  return n == data.size();
}

bool LittleFsStore::rename(const char* from, const char* to) {
  if (LittleFS.rename(from, to)) return true;
  return LittleFS.exists(to) && LittleFS.remove(to) && LittleFS.rename(from, to);
}

bool LittleFsStore::read(const char* path, std::string& data) {
  File f = LittleFS.open(path, "r");
  if (!f) return false;
  data.assign(f.size(), '\0');
  const size_t n = f.read(reinterpret_cast<uint8_t*>(&data[0]), data.size());
  f.close();
  return n == data.size();
}

}  // namespace core

#endif
