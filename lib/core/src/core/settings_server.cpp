#if defined(ARDUINO)

#include "core/settings_server.h"

#include <Arduino.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>
#include <esp_system.h>

#include <cstring>
#include <mutex>

#include "core/firmware_check.h"
#include "core/firmware_writer.h"
#include "core/http.h"
#include "core/littlefs_store.h"
#include "core/secrets.h"
#include "core/settings_api.h"
#include "core/settings_session.h"
#include "core/time.h"
#include "core/update_check.h"

namespace core {

namespace {

constexpr uint32_t kTaskStack = 16384;  // HTTPS requests of the setup help run here
constexpr uint32_t kWifiTestTimeoutMs = 20000;
constexpr int kMaxNetworks = 20;
constexpr char kJson[] = "application/json; charset=utf-8";

// Kept over a software restart (not over a power cut): the open session.
// Raw bytes, so no constructor clears them at start.
RTC_NOINIT_ATTR uint8_t kept[sizeof(SettingsSession::Snapshot)];

std::mutex mutex;  // session, restart flag
SettingsSession session([] { return esp_random(); });
Wifi* wifi = nullptr;
SettingsServerOptions options;
WebServer server(80);
bool listening = false;
int64_t shown_close_ms = 0;  // closing time on the display (monotonic)
int64_t shown_close_wall_s = 0;
enum class WifiTest { kIdle, kTesting, kFailed } wifi_test = WifiTest::kIdle;
std::string wifi_ssid;  // network to test (task only)
std::string wifi_password;
bool restart_requested = false;

// --- Helpers ---------------------------------------------------------------------

void sendJson(int status, const std::string& json) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, kJson, json.c_str());
}

std::string escaped(const std::string& text) {
  JsonDocument doc;
  doc.set(text);
  std::string out;
  serializeJson(doc, out);
  return out;
}

void sendError(int status, const char* error) {
  sendJson(status, std::string("{\"error\":") + escaped(error) + "}");
}

std::string cookieToken() {
  return SettingsSession::tokenFromCookie(server.header("Cookie").c_str());
}

// Checks the session cookie; counts as an action unless passive. Sends 401
// itself when not logged in.
bool authorized(bool passive = false) {
  std::lock_guard<std::mutex> lock(mutex);
  const int64_t now = monotonicMs();
  if (session.authorized(cookieToken(), now)) {
    if (!passive) session.touch(now);
    return true;
  }
  const bool open = session.isOpen(now);
  sendJson(401, std::string("{\"open\":") + (open ? "true" : "false") +
                    ",\"lang\":" + escaped(options.language) + "}");
  return false;
}

int64_t closesInS() {
  std::lock_guard<std::mutex> lock(mutex);
  const int64_t left = session.closesAtMs() - monotonicMs();
  return left > 0 ? left / 1000 : 0;
}

// Restart after the reply: the session is kept for the restarted device.
void restartSoon() {
  std::lock_guard<std::mutex> lock(mutex);
  restart_requested = true;
}

// --- Routes ------------------------------------------------------------------------

void handlePage() {
  server.sendHeader("Content-Encoding", "gzip");
  server.sendHeader("Cache-Control", "no-cache");
  server.send_P(200, "text/html; charset=utf-8", reinterpret_cast<const char*>(options.page_gz),
                options.page_gz_size);
}

void handleLogin() {
  JsonDocument body;
  if (deserializeJson(body, server.arg("plain").c_str())) {
    sendError(400, "invalid request");
    return;
  }
  std::string token;
  SettingsSession::Login result;
  {
    std::lock_guard<std::mutex> lock(mutex);
    result = session.login(body["code"] | "", monotonicMs(), token);
  }
  if (result == SettingsSession::Login::kOk) {
    server.sendHeader(
        "Set-Cookie",
        ("inktavla_session=" + token + "; Path=/; HttpOnly; SameSite=Strict").c_str());
    Serial.println("Settings page: logged in");
    sendJson(200, "{\"ok\":true}");
  } else if (result == SettingsSession::Login::kWrongCode) {
    Serial.println("Settings page: wrong code");
    sendError(403, "wrong code");
  } else {
    Serial.println("Settings page: closed (too many wrong codes or time up)");
    sendError(401, "closed");
  }
}

