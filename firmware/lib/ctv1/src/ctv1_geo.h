#pragma once
// Geofence helpers on degrees * 1e7 coordinates. Same rules as the server (docs/06):
// an alert needs 2 fixes in a row on the other side, or 1 fix more than CTV1_FENCE_MARGIN_M out.
#include <stdint.h>

#define CTV1_FENCE_MARGIN_M 25

struct Ctv1Point {
  int32_t lat_e7;
  int32_t lon_e7;
};

// Ray casting. Works for any simple polygon smaller than a few hundred km.
bool ctv1_point_in_polygon(const Ctv1Point &p, const Ctv1Point *poly, uint8_t n);

// Distance in metres (equirectangular, accurate to well under 1 % at farm scale).
uint32_t ctv1_distance_m(const Ctv1Point &a, const Ctv1Point &b);

// Bearing from a to b in whole degrees, 0 = north, 90 = east.
uint16_t ctv1_bearing_deg(const Ctv1Point &a, const Ctv1Point &b);

// Shortest distance from p to the polygon outline, in metres.
uint32_t ctv1_distance_to_edge_m(const Ctv1Point &p, const Ctv1Point *poly, uint8_t n);

enum Ctv1FenceEvent : uint8_t { CTV1_FENCE_NONE, CTV1_FENCE_LEFT, CTV1_FENCE_RETURNED };

struct Ctv1FenceState {
  bool inside = true;   // confirmed side
  uint8_t pending = 0;  // fixes in a row on the other side
  // Feed one fix. Returns LEFT / RETURNED when the confirmed side changes.
  Ctv1FenceEvent update(bool inside_now, uint32_t edge_dist_m);
};
