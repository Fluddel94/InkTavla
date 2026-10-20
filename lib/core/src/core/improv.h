#pragma once

// Improv Serial protocol (version 1, https://www.improv-wifi.com/serial/):
// entering Wi-Fi credentials over USB from the browser. Own small
// implementation from the MVG departure display, no extra library.
//
// Packet: "IMPROV" | version (1) | type | length | data | checksum (sum of
// all previous bytes, 8 bit). Only the protocol is here; the device part
// (connecting, saving) is in core/serial_link.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace core::improv {

inline constexpr uint8_t kVersion = 1;

enum Type : uint8_t {
  kCurrentState = 0x01,
  kErrorState = 0x02,
  kRpc = 0x03,
  kRpcResult = 0x04,
};

enum State : uint8_t {
  kReady = 0x02,         // ready, no authorisation needed
  kProvisioning = 0x03,  // connecting with new credentials
  kProvisioned = 0x04,   // connected
};

enum Error : uint8_t {
  kNoError = 0x00,
  kInvalidRpc = 0x01,
  kUnknownRpc = 0x02,
  kUnableToConnect = 0x03,
};

enum Command : uint8_t {
  kWifiSettings = 0x01,
  kGetState = 0x02,
  kGetInfo = 0x03,
  kScan = 0x04,
};

// A complete packet ready to send (with checksum and a newline for
// readability in the serial monitor). data may be at most 255 bytes.
std::string packet(uint8_t type, const uint8_t* data, size_t length);

// RPC result: command, length, then each text with a length byte before it.
// Texts that no longer fit into the packet are left out.
std::string rpcResult(uint8_t command, const std::vector<std::string>& texts);

// Finds packets in a byte stream that may contain other output.
class Parser {
 public:
  enum class Result { kNone, kPacket, kBadChecksum };

  Result feed(uint8_t b);

  // The last complete packet (valid after kPacket).
  uint8_t type() const { return buffer_[7]; }
  const uint8_t* data() const { return buffer_ + 9; }
  size_t length() const { return buffer_[8]; }

  // True while a packet is being received (after the "IMPROV" header).
  bool receiving() const { return pos_ >= 6; }

 private:
  uint8_t buffer_[9 + 255 + 1] = {};
  size_t pos_ = 0;
};

struct Rpc {
  uint8_t command = 0;
  std::vector<uint8_t> args;
};

// Data of an RPC packet: command, length of the arguments, arguments.
bool parseRpc(const uint8_t* data, size_t length, Rpc& rpc);

// Arguments of kWifiSettings: SSID length, SSID, password length, password.
bool parseWifiSettings(const Rpc& rpc, std::string& ssid, std::string& password);

}  // namespace core::improv
