"""Stand-in for the display's settings server, to try the settings page
(web/) in a browser without the device.

    python tools/mock_device.py [--port 8080]

then open http://localhost:8080/ (or http://localhost:8080/?code=123456
to skip the login; the code is always 123456). Serves web/ as it is (no
embedding) and answers the routes of lib/core/src/core/settings_server.cpp
with made-up data (Marienplatz, Munich, "Heimnetz"). Nothing is written
anywhere; a saved configuration only lives until the server stops.

Switches for checks (GET, answer "ok"):
    /_mock/closed      next requests get 401 (page closed)
    /_mock/issues      the next save answers 422 with a problem
    /_mock/blocking    same, with a blocking problem
    /_mock/extra       adds a made-up widget "quote" without a symbol or page
                       of its own (in place 2)
    /_mock/reset       back to the start state

Python standard library only. Used by hand and by Playwright checks; it is
not part of the firmware.
"""

import argparse
import copy
import json
import os
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import parse_qs, urlparse

WEB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "web")
CODE = "123456"
STARTED = time.time()

CONFIG = {
    "version": 1,
    "language": "de",
    "layout": {"slots": ["mvg", "calendar", "weather"]},
    "display": {"full_refresh_minutes": 360},
    "widgets": {
        "mvg": {"station": "de:09162:2", "name": "Marienplatz",
                "sections": [{"name": "Zentrum", "lines": ["S1:R:S", "U6:H:U"]},
                             {"name": "Auswärts", "lines": ["S1:H:S", "S8:H:S"]}]},
        "weather": {"name": "München", "latitude": 48.137, "longitude": 11.575},
    },
}

LAYOUT = {
    "widgets": [{"id": "mvg", "min": "M", "band": True},
                {"id": "weather", "min": "S", "band": True, "band_alone": True},
                {"id": "calendar", "min": "S", "band": True}],
    "layouts": [{"steps": ["M", "S", "S"], "wide": "M"}, {"steps": ["L", "L"]}],
    "steps": {"S": 214, "M": 280, "L": 367},
    "screen": {"width": 800, "height": 480, "band": 205, "margin": 20, "gap": 26},
}

LINES = [
    {"key": "S1", "label": "S1", "type": "S", "H": ["Freising", "Flughafen"], "R": ["Ostbahnhof", "Leuchtenbergring"]},
    {"key": "S8", "label": "S8", "type": "S", "H": ["Flughafen München"], "R": ["Herrsching"]},
    {"key": "U3", "label": "U3", "type": "U", "H": ["Moosach"], "R": ["Fürstenried West"]},
    {"key": "U6", "label": "U6", "type": "U", "H": ["Garching-Forschungszentrum"], "R": ["Klinikum Großhadern"]},
    {"key": "N40", "label": "N40", "type": "B", "H": [], "R": []},
]

LOG = "\n".join([
    "2026-11-21 08:00:01 INFO  core     start 1 since power on, reason power on",
    "2026-11-21 08:00:04 INFO  wifi     connected, -58 dBm",
    "2026-11-21 08:12:30 WARN  mvg      request failed: timeout",
    "2026-11-21 08:13:31 INFO  mvg      ok again",
])

state = {}


def reset():
    state.clear()
    state.update(config=copy.deepcopy(CONFIG), calendar=False, logged_in=False, closed=False,
                 issues=None, extra=False, touched=time.time())


reset()


