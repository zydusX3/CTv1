#pragma once
// ---- Per-tag settings: change TAG_ID for every collar you flash ----
#define TAG_ID 1

// Farm key: 16 random bytes, the SAME on every tag and on the home beacon. Keep it private.
// Make one with:  python3 -c "import os; print(', '.join('0x%02x' % b for b in os.urandom(16)))"
#define FARM_KEY {0x43, 0x54, 0x76, 0x31, 0x2d, 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x2d, 0x6d, 0x65, 0x21, 0x21}

// ---- Radio: must match the home beacon. Pick a frequency legal in your country ----
#define LORA_FREQ_HZ 865062500  // IN865 example. EU868: 868100000, US915: 903900000, AU915: 916800000
#define LORA_SF 9
#define LORA_BW_KHZ 125
#define LORA_CR 0               // 0 = 4/5
#define LORA_PREAMBLE 8
#define LORA_TX_DBM 14          // raise to 20-22 only where your region allows it
#define RX_WINDOW_MS 1500       // how long the tag listens for the ACK after sending

// ---- Reporting ----
#define REPORT_INTERVAL_S 600     // normal: every 10 min
#define REPORT_JITTER_S 30        // random 0..30 s added so tags never stay in step
#define FIRST_REPORT_S 10         // first report after power-up
#define RETRY_MIN_MS 5000         // one retry if no ACK, after 5..15 s
#define RETRY_MAX_MS 15000

// ---- GPS (u-blox MAX-M10S, NMEA at 9600 baud) ----
#define GPS_BAUD 9600
#define GPS_FIRST_FIX_TIMEOUT_S 180  // cold start, almanac not yet stored
#define GPS_FIX_TIMEOUT_S 45         // hot start normally takes 2-5 s
#define GPS_GOOD_HDOP_X10 25         // stop as soon as HDOP <= 2.5

// ---- Accelerometer (LIS3DH) ----
#define ACCEL_I2C_ADDR 0x18
#define ACCEL_THRESHOLD 8       // x 16 mg = 128 mg above the filtered baseline
#define MOVING_THRESHOLD 30     // motion events per report that count as "walking"

// ---- Battery divider (1 M / 1 M) ----
#define BATT_DIVIDER 2.0f
#define BATT_CALIBRATION 1.00f  // measure the cell with a multimeter and adjust

// ---- Pins (see docs/02-hardware-collar-tag.md) ----
#define PIN_GPS_EN PA8
#define PIN_ACC_INT PA0
#define PIN_BATT PB3
#define PIN_LED PA1
#define PIN_BUZZER PB5

// ---- Optional on-tag fence ----
// With the paddock corners here the tag notices by itself when it is outside and switches to
// 2-minute reports straight away. Leave FENCE_ENABLED at 0 to let the server do all fence checks.
#define FENCE_ENABLED 0
#define FENCE_POINTS {{108516170, 762663420}, {108517970, 762708240}, {108501800, 762715560}, \
                      {108484730, 762711900}, {108482930, 762669840}, {108497300, 762660690}}

#define DEBUG_PRINT 1  // log to UART2 (the programming port) at 115200
