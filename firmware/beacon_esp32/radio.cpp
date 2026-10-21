// Line protocol with the RAK3172 running firmware/gateway_radio (LINK_ON_USB 0).
#include "beacon.h"

RadioStats radio_stats;
static char line[160];
static size_t line_len = 0;

void radio_begin() {
  pinMode(PIN_RADIO_RESET, OUTPUT_OPEN_DRAIN);
  digitalWrite(PIN_RADIO_RESET, HIGH);
  Serial1.begin(RADIO_BAUD, SERIAL_8N1, PIN_RADIO_RX, PIN_RADIO_TX);
  radio_stats.last_line_ms = millis();
}

static void radio_reset() {
  Serial.println("radio silent, resetting RAK3172");
  digitalWrite(PIN_RADIO_RESET, LOW);
  delay(100);
  digitalWrite(PIN_RADIO_RESET, HIGH);
  radio_stats.resets++;
  radio_stats.online = false;
  radio_stats.last_line_ms = millis();
  mqtt_publish_event("{\"type\":\"radio_reset\"}");
}

void radio_send_command(uint16_t tag, uint8_t cmd, uint8_t arg) {
  Serial1.printf("CMD,%u,%u,%u\n", tag, cmd, arg);
  Serial.printf("-> CMD,%u,%u,%u\n", tag, cmd, arg);
}

static void handle_line(char *s) {
  radio_stats.last_line_ms = millis();
  radio_stats.online = true;
  char ev[128];

  if (strncmp(s, "UP,", 3) == 0) {
    int rssi, snr, off = 0;
    uint8_t raw[32];
    if (sscanf(s, "UP,%d,%d,%n", &rssi, &snr, &off) == 2 && off > 0) {
      size_t n = ctv1_hex_decode(s + off, raw, sizeof(raw));
      if (n) {
        radio_stats.up++;
        herd_handle_uplink(raw, n, (int16_t)rssi, (int8_t)snr);
        return;
      }
    }
    Serial.printf("malformed: %s\n", s);
  } else if (strncmp(s, "DUP,", 4) == 0) {
    radio_stats.dup++;
  } else if (strncmp(s, "BAD,", 4) == 0) {
    radio_stats.bad++;  // packet with the wrong farm key: a neighbour's LoRa device, or a tag set up wrongly
  } else if (strncmp(s, "ST,", 3) == 0) {
    unsigned long up, rx, bad, q;
    if (sscanf(s, "ST,%lu,%lu,%lu,%lu", &up, &rx, &bad, &q) == 4) {
      radio_stats.radio_uptime_s = up;
      radio_stats.radio_rx = rx;
      radio_stats.radio_bad = bad;
      radio_stats.radio_queued = q;
    }
  } else if (strncmp(s, "OK,done,", 8) == 0 || strncmp(s, "OK,queued,", 10) == 0) {
    unsigned tag, cmd;
    bool done = s[3] == 'd';
    if (sscanf(s + (done ? 8 : 10), "%u,%u", &tag, &cmd) == 2) {
      snprintf(ev, sizeof(ev), "{\"type\":\"%s\",\"tag\":%u,\"cmd\":%u}", done ? "cmd_done" : "cmd_queued", tag, cmd);
      mqtt_publish_event(ev);
    }
  } else if (strncmp(s, "ERR,", 4) == 0) {
    snprintf(ev, sizeof(ev), "{\"type\":\"radio_error\",\"detail\":\"%.40s\"}", s + 4);
    mqtt_publish_event(ev);
  }
  Serial.printf("radio: %s\n", s);
}

void radio_poll() {
  while (Serial1.available()) {
    char c = (char)Serial1.read();
    if (c == '\r') continue;
    if (c == '\n') {
      line[line_len] = 0;
      if (line_len) handle_line(line);
      line_len = 0;
    } else if (line_len < sizeof(line) - 1) {
      line[line_len++] = c;
    }
  }
  if (millis() - radio_stats.last_line_ms > RADIO_SILENT_RESET_MS) radio_reset();
}
