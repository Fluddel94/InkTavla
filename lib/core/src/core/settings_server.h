#pragma once

// The settings page in the home network (http://<address>/), served by the
// web server of the Arduino core in its own FreeRTOS task, so the page
// answers while the main loop refreshes the panel or fetches data.
//
// Security (Grundsatz 12): plain HTTP in the home network, open only on
// request (a button) and for 30 minutes after the last action, a one-time
// code on the display (core/settings_session.h), secrets never shown or
// logged. The page itself (HTML, CSS, JavaScript from web/) is compiled into
// the firmware; it holds no data until the browser has logged in.
//
// Routes (all JSON except the page):
//   GET  /                    the page (gzip)
//   GET  /api/state           everything the page shows (401 until logged in)
//   GET  /api/ping            {"closes_in_s"}; not counted as an action
//   POST /api/login           {"code"}; sets the session cookie
//   POST /api/close           closes the page
//   POST /api/config          saves (core/settings_api.h), then restarts
//   POST /api/reset           empty configuration, then restarts
//   POST /api/restart
//   GET  /api/config/download the stored config.json
//   GET  /api/wifi/scan       networks in range
//   POST /api/wifi            {"ssid", "password"}: test, save on success
//   GET  /api/wifi/status     {"state": "idle"|"testing"|"failed"}
//   POST /api/firmware        upload of a firmware file (multipart), checked
//                             with core/firmware_check.h, then restarts
//   GET  /api/update/check    newest release on GitHub (core/update_check.h),
//                             only on request; remembers an offered version
//   POST /api/update/install  {"version"}: downloads the offered release in
//                             its own task, written like an upload
//                             (core/firmware_writer.h), then restarts
//   GET  /api/update/status   {"state": "idle"|"downloading"|"failed"|"done",
//                             "bytes", "total", "error"}; not an action
//   GET  /api/system          state of the device (core/system_report.h)
//   GET  /api/log             the system log as text (core/syslog.h);
//                             ?download=1 as a file
//   GET  /api/<widget>/...    setup help of the widgets (SettingsRoute)
//   POST /api/<widget>/...    same with a JSON body, e.g. POST
//                             /api/calendar/test {"url"} (an address is a
//                             secret: never in the URL of a request)
//
// After saving, the session is kept in memory that survives the restart,
// so the page opens again for the rest of its time with the same code.

#if defined(ARDUINO)

#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/access_notice.h"
#include "core/config_command.h"
#include "core/system_report.h"
#include "core/wifi.h"

namespace core {

// A request of a widget's setup help, e.g. GET /api/mvg/stations?q=...
// handler gets the query arguments and returns the JSON reply; false: the
// service could not be reached (502). A POST route gets the text members
// of its JSON body instead (secrets must not be part of a URL).
struct SettingsRoute {
  std::string path;
  std::function<bool(const std::map<std::string, std::string>& args, std::string& json)> handler;
  bool post = false;
};

struct SettingsServerOptions {
  const uint8_t* page_gz = nullptr;  // the page, gzip-compressed
  size_t page_gz_size = 0;
  const char* firmware_version = "";
  std::string language = "de";  // of the configuration
  ConfigValidator validate;
  // JSON object members describing the widgets and layouts for the page
  // (see widgets::settingsLayoutJson()); they become members of the state.
  std::function<std::string()> layout_json;
  std::vector<SettingsRoute> routes;
  // State of the device for the tab "System" (called in the server task).
  std::function<SystemReport()> system_report;
};

class SettingsServer {
 public:
  // Once in setup(): restores a session kept over a restart and starts the
  // task (it listens only while the page is open and Wi-Fi is connected).
  void begin(Wifi& wifi, SettingsServerOptions options);

  // The button: opens the page, or closes it when it is open.
  void toggle();

  // Opens the page unless it is open already (once after the start when
  // there are no settings yet, see core::opensSettingsOnStart()).
  void openIfClosed();

  bool isOpen();

  // What the display shows while the page is open (mode kNone if closed or
  // without Wi-Fi). wall_s: current Unix time (closing time on the
  // display).
  AccessNotice notice(int64_t wall_s);
};

}  // namespace core

#endif
