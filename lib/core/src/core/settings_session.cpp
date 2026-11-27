#include "core/settings_session.h"

#include <cstdio>
#include <cstring>

namespace core {

bool equalConstantTime(const std::string& a, const std::string& b) {
  if (a.size() != b.size()) return false;
  unsigned char diff = 0;
  for (size_t i = 0; i < a.size(); i++) {
    diff |= static_cast<unsigned char>(a[i] ^ b[i]);
  }
  return diff == 0;
}

void SettingsSession::open(int64_t now_ms) {
  char code[kCodeLength + 1];
  std::snprintf(code, sizeof(code), "%06u", static_cast<unsigned>(random_() % 1000000));
  code_ = code;
  token_.clear();
  replaced_token_.clear();
  wrong_ = 0;
  open_ = true;
  closes_ms_ = now_ms + kTimeoutMs;
}

void SettingsSession::close() {
  open_ = false;
  code_.clear();
  token_.clear();
  replaced_token_.clear();
  wrong_ = 0;
}

bool SettingsSession::isOpen(int64_t now_ms) {
  if (open_ && now_ms >= closes_ms_) close();
  return open_;
}

void SettingsSession::touch(int64_t now_ms) {
  if (isOpen(now_ms)) closes_ms_ = now_ms + kTimeoutMs;
}

SettingsSession::Login SettingsSession::login(const std::string& code, int64_t now_ms,
                                              std::string& token) {
  if (!isOpen(now_ms)) return Login::kClosed;
  std::string digits;
  for (char c : code) {
    if (c != ' ') digits += c;
  }
  if (!equalConstantTime(digits, code_)) {
    if (++wrong_ >= kMaxWrongCodes) {
      close();
      return Login::kClosed;
    }
    return Login::kWrongCode;
  }
  // One session at a time; a second login (other browser) replaces it.
  replaced_token_ = token_;
  token_.clear();
  char part[9];
  for (size_t i = 0; i < kTokenLength / 8; i++) {
    std::snprintf(part, sizeof(part), "%08x", static_cast<unsigned>(random_()));
    token_ += part;
  }
  token = token_;
  wrong_ = 0;
  touch(now_ms);
  return Login::kOk;
}

bool SettingsSession::authorized(const std::string& token, int64_t now_ms) {
  return isOpen(now_ms) && !token_.empty() && equalConstantTime(token, token_);
}

bool SettingsSession::replaced(const std::string& token, int64_t now_ms) {
  return isOpen(now_ms) && !replaced_token_.empty() && equalConstantTime(token, replaced_token_);
}

SettingsSession::Snapshot SettingsSession::snapshot(int64_t now_ms) const {
  Snapshot s;
  if (!open_ || now_ms >= closes_ms_) return s;
  s.magic = kMagic;
  std::strncpy(s.code, code_.c_str(), kCodeLength);
  std::strncpy(s.token, token_.c_str(), kTokenLength);
  s.remaining_ms = closes_ms_ - now_ms;
  return s;
}

void SettingsSession::restore(const Snapshot& s, int64_t now_ms) {
  if (s.magic != kMagic || s.remaining_ms <= 0 || s.remaining_ms > kTimeoutMs) return;
  const std::string code(s.code, strnlen(s.code, kCodeLength));
  const std::string token(s.token, strnlen(s.token, kTokenLength));
  if (code.size() != kCodeLength) return;
  for (char c : code) {
    if (c < '0' || c > '9') return;
  }
  open_ = true;
  code_ = code;
  token_ = token.size() == kTokenLength ? token : "";
  wrong_ = 0;
  closes_ms_ = now_ms + s.remaining_ms;
}

int64_t SettingsSession::shownClose(int64_t shown_ms, int64_t actual_ms) {
  if (shown_ms == 0 || actual_ms < shown_ms || actual_ms - shown_ms >= kShownStepMs) {
    return actual_ms;
  }
  return shown_ms;
}

std::string SettingsSession::tokenFromCookie(const std::string& header) {
  const std::string name = "inktavla_session=";
  size_t pos = 0;
  while ((pos = header.find(name, pos)) != std::string::npos) {
    if (pos == 0 || header[pos - 1] == ' ' || header[pos - 1] == ';') {
      const size_t start = pos + name.size();
      const size_t end = header.find(';', start);
      return header.substr(start, end == std::string::npos ? std::string::npos : end - start);
    }
    pos += name.size();
  }
  return "";
}

}  // namespace core
