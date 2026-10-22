#pragma once
// Shared declarations for the home beacon's ESP32 modules.
#include <Arduino.h>
#include <CTv1.h>
#include "config.h"

// settings.cpp: stored in NVS flash, edited on the web settings page
struct Settings {
  String wifi_ssid, wifi_pass;
  String mqtt_host, mqtt_user, mqtt_pass, mqtt_ca;  // empty CA with TLS on = encrypted but not verified
  uint16_t mqtt_port;
  bool mqtt_tls;
  String gw_id;       // MQTT topic part: ctv1/gw/<gw_id>/...
  String admin_pass;  // web page password (user "admin"); empty = no password
  bool siren;         // sound the siren when a tag leaves the fence
};
extern Settings settings;
void settings_load();
void settings_save();
String settings_load_fence();
void settings_save_fence(const String &json);

// radio.cpp: text link to the RAK3172 (see lib/ctv1/src/ctv1_gwlink.h)
struct RadioStats {
  bool online;
  uint32_t last_line_ms, up, dup, bad, resets;
  uint32_t radio_uptime_s, radio_rx, radio_bad, radio_queued;
};
extern RadioStats radio_stats;
void radio_begin();
void radio_poll();
void radio_send_command(uint16_t tag, uint8_t cmd, uint8_t arg);

// herd.cpp: last report per tag, local fence check, alarm outputs
struct TagState {
  bool used;
  Ctv1Uplink last;
  int16_t rssi;
  int8_t snr;
  uint32_t seen_ms;
  uint32_t reports;
  Ctv1FenceState fence;
};
extern TagState herd[MAX_HERD];
extern Ctv1Point fence_pts[MAX_FENCE_POINTS];
extern uint8_t fence_n;
extern String fence_name;
void herd_handle_uplink(const uint8_t *raw, size_t len, int16_t rssi, int8_t snr);
bool fence_set_from_json(const char *json, bool save);
void fence_load();
void alarm_trigger(const String &why);
void alarm_clear();
void alarm_poll();
bool alarm_active();
const String &alarm_reason();

// mqtt.cpp: publish reports/status/events, receive commands and fences, offline buffer
void mqtt_begin();
void mqtt_poll();
bool mqtt_connected();
void mqtt_queue_uplink(const uint8_t *raw, size_t len, int16_t rssi, int8_t snr);
void mqtt_publish_event(const char *json);
uint32_t buffer_count();
uint32_t buffer_dropped();
int64_t now_unix();  // 0 until NTP time is known

// web.cpp: status page, commands, settings, setup hotspot
void web_begin();
void web_poll();
void portal_start();
bool portal_active();

// beacon_esp32.ino
bool mains_present();
String html_escape(const String &s);
