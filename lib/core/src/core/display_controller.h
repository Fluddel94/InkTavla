#pragma once

// Display controller: the main loop of the device.
//
// It decides *when* data is fetched (scheduler), keeps track of how fresh
// the data of every slot is, has the screen drawn and decides between no,
// partial and full refresh. It is the only part that talks to the panel.
//
// What is drawn and where is decided above core: widgets implement Content
// (fetching) and a ScreenRenderer composes the whole frame from the state
// passed in. Time comes from a Clock, so the whole loop runs on the PC with a
// simulated clock, simulated network and a PNG display (see hal::PngDisplay).

#include <cstdint>
#include <vector>

#include "core/access_notice.h"
#include "core/freshness.h"
#include "core/refresh.h"
#include "core/scheduler.h"
#include "hal/display.h"

namespace core {

class Clock {
 public:
  virtual ~Clock() = default;
  virtual int64_t monotonicMs() const = 0;  // milliseconds since start, never goes back
  virtual int64_t wallTime() const = 0;     // Unix seconds, invalid before NTP
};

// Clock of the device (or the PC): monotonicMs() and the system time.
class SystemClock : public Clock {
 public:
  int64_t monotonicMs() const override;
  int64_t wallTime() const override;
};

// Data of one slot as seen by the controller (implemented by the widgets).
class Content {
 public:
  virtual ~Content() = default;

  // False for contents without data of their own (e.g. the header).
  virtual bool fetches() const { return false; }
  virtual Schedule schedule() const { return {}; }

  // False if settings are missing; then nothing is fetched.
  virtual bool isSetUp() const { return true; }

  // Fetches fresh data; true on success. A failure keeps the old data.
  virtual bool fetch(int64_t wall_s) {
    (void)wall_s;
    return true;
  }
};

struct ContentStatus {
  DataState state = DataState::kFresh;
  int64_t last_success_wall_s = 0;  // for the age hint
};

struct ScreenState {
  int64_t wall_s = 0;
  // Network as shown: a lost connection only counts as offline after
  // DisplayController::kOfflineGraceMs, like a failing data source.
  bool network_connected = false;
  std::vector<ContentStatus> contents;  // in the order of addContent()
  // Settings page or Wi-Fi setup hotspot, if open.
  AccessNotice notice;
};

// Composes the frame from the state (implemented above core).
class ScreenRenderer {
 public:
  virtual ~ScreenRenderer() = default;
  virtual FrameView render(const ScreenState& state) = 0;
  // Areas checked for changes, e.g. the header and every slot.
  virtual std::vector<hal::Rect> areas() const = 0;
};

class DisplayController {
 public:
  // Redraw at this second of every minute (clock in the header), aligned
  // with the MVG update so that both change in one refresh.
  static constexpr int32_t kRedrawSecond = 1;
  // A connection lost for less than this is not shown (Wi-Fi symbol,
  // "Keine Verbindung"): short dropouts are bridged like failing updates.
  // The time counts from the loss, or from the start before the first
  // connection.
  static constexpr int64_t kOfflineGraceMs = Freshness::kGraceMs;

  DisplayController(Clock& clock, hal::Display& display, ScreenRenderer& renderer,
                    int full_refresh_minutes);

  // Adds the content of the next slot (order = order in ScreenState).
  void addContent(Content& content);

  // Current state of the network; the screen shows it after the grace
  // period (see kOfflineGraceMs).
  void setNetworkConnected(bool connected);

  // Fetches everything now (e.g. a button press).
  void requestUpdate();

  // The next refresh of the panel is a full one (e.g. a button press
  // against ghosting); redraws at once.
  void requestFullRefresh();

  // Shows or hides the notice of the settings page or the Wi-Fi setup;
  // redraws at once when it changes.
  void setAccessNotice(const AccessNotice& notice);

  // One pass of the main loop: runs due fetches, redraws and refreshes the
  // panel if anything changed. Returns the milliseconds until the next pass
  // is needed.
  int64_t step();

  // State used for the last frame.
  const ScreenState& state() const { return state_; }

 private:
  struct Entry {
    Content* content;
    Freshness freshness;
    Scheduler::TaskId task = -1;  // -1: nothing to fetch
  };

  void buildState(int64_t now_ms, int64_t wall_s);
  // Whether the network is shown as connected at this time.
  bool networkShown(int64_t now_ms) const;
  void redraw(int64_t now_ms);

  Clock& clock_;
  hal::Display& display_;
  ScreenRenderer& renderer_;
  RefreshPlanner planner_;
  Scheduler scheduler_;
  Scheduler::TaskId redraw_task_;
  std::vector<Entry> entries_;
  ScreenState state_;
  std::vector<uint8_t> shown_;  // frame on the panel; large, ends up in PSRAM
  FrameView shown_view_;
  bool network_connected_ = false;
  int64_t network_changed_ms_ = 0;  // last change of network_connected_
  bool network_shown_ = true;       // network as shown in the last frame
  bool force_redraw_ = true;
  bool wall_valid_ = false;
  AccessNotice notice_;
};

}  // namespace core
