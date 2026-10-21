#include "ctv1_math.h"
#include <stdint.h>

// sin(0..90 degrees) * 32767, 1 degree steps
static const int16_t SIN_Q15[91] = {
    0, 572, 1144, 1715, 2286, 2856, 3425, 3993, 4560, 5126,
    5690, 6252, 6813, 7371, 7927, 8481, 9032, 9580, 10126, 10668,
    11207, 11743, 12275, 12803, 13328, 13848, 14364, 14876, 15383, 15886,
    16383, 16876, 17364, 17846, 18323, 18794, 19260, 19720, 20173, 20621,
    21062, 21497, 21925, 22347, 22762, 23170, 23571, 23964, 24351, 24730,
    25101, 25465, 25821, 26169, 26509, 26841, 27165, 27481, 27788, 28087,
    28377, 28659, 28932, 29196, 29451, 29697, 29934, 30162, 30381, 30591,
    30791, 30982, 31163, 31335, 31498, 31650, 31794, 31927, 32051, 32165,
    32269, 32364, 32448, 32523, 32587, 32642, 32687, 32722, 32747, 32762,
    32767
};

static float sin_0_90(float d) {  // d in [0, 90]
  int i = (int)d;
  if (i >= 90) return 1.0f;
  float f = d - (float)i;
  return ((float)SIN_Q15[i] + f * (float)(SIN_Q15[i + 1] - SIN_Q15[i])) * (1.0f / 32767.0f);
}

float ctv1_sin_deg(float deg) {
  float d = deg - 360.0f * (float)(int)(deg / 360.0f);  // wrap to (-360, 360)
  if (d < 0) d += 360.0f;
  if (d <= 90.0f) return sin_0_90(d);
  if (d <= 180.0f) return sin_0_90(180.0f - d);
  if (d <= 270.0f) return -sin_0_90(d - 180.0f);
  return -sin_0_90(360.0f - d);
}

float ctv1_cos_deg(float deg) { return ctv1_sin_deg(deg + 90.0f); }

static float atan_unit_deg(float z) {  // |z| <= 1, minimax polynomial, error ~1e-5 rad
  float z2 = z * z;
  float r = z * (0.9998660f + z2 * (-0.3302995f + z2 * (0.1801410f + z2 * (-0.0851330f + z2 * 0.0208351f))));
  return r * 57.2957795f;
}

float ctv1_atan2_deg(float y, float x) {
  float ax = x < 0 ? -x : x, ay = y < 0 ? -y : y;
  if (ax == 0 && ay == 0) return 0;
  float a = ax >= ay ? atan_unit_deg(ay / ax) : 90.0f - atan_unit_deg(ax / ay);
  if (x < 0) a = 180.0f - a;
  return y < 0 ? -a : a;
}

float ctv1_atan_deg(float z) { return ctv1_atan2_deg(z, 1.0f); }
