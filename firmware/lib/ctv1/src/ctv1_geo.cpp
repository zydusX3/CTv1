#include "ctv1_geo.h"
#include <math.h>

static const float M_PER_E7_DEG = 0.0111319f;  // metres per 1e-7 degree of latitude

static float cos_lat(int32_t lat_e7) { return cosf((float)lat_e7 * 1e-7f * 3.14159265f / 180.0f); }

bool ctv1_point_in_polygon(const Ctv1Point &p, const Ctv1Point *poly, uint8_t n) {
  // work relative to p so the products fit in 64 bits for any farm-sized polygon
  bool inside = false;
  for (uint8_t i = 0, j = n - 1; i < n; j = i++) {
    int64_t yi = (int64_t)poly[i].lat_e7 - p.lat_e7, yj = (int64_t)poly[j].lat_e7 - p.lat_e7;
    int64_t xi = (int64_t)poly[i].lon_e7 - p.lon_e7, xj = (int64_t)poly[j].lon_e7 - p.lon_e7;
    if ((yi > 0) != (yj > 0)) {
      // x coordinate where the edge crosses y = 0, compared with 0 without dividing
      int64_t num = xi * (yj - yi) - yi * (xj - xi);  // = x_cross * (yj - yi)
      if ((yj - yi) > 0 ? num > 0 : num < 0) inside = !inside;
    }
  }
  return inside;
}

static void to_xy(const Ctv1Point &origin, const Ctv1Point &p, float k, float &x, float &y) {
  x = (float)(p.lon_e7 - origin.lon_e7) * M_PER_E7_DEG * k;
  y = (float)(p.lat_e7 - origin.lat_e7) * M_PER_E7_DEG;
}

uint32_t ctv1_distance_m(const Ctv1Point &a, const Ctv1Point &b) {
  float x, y;
  to_xy(a, b, cos_lat((a.lat_e7 / 2) + (b.lat_e7 / 2)), x, y);
  return (uint32_t)(sqrtf(x * x + y * y) + 0.5f);
}

uint16_t ctv1_bearing_deg(const Ctv1Point &a, const Ctv1Point &b) {
  float x, y;
  to_xy(a, b, cos_lat(a.lat_e7), x, y);
  float deg = atan2f(x, y) * 180.0f / 3.14159265f;
  if (deg < 0) deg += 360.0f;
  uint16_t d = (uint16_t)(deg + 0.5f);
  return d >= 360 ? 0 : d;
}

uint32_t ctv1_distance_to_edge_m(const Ctv1Point &p, const Ctv1Point *poly, uint8_t n) {
  float k = cos_lat(p.lat_e7), best = 1e12f;
  for (uint8_t i = 0, j = n - 1; i < n; j = i++) {
    float ax, ay, bx, by;
    to_xy(p, poly[j], k, ax, ay);
    to_xy(p, poly[i], k, bx, by);
    float dx = bx - ax, dy = by - ay, len2 = dx * dx + dy * dy;
    float t = len2 > 0 ? -(ax * dx + ay * dy) / len2 : 0;
    t = t < 0 ? 0 : (t > 1 ? 1 : t);
    float cx = ax + t * dx, cy = ay + t * dy, d2 = cx * cx + cy * cy;
    if (d2 < best) best = d2;
  }
  return (uint32_t)(sqrtf(best) + 0.5f);
}

Ctv1FenceEvent Ctv1FenceState::update(bool inside_now, uint32_t edge_dist_m) {
  if (inside_now == inside) {
    pending = 0;
    return CTV1_FENCE_NONE;
  }
  pending++;
  if (pending >= 2 || (!inside_now && edge_dist_m > CTV1_FENCE_MARGIN_M)) {
    inside = inside_now;
    pending = 0;
    return inside ? CTV1_FENCE_RETURNED : CTV1_FENCE_LEFT;
  }
  return CTV1_FENCE_NONE;
}
