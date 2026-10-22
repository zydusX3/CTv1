#pragma once
// Tilt-compensated compass heading (as in NXP AN4248), for the handheld's LSM303AGR.
// Body axes: x forward (top of the screen), y right, z down. Feed raw readings already mapped
// to these axes; units don't matter as long as each sensor is consistent.
#include <stdint.h>

// Magnetic heading in degrees 0..359 (0 = magnetic north, 90 = east).
// mag_* should already have the hard-iron offset removed.
uint16_t ctv1_compass_heading(float ax, float ay, float az, float mx, float my, float mz);

// Hard-iron calibration: feed readings while the device is turned in all directions,
// then offset = centre of the min/max box.
struct Ctv1MagCal {
  int16_t min[3], max[3];
  uint16_t samples;
  void reset();
  void add(int16_t x, int16_t y, int16_t z);
  bool good() const;  // enough spread on all axes
  void offset(int16_t out[3]) const;
};

// Arrow angle on screen: where to point relative to the top of the device, 0..359 clockwise.
uint16_t ctv1_relative_bearing(uint16_t bearing_true, uint16_t heading_mag, int16_t declination_deg);