class Handler(BaseHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def send(self, status, body, kind="application/json"):
        data = body if isinstance(body, bytes) else (
            json.dumps(body).encode() if kind == "application/json" else body.encode())
        self.send_response(status)
        self.send_header("Content-Type", kind + ("; charset=utf-8" if "text" in kind or "json" in kind else ""))
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(data)

    def body(self):
        n = int(self.headers.get("Content-Length") or 0)
        raw = self.rfile.read(n) if n else b""
        try:
            return json.loads(raw or b"null")
        except ValueError:
            return None

    def allowed(self):
        if state["closed"]:
            self.send(401, {"open": False, "lang": state["config"].get("language", "de")})
            return False
        if not state["logged_in"]:
            self.send(401, {"open": True, "lang": state["config"].get("language", "de")})
            return False
        return True

    def closes_in(self):
        return max(0, int(1800 - (time.time() - state["touched"])))

    def do_GET(self):
        url = urlparse(self.path)
        q = {k: v[0] for k, v in parse_qs(url.query).items()}
        p = url.path
        if p.startswith("/_mock/"):
            what = p[len("/_mock/"):]
            if what == "closed":
                state["closed"] = True
            elif what in ("issues", "blocking"):
                state["issues"] = what
            elif what == "extra":
                state["extra"] = True
                state["config"]["layout"]["slots"] = ["mvg", "quote", "weather"]
            elif what == "reset":
                reset()
            return self.send(200, "ok", "text/plain")
        if not p.startswith("/api/"):
            name = "index.html" if p in ("/", "") else p.lstrip("/")
            path = os.path.normpath(os.path.join(WEB, name))
            if not path.startswith(os.path.normpath(WEB)) or not os.path.isfile(path):
                return self.send(404, "not found", "text/plain")
            kind = {"html": "text/html", "css": "text/css", "js": "text/javascript"}.get(name.rsplit(".", 1)[-1],
                                                                                      "application/octet-stream")
            with open(path, "rb") as f:
                return self.send(200, f.read(), kind)
        route = p[len("/api/"):]
        if route == "state":
            if not self.allowed():
                return
            s = {"lang": state["config"].get("language", "de"), "closes_in_s": self.closes_in(), "timeout_s": 1800,
                 "config": state["config"], "config_problem": "", "secrets": {"calendar_url": state["calendar"]},
                 "wifi": {"ssid": "Heimnetz", "rssi": -58, "ip": "192.168.178.42"},
                 "device": {"firmware": "0.4.0-dev"}}
            s.update(copy.deepcopy(LAYOUT))
            if state["extra"]:
                s["widgets"].append({"id": "quote", "min": "S", "band": False})
            return self.send(200, s)
        if not self.allowed():
            return
        if route == "ping":
            return self.send(200, {"closes_in_s": self.closes_in()})
        state["touched"] = time.time()
        if route == "mvg/stations":
            return self.send(200, [{"id": "de:09162:2", "name": "Marienplatz", "place": "München", "types": "SUB"},
                                   {"id": "de:09162:6", "name": "Hauptbahnhof", "place": "München", "types": "SUTB"}])
        if route == "mvg/station":
            return self.send(200, {"id": q.get("id", ""), "name": "Haltestelle " + q.get("id", "")})
        if route == "mvg/lines":
            time.sleep(0.4)
            return self.send(200, LINES)
        if route == "weather/places":
            return self.send(200, [{"name": "München", "region": "Bayern", "country": "Deutschland",
                                    "lat": 48.137, "lon": 11.575}])
        if route == "wifi/scan":
            time.sleep(0.5)
            return self.send(200, [{"ssid": "Heimnetz", "rssi": -58, "secure": True},
                                   {"ssid": "Gast", "rssi": -77, "secure": False}])
        if route == "wifi/status":
            return self.send(200, {"state": "failed"})
        if route == "config/download":
            return self.send(200, state["config"])
        if route == "update/check":
            return self.send(200, {"status": "up_to_date", "installed": "0.4.0-dev", "latest": "0.3.0"})
        if route == "update/status":
            return self.send(200, {"state": "idle"})
        if route == "system":
            up = int(time.time() - STARTED) + 3 * 86400
            return self.send(200, {
                "device": {"firmware": "0.4.0-dev", "uptime_s": up, "restart_reason": "power on", "boot": 1,
                           "wifi_connected": True, "rssi": -58, "time_valid": True, "heap_free": 180000,
                           "heap_min_free": 150000, "heap_largest": 90000, "psram_free": 7000000,
                           "psram_size": 8388608},
                "wifi": {"drops": 0, "drops_24h": 0},
                "log": {"warnings": 1, "errors": 0},
                "sources": [
                    {"name": "mvg", "state": "ok", "last_success": int(time.time()) - 60, "duration_ms": 900,
                     "successes": 400, "failures": 1, "fails": 1, "fails_24h": 1,
                     "last_failure": int(time.time()) - 3000, "problem": "timeout"},
                    {"name": "weather", "state": "ok", "last_success": int(time.time()) - 700, "duration_ms": 600,
                     "successes": 30, "failures": 0},
                    {"name": "calendar", "state": "not set up", "successes": 0, "failures": 0}]})
        if route == "log":
            return self.send(200, LOG, "text/plain")
        return self.send(404, {"error": "unknown route"})

    def do_POST(self):
        p = urlparse(self.path).path
        body = self.body()
        if p == "/api/login":
            if state["closed"]:
                return self.send(401, {"open": False})
            if isinstance(body, dict) and str(body.get("code", "")).replace(" ", "") == CODE:
                state["logged_in"] = True
                state["touched"] = time.time()
                return self.send(200, {"ok": True})
            return self.send(403, {"error": "wrong code"})
        if not self.allowed():
            return
        if p == "/api/close":
            state["closed"] = True
            return self.send(200, {"ok": True})
        state["touched"] = time.time()
        if p == "/api/config":
            if state["issues"] and not (body or {}).get("force"):
                blocking = state["issues"] == "blocking"
                state["issues"] = None
                return self.send(422, {"issues": [{"path": "widgets.mvg.sections", "blocking": blocking,
                                                   "message": "section 2 has no lines"}]})
            state["config"] = body.get("config", state["config"])
            secrets = body.get("secrets") or {}
            if "calendar_url" in secrets:
                state["calendar"] = secrets["calendar_url"] is not None
            return self.send(200, {"ok": True})
        if p in ("/api/restart", "/api/reset"):
            if p == "/api/reset":
                state["config"] = copy.deepcopy(CONFIG)
            return self.send(200, {"ok": True})
        if p == "/api/wifi":
            return self.send(200, {"ok": True})
        if p == "/api/update/install":
            return self.send(200, {"ok": True})
        if p == "/api/firmware":
            self.rfile.read(int(self.headers.get("Content-Length") or 0))
            return self.send(400, {"error": "not an InkTavla firmware"})
        return self.send(404, {"error": "unknown route"})


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--port", type=int, default=8080)
    args = parser.parse_args()
    print("Settings page: http://localhost:%d/?code=%s" % (args.port, CODE))
    ThreadingHTTPServer(("127.0.0.1", args.port), Handler).serve_forever()


if __name__ == "__main__":
    main()
