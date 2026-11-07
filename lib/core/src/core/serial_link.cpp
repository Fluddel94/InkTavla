#if defined(ARDUINO)

#include "core/serial_link.h"

#include <Arduino.h>
#include <WiFi.h>

#include <utility>
#include <vector>

#include "core/improv.h"
#include "core/littlefs_store.h"
#include "core/syslog.h"
#include "core/time.h"

namespace core {

namespace {

constexpr char kLogPrefix[] = "INKTAVLA LOG ";

constexpr uint32_t kTaskStack = 8192;
constexpr uint32_t kPollMs = 20;
constexpr uint32_t kImprovConnectTimeoutMs = 20000;
constexpr int kMaxNetworks = 20;
constexpr char kChipFamily[] = "ESP32-S3";

Wifi* wifi = nullptr;
SerialLinkOptions options;

// Sends in one piece so other output does not split it; retries briefly if
// the send buffer is full (output does not block, setTxTimeoutMs(0)).
void send(const std::string& data) {
  size_t sent = 0;
  for (int tries = 0; sent < data.size() && tries < 200; tries++) {
    const size_t n =
        Serial.write(reinterpret_cast<const uint8_t*>(data.data()) + sent, data.size() - sent);
    sent += n;
    if (n == 0) vTaskDelay(pdMS_TO_TICKS(2));
  }
}

// --- Improv --------------------------------------------------------------------

void sendState(uint8_t state) { send(improv::packet(improv::kCurrentState, &state, 1)); }
void sendError(uint8_t error) { send(improv::packet(improv::kErrorState, &error, 1)); }

uint8_t currentState() {
  return WiFi.status() == WL_CONNECTED ? improv::kProvisioned : improv::kReady;
}

void restoreSavedWifi() {
  WiFi.disconnect();
  std::string ssid, password;
  if (Wifi::loadCredentials(ssid, password)) wifi->connect(ssid, password);
}

void handleWifiSettings(const improv::Rpc& rpc) {
  std::string ssid, password;
  if (!improv::parseWifiSettings(rpc, ssid, password)) {
    sendError(improv::kInvalidRpc);
    return;
  }
  wifi->setBusy(true);
  sendError(improv::kNoError);
  sendState(improv::kProvisioning);
  Serial.printf("Improv: new Wi-Fi credentials, connecting to \"%s\"\n", ssid.c_str());
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  wifi->connect(ssid, password);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kImprovConnectTimeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (WiFi.status() == WL_CONNECTED && Wifi::saveCredentials(ssid, password)) {
    wifi->markCredentialsSaved();
    wifi->setFailedNetwork("");
    Serial.println("Improv: connected, credentials saved");
    logInfo("wifi", "new network set up over USB, saved");  // never its name
    sendState(improv::kProvisioned);
    send(improv::rpcResult(improv::kWifiSettings, {}));
  } else {
    Serial.println("Improv: connection failed, the old credentials stay");
    logWarning("wifi", "network from the USB setup not reached, nothing saved");
    // Only shown while no network is saved (the Wi-Fi setup screen).
    wifi->setFailedNetwork(ssid);
    restoreSavedWifi();
    sendError(improv::kUnableToConnect);
    sendState(improv::kReady);
  }
  wifi->setBusy(false);
}

void handleScan() {
  wifi->setBusy(true);
  int count = WiFi.scanNetworks();
  bool disconnected = false;
  if (count < 0 && WiFi.status() != WL_CONNECTED) {
    // The scan fails while an attempt is running (e.g. wrong password).
    WiFi.disconnect();
    vTaskDelay(pdMS_TO_TICKS(100));
    disconnected = true;
    count = WiFi.scanNetworks();
  }
  // Strongest first, every name once, hidden networks left out.
  std::vector<bool> used(count > 0 ? count : 0, false);
  std::vector<std::string> names;
  for (int sent = 0; sent < kMaxNetworks;) {
    int best = -1;
    for (int i = 0; i < count; i++) {
      if (!used[i] && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) best = i;
    }
    if (best < 0) break;
    used[best] = true;
    const std::string name = WiFi.SSID(best).c_str();
    bool known = name.empty();
    for (const std::string& n : names) known = known || n == name;
    if (known) continue;
    names.push_back(name);
    send(improv::rpcResult(improv::kScan,
                           {name, std::to_string(WiFi.RSSI(best)),
                            WiFi.encryptionType(best) == WIFI_AUTH_OPEN ? "NO" : "YES"}));
    sent++;
  }
  send(improv::rpcResult(improv::kScan, {}));
  WiFi.scanDelete();
  if (disconnected) restoreSavedWifi();
  wifi->setBusy(false);
}

void handleImprov(const improv::Parser& parser) {
  if (parser.type() != improv::kRpc) return;  // other types come from the device
  improv::Rpc rpc;
  if (!improv::parseRpc(parser.data(), parser.length(), rpc)) {
    sendError(improv::kInvalidRpc);
    return;
  }
  switch (rpc.command) {
    case improv::kWifiSettings:
      handleWifiSettings(rpc);
      break;
    case improv::kGetState:
      sendState(currentState());
      break;
    case improv::kGetInfo:
      send(improv::rpcResult(improv::kGetInfo, {options.firmware_name, options.firmware_version,
                                                kChipFamily, options.device_name}));
      break;
    case improv::kScan:
      handleScan();
      break;
    default:
      sendError(improv::kUnknownRpc);
      break;
  }
}

// --- Configuration commands ------------------------------------------------------

// "INKTAVLA LOG": the header ("# ..." lines: device, Wi-Fi drops, sources)
// and the whole system log, line by line (it may be larger than the free
// internal heap).
void sendLog() {
  send(std::string(kLogPrefix) + std::to_string(systemLog().size()) + "\n");
  if (options.log_header) send(options.log_header());
  LogEntry e;
  uint32_t seq = 0;
  while (systemLog().next(seq, e)) {
    send(formatLogLine(e) + "\n");
    seq = e.seq;
  }
  send("INKTAVLA OK\n");
}

void handleCommand(const CommandParser::Event& event) {
  if (event.kind == CommandParser::Event::Kind::kNone) return;
  if (event.kind == CommandParser::Event::Kind::kLog) {
    sendLog();
    return;
  }
  LittleFsStore store;
  NvsSecretStore secrets;
  const CommandReply reply = core::handleCommand(event, store, options.validate,
                                                 options.info ? options.info() : "", &secrets);
  send(reply.text);
  if (reply.restart) {
    logInfo("system", "restarting (settings over USB)");
    Serial.flush();
    vTaskDelay(pdMS_TO_TICKS(500));
    ESP.restart();
  }
}

void task(void*) {
  improv::Parser improv_parser;
  CommandParser commands;
  for (;;) {
    while (Serial.available() > 0) {
      const uint8_t b = static_cast<uint8_t>(Serial.read());
      const int64_t now = monotonicMs();
      if (commands.receivingPayload()) {  // file bytes go nowhere else
        handleCommand(commands.feed(b, now));
        continue;
      }
      const improv::Parser::Result result = improv_parser.feed(b);
      if (result == improv::Parser::Result::kPacket) {
        handleImprov(improv_parser);
      } else if (result == improv::Parser::Result::kBadChecksum) {
        sendError(improv::kInvalidRpc);
      }
      handleCommand(commands.feed(b, now));
    }
    handleCommand(commands.poll(monotonicMs()));
    vTaskDelay(pdMS_TO_TICKS(kPollMs));
  }
}

}  // namespace

void startSerialLink(Wifi& w, SerialLinkOptions o) {
  wifi = &w;
  options = std::move(o);
  xTaskCreate(task, "serial_link", kTaskStack, nullptr, 1, nullptr);
}

}  // namespace core

#endif
