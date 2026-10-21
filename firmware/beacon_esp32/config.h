#pragma once
// Home beacon, ESP32-S3 side. Wiring: docs/03-hardware-home-beacon.md

// ---- Pins ----
#define PIN_RADIO_RX 18      // ESP32 RX  <- RAK3172 PB6 (UART1 TX)
#define PIN_RADIO_TX 17      // ESP32 TX  -> RAK3172 PB7 (UART1 RX)
#define PIN_RADIO_RESET 16   // RAK3172 NRST, active low
#define PIN_VBUS_SENSE 1     // 100 k / 100 k divider from the 5 V adapter
#define PIN_LED_RX 4         // green: blinks for every tag report
#define PIN_LED_ALARM 5      // red: on = alarm, slow blink = no WiFi / server
#define PIN_SIREN 6          // MOSFET gate for the optional siren / relay
#define PIN_BOOT_BTN 0       // hold at power-up: setup hotspot. Press: silence alarm

// ---- Behaviour ----
#define RADIO_BAUD 115200
#define RADIO_SILENT_RESET_MS 180000  // reset the RAK3172 if it says nothing for 3 min (it sends status every 60 s)
#define STATUS_EVERY_MS 60000
#define BUFFER_RECORDS 2000           // reports kept while the server is unreachable
#define MAX_HERD 64
#define MAX_FENCE_POINTS 32
#define SIREN_SECONDS 30
#define PORTAL_AFTER_MS 60000         // open the setup hotspot if WiFi is not up after 1 min
#define PORTAL_CLOSE_AFTER_MS 120000  // close it again 2 min after WiFi connects
#define VBUS_PRESENT_MV 1800          // 5 V through the divider reads ~2500 mV

// ---- Names ----
#define HOSTNAME "ctv1-beacon"        // http://ctv1-beacon.local
#define AP_NAME_PREFIX "CTv1-Beacon-"
#define AP_PASSWORD "ctv1setup"       // printed on the box label; at least 8 characters
#define FW_VERSION "0.1.0"
