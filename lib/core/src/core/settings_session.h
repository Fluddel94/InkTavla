#pragma once

// Access to the settings page (Grundsatz 12 of the project): the page is
// only open on request (a button) and closes 30 minutes after the last
// action. Opening it shows a one-time code of six digits on the display;
// whoever enters it (or scans the QR code with it) gets a session cookie.
// One session at a time: a login on a second browser ends the first one,
// which is then told so (replaced()); the code stays valid.
// After kMaxWrongCodes wrong codes the page closes at once.
//
// Pure logic with an injected random source, tested on the PC; the web
// server is core/settings_server.h.

#include <cstdint>
#include <functional>
#include <string>

namespace core {

class SettingsSession {
 public:
  static constexpr int64_t kTimeoutMs = 30 * 60 * 1000;
  static constexpr int kMaxWrongCodes = 5;
  static constexpr size_t kCodeLength = 6;
  static constexpr size_t kTokenLength = 32;  // hex digits (128 bits)

  // State kept over a restart (e.g. after saving), so the page opens again
  // for the rest of its time with the same code and the browser stays
  // logged in. Plain data for memory that survives a software reset.
  struct Snapshot {
    uint32_t magic = 0;
    char code[kCodeLength + 1] = {};
    char token[kTokenLength + 1] = {};
    int64_t remaining_ms = 0;
  };

  enum class Login { kOk, kWrongCode, kClosed };

  // random: a random 32-bit number (hardware RNG on the device).
  explicit SettingsSession(std::function<uint32_t()> random) : random_(std::move(random)) {}

  // Opens the page with a new code; old sessions end.
  void open(int64_t now_ms);
  void close();

  // Open (closes it once the time is up).
  bool isOpen(int64_t now_ms);

  // An action on the page: the 30 minutes start again.
  void touch(int64_t now_ms);

  int64_t closesAtMs() const { return closes_ms_; }
  const std::string& code() const { return code_; }

  // Checks a code (spaces are ignored: "482 913"). On success token is the
  // session token for the cookie.
  Login login(const std::string& code, int64_t now_ms, std::string& token);

  // True for the token of the current session (while open).
  bool authorized(const std::string& token, int64_t now_ms);

  // True for the token of the session that the last login replaced (while
  // open), so that browser can say it was logged out by another device.
  bool replaced(const std::string& token, int64_t now_ms);

  Snapshot snapshot(int64_t now_ms) const;
  // Opens the page again from a snapshot (does nothing if it is invalid or
  // its time is up).
  void restore(const Snapshot& snapshot, int64_t now_ms);

  // Closing time to show on the display: every action moves the real one,
  // but the e-paper is only refreshed when it moves by kShownStepMs or
  // more (or earlier). 0 = nothing shown yet.
  static constexpr int64_t kShownStepMs = 5 * 60 * 1000;
  static int64_t shownClose(int64_t shown_ms, int64_t actual_ms);

  // Value of the cookie "inktavla_session" in a Cookie header, "" if none.
  static std::string tokenFromCookie(const std::string& header);

 private:
  static constexpr uint32_t kMagic = 0x494b5453;  // "IKTS"

  std::function<uint32_t()> random_;
  bool open_ = false;
  int64_t closes_ms_ = 0;
  std::string code_;
  std::string token_;           // empty: nobody logged in yet
  std::string replaced_token_;  // session ended by the last login, "" if none
  int wrong_ = 0;
};

// Compares in constant time (no early exit at the first difference).
bool equalConstantTime(const std::string& a, const std::string& b);

}  // namespace core
