// Unit tests for the network parts that work without hardware: URL encoding
// and building, removing chunk lengths, the Wi-Fi reconnection rules, the
// Improv protocol, the configuration commands on the serial line and the
// secrets set over it.

#include <unity.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "core/chunked.h"
#include "core/config_command.h"
#include "core/improv.h"
#include "core/secrets.h"
#include "core/url.h"
#include "core/wifi_policy.h"
#include "widgets/mvg/api.h"

using core::CommandParser;
using core::WifiPolicy;

namespace {

using Action = WifiPolicy::Action;

std::string decodeChunked(const std::string& raw, core::ChunkedDecoder& d) {
  std::string body;
  for (char c : raw) {
    const int b = d.feed(static_cast<uint8_t>(c));
    if (b != core::ChunkedDecoder::kNoByte) body += static_cast<char>(b);
  }
  return body;
}

std::vector<CommandParser::Event> feedAll(CommandParser& p, const std::string& bytes,
                                          int64_t now = 0) {
  std::vector<CommandParser::Event> events;
  for (char c : bytes) {
    CommandParser::Event e = p.feed(static_cast<uint8_t>(c), now);
    if (e.kind != CommandParser::Event::Kind::kNone) events.push_back(e);
  }
  return events;
}

std::string crcHex(const std::string& data) {
  char buffer[12];
  std::snprintf(buffer, sizeof(buffer), "%08lx",
                static_cast<unsigned long>(
                    core::crc32(reinterpret_cast<const uint8_t*>(data.data()), data.size())));
  return buffer;
}

std::string put(const std::string& data) {
  return "INKTAVLA CONFIG PUT " + std::to_string(data.size()) + " " + crcHex(data) + "\n" + data;
}

std::string secretSet(const std::string& name, const std::string& value) {
  return "INKTAVLA SECRET SET " + name + " " + std::to_string(value.size()) + " " + crcHex(value) +
         "\n" + value;
}

class MemorySecrets : public core::SecretStore {
 public:
  bool write(const char* name, const std::string& value) override {
    if (fail) return false;
    values[name] = value;
    return true;
  }
  bool erase(const char* name) override {
    values.erase(name);
    return true;
  }
  bool read(const char* name, std::string& value) override {
    auto it = values.find(name);
    if (it == values.end()) return false;
    value = it->second;
    return true;
  }
  std::map<std::string, std::string> values;
  bool fail = false;
};

class MemoryStore : public core::ConfigStore {
 public:
  bool write(const char* path, const std::string& data) override {
    log.push_back(std::string("write ") + path);
    if (fail_write) return false;
    files[path] = data;
    return true;
  }
  bool rename(const char* from, const char* to) override {
    log.push_back(std::string("rename ") + from + " " + to);
    if (files.count(from) == 0) return false;
    files[to] = files[from];
    files.erase(from);
    return true;
  }
  bool read(const char* path, std::string& data) override {
    if (files.count(path) == 0) return false;
    data = files[path];
    return true;
  }
  std::map<std::string, std::string> files;
  std::vector<std::string> log;
  bool fail_write = false;
};

}  // namespace

void setUp() {}
void tearDown() {}

// --- URLs ------------------------------------------------------------------------

void test_url_encode() {
  TEST_ASSERT_EQUAL_STRING("Pasing-1_a.b~", core::urlEncode("Pasing-1_a.b~").c_str());
  TEST_ASSERT_EQUAL_STRING("M%C3%BCnchen%20Hbf", core::urlEncode("München Hbf").c_str());
  TEST_ASSERT_EQUAL_STRING("de%3A09162%3A10", core::urlEncode("de:09162:10").c_str());
  TEST_ASSERT_EQUAL_STRING("a%26b%3Dc%2Fd%3F", core::urlEncode("a&b=c/d?").c_str());
  TEST_ASSERT_EQUAL_STRING("", core::urlEncode("").c_str());
}