void handleState() {
  if (!authorized()) return;
  LittleFsStore store;
  NvsSecretStore secrets;
  std::string problem;
  const std::string config = storedConfig(store, &problem);
  JsonDocument state;
  state["lang"] = options.language;
  state["closes_in_s"] = closesInS();
  state["timeout_s"] = SettingsSession::kTimeoutMs / 1000;
  JsonDocument parsed;
  deserializeJson(parsed, config);
  state["config"] = parsed;
  state["config_problem"] = problem;
  state["secrets"]["calendar_url"] = secrets.has(kSecretCalendarUrl);
  state["wifi"]["ssid"] = WiFi.SSID().c_str();
  state["wifi"]["rssi"] = WiFi.RSSI();
  state["wifi"]["ip"] = WiFi.localIP().toString().c_str();
  state["device"]["firmware"] = options.firmware_version;
  state["device"]["uptime_s"] = monotonicMs() / 1000;
  state["device"]["heap"] = ESP.getFreeHeap();
  state["device"]["psram"] = ESP.getFreePsram();
  if (options.layout_json) {
    JsonDocument layout;
    deserializeJson(layout, "{" + options.layout_json() + "}");
    state["widgets"] = layout["widgets"];
    state["slots"] = layout["slots"];
  }
  std::string out;
  serializeJson(state, out);
  sendJson(200, out);
}

void handlePing() {
  if (!authorized(true)) return;
  sendJson(200, "{\"closes_in_s\":" + std::to_string(closesInS()) + "}");
}

void handleClose() {
  if (!authorized(true)) return;
  {
    std::lock_guard<std::mutex> lock(mutex);
    session.close();
  }
  Serial.println("Settings page: closed on the page");
  sendJson(200, "{\"ok\":true}");
}

void handleSave() {
  if (!authorized()) return;
  LittleFsStore store;
  NvsSecretStore secrets;
  const SettingsSaveResult r =
      saveSettings(server.arg("plain").c_str(), store, options.validate, &secrets);
  Serial.printf("Settings page: save -> %d\n", r.status);  // never the content
  sendJson(r.status, r.json);
  if (r.restart) restartSoon();
}

void handleReset() {
  if (!authorized()) return;
  LittleFsStore store;
  NvsSecretStore secrets;
  const SettingsSaveResult r = resetSettings(store, secrets);
  Serial.println("Settings page: settings reset");
  sendJson(r.status, r.json);
  if (r.restart) restartSoon();
}

void handleRestart() {
  if (!authorized()) return;
  sendJson(200, "{\"ok\":true}");
  restartSoon();
}

void handleDownload() {
  if (!authorized()) return;
  LittleFsStore store;
  const std::string config = storedConfig(store);
  server.sendHeader("Content-Disposition", "attachment; filename=\"config.json\"");
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, kJson, config.c_str());
}

void handleScan() {
  if (!authorized()) return;
  wifi->setBusy(true);
  const int count = WiFi.scanNetworks();
  wifi->setBusy(false);
  // Strongest first, every name once, hidden networks left out.
  JsonDocument out;
  JsonArray list = out.to<JsonArray>();
  std::vector<bool> used(count > 0 ? count : 0, false);
  while (list.size() < kMaxNetworks) {
    int best = -1;
    for (int i = 0; i < count; i++) {
      if (!used[i] && (best < 0 || WiFi.RSSI(i) > WiFi.RSSI(best))) best = i;
    }
    if (best < 0) break;
    used[best] = true;
    const String name = WiFi.SSID(best);
    bool known = name.isEmpty();
    for (JsonObject n : list) known = known || name == n["ssid"].as<const char*>();
    if (known) continue;
    JsonObject n = list.add<JsonObject>();
    n["ssid"] = name.c_str();
    n["rssi"] = WiFi.RSSI(best);
    n["secure"] = WiFi.encryptionType(best) != WIFI_AUTH_OPEN;
  }
  WiFi.scanDelete();
  std::string json;
  serializeJson(out, json);
  sendJson(200, json);
}

