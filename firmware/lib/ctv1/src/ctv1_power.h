#pragma once
// Battery and report-interval decisions for the collar tag.
#include <stdint.h>

// Li-ion cell voltage (resting, light load) to 0..100 %.
uint8_t ctv1_batt_percent(uint16_t mv);

struct Ctv1ScheduleIn {
  uint8_t batt_pct;
  bool outside_fence;      // confirmed outside by the tag's own fence check
  bool moving;             // accelerometer says the animal is walking
  bool fast_mode;          // farmer asked for 2-minute tracking
  uint16_t base_interval_s;
};

#define CTV1_FAST_INTERVAL_S 120
#define CTV1_LOW_BATT_PCT 20
#define CTV1_CRITICAL_BATT_PCT 10

// Seconds until the next report (before random jitter).
uint32_t ctv1_next_interval_s(const Ctv1ScheduleIn &in);

// Random 0..max_s jitter from any 32-bit random value, so tags never stay in step.
uint32_t ctv1_jitter_s(uint32_t random_value, uint32_t max_s);

// ---- GPS power saving ----
// The GPS (~25 mA) is the biggest drain on the tag. Skip it when it cannot add anything:
//  - the animal has been still since the last good fix (resting cows don't move), or
//  - the last few attempts failed (in a shed, under a roof): then only try every 4th report.
// A lost or fast-tracked animal always gets a fresh fix.
#define CTV1_STILL_ACTIVITY 10         // motion events since the last fix that still count as "not moved"
#define CTV1_STILL_MAX_AGE_S 1800      // but take a fresh fix at least every 30 min
#define CTV1_GPS_FAILS_BEFORE_BACKOFF 3
#define CTV1_GPS_BACKOFF_EVERY 4

enum Ctv1GpsDecision : uint8_t { CTV1_GPS_RUN, CTV1_GPS_SKIP_STILL, CTV1_GPS_SKIP_BACKOFF };

struct Ctv1GpsPolicyIn {
  bool ever_fixed;
  bool urgent;                  // outside the fence or fast-tracking
  uint16_t activity_since_fix;  // accelerometer events since the last good fix
  uint32_t secs_since_fix;
  uint8_t fails_in_row;         // GPS attempts in a row without a fix
  uint32_t report_no;           // counts up every report
};

// Returns what to do and, for CTV1_GPS_RUN, how long to wait for a fix.
Ctv1GpsDecision ctv1_gps_decide(const Ctv1GpsPolicyIn &in, uint16_t &timeout_s);