void test_mvg_urls() {
  widgets::mvg::Config c;
  c.station = "de:09162:10";
  TEST_ASSERT_EQUAL_STRING(
      "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=de:09162:10&limit=60"
      "&transportTypes=SBAHN,UBAHN,TRAM,BUS,REGIONAL_BUS,BAHN",
      widgets::mvg::departuresUrl(c).c_str());
  // With a line selection: limit 100, only the selected types.
  c.lines.parse("S3:B:S,RE80:R:Z");
  TEST_ASSERT_EQUAL_STRING(
      "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=de:09162:10&limit=100"
      "&transportTypes=SBAHN,BAHN",
      widgets::mvg::departuresUrl(c).c_str());
  // With sections as well.
  c.lines.parse("");
  c.sections.resize(2);
  c.sections[0].lines.parse("19:H:T");
  c.sections[1].lines.parse("S3:R:S");
  TEST_ASSERT_EQUAL_STRING(
      "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=de:09162:10&limit=100"
      "&transportTypes=SBAHN,TRAM",
      widgets::mvg::departuresUrl(c).c_str());
  TEST_ASSERT_EQUAL_STRING("https://www.mvg.de/.rest/zdm/stations/de:09162:10",
                           widgets::mvg::stationNameUrl("de:09162:10").c_str());
  // Anything unexpected is encoded.
  TEST_ASSERT_EQUAL_STRING("https://www.mvg.de/.rest/zdm/stations/a%2F..%3Fx",
                           widgets::mvg::stationNameUrl("a/..?x").c_str());
}

// --- Chunked transfer encoding -------------------------------------------------------

void test_chunk_lengths_are_removed() {
  core::ChunkedDecoder d;
  const std::string raw =
      "1a\r\nabcdefghijklmnopqrstuvwxyz\r\n10;name=value\r\n0123456789ABCDEF\r\n"
      "B\r\n[{\"a\":1}]\r\n\r\n0\r\n\r\n";
  TEST_ASSERT_EQUAL_STRING("abcdefghijklmnopqrstuvwxyz0123456789ABCDEF[{\"a\":1}]\r\n",
                           decodeChunked(raw, d).c_str());
  TEST_ASSERT_TRUE(d.finished());
  TEST_ASSERT_FALSE(d.failed());
  // Nothing after the last chunk.
  TEST_ASSERT_EQUAL_INT(core::ChunkedDecoder::kNoByte, d.feed('x'));
}

void test_chunk_split_like_the_network() {
  // Lines with LF only and upper-case hex digits.
  core::ChunkedDecoder d;
  TEST_ASSERT_EQUAL_STRING("[1,2]", decodeChunked("5\n[1,2]\r\n0\n", d).c_str());
  TEST_ASSERT_TRUE(d.finished());
  core::ChunkedDecoder e;
  const std::string body(0x1F, 'x');
  TEST_ASSERT_EQUAL_STRING(body.c_str(), decodeChunked("1F\r\n" + body + "\r\n", e).c_str());
  TEST_ASSERT_FALSE(e.finished());
}

void test_malformed_chunks_fail() {
  core::ChunkedDecoder d;
  decodeChunked("zz\r\n", d);
  TEST_ASSERT_TRUE(d.failed());
  core::ChunkedDecoder e;
  decodeChunked("3\r\nabcXY", e);  // no CRLF after the data
  TEST_ASSERT_TRUE(e.failed());
  core::ChunkedDecoder f;
  decodeChunked("3\r\nabcX\n", f);  // CR missing
  TEST_ASSERT_TRUE(f.failed());
  core::ChunkedDecoder g;
  decodeChunked("3\r\nabc\rX", g);  // LF missing
  TEST_ASSERT_TRUE(g.failed());
}

// --- Wi-Fi reconnection ----------------------------------------------------------------

void test_wifi_attempt_ends_with_timeout_then_pauses_grow() {
  WifiPolicy p;
  p.attemptStarted(0);
  TEST_ASSERT_TRUE(p.poll(9999, false, true) == Action::kNone);   // attempt still running
  TEST_ASSERT_TRUE(p.poll(10000, false, true) == Action::kNone);  // timed out, pause 5 s
  TEST_ASSERT_EQUAL_INT(1, p.failures());
  TEST_ASSERT_TRUE(p.poll(14999, false, true) == Action::kNone);
  TEST_ASSERT_TRUE(p.poll(15000, false, true) == Action::kConnect);
  // Second failure reported by the Wi-Fi driver: pause 15 s from then on.
  p.disconnected(16000);
  TEST_ASSERT_TRUE(p.poll(16000, false, true) == Action::kNone);
  TEST_ASSERT_EQUAL_INT(2, p.failures());
  TEST_ASSERT_TRUE(p.poll(30999, false, true) == Action::kNone);
  TEST_ASSERT_TRUE(p.poll(31000, false, true) == Action::kConnect);
}

