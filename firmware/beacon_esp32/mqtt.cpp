// MQTT link to the server (Mosquitto on a Raspberry Pi or a VPS) plus an in-RAM buffer
// that holds reports while WiFi or the server is down and replays them in order afterwards.
#include <PubSubClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "beacon.h"

static WiFiClient plain_client;
static WiFiClientSecure tls_client;
static PubSubClient mqtt;
static uint32_t last_attempt_ms = 0, last_status_ms = 0;
static bool announced = false;

struct Record {
  uint8_t raw[CTV1_UPLINK_LEN];
  int16_t rssi;
  int8_t snr;
  uint32_t rx_ms;
  int64_t ts;  // 0 if the time was not known yet when it arrived
};
static Record *buf = nullptr;
static uint32_t buf_cap = 0, buf_head = 0, buf_count = 0, dropped = 0;

int64_t now_unix() {
  time_t t = time(nullptr);
  return t > 1700000000 ? (int64_t)t : 0;
}

static String topic(const char *leaf) { return "ctv1/gw/" + settings.gw_id + "/" + leaf; }

bool mqtt_connected() { return mqtt.connected(); }
uint32_t buffer_count() { return buf_count; }
uint32_t buffer_dropped() { return dropped; }

void mqtt_publish_event(const char *json) {
  if (mqtt.connected()) mqtt.publish(topic("event").c_str(), json);
  Serial.printf("event: %s\n", json);
}

static bool publish_record(const Record &r) {
  Ctv1Uplink up;
  if (!ctv1_decode_uplink(r.raw, CTV1_UPLINK_LEN, up)) return true;  // never happens; drop it
  int64_t ts = r.ts;
  if (!ts && now_unix()) ts = now_unix() - (int64_t)((millis() - r.rx_ms) / 1000);
  char js[400];
  if (!ctv1_json_uplink(js, sizeof(js), settings.gw_id.c_str(), up, r.rssi, r.snr, ts, r.raw, CTV1_UPLINK_LEN)) return true;
  return mqtt.publish(topic("up").c_str(), js);
}

void mqtt_queue_uplink(const uint8_t *raw, size_t len, int16_t rssi, int8_t snr) {
  if (len != CTV1_UPLINK_LEN) return;
  Record r;
  memcpy(r.raw, raw, len);
  r.rssi = rssi;
  r.snr = snr;
  r.rx_ms = millis();
  r.ts = now_unix();
  if (mqtt.connected() && buf_count == 0 && publish_record(r)) return;
  if (!buf_cap) return;
  if (buf_count == buf_cap) {  // full: drop the oldest
    buf_head = (buf_head + 1) % buf_cap;
    buf_count--;
    dropped++;
  }
  buf[(buf_head + buf_count) % buf_cap] = r;
  buf_count++;
}

static void on_message(char *t, byte *payload, unsigned int len) {
  static char msg[2048];
  if (len >= sizeof(msg)) return;
  memcpy(msg, payload, len);
  msg[len] = 0;
  String tp(t);
  char ev[96];
  if (tp.endsWith("/down")) {
    Ctv1GwCommand c;
    if (ctv1_json_parse_down(msg, c)) {
      radio_send_command(c.tag_id, c.cmd, c.arg);
    } else {
      mqtt_publish_event("{\"type\":\"bad_command\"}");
    }
  } else if (tp.endsWith("/fence")) {
    bool ok = fence_set_from_json(msg, true);
    snprintf(ev, sizeof(ev), "{\"type\":\"%s\",\"corners\":%u}", ok ? "fence_updated" : "fence_invalid", ok ? fence_n : 0);
    mqtt_publish_event(ev);
  }
}

