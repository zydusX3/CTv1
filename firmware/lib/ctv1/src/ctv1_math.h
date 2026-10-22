#pragma once
// Small trig in degrees without libm. The STM32WL has no FPU, and libm's sinf/cosf/atan2f pull in
// ~6 KB of argument-reduction code that doesn't fit next to RUI3. Error < 0.01 degree / 1e-4.
float ctv1_sin_deg(float deg);
float ctv1_cos_deg(float deg);
float ctv1_atan2_deg(float y, float x);  // -180..180
float ctv1_atan_deg(float z);            // -90..90