void test_wifi_restart_after_six_failures_and_reset_when_connected() {
  // Every attempt times out; the sixth failure in a row restarts Wi-Fi.
  WifiPolicy q;
  q.attemptStarted(0);
  int restarts = 0;
  int connects = 0;
  int64_t now = 0;
  for (; now < 2000000 && q.failures() < 7; now += 500) {
    const Action a = q.poll(now, false, true);
    if (a == Action::kRestartAndConnect) {
      restarts++;
      TEST_ASSERT_EQUAL_INT(6, q.failures());
    }
    if (a == Action::kConnect) connects++;
  }
  TEST_ASSERT_EQUAL_INT(1, restarts);
  TEST_ASSERT_EQUAL_INT(5, connects);
  q.connected();
  TEST_ASSERT_EQUAL_INT(0, q.failures());
  TEST_ASSERT_TRUE(q.poll(now, true, true) == Action::kNone);
}

void test_wifi_lost_connection_and_missing_credentials() {
  WifiPolicy p;
  p.attemptStarted(0);
  TEST_ASSERT_TRUE(p.poll(3000, true, true) == Action::kNone);  // connected
  p.connected();
  p.disconnected(100000);  // connection lost
  TEST_ASSERT_TRUE(p.poll(101999, false, true) == Action::kNone);
  TEST_ASSERT_TRUE(p.poll(102000, false, true) == Action::kConnect);  // after 2 s
  WifiPolicy none;
  TEST_ASSERT_TRUE(none.poll(100000, false, false) == Action::kNone);  // nothing to connect with
}

// --- Improv ---------------------------------------------------------------------------

void test_improv_packet_and_checksum() {
  const uint8_t state = core::improv::kProvisioned;
  const std::string p = core::improv::packet(core::improv::kCurrentState, &state, 1);
  const std::string expected = std::string("IMPROV\x01\x01\x01\x04", 10);
  TEST_ASSERT_EQUAL_size_t(12, p.size());
  TEST_ASSERT_TRUE(p.compare(0, 10, expected) == 0);
  uint8_t sum = 0;
  for (size_t i = 0; i < 10; i++) sum += static_cast<uint8_t>(p[i]);
  TEST_ASSERT_EQUAL_HEX8(sum, static_cast<uint8_t>(p[10]));
  TEST_ASSERT_EQUAL_CHAR('\n', p[11]);
}

void test_improv_parser_finds_packets_between_other_output() {
  // RPC "wifi settings" with SSID "Home" and password "secret12".
  const std::string args = std::string("\x04Home\x08secret12", 14);
  const std::string rpc =
      std::string(1, core::improv::kWifiSettings) + static_cast<char>(args.size()) + args;
  const std::string packet = core::improv::packet(
      core::improv::kRpc, reinterpret_cast<const uint8_t*>(rpc.data()), rpc.size());
  const std::string stream = "Wi-Fi: no credentials\nIMPRO" + packet + "INKTAVLA INFO\n";

  core::improv::Parser parser;
  int packets = 0;
  for (char c : stream) {
    if (parser.feed(static_cast<uint8_t>(c)) == core::improv::Parser::Result::kPacket) {
      packets++;
      TEST_ASSERT_EQUAL_HEX8(core::improv::kRpc, parser.type());
      core::improv::Rpc r;
      TEST_ASSERT_TRUE(core::improv::parseRpc(parser.data(), parser.length(), r));
      std::string ssid, password;
      TEST_ASSERT_TRUE(core::improv::parseWifiSettings(r, ssid, password));
      TEST_ASSERT_EQUAL_STRING("Home", ssid.c_str());
      TEST_ASSERT_EQUAL_STRING("secret12", password.c_str());
    }
  }
  TEST_ASSERT_EQUAL_INT(1, packets);

  // A damaged checksum is reported.
  std::string broken = packet;
  broken[broken.size() - 2] ^= 0x01;
  core::improv::Parser p2;
  core::improv::Parser::Result last = core::improv::Parser::Result::kNone;
  for (char c : broken) {
    const auto r = p2.feed(static_cast<uint8_t>(c));
    if (r != core::improv::Parser::Result::kNone) last = r;
  }
  TEST_ASSERT_TRUE(last == core::improv::Parser::Result::kBadChecksum);
}

