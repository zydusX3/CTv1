// Local web pages: herd status with Buzz buttons, settings, and the first-time setup hotspot.
// Reach it at http://ctv1-beacon.local, or http://192.168.4.1 when joined to the CTv1-Beacon-XXXX hotspot.
#include <DNSServer.h>
#include <WebServer.h>
#include <WiFi.h>
#include "beacon.h"

static WebServer server(80);
static DNSServer dns;
static bool portal = false;

bool portal_active() { return portal; }

static const char CSS[] PROGMEM =
    "<meta name=viewport content='width=device-width,initial-scale=1'><style>"
    "body{font-family:system-ui,sans-serif;margin:0 auto;padding:16px;max-width:760px;background:#f4f6ef;color:#1b2418}"
    "h1{font-size:24px;margin:4px 0 12px}h2{font-size:17px;margin:20px 0 8px}"
    ".card{background:#fff;border:1px solid #d6dccb;border-radius:10px;padding:12px;margin:8px 0}"
    ".alarm{background:#c8322b;color:#fff;border:0}.ok{color:#2e8540}.warn{color:#b86e00}"
    "table{border-collapse:collapse;width:100%}td,th{padding:6px 4px;border-bottom:1px solid #d6dccb;text-align:left;font-size:14px}"
    "button,input[type=submit]{font:inherit;padding:8px 14px;border-radius:8px;border:1px solid #2f6b3a;background:#2f6b3a;color:#fff;cursor:pointer}"
    "label{display:block;margin:10px 0 4px;font-weight:600}input,textarea{font:inherit;width:100%;box-sizing:border-box;padding:8px;border:1px solid #d6dccb;border-radius:6px}"
    "input[type=checkbox]{width:auto}.wrap{overflow-x:auto}a{color:#2f6b3a}small{color:#5d6a58}"
    "</style>";

String html_escape(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (char c : s) {
    if (c == '<') o += "&lt;";
    else if (c == '>') o += "&gt;";
    else if (c == '&') o += "&amp;";
    else if (c == '"') o += "&quot;";
    else if (c == '\'') o += "&#39;";
    else o += c;
  }
  return o;
}

