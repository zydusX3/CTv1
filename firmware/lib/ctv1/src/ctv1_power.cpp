#include "ctv1_power.h"

static const struct { uint16_t mv; uint8_t pct; } CURVE[] = {
  {3000, 0}, {3450, 5}, {3680, 10}, {3740, 20}, {3770, 30}, {3790, 40},
  {3820, 50}, {3870, 60}, {3920, 70}, {4000, 80}, {4100, 90}, {4200, 100},
};

uint8_t ctv1_batt_percent(uint16_t mv) {
  const int n = sizeof(CURVE) / sizeof(CURVE[0]);
  if (mv <= CURVE[0].mv) return 0;
  if (mv >= CURVE[n - 1].mv) return 100;
  for (int i = 1; i < n; i++) {
    if (mv <= CURVE[i].mv) {
      uint32_t span = CURVE[i].mv - CURVE[i - 1].mv;
      return (uint8_t)(CURVE[i - 1].pct + ((uint32_t)(mv - CURVE[i - 1].mv) * (CURVE[i].pct - CURVE[i - 1].pct) + span / 2) / span);
    }
  }
  return 100;
}

uint32_t ctv1_next_interval_s(const Ctv1ScheduleIn &in) {
  if (in.batt_pct < CTV1_CRITICAL_BATT_PCT) return in.outside_fence ? 600 : 3600;
  if (in.outside_fence || in.fast_mode) return CTV1_FAST_INTERVAL_S;
  uint32_t base = in.base_interval_s ? in.base_interval_s : 600;
  if (in.batt_pct < CTV1_LOW_BATT_PCT) return base * 3;
  return base;
}

uint32_t ctv1_jitter_s(uint32_t random_value, uint32_t max_s) { return max_s ? random_value % (max_s + 1) : 0; }