static void publish_status() {
  char js[512];
  uint8_t heard = 0;
  for (auto &h : herd) heard += h.used;
  snprintf(js, sizeof(js),
           "{\"fw\":\"%s\",\"uptime_s\":%lu,\"wifi_rssi\":%d,\"ip\":\"%s\",\"mains\":%s,\"radio_online\":%s,"
           "\"radio_reports\":%lu,\"radio_rejected\":%lu,\"radio_resets\":%lu,\"commands_queued\":%lu,"
           "\"buffered\":%lu,\"dropped\":%lu,\"tags_heard\":%u,\"fence_corners\":%u,\"alarm\":%s,\"ts\":%lld}",
           FW_VERSION, millis() / 1000, WiFi.RSSI(), WiFi.localIP().toString().c_str(), mains_present() ? "true" : "false",
           radio_stats.online ? "true" : "false", (unsigned long)radio_stats.up, (unsigned long)radio_stats.bad,
           (unsigned long)radio_stats.resets, (unsigned long)radio_stats.radio_queued, (unsigned long)buf_count,
           (unsigned long)dropped, heard, fence_n, alarm_active() ? "true" : "false", (long long)now_unix());
  mqtt.publish(topic("status").c_str(), js, true);
}

void mqtt_begin() {
  buf_cap = BUFFER_RECORDS;
  buf = (Record *)(psramFound() ? ps_malloc(sizeof(Record) * buf_cap) : malloc(sizeof(Record) * buf_cap));
  if (!buf) {
    buf_cap = 200;
    buf = (Record *)malloc(sizeof(Record) * buf_cap);
    if (!buf) buf_cap = 0;
  }
  configTime(0, 0, "pool.ntp.org", "time.google.com");
  if (settings.mqtt_tls) {
    if (settings.mqtt_ca.length()) tls_client.setCACert(settings.mqtt_ca.c_str());
    else tls_client.setInsecure();  // encrypted, but the server's identity is not checked
    mqtt.setClient(tls_client);
  } else {
    mqtt.setClient(plain_client);
  }
  mqtt.setServer(settings.mqtt_host.c_str(), settings.mqtt_port);
  mqtt.setBufferSize(2048);
  mqtt.setKeepAlive(30);
  mqtt.setCallback(on_message);
}

void mqtt_poll() {
  if (!settings.mqtt_host.length() || WiFi.status() != WL_CONNECTED) return;
  if (!mqtt.connected()) {
    if (millis() - last_attempt_ms < 5000 && last_attempt_ms) return;
    last_attempt_ms = millis();
    String id = String("ctv1-") + settings.gw_id + "-" + String((uint32_t)ESP.getEfuseMac(), HEX);
    String will = topic("online");
    bool ok = settings.mqtt_user.length()
                  ? mqtt.connect(id.c_str(), settings.mqtt_user.c_str(), settings.mqtt_pass.c_str(), will.c_str(), 1, true, "false")
                  : mqtt.connect(id.c_str(), will.c_str(), 1, true, "false");
    if (!ok) {
      Serial.printf("MQTT connect to %s:%u failed, state %d\n", settings.mqtt_host.c_str(), settings.mqtt_port, mqtt.state());
      return;
    }
    Serial.println("MQTT connected");
    mqtt.publish(will.c_str(), "true", true);
    mqtt.subscribe(topic("down").c_str(), 1);
    mqtt.subscribe(topic("fence").c_str(), 1);  // retained, so the beacon gets the fence on every connect
    if (!announced) {
      announced = true;
      mqtt_publish_event("{\"type\":\"beacon_started\",\"fw\":\"" FW_VERSION "\"}");
    }
    publish_status();
    last_status_ms = millis();
  }
  mqtt.loop();

  for (int i = 0; i < 20 && buf_count && mqtt.connected(); i++) {  // replay a few per loop
    if (!publish_record(buf[buf_head])) break;
    buf_head = (buf_head + 1) % buf_cap;
    buf_count--;
  }
  if (millis() - last_status_ms >= STATUS_EVERY_MS) {
    last_status_ms = millis();
    publish_status();
  }
}