void handleWifi() {
  if (!authorized()) return;
  JsonDocument body;
  if (deserializeJson(body, server.arg("plain").c_str())) {
    sendError(400, "invalid request");
    return;
  }
  const std::string ssid = body["ssid"] | "";
  const std::string password = body["password"] | "";
  if (ssid.empty() || ssid.size() > 32 || password.size() > 64) {
    sendError(400, "invalid network");
    return;
  }
  wifi_ssid = ssid;
  wifi_password = password;
  wifi_test = WifiTest::kTesting;
  sendJson(202, "{\"ok\":true}");
}

void handleWifiStatus() {
  if (!authorized(true)) return;
  const char* state = wifi_test == WifiTest::kTesting  ? "testing"
                      : wifi_test == WifiTest::kFailed ? "failed"
                                                       : "idle";
  sendJson(200, std::string("{\"state\":\"") + state + "\"}");
}

// --- Firmware upload ------------------------------------------------------------------

// Only one new firmware at a time: a file upload or a download (the
// Update library has one OTA slot to write).
bool firmware_busy = false;  // under mutex

bool claimFirmware() {
  std::lock_guard<std::mutex> lock(mutex);
  if (firmware_busy) return false;
  firmware_busy = true;
  return true;
}

void releaseFirmware() {
  std::lock_guard<std::mutex> lock(mutex);
  firmware_busy = false;
}

struct Upload {
  bool allowed = false;
  bool claimed = false;
  std::string error;  // set when refused before the writer started
  FirmwareWriter writer;
};
Upload* upload = nullptr;

bool uploadOk() { return upload != nullptr && upload->error.empty() && !upload->writer.failed(); }

// Called for every part of the uploaded file (multipart form "firmware").
void handleFirmwarePart() {
  HTTPUpload& part = server.upload();
  if (part.status == UPLOAD_FILE_START) {
    if (upload != nullptr && upload->claimed) releaseFirmware();
    delete upload;
    upload = new Upload();
    {
      std::lock_guard<std::mutex> lock(mutex);
      const int64_t now = monotonicMs();
      upload->allowed = session.authorized(cookieToken(), now);
      if (upload->allowed) session.touch(now);
    }
    if (!upload->allowed) {
      upload->error = "not logged in";
      return;
    }
    if (!claimFirmware()) {
      upload->error = "an update is already running";
      return;
    }
    upload->claimed = true;
    Serial.println("Firmware update: receiving");
    upload->writer.begin();
  } else if (part.status == UPLOAD_FILE_WRITE) {
    if (!uploadOk()) return;
    upload->writer.write(part.buf, part.currentSize);
    std::lock_guard<std::mutex> lock(mutex);
    session.touch(monotonicMs());  // a long upload is an action
  } else if (part.status == UPLOAD_FILE_END) {
    if (!uploadOk()) return;
    if (upload->writer.finish()) Serial.println("Firmware update: restarting");
  } else if (part.status == UPLOAD_FILE_ABORTED) {
    if (upload != nullptr) {
      if (upload->error.empty()) upload->error = "upload aborted";
      upload->writer.abort();
    }
  }
}

void handleFirmwareDone() {
  if (upload == nullptr || !upload->allowed) {
    sendJson(401, "{\"open\":true}");
    return;
  }
  const bool ok = uploadOk() && upload->writer.finished();
  const std::string error = !upload->error.empty()            ? upload->error
                            : !upload->writer.error().empty() ? upload->writer.error()
                                                              : std::string("no file");
  if (!ok) upload->writer.abort();
  if (upload->claimed && !ok) releaseFirmware();  // after success it stays busy until the restart
  delete upload;
  upload = nullptr;
  if (!ok) {
    sendError(422, error.c_str());
    return;
  }
  sendJson(200, "{\"ok\":true}");
  restartSoon();
}

// --- Update button (core/update_check.h) ----------------------------------------------