void test_improv_rpc_validation_and_result() {
  core::improv::Rpc r;
  const uint8_t short_rpc[] = {0x01, 0x05, 0x01};
  TEST_ASSERT_FALSE(core::improv::parseRpc(short_rpc, sizeof(short_rpc), r));
  const uint8_t empty_ssid[] = {0x01, 0x02, 0x00, 0x00};
  TEST_ASSERT_TRUE(core::improv::parseRpc(empty_ssid, sizeof(empty_ssid), r));
  std::string ssid, password;
  TEST_ASSERT_FALSE(core::improv::parseWifiSettings(r, ssid, password));
  const uint8_t open_network[] = {0x01, 0x03, 0x01, 'X', 0x00};
  TEST_ASSERT_TRUE(core::improv::parseRpc(open_network, sizeof(open_network), r));
  TEST_ASSERT_TRUE(core::improv::parseWifiSettings(r, ssid, password));
  TEST_ASSERT_EQUAL_STRING("X", ssid.c_str());
  TEST_ASSERT_EQUAL_STRING("", password.c_str());

  const std::string result = core::improv::rpcResult(core::improv::kGetInfo, {"InkTavla", "0.1"});
  // Header, version, type, length 2 + 9 + 4 = 15, command, data length 13.
  TEST_ASSERT_EQUAL_HEX8(core::improv::kRpcResult, static_cast<uint8_t>(result[7]));
  TEST_ASSERT_EQUAL_INT(15, static_cast<uint8_t>(result[8]));
  TEST_ASSERT_EQUAL_HEX8(core::improv::kGetInfo, static_cast<uint8_t>(result[9]));
  TEST_ASSERT_EQUAL_INT(13, static_cast<uint8_t>(result[10]));
  TEST_ASSERT_EQUAL_INT(8, static_cast<uint8_t>(result[11]));
  // Texts that do not fit are left out.
  const std::string big =
      core::improv::rpcResult(core::improv::kScan, {std::string(200, 'a'), std::string(100, 'b')});
  TEST_ASSERT_EQUAL_INT(2 + 201, static_cast<uint8_t>(big[8]));
}

// --- Configuration commands --------------------------------------------------------------

void test_commands_get_info_and_unknown() {
  CommandParser p;
  auto events = feedAll(p, "INKTAVLA CONFIG GET\r\nINKTAVLA INFO\nINKTAVLA REBOOT\n");
  TEST_ASSERT_EQUAL_size_t(3, events.size());
  TEST_ASSERT_TRUE(events[0].kind == CommandParser::Event::Kind::kConfigGet);
  TEST_ASSERT_TRUE(events[1].kind == CommandParser::Event::Kind::kInfo);
  TEST_ASSERT_TRUE(events[2].kind == CommandParser::Event::Kind::kError);
  // Other text on the line is ignored, also very long lines.
  events = feedAll(p, "hello\nINKTAVLA\n" + std::string(500, 'x') + "INKTAVLA INFO\n");
  TEST_ASSERT_EQUAL_size_t(0, events.size());
  events = feedAll(p, "INKTAVLA INFO\n");
  TEST_ASSERT_EQUAL_size_t(1, events.size());
  // The log is sent by the serial link itself, line by line.
  events = feedAll(p, "INKTAVLA LOG\n");
  TEST_ASSERT_EQUAL_size_t(1, events.size());
  TEST_ASSERT_TRUE(events[0].kind == CommandParser::Event::Kind::kLog);
}

void test_command_put_receives_exact_bytes() {
  CommandParser p;
  // The file may contain newlines and even an Improv header.
  const std::string json = "{\n  \"language\": \"en\", \"x\": \"IMPROV\"\n}";
  auto events = feedAll(p, put(json));
  TEST_ASSERT_EQUAL_size_t(1, events.size());
  TEST_ASSERT_TRUE(events[0].kind == CommandParser::Event::Kind::kConfigPut);
  TEST_ASSERT_EQUAL_STRING(json.c_str(), events[0].payload.c_str());
  TEST_ASSERT_FALSE(p.receivingPayload());
}

