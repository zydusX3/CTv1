#pragma once
// Handheld beacon (docs/04-hardware-handheld.md). Farm key and radio must match the tags; these are
// defaults, and ATC+CTKEY / ATC+CTFREQ / ATC+CTSF / ATC+CTPWR on the programming port override them.
#define DEFAULT_FARM_KEY {0x43, 0x54, 0x76, 0x31, 0x2d, 0x63, 0x68, 0x61, 0x6e, 0x67, 0x65, 0x2d, 0x6d, 0x65, 0x21, 0x21}
#define DEFAULT_FREQ_HZ 865062500
#define DEFAULT_SF 10              // SF10: ~5 km with the roof antenna (SF9 ~2-4 km, SF11 for hills)
#define DEFAULT_TX_DBM 20          // India 865-867 MHz allows up to 1 W (30 dBm); use 14 in EU868

#define LORA_BW_KHZ 125
#define LORA_CR 0
#define LORA_PREAMBLE 8

#define MAX_TAGS 32
#define GPS_BAUD 9600
#define OWN_FIX_MAX_AGE_MS 5000

// Magnetic declination where you are, in degrees (east positive). Look it up once, e.g. on
// https://www.ngdc.noaa.gov/geomag/calculators/magcalc.shtml . Within a few degrees is fine.
#define MAG_DECLINATION_DEG 0

// ---- Display ----
#define OLED_ADDR 0x3C
#define OLED_SH1106 1  // 1 = 1.3" SH1106, 0 = 0.96" SSD1306

// ---- LSM303AGR mounting ----
// Which chip axis points to the TOP of the screen (FWD), to the RIGHT, and DOWN through the back,
// as 0 = x, 1 = y, 2 = z with a sign. Default: chip flat, face up, its x arrow towards the top of the screen.
// Check: hold the handheld flat, top towards north, and look at "hdg" in the serial log (should be ~0).
#define AXIS_FWD 0
#define SIGN_FWD 1
#define AXIS_RIGHT 1
#define SIGN_RIGHT -1
#define AXIS_DOWN 2
#define SIGN_DOWN -1
#define ACC_ADDR 0x19
#define MAG_ADDR 0x1E

// ---- Pins ----
#define PIN_GPS_EN PA8
#define PIN_BTN_PREV PA0
#define PIN_BTN_OK PA1
#define PIN_BTN_NEXT PB4
#define PIN_VIB PB5
#define PIN_BUZZER PA10
#define PIN_BATT PB3

#define LONG_PRESS_MS 1000
#define CAL_HOLD_MS 2000    // hold < and > together to calibrate the compass
#define CAL_SECONDS 20
#define FAST_HOURS 1        // long-press OK: tag reports every 2 min for this long

#define DEBUG_PRINT 1
