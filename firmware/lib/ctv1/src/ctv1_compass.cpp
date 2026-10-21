#include "ctv1_compass.h"
#include "ctv1_math.h"

uint16_t ctv1_compass_heading(float ax, float ay, float az, float mx, float my, float mz) {
  float roll = ctv1_atan2_deg(ay, az);
  float sr = ctv1_sin_deg(roll), cr = ctv1_cos_deg(roll);
  float den = ay * sr + az * cr;
  float pitch = den != 0 ? ctv1_atan_deg(-ax / den) : (ax < 0 ? 90.0f : -90.0f);
  float sp = ctv1_sin_deg(pitch), cp = ctv1_cos_deg(pitch);
  float bfy = mz * sr - my * cr;
  float bfx = mx * cp + my * sp * sr + mz * sp * cr;
  float deg = ctv1_atan2_deg(bfy, bfx);
  if (deg < 0) deg += 360.0f;
  uint16_t d = (uint16_t)(deg + 0.5f);
  return d >= 360 ? 0 : d;
}

void Ctv1MagCal::reset() {
  for (int i = 0; i < 3; i++) {
    min[i] = 32767;
    max[i] = -32768;
  }
  samples = 0;
}

void Ctv1MagCal::add(int16_t x, int16_t y, int16_t z) {
  int16_t v[3] = {x, y, z};
  for (int i = 0; i < 3; i++) {
    if (v[i] < min[i]) min[i] = v[i];
    if (v[i] > max[i]) max[i] = v[i];
  }
  if (samples < 65535) samples++;
}

bool Ctv1MagCal::good() const {
  if (samples < 50) return false;
  int32_t spread[3];
  for (int i = 0; i < 3; i++) spread[i] = (int32_t)max[i] - min[i];
  // every axis should have seen at least half the swing of the largest one
  int32_t big = spread[0] > spread[1] ? spread[0] : spread[1];
  if (spread[2] > big) big = spread[2];
  return big > 100 && spread[0] * 2 >= big && spread[1] * 2 >= big && spread[2] * 2 >= big;
}

void Ctv1MagCal::offset(int16_t out[3]) const {
  for (int i = 0; i < 3; i++) out[i] = (int16_t)(((int32_t)min[i] + max[i]) / 2);
}

uint16_t ctv1_relative_bearing(uint16_t bearing_true, uint16_t heading_mag, int16_t declination_deg) {
  int32_t heading_true = (int32_t)heading_mag + declination_deg;
  int32_t r = ((int32_t)bearing_true - heading_true) % 360;
  if (r < 0) r += 360;
  return (uint16_t)r;
}
