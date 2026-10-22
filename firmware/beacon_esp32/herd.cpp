// Last report per tag, the beacon's own fence check, and the alarm outputs (LED + siren).
// The fence check here is a backup: it still works when the internet or the server is down.
#include <WiFi.h>
#include "beacon.h"

TagState herd[MAX_HERD];
Ctv1Point fence_pts[MAX_FENCE_POINTS];
uint8_t fence_n = 0;
String fence_name;

static bool alarm_on = false;
static String alarm_why;
static uint32_t siren_until = 0, rx_led_until = 0;

static TagState *herd_find(uint16_t id, bool create) {
  TagState *free_slot = nullptr;
  for (auto &t : herd) {
    if (t.used && t.last.tag_id == id) return &t;
    if (!t.used && !free_slot) free_slot = &t;
  }
  if (!create || !free_slot) return nullptr;
  *free_slot = TagState();
  free_slot->used = true;
  free_slot->last.tag_id = id;
  return free_slot;
}

void herd_handle_uplink(const uint8_t *raw, size_t len, int16_t rssi, int8_t snr) {
  Ctv1Uplink up;
  if (!ctv1_decode_uplink(raw, len, up)) return;
  digitalWrite(PIN_LED_RX, HIGH);
  rx_led_until = millis() + 80;

  TagState *t = herd_find(up.tag_id, true);
  if (t) {
    t->last = up;
    t->rssi = rssi;
    t->snr = snr;
    t->seen_ms = millis();
    t->reports++;
    if (fence_n >= 3 && (up.flags & CTV1_FLAG_FIX_OK)) {
      Ctv1Point p = {up.lat_e7, up.lon_e7};
      bool inside = ctv1_point_in_polygon(p, fence_pts, fence_n);
      uint32_t edge = ctv1_distance_to_edge_m(p, fence_pts, fence_n);
      Ctv1FenceEvent ev = t->fence.update(inside, edge);
      if (ev != CTV1_FENCE_NONE) {
        char js[160];
        snprintf(js, sizeof(js), "{\"type\":\"%s\",\"tag\":%u,\"fence\":\"%s\",\"distance_m\":%lu,\"source\":\"beacon\"}",
                 ev == CTV1_FENCE_LEFT ? "left_fence" : "returned", up.tag_id, fence_name.c_str(), (unsigned long)edge);
        mqtt_publish_event(js);
        if (ev == CTV1_FENCE_LEFT) alarm_trigger("Tag " + String(up.tag_id) + " left " + fence_name);
      }
    }
  }
  mqtt_queue_uplink(raw, len, rssi, snr);
  Serial.printf("tag %u seq %u rssi %d snr %d\n", up.tag_id, up.seq, rssi, snr);
}

bool fence_set_from_json(const char *json, bool save) {
  Ctv1Point pts[MAX_FENCE_POINTS];
  uint8_t n = ctv1_json_parse_fence(json, pts, MAX_FENCE_POINTS);
  if (!n) return false;
  memcpy(fence_pts, pts, sizeof(Ctv1Point) * n);
  fence_n = n;
  fence_name = "the fence";
  const char *k = strstr(json, "\"name\"");
  if (k && (k = strchr(k + 6, '"'))) {
    const char *e = strchr(k + 1, '"');
    if (e && e - k - 1 < 40) fence_name = String(k + 1).substring(0, e - k - 1);
  }
  for (auto &t : herd) t.fence = Ctv1FenceState();  // new outline: start everyone as inside
  if (save) settings_save_fence(json);
  Serial.printf("fence '%s' with %u corners\n", fence_name.c_str(), n);
  return true;
}

void fence_load() {
  String js = settings_load_fence();
  if (js.length()) fence_set_from_json(js.c_str(), false);
}

void alarm_trigger(const String &why) {
  alarm_on = true;
  alarm_why = why;
  if (settings.siren) siren_until = millis() + SIREN_SECONDS * 1000UL;
  Serial.printf("ALARM: %s\n", why.c_str());
}

void alarm_clear() {
  alarm_on = false;
  siren_until = 0;
  alarm_why = "";
}

bool alarm_active() { return alarm_on; }
const String &alarm_reason() { return alarm_why; }

void alarm_poll() {
  uint32_t now = millis();
  digitalWrite(PIN_SIREN, (int32_t)(siren_until - now) > 0 ? HIGH : LOW);
  if ((int32_t)(rx_led_until - now) <= 0) digitalWrite(PIN_LED_RX, LOW);

  static uint32_t pressed_at = 0;  // short press on BOOT silences the alarm
  if (digitalRead(PIN_BOOT_BTN) == LOW) {
    if (!pressed_at) pressed_at = now;
  } else if (pressed_at) {
    if (now - pressed_at > 50) alarm_clear();
    pressed_at = 0;
  }

  bool trouble = WiFi.status() != WL_CONNECTED || (settings.mqtt_host.length() && !mqtt_connected()) || !radio_stats.online;
  digitalWrite(PIN_LED_ALARM, alarm_on ? HIGH : (trouble ? ((now / 1000) % 2) : LOW));
}