// The largest firmware that fits into an OTA slot (partitions.csv: 4 MB).
constexpr size_t kMaxFirmwareBytes = 0x400000;
constexpr uint32_t kDownloadStack = 16384;  // HTTPS like the setup help

enum class Download { kIdle, kRunning, kFailed, kDone };
struct OnlineUpdate {
  std::string tag;  // offered by the last check, empty if none
  std::string version;
  Download state = Download::kIdle;
  size_t bytes = 0;
  int64_t total = -1;
  std::string error;
};
OnlineUpdate online;  // under mutex

void handleUpdateCheck() {
  if (!authorized()) return;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (online.state == Download::kRunning) {
      sendError(409, "an update is already running");
      return;
    }
  }
  std::string location;
  const int status = fetchRedirect(latestReleaseUrl(kUpdateRepository), location);
  const UpdateInfo info =
      evaluateLatestRelease(status, location, kUpdateRepository, options.firmware_version);
  {
    std::lock_guard<std::mutex> lock(mutex);
    const bool offer = info.status == UpdateStatus::kAvailable;
    online.tag = offer ? info.tag : "";
    online.version = offer ? info.latest : "";
    online.state = Download::kIdle;
    online.error.clear();
  }
  const std::string json = updateInfoJson(info);
  Serial.printf("Update check: %s\n", json.c_str());  // versions and status only
  sendJson(200, json);
}

void downloadTask(void*) {
  std::string tag;
  {
    std::lock_guard<std::mutex> lock(mutex);
    tag = online.tag;
  }
  Serial.printf("Firmware update: downloading %s\n", tag.c_str());
  FirmwareWriter writer;
  bool started = false;
  HttpResult r;
  const bool fetched = fetchStream(
      releaseFirmwareUrl(kUpdateRepository, tag),
      [&](const char* data, size_t length) {
        if (!started) return false;
        if (!writer.write(reinterpret_cast<const uint8_t*>(data), length)) return false;
        std::lock_guard<std::mutex> lock(mutex);
        online.bytes = writer.bytes();
        session.touch(monotonicMs());  // a long download is an action
        return true;
      },
      kMaxFirmwareBytes, &r,
      [&](int64_t length) {
        {
          std::lock_guard<std::mutex> lock(mutex);
          online.total = length;
        }
        started = writer.begin(length > 0 ? static_cast<size_t>(length) : 0);
      });
  const bool ok = fetched && writer.finish();
  std::string error;
  if (!ok) {
    writer.abort();
    if (writer.failed()) {
      error = writer.error();
    } else if (r.status == 404) {
      error = "the release has no firmware.bin";
    } else if (r.status <= 0) {
      error = "no connection";
    } else if (r.status != 200) {
      error = "download failed (HTTP " + std::to_string(r.status) + ")";
    } else {
      error = r.error.empty() || r.timed_out ? "download incomplete" : r.error;
    }
  }
  {
    std::lock_guard<std::mutex> lock(mutex);
    online.state = ok ? Download::kDone : Download::kFailed;
    online.error = error;
    if (ok) restart_requested = true;  // the server task restarts the device
    firmware_busy = false;
  }
  if (ok) Serial.println("Firmware update: restarting");
  vTaskDelete(nullptr);
}

void handleUpdateInstall() {
  if (!authorized()) return;
  JsonDocument body;
  if (deserializeJson(body, server.arg("plain").c_str())) {
    sendError(400, "invalid request");
    return;
  }
  const std::string version = body["version"] | "";
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (online.tag.empty() || version != online.version) {
      sendError(409, "check for updates first");
      return;
    }
    if (firmware_busy) {
      sendError(409, "an update is already running");
      return;
    }
    firmware_busy = true;
    online.state = Download::kRunning;
    online.bytes = 0;
    online.total = -1;
    online.error.clear();
  }
  if (xTaskCreate(downloadTask, "fw-download", kDownloadStack, nullptr, 1, nullptr) != pdPASS) {
    std::lock_guard<std::mutex> lock(mutex);
    online.state = Download::kFailed;
    online.error = "not enough memory";
    firmware_busy = false;
    sendError(500, "not enough memory");
    return;
  }
  sendJson(202, "{\"ok\":true}");
}