static bool auth_ok() {
  if (portal || !settings.admin_pass.length()) return true;
  if (server.authenticate("admin", settings.admin_pass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "CTv1 beacon");
  return false;
}

static String ago(uint32_t ms) {
  uint32_t s = (millis() - ms) / 1000;
  if (s < 60) return String(s) + " s ago";
  if (s < 3600) return String(s / 60) + " min ago";
  return String(s / 3600) + " h " + String((s / 60) % 60) + " min ago";
}

static void page_status() {
  if (!auth_ok()) return;
  String h;
  h.reserve(6000);
  h += "<!doctype html><title>CTv1 beacon</title>";
  h += FPSTR(CSS);
  h += "<meta http-equiv=refresh content=30><h1>CTv1 home beacon</h1>";
  if (alarm_active()) {
    h += "<div class='card alarm'><b>" + html_escape(alarm_reason()) +
         "</b><form method=post action=/silence style='margin-top:8px'><button>Silence alarm</button></form></div>";
  }
  h += "<div class=card>";
  h += String("WiFi: ") + (WiFi.status() == WL_CONNECTED ? "<span class=ok>connected</span> (" + String(WiFi.RSSI()) + " dBm)" : "<span class=warn>not connected</span>");
  h += String(" · Server: ") + (!settings.mqtt_host.length() ? "<span class=warn>not set</span>" : mqtt_connected() ? "<span class=ok>connected</span>" : "<span class=warn>not connected</span>");
  h += String(" · Radio: ") + (radio_stats.online ? "<span class=ok>OK</span>" : "<span class=warn>no reply</span>");
  h += String(" · Power: ") + (mains_present() ? "mains" : "<span class=warn>battery backup</span>");
  if (buffer_count()) h += "<br>" + String(buffer_count()) + " reports waiting for the server";
  h += "<br><small>" + String(radio_stats.up) + " reports received, " + String(radio_stats.bad) +
       " rejected (wrong key). Fence: " + (fence_n ? html_escape(fence_name) + ", " + String(fence_n) + " corners" : String("none")) +
       ".</small></div>";

  h += "<h2>Animals</h2><div class='card wrap'><table><tr><th>Tag</th><th>Last report</th><th>Position</th><th>Battery</th><th>Signal</th><th></th></tr>";
  int shown = 0;
  for (auto &t : herd) {
    if (!t.used || !t.reports) continue;
    shown++;
    char lat[16], lon[16];
    ctv1_format_e7(lat, sizeof(lat), t.last.lat_e7);
    ctv1_format_e7(lon, sizeof(lon), t.last.lon_e7);
    h += "<tr><td><b>#" + String(t.last.tag_id) + "</b>";
    if (!t.fence.inside) h += " <span class=warn>outside</span>";
    h += "</td><td>" + ago(t.seen_ms) + "</td><td>";
    if (t.last.lat_e7 || t.last.lon_e7)
      h += String("<a target=_blank href='https://www.openstreetmap.org/?mlat=") + lat + "&mlon=" + lon + "#map=18/" + lat + "/" + lon + "'>" + lat + ", " + lon + "</a>";
    else
      h += "no fix yet";
    if (!(t.last.flags & CTV1_FLAG_FIX_OK)) h += " <small>(old)</small>";
    h += "</td><td>" + String(ctv1_batt_percent(t.last.batt_mv)) + "%</td><td>" + String(t.rssi) + " dBm</td>";
    h += "<td><form method=post action=/cmd><input type=hidden name=tag value=" + String(t.last.tag_id) +
         "><input type=hidden name=cmd value=1><input type=hidden name=arg value=30><button>Buzz</button></form></td></tr>";
  }
  if (!shown) h += "<tr><td colspan=6>No tag has reported yet. Tags report within a minute of being switched on.</td></tr>";
  h += "</table><small>A buzz goes out right after the tag's next report (up to 10 min).</small></div>";
  h += "<p><a href=/settings>Settings</a> · <a href=/api/herd>Data (JSON)</a> · firmware " FW_VERSION "</p>";
  server.send(200, "text/html", h);
}

static void page_settings() {
  if (!auth_ok()) return;
  int n = WiFi.scanNetworks();
  String h;
  h.reserve(5000);
  h += "<!doctype html><title>CTv1 beacon settings</title>";
  h += FPSTR(CSS);
  h += "<h1>Beacon settings</h1><form method=post action=/settings><div class=card>";
  h += "<label for=ssid>WiFi network</label><input id=ssid name=ssid list=nets value='" + html_escape(settings.wifi_ssid) + "'><datalist id=nets>";
  for (int i = 0; i < n; i++) h += "<option value='" + html_escape(WiFi.SSID(i)) + "'>";
  h += "</datalist><label for=wpass>WiFi password</label><input id=wpass name=wpass type=password placeholder='unchanged if empty'>";
  h += "</div><div class=card>";
  h += "<label for=mhost>Server (MQTT broker) address</label><input id=mhost name=mhost placeholder='192.168.1.20 or mqtt.example.com' value='" + html_escape(settings.mqtt_host) + "'>";
  h += "<label for=mport>Port</label><input id=mport name=mport type=number value=" + String(settings.mqtt_port) + ">";
  h += "<label for=muser>MQTT user</label><input id=muser name=muser value='" + html_escape(settings.mqtt_user) + "'>";
  h += "<label for=mpass>MQTT password</label><input id=mpass name=mpass type=password placeholder='unchanged if empty'>";
  h += String("<label><input type=checkbox name=mtls") + (settings.mqtt_tls ? " checked" : "") + "> Use TLS (port 8883)</label>";
  h += "<label for=mca>Server CA certificate (PEM, optional)</label><textarea id=mca name=mca rows=4>" + html_escape(settings.mqtt_ca) + "</textarea>";
  h += "<label for=gw>Beacon name (MQTT topic ctv1/gw/&lt;name&gt;)</label><input id=gw name=gw value='" + html_escape(settings.gw_id) + "'>";
  h += "</div><div class=card>";
  h += String("<label><input type=checkbox name=siren") + (settings.siren ? " checked" : "") + "> Sound the siren when an animal leaves the fence</label>";
  h += "<label for=admin>Password for these pages (user: admin)</label><input id=admin name=admin type=password placeholder='unchanged if empty'>";
  h += "</div><input type=submit value='Save and restart'></form><p><a href=/>Back</a></p>";
  server.send(200, "text/html", h);
}

static void save_settings() {
  if (!auth_ok()) return;
  settings.wifi_ssid = server.arg("ssid");
  if (server.arg("wpass").length()) settings.wifi_pass = server.arg("wpass");
  settings.mqtt_host = server.arg("mhost");
  settings.mqtt_host.trim();
  long port = server.arg("mport").toInt();
  settings.mqtt_port = (port > 0 && port < 65536) ? (uint16_t)port : 1883;
  settings.mqtt_user = server.arg("muser");
  if (server.arg("mpass").length()) settings.mqtt_pass = server.arg("mpass");
  settings.mqtt_tls = server.hasArg("mtls");
  settings.mqtt_ca = server.arg("mca");
  String gw = server.arg("gw");
  gw.trim();
  bool gw_ok = gw.length() > 0 && gw.length() <= 24;
  for (char c : gw) gw_ok = gw_ok && (isalnum((unsigned char)c) || c == '-' || c == '_');
  if (gw_ok) settings.gw_id = gw;
  settings.siren = server.hasArg("siren");
  if (server.arg("admin").length()) settings.admin_pass = server.arg("admin");
  settings_save();
  String h = "<!doctype html><title>Saved</title>";
  h += FPSTR(CSS);
  h += "<h1>Saved</h1><div class=card>The beacon is restarting and will join <b>" + html_escape(settings.wifi_ssid) +
       "</b>. Reconnect your phone to that network and open <b>http://" HOSTNAME ".local</b>.";
  if (!gw_ok) h += "<br>The beacon name was not changed: use letters, digits, - or _ only.";
  h += "</div>";
  server.send(200, "text/html", h);
  delay(1500);
  ESP.restart();
}

static void post_cmd() {
  if (!auth_ok()) return;
  long tag = server.arg("tag").toInt(), cmd = server.arg("cmd").toInt(), arg = server.arg("arg").toInt();
  if (tag > 0 && tag < 65536 && cmd >= 0 && cmd <= 3 && arg >= 0 && arg <= 255) radio_send_command(tag, cmd, arg);
  server.sendHeader("Location", "/");
  server.send(303);
}

static void api_herd() {
  if (!auth_ok()) return;
  String js = "[";
  char one[400];
  for (auto &t : herd) {
    if (!t.used || !t.reports) continue;
    int64_t ts = now_unix() ? now_unix() - (int64_t)((millis() - t.seen_ms) / 1000) : 0;
    if (ctv1_json_uplink(one, sizeof(one), settings.gw_id.c_str(), t.last, t.rssi, t.snr, ts, nullptr, 0)) {
      if (js.length() > 1) js += ",";
      js += one;
    }
  }
  js += "]";
  server.send(200, "application/json", js);
}

void portal_start() {
  if (portal) return;
  portal = true;
  String ap = String(AP_NAME_PREFIX) + String((uint32_t)ESP.getEfuseMac() & 0xffff, HEX);
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(ap.c_str(), AP_PASSWORD);
  dns.start(53, "*", WiFi.softAPIP());
  Serial.printf("setup hotspot '%s' (password %s), open http://%s\n", ap.c_str(), AP_PASSWORD, WiFi.softAPIP().toString().c_str());
}

static void portal_stop() {
  dns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  portal = false;
  Serial.println("setup hotspot closed");
}

void web_begin() {
  server.on("/", HTTP_GET, page_status);
  server.on("/settings", HTTP_GET, page_settings);
  server.on("/settings", HTTP_POST, save_settings);
  server.on("/cmd", HTTP_POST, post_cmd);
  server.on("/silence", HTTP_POST, []() {
    if (!auth_ok()) return;
    alarm_clear();
    server.sendHeader("Location", "/");
    server.send(303);
  });
  server.on("/api/herd", HTTP_GET, api_herd);
  server.onNotFound([]() {
    if (portal) {  // captive portal: phones open this automatically
      server.sendHeader("Location", String("http://") + WiFi.softAPIP().toString() + "/settings");
      server.send(302);
    } else {
      server.send(404, "text/plain", "Not found");
    }
  });
  server.begin();
}

void web_poll() {
  if (portal) {
    dns.processNextRequest();
    static uint32_t connected_since = 0;
    if (WiFi.status() == WL_CONNECTED && settings.wifi_ssid.length()) {
      if (!connected_since) connected_since = millis();
      if (millis() - connected_since > PORTAL_CLOSE_AFTER_MS) portal_stop();
    } else {
      connected_since = 0;
    }
  }
  server.handleClient();
}
