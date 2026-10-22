// CTv1 home beacon, ESP32-S3 side: WiFi + MQTT bridge for the RAK3172 radio, local web page,
// backup fence alarm, mains-power monitor. Board: ESP32S3 Dev Module (Arduino-ESP32 core 3.x).
// Needs the PubSubClient library and firmware/lib/ctv1. See firmware/README.md.
#include <ESPmDNS.h>
#include <WiFi.h>
#include "beacon.h"

static bool mdns_started = false;
static bool mains = true;

bool mains_present() { return mains; }

static void power_poll() {
  static uint32_t last = 0;
  static uint8_t streak = 0;
  if (millis() - last < 1000) return;
  last = millis();
  bool now_mains = analogReadMilliVolts(PIN_VBUS_SENSE) > VBUS_PRESENT_MV;
  streak = now_mains != mains ? streak + 1 : 0;
  if (streak >= 3) {  // same reading for 3 s
    mains = now_mains;
    streak = 0;
    mqtt_publish_event(mains ? "{\"type\":\"power_restored\"}" : "{\"type\":\"power_lost\"}");
  }
}

static void wifi_poll() {
  static uint32_t lost_since = 0;
  if (WiFi.status() == WL_CONNECTED) {
    lost_since = 0;
    if (!mdns_started && MDNS.begin(HOSTNAME)) {
      MDNS.addService("http", "tcp", 80);
      mdns_started = true;
      Serial.printf("WiFi up: http://%s.local  (%s)\n", HOSTNAME, WiFi.localIP().toString().c_str());
    }
    return;
  }
  if (!lost_since) lost_since = millis();
  if (!portal_active() && millis() - lost_since > PORTAL_AFTER_MS) portal_start();  // let someone fix the settings
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_LED_RX, OUTPUT);
  pinMode(PIN_LED_ALARM, OUTPUT);
  pinMode(PIN_SIREN, OUTPUT);
  digitalWrite(PIN_SIREN, LOW);
  pinMode(PIN_BOOT_BTN, INPUT_PULLUP);
  mains = analogReadMilliVolts(PIN_VBUS_SENSE) > VBUS_PRESENT_MV;

  settings_load();
  fence_load();
  radio_begin();
  Serial.printf("\nCTv1 home beacon %s, name '%s'\n", FW_VERSION, settings.gw_id.c_str());

  WiFi.mode(WIFI_STA);
  WiFi.setHostname(HOSTNAME);
  bool force_setup = digitalRead(PIN_BOOT_BTN) == LOW;  // BOOT held at power-up
  if (settings.wifi_ssid.length()) WiFi.begin(settings.wifi_ssid.c_str(), settings.wifi_pass.c_str());
  if (!settings.wifi_ssid.length() || force_setup) portal_start();

  mqtt_begin();
  web_begin();
}

void loop() {
  radio_poll();
  wifi_poll();
  mqtt_poll();
  web_poll();
  alarm_poll();
  power_poll();
  delay(2);
}