void handleUpdateStatus() {
  if (!authorized(true)) return;
  JsonDocument out;
  {
    std::lock_guard<std::mutex> lock(mutex);
    out["state"] = online.state == Download::kRunning  ? "downloading"
                   : online.state == Download::kFailed ? "failed"
                   : online.state == Download::kDone   ? "done"
                                                       : "idle";
    out["bytes"] = online.bytes;
    if (online.total > 0) out["total"] = online.total;
    if (!online.error.empty()) out["error"] = online.error;
  }
  std::string json;
  serializeJson(out, json);
  sendJson(200, json);
}

void handleRoute(const SettingsRoute& route) {
  if (!authorized()) return;
  std::map<std::string, std::string> args;
  for (int i = 0; i < server.args(); i++) {
    if (server.argName(i) != "plain") args[server.argName(i).c_str()] = server.arg(i).c_str();
  }
  std::string json;
  if (!route.handler(args, json)) {
    sendError(502, "service not reachable");
    return;
  }
  sendJson(200, json);
}

void registerRoutes() {
  server.on("/", HTTP_GET, handlePage);
  server.on("/api/state", HTTP_GET, handleState);
  server.on("/api/ping", HTTP_GET, handlePing);
  server.on("/api/login", HTTP_POST, handleLogin);
  server.on("/api/close", HTTP_POST, handleClose);
  server.on("/api/config", HTTP_POST, handleSave);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.on("/api/restart", HTTP_POST, handleRestart);
  server.on("/api/config/download", HTTP_GET, handleDownload);
  server.on("/api/wifi/scan", HTTP_GET, handleScan);
  server.on("/api/wifi", HTTP_POST, handleWifi);
  server.on("/api/wifi/status", HTTP_GET, handleWifiStatus);
  server.on("/api/firmware", HTTP_POST, handleFirmwareDone, handleFirmwarePart);
  server.on("/api/update/check", HTTP_GET, handleUpdateCheck);
  server.on("/api/update/install", HTTP_POST, handleUpdateInstall);
  server.on("/api/update/status", HTTP_GET, handleUpdateStatus);
  for (const SettingsRoute& r : options.routes) {
    server.on(("/api/" + r.path).c_str(), HTTP_GET, [&r] { handleRoute(r); });
  }
  server.onNotFound([] { sendError(404, "not found"); });
  const char* headers[] = {"Cookie"};
  server.collectHeaders(headers, 1);
}

// --- Wi-Fi test ----------------------------------------------------------------------

void restoreSavedWifi() {
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  std::string ssid, password;
  if (Wifi::loadCredentials(ssid, password)) wifi->connect(ssid, password);
}

// Leaves the network, tries the new one; saves it and restarts on success,
// goes back to the saved network otherwise (as with Improv).
void testWifi() {
  Serial.printf("Settings page: testing Wi-Fi \"%s\"\n", wifi_ssid.c_str());
  vTaskDelay(pdMS_TO_TICKS(500));  // let the reply go out
  wifi->setBusy(true);
  WiFi.disconnect();
  vTaskDelay(pdMS_TO_TICKS(100));
  wifi->connect(wifi_ssid, wifi_password);
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < kWifiTestTimeoutMs) {
    vTaskDelay(pdMS_TO_TICKS(100));
  }
  if (WiFi.status() == WL_CONNECTED && Wifi::saveCredentials(wifi_ssid, wifi_password)) {
    Serial.println("Settings page: new Wi-Fi works, saved, restarting");
    wifi_password.clear();
    wifi->setBusy(false);
    restartSoon();
    return;
  }
  Serial.println("Settings page: new Wi-Fi failed, back to the saved one");
  wifi_password.clear();
  restoreSavedWifi();
  wifi->setBusy(false);
  wifi_test = WifiTest::kFailed;
}

// --- Task ------------------------------------------------------------------------------