void test_command_put_errors() {
  CommandParser p;
  auto events = feedAll(p, "INKTAVLA CONFIG PUT 3 00000000\nabc");
  TEST_ASSERT_EQUAL_size_t(1, events.size());
  TEST_ASSERT_TRUE(events[0].kind == CommandParser::Event::Kind::kError);  // checksum

  for (const char* line :
       {"INKTAVLA CONFIG PUT 99999 1\n", "INKTAVLA CONFIG PUT 0 1\n", "INKTAVLA CONFIG PUT x 1\n",
        "INKTAVLA CONFIG PUT 10\n", "INKTAVLA CONFIG PUT -5 1\n", "INKTAVLA CONFIG PUT 5 xyz\n"}) {
    events = feedAll(p, line);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1, events.size(), line);
    TEST_ASSERT_TRUE_MESSAGE(events[0].kind == CommandParser::Event::Kind::kError, line);
    TEST_ASSERT_FALSE(p.receivingPayload());
  }

  // Interrupted transfer: timeout after 5 s without a byte.
  events = feedAll(p, "INKTAVLA CONFIG PUT 10 0\nabc", 1000);
  TEST_ASSERT_TRUE(p.receivingPayload());
  TEST_ASSERT_TRUE(p.poll(5999).kind == CommandParser::Event::Kind::kNone);
  TEST_ASSERT_TRUE(p.poll(6000).kind == CommandParser::Event::Kind::kError);
  TEST_ASSERT_FALSE(p.receivingPayload());
  events = feedAll(p, "INKTAVLA INFO\n");  // back to normal
  TEST_ASSERT_EQUAL_size_t(1, events.size());
}

void test_handle_put_saves_atomically_and_reports_issues() {
  MemoryStore store;
  store.files["/config.json"] = "{\"old\":true}";
  CommandParser::Event e;
  e.kind = CommandParser::Event::Kind::kConfigPut;
  e.payload = R"({"language":"xx","display":{"full_refresh_minutes":5}})";
  const core::ConfigValidator validate = [](JsonObjectConst root, core::ConfigIssues& issues) {
    core::parseCoreConfig(root, issues);
  };
  const core::CommandReply r = core::handleCommand(e, store, validate, "");
  TEST_ASSERT_TRUE(r.restart);
  TEST_ASSERT_EQUAL_STRING(e.payload.c_str(), store.files["/config.json"].c_str());
  TEST_ASSERT_EQUAL_size_t(0, store.files.count("/config.json.tmp"));
  TEST_ASSERT_EQUAL_STRING("write /config.json.tmp", store.log[0].c_str());
  TEST_ASSERT_EQUAL_STRING("rename /config.json.tmp /config.json", store.log[1].c_str());
  TEST_ASSERT_EQUAL_STRING(
      "INKTAVLA ISSUE language: must be one of \"de\", \"en\"\n"
      "INKTAVLA OK saved, 1 issue, restarting\n",
      r.text.c_str());
}

void test_handle_put_rejects_invalid_files() {
  MemoryStore store;
  store.files["/config.json"] = "{\"old\":true}";
  CommandParser::Event e;
  e.kind = CommandParser::Event::Kind::kConfigPut;
  for (const char* bad : {"{\"language\": ", "[1,2]", "not json"}) {
    e.payload = bad;
    const core::CommandReply r = core::handleCommand(e, store, nullptr, "");
    TEST_ASSERT_FALSE(r.restart);
    TEST_ASSERT_TRUE_MESSAGE(r.text.rfind("INKTAVLA ERR ", 0) == 0, bad);
    TEST_ASSERT_EQUAL_size_t(0, store.log.size());
  }
  // Writing fails: the old file stays.
  e.payload = "{}";
  store.fail_write = true;
  const core::CommandReply r = core::handleCommand(e, store, nullptr, "");
  TEST_ASSERT_FALSE(r.restart);
  TEST_ASSERT_EQUAL_STRING("{\"old\":true}", store.files["/config.json"].c_str());
}

