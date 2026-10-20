#include "core/improv.h"

#include <cstring>

namespace core::improv {

namespace {
constexpr char kHeader[] = "IMPROV";
}

std::string packet(uint8_t type, const uint8_t* data, size_t length) {
  if (length > 255) length = 255;
  std::string out(kHeader, 6);
  out += static_cast<char>(kVersion);
  out += static_cast<char>(type);
  out += static_cast<char>(length);
  if (length > 0) out.append(reinterpret_cast<const char*>(data), length);
  uint8_t checksum = 0;
  for (char c : out) checksum += static_cast<uint8_t>(c);
  out += static_cast<char>(checksum);
  out += '\n';
  return out;
}

std::string rpcResult(uint8_t command, const std::vector<std::string>& texts) {
  uint8_t data[255];
  size_t pos = 2;
  for (const std::string& t : texts) {
    if (t.size() > 255 || pos + 1 + t.size() > 255) break;
    data[pos++] = static_cast<uint8_t>(t.size());
    std::memcpy(data + pos, t.data(), t.size());
    pos += t.size();
  }
  data[0] = command;
  data[1] = static_cast<uint8_t>(pos - 2);
  return packet(kRpcResult, data, pos);
}

Parser::Result Parser::feed(uint8_t b) {
  // Header "IMPROV" byte by byte.
  if (pos_ < 6) {
    if (b == static_cast<uint8_t>(kHeader[pos_])) {
      buffer_[pos_++] = b;
    } else {
      pos_ = 0;
      if (b == static_cast<uint8_t>(kHeader[0])) buffer_[pos_++] = b;
    }
    return Result::kNone;
  }
  buffer_[pos_++] = b;
  if (pos_ == 7 && b != kVersion) {
    pos_ = 0;
    return Result::kNone;
  }
  if (pos_ >= 9 && pos_ == 9 + static_cast<size_t>(buffer_[8]) + 1) {
    pos_ = 0;
    uint8_t checksum = 0;
    const size_t end = 9 + buffer_[8];
    for (size_t i = 0; i < end; i++) checksum += buffer_[i];
    return checksum == buffer_[end] ? Result::kPacket : Result::kBadChecksum;
  }
  return Result::kNone;
}

bool parseRpc(const uint8_t* data, size_t length, Rpc& rpc) {
  if (length < 2 || 2u + data[1] > length) return false;
  rpc.command = data[0];
  rpc.args.assign(data + 2, data + 2 + data[1]);
  return true;
}

bool parseWifiSettings(const Rpc& rpc, std::string& ssid, std::string& password) {
  const std::vector<uint8_t>& a = rpc.args;
  if (a.size() < 2) return false;
  const size_t ssid_length = a[0];
  if (ssid_length == 0 || 1 + ssid_length + 1 > a.size()) return false;
  const size_t password_length = a[1 + ssid_length];
  if (1 + ssid_length + 1 + password_length > a.size()) return false;
  ssid.assign(reinterpret_cast<const char*>(a.data() + 1), ssid_length);
  password.assign(reinterpret_cast<const char*>(a.data() + 2 + ssid_length), password_length);
  return true;
}

}  // namespace core::improv