void task(void*) {
  for (;;) {
    bool open;
    bool restart;
    {
      std::lock_guard<std::mutex> lock(mutex);
      open = session.isOpen(monotonicMs());
      restart = restart_requested;
    }
    if (restart) {
      vTaskDelay(pdMS_TO_TICKS(300));  // the reply is on its way
      {
        std::lock_guard<std::mutex> lock(mutex);
        const SettingsSession::Snapshot snapshot = session.snapshot(monotonicMs());
        memcpy(kept, &snapshot, sizeof(snapshot));
      }
      Serial.println("Settings page: restarting");
      Serial.flush();
      ESP.restart();
    }
    const bool online = WiFi.status() == WL_CONNECTED;
    if (open && online && !listening) {
      server.begin();
      listening = true;
      Serial.printf("Settings page: open at http://%s/\n", WiFi.localIP().toString().c_str());
    } else if ((!open || !online) && listening && wifi_test != WifiTest::kTesting) {
      server.stop();
      listening = false;
      Serial.println("Settings page: closed");
    }
    if (wifi_test == WifiTest::kTesting) {
      testWifi();
      continue;
    }
    if (listening) server.handleClient();
    vTaskDelay(pdMS_TO_TICKS(listening ? 2 : 200));
  }
}

}  // namespace

void SettingsServer::begin(Wifi& w, SettingsServerOptions o) {
  wifi = &w;
  options = std::move(o);
  // A session kept over a restart (only after a software reset; the memory
  // is random after power-on, which restore() rejects).
  if (esp_reset_reason() == ESP_RST_SW) {
    SettingsSession::Snapshot snapshot;
    memcpy(&snapshot, kept, sizeof(snapshot));
    std::lock_guard<std::mutex> lock(mutex);
    session.restore(snapshot, monotonicMs());
    if (session.isOpen(monotonicMs())) Serial.println("Settings page: open again after restart");
  }
  memset(kept, 0, sizeof(kept));
  registerRoutes();
  xTaskCreate(task, "settings", kTaskStack, nullptr, 1, nullptr);
}

namespace {

// Opens a new session (new one-time code); the mutex is held.
void openLocked(int64_t now) {
  session.open(now);
  shown_close_ms = 0;
  shown_close_wall_s = 0;
  wifi_test = WifiTest::kIdle;
}

}  // namespace

void SettingsServer::toggle() {
  std::lock_guard<std::mutex> lock(mutex);
  const int64_t now = monotonicMs();
  if (session.isOpen(now)) {
    session.close();
    Serial.println("Settings page: closed by the button");
  } else {
    openLocked(now);
    Serial.println("Settings page: opened by the button");
  }
}

void SettingsServer::openIfClosed() {
  std::lock_guard<std::mutex> lock(mutex);
  const int64_t now = monotonicMs();
  if (session.isOpen(now)) return;
  openLocked(now);
  Serial.println("Settings page: opened automatically (no settings yet)");
}

bool SettingsServer::isOpen() {
  std::lock_guard<std::mutex> lock(mutex);
  return session.isOpen(monotonicMs());
}

AccessNotice SettingsServer::notice(int64_t wall_s) {
  AccessNotice n;
  if (WiFi.status() != WL_CONNECTED) return n;
  std::lock_guard<std::mutex> lock(mutex);
  const int64_t now = monotonicMs();
  if (!session.isOpen(now)) {
    shown_close_ms = 0;
    shown_close_wall_s = 0;
    return n;
  }
  n.mode = AccessMode::kSettings;
  n.address = std::string("http://") + WiFi.localIP().toString().c_str();
  n.code = session.code();
  n.qr = n.address + "/?code=" + n.code;
  const int64_t shown = SettingsSession::shownClose(shown_close_ms, session.closesAtMs());
  if (shown != shown_close_ms || (shown_close_wall_s == 0 && isValidUnixTime(wall_s))) {
    shown_close_ms = shown;
    // Whole minutes, computed once: the notice stays the same until the
    // shown time moves.
    shown_close_wall_s =
        isValidUnixTime(wall_s) ? (wall_s + (shown - now) / 1000 + 30) / 60 * 60 : 0;
  }
  n.closes_wall_s = shown_close_wall_s;
  return n;
}

}  // namespace core

#endif