void test_handle_get_and_info() {
  MemoryStore store;
  CommandParser::Event e;
  e.kind = CommandParser::Event::Kind::kConfigGet;
  TEST_ASSERT_EQUAL_STRING("INKTAVLA ERR no configuration file\n",
                           core::handleCommand(e, store, nullptr, "").text.c_str());
  store.files["/config.json"] = "{}";
  TEST_ASSERT_EQUAL_STRING(("INKTAVLA DATA 2 " + crcHex("{}") + "\n{}\nINKTAVLA OK\n").c_str(),
                           core::handleCommand(e, store, nullptr, "").text.c_str());
  e.kind = CommandParser::Event::Kind::kInfo;
  TEST_ASSERT_EQUAL_STRING("INKTAVLA INFO firmware=1\nINKTAVLA OK\n",
                           core::handleCommand(e, store, nullptr, "firmware=1").text.c_str());
  e.kind = CommandParser::Event::Kind::kError;
  e.error = "unknown command";
  TEST_ASSERT_EQUAL_STRING("INKTAVLA ERR unknown command\n",
                           core::handleCommand(e, store, nullptr, "").text.c_str());
}

// --- Secrets ----------------------------------------------------------------------------

constexpr char kAddress[] =
    "https://calendar.example.invalid/ical/private-0123456789abcdef/basic.ics";

void test_secret_commands_are_parsed() {
  CommandParser p;
  // The value may contain anything printable, also "INKTAVLA" or "IMPROV".
  auto events =
      feedAll(p, secretSet("calendar_url", kAddress) + "INKTAVLA SECRET CLEAR calendar_url\n");
  TEST_ASSERT_EQUAL_size_t(2, events.size());
  TEST_ASSERT_TRUE(events[0].kind == CommandParser::Event::Kind::kSecretSet);
  TEST_ASSERT_EQUAL_STRING("calendar_url", events[0].name.c_str());
  TEST_ASSERT_EQUAL_STRING(kAddress, events[0].payload.c_str());
  TEST_ASSERT_TRUE(events[1].kind == CommandParser::Event::Kind::kSecretClear);
  TEST_ASSERT_EQUAL_STRING("calendar_url", events[1].name.c_str());

  for (const char* line :
       {"INKTAVLA SECRET SET wifi_pass 3 0\n", "INKTAVLA SECRET SET calendar_url 0 0\n",
        "INKTAVLA SECRET SET calendar_url 2000 0\n", "INKTAVLA SECRET SET calendar_url 5\n",
        "INKTAVLA SECRET SET calendar_url\n", "INKTAVLA SECRET CLEAR other\n"}) {
    events = feedAll(p, line);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1, events.size(), line);
    TEST_ASSERT_TRUE_MESSAGE(events[0].kind == CommandParser::Event::Kind::kError, line);
    TEST_ASSERT_FALSE(p.receivingPayload());
  }
  // A damaged value is not handed on; then the parser works normally.
  events = feedAll(p, "INKTAVLA SECRET SET calendar_url 3 00000000\nabc");
  TEST_ASSERT_TRUE(events[0].kind == CommandParser::Event::Kind::kError);
  events = feedAll(p, put("{}"));
  TEST_ASSERT_TRUE(events[0].kind == CommandParser::Event::Kind::kConfigPut);
  TEST_ASSERT_TRUE(events[0].name.empty());
}

void test_secret_values_are_checked() {
  std::string reason;
  TEST_ASSERT_TRUE(core::checkSecret("calendar_url", kAddress, reason));
  TEST_ASSERT_TRUE(core::checkSecret("calendar_url", "webcal://example.invalid/a.ics", reason));
  TEST_ASSERT_TRUE(core::checkSecret("calendar_url", "HTTPS://example.invalid/a.ics", reason));
  TEST_ASSERT_FALSE(core::checkSecret("calendar_url", "http://example.invalid/a.ics", reason));
  TEST_ASSERT_FALSE(core::checkSecret("calendar_url", "https://", reason));
  TEST_ASSERT_FALSE(core::checkSecret("calendar_url", "https://a b", reason));
  TEST_ASSERT_FALSE(core::checkSecret("calendar_url", "", reason));
  TEST_ASSERT_FALSE(core::checkSecret("calendar_url", "https://\xC3\xA4", reason));
  TEST_ASSERT_FALSE(core::checkSecret("other", kAddress, reason));
  TEST_ASSERT_EQUAL_STRING("unknown secret", reason.c_str());
  TEST_ASSERT_EQUAL_STRING("https://example.invalid/a.ics",
                           core::secretUrl("webcal://example.invalid/a.ics").c_str());
  TEST_ASSERT_EQUAL_STRING(kAddress, core::secretUrl(kAddress).c_str());
}

