#include "core/config_command.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace core {

namespace {

constexpr char kPrefix[] = "INKTAVLA ";
constexpr char kTempPath[] = "/config.json.tmp";

std::string hex(uint32_t value) {
  char buffer[12];
  std::snprintf(buffer, sizeof(buffer), "%08lx", static_cast<unsigned long>(value));
  return buffer;
}

// Parses an unsigned decimal or hexadecimal number covering the whole text.
bool parseNumber(const std::string& text, int base, unsigned long max, unsigned long& out) {
  if (text.empty() || text.size() > 10) return false;
  char* end = nullptr;
  const unsigned long value = std::strtoul(text.c_str(), &end, base);
  if (end == nullptr || *end != '\0' || value > max) return false;
  for (char c : text) {
    if (c == '-' || c == '+' || c == ' ') return false;
  }
  out = value;
  return true;
}

CommandParser::Event error(const std::string& reason) {
  CommandParser::Event e;
  e.kind = CommandParser::Event::Kind::kError;
  e.error = reason;
  return e;
}

}  // namespace

uint32_t crc32(const uint8_t* data, size_t length, uint32_t crc) {
  crc = ~crc;
  for (size_t i = 0; i < length; i++) {
    crc ^= data[i];
    for (int k = 0; k < 8; k++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

CommandParser::Event CommandParser::feed(uint8_t b, int64_t now_ms) {
  last_byte_ms_ = now_ms;
  if (payload_left_ > 0) {
    payload_ += static_cast<char>(b);
    if (--payload_left_ > 0) return {};
    const uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(payload_.data()), payload_.size());
    if (crc != payload_crc_) {
      payload_.clear();
      return error("checksum mismatch, nothing saved");
    }
    Event e;
    e.kind = Event::Kind::kConfigPut;
    e.payload.swap(payload_);
    return e;
  }
  if (b == '\n') {
    const bool skipped = overflow_;
    overflow_ = false;
    std::string text;
    text.swap(line_);
    if (!text.empty() && text.back() == '\r') text.pop_back();
    if (skipped) return {};
    return line(text);
  }
  if (overflow_) return {};
  if (line_.size() >= kMaxLine) {
    line_.clear();
    overflow_ = true;  // ignore the rest of this line
    return {};
  }
  line_ += static_cast<char>(b);
  return {};
}

CommandParser::Event CommandParser::poll(int64_t now_ms) {
  if (payload_left_ == 0 || now_ms - last_byte_ms_ < kPayloadTimeoutMs) return {};
  payload_left_ = 0;
  payload_.clear();
  return error("timeout while receiving, nothing saved");
}

CommandParser::Event CommandParser::line(const std::string& text) {
  if (text.compare(0, std::strlen(kPrefix), kPrefix) != 0) return {};  // not for us
  const std::string command = text.substr(std::strlen(kPrefix));
  Event e;
  if (command == "CONFIG GET") {
    e.kind = Event::Kind::kConfigGet;
    return e;
  }
  if (command == "INFO") {
    e.kind = Event::Kind::kInfo;
    return e;
  }
  if (command.compare(0, 11, "CONFIG PUT ") == 0) {
    const std::string args = command.substr(11);
    const size_t space = args.find(' ');
    unsigned long length = 0;
    unsigned long crc = 0;
    if (space == std::string::npos || !parseNumber(args.substr(0, space), 10, 0xFFFFFFFF, length) ||
        !parseNumber(args.substr(space + 1), 16, 0xFFFFFFFF, crc)) {
      return error("usage: CONFIG PUT <bytes> <crc32>");
    }
    if (length == 0 || length > kMaxConfigBytes) {
      return error("size must be 1 to " + std::to_string(kMaxConfigBytes) + " bytes");
    }
    payload_.clear();
    payload_.reserve(length);
    payload_left_ = length;
    payload_crc_ = static_cast<uint32_t>(crc);
    return {};
  }
  return error("unknown command");
}

CommandReply handleCommand(const CommandParser::Event& event, ConfigStore& store,
                           const ConfigValidator& validate, const std::string& info) {
  using Kind = CommandParser::Event::Kind;
  CommandReply reply;
  switch (event.kind) {
    case Kind::kNone:
      return reply;
    case Kind::kError:
      reply.text = std::string(kPrefix) + "ERR " + event.error + "\n";
      return reply;
    case Kind::kInfo:
      reply.text = std::string(kPrefix) + "INFO " + info + "\n" + kPrefix + "OK\n";
      return reply;
    case Kind::kConfigGet: {
      std::string data;
      if (!store.read(kConfigPath, data)) {
        reply.text = std::string(kPrefix) + "ERR no configuration file\n";
        return reply;
      }
      const uint32_t crc = crc32(reinterpret_cast<const uint8_t*>(data.data()), data.size());
      reply.text = std::string(kPrefix) + "DATA " + std::to_string(data.size()) + " " + hex(crc) +
                   "\n" + data + "\n" + kPrefix + "OK\n";
      return reply;
    }
    case Kind::kConfigPut:
      break;
  }

  JsonDocument doc;
  ConfigIssues issues;
  if (!readConfig(event.payload.data(), event.payload.size(), doc, issues)) {
    const std::string why = issues.empty() ? "invalid" : issues.front().message;
    reply.text = std::string(kPrefix) + "ERR " + why + ", nothing saved\n";
    return reply;
  }
  if (validate) validate(doc.as<JsonObjectConst>(), issues);
  if (!store.write(kTempPath, event.payload) || !store.rename(kTempPath, kConfigPath)) {
    reply.text = std::string(kPrefix) + "ERR cannot write the file, old configuration kept\n";
    return reply;
  }
  for (const ConfigIssue& i : issues) {
    reply.text += std::string(kPrefix) + "ISSUE " + (i.path.empty() ? "(file)" : i.path) + ": " +
                  i.message + "\n";
  }
  reply.text += std::string(kPrefix) + "OK saved, " + std::to_string(issues.size()) +
                (issues.size() == 1 ? " issue" : " issues") + ", restarting\n";
  reply.restart = true;
  return reply;
}

}  // namespace core
