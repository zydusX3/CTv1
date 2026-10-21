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