void test_handle_secrets_never_repeats_them() {
  MemoryStore store;
  MemorySecrets secrets;
  CommandParser::Event e;
  e.kind = CommandParser::Event::Kind::kSecretSet;
  e.name = "calendar_url";
  e.payload = kAddress;
  core::CommandReply r = core::handleCommand(e, store, nullptr, "", &secrets);
  TEST_ASSERT_TRUE(r.restart);
  TEST_ASSERT_EQUAL_STRING("INKTAVLA OK calendar_url saved, restarting\n", r.text.c_str());
  TEST_ASSERT_EQUAL_STRING(kAddress, secrets.values["calendar_url"].c_str());
  TEST_ASSERT_TRUE(secrets.has("calendar_url"));
  TEST_ASSERT_EQUAL_size_t(0, store.log.size());  // never in the configuration file

  // Invalid: refused, the old value stays, the reply does not repeat it.
  e.payload = "http://calendar.example.invalid/secret-token";
  r = core::handleCommand(e, store, nullptr, "", &secrets);
  TEST_ASSERT_FALSE(r.restart);
  TEST_ASSERT_TRUE(r.text.rfind("INKTAVLA ERR calendar_url must start with https://", 0) == 0);
  TEST_ASSERT_TRUE(r.text.find("secret-token") == std::string::npos);
  TEST_ASSERT_EQUAL_STRING(kAddress, secrets.values["calendar_url"].c_str());

  secrets.fail = true;
  e.payload = kAddress;
  r = core::handleCommand(e, store, nullptr, "", &secrets);
  TEST_ASSERT_FALSE(r.restart);
  TEST_ASSERT_EQUAL_STRING("INKTAVLA ERR cannot save calendar_url\n", r.text.c_str());

  e.kind = CommandParser::Event::Kind::kSecretClear;
  r = core::handleCommand(e, store, nullptr, "", &secrets);
  TEST_ASSERT_TRUE(r.restart);
  TEST_ASSERT_EQUAL_STRING("INKTAVLA OK calendar_url cleared, restarting\n", r.text.c_str());
  TEST_ASSERT_FALSE(secrets.has("calendar_url"));

  // Without a secret store (e.g. on the PC): an error, nothing else.
  r = core::handleCommand(e, store, nullptr, "");
  TEST_ASSERT_FALSE(r.restart);
  TEST_ASSERT_EQUAL_STRING("INKTAVLA ERR secrets not available\n", r.text.c_str());
}

void test_crc32_matches_zlib() {
  const std::string text = "123456789";
  TEST_ASSERT_EQUAL_HEX32(0xCBF43926,
                          core::crc32(reinterpret_cast<const uint8_t*>(text.data()), text.size()));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_url_encode);
  RUN_TEST(test_mvg_urls);
  RUN_TEST(test_chunk_lengths_are_removed);
  RUN_TEST(test_chunk_split_like_the_network);
  RUN_TEST(test_malformed_chunks_fail);
  RUN_TEST(test_wifi_attempt_ends_with_timeout_then_pauses_grow);
  RUN_TEST(test_wifi_restart_after_six_failures_and_reset_when_connected);
  RUN_TEST(test_wifi_lost_connection_and_missing_credentials);
  RUN_TEST(test_improv_packet_and_checksum);
  RUN_TEST(test_improv_parser_finds_packets_between_other_output);
  RUN_TEST(test_improv_rpc_validation_and_result);
  RUN_TEST(test_commands_get_info_and_unknown);
  RUN_TEST(test_command_put_receives_exact_bytes);
  RUN_TEST(test_command_put_errors);
  RUN_TEST(test_handle_put_saves_atomically_and_reports_issues);
  RUN_TEST(test_handle_put_rejects_invalid_files);
  RUN_TEST(test_handle_get_and_info);
  RUN_TEST(test_secret_commands_are_parsed);
  RUN_TEST(test_secret_values_are_checked);
  RUN_TEST(test_handle_secrets_never_repeats_them);
  RUN_TEST(test_crc32_matches_zlib);
  return UNITY_END();
}
