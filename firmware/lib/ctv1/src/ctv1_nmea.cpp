#include "ctv1_nmea.h"
#include <string.h>

static int hexval(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  return -1;
}

static uint32_t parse_uint(const char *s) {
  uint32_t v = 0;
  while (*s >= '0' && *s <= '9') v = v * 10 + (uint32_t)(*s++ - '0');
  return v;
}

// "12.34" -> 123 with scale 10 (fixed point, extra decimals truncated)
static uint32_t parse_fixed(const char *s, uint32_t scale) {
  uint32_t whole = parse_uint(s), frac = 0, div = 1;
  const char *dot = strchr(s, '.');
  if (dot) {
    for (const char *p = dot + 1; *p >= '0' && *p <= '9' && div < scale; p++) {
      frac = frac * 10 + (uint32_t)(*p - '0');
      div *= 10;
    }
  }
  return whole * scale + frac * (scale / div);
}

// NMEA "ddmm.mmmmm" / "dddmm.mmmmm" -> degrees * 1e7
static bool parse_coord(const char *s, char hemi, int32_t &out) {
  const char *dot = strchr(s, '.');
  if (!dot || dot - s < 3) return false;
  int deg_digits = (int)(dot - s) - 2;
  uint32_t deg = 0;
  for (int i = 0; i < deg_digits; i++) deg = deg * 10 + (uint32_t)(s[i] - '0');
  uint32_t min_e5 = parse_fixed(s + deg_digits, 100000);  // minutes * 1e5
  int64_t v = (int64_t)deg * 10000000 + ((int64_t)min_e5 * 100 + 30) / 60;
  out = (int32_t)((hemi == 'S' || hemi == 'W') ? -v : v);
  return true;
}

void Ctv1Nmea::reset() {
  len_ = 0;
  in_ = false;
  memset(&fix_, 0, sizeof(fix_));
  good_ = bad_ = 0;
}

bool Ctv1Nmea::feed(char c) {
  if (c == '$') {
    in_ = true;
    len_ = 0;
    return false;
  }
  if (!in_) return false;
  if (c == '\r' || c == '\n') {
    in_ = false;
    buf_[len_] = 0;
    return parse();
  }
  if (len_ >= sizeof(buf_) - 1) {  // too long, not a sentence we want
    in_ = false;
    return false;
  }
  buf_[len_++] = c;
  return false;
}

bool Ctv1Nmea::parse() {
  // checksum: XOR of everything between '$' and '*'
  char *star = strrchr(buf_, '*');
  if (!star || star[1] == 0 || star[2] == 0) return false;
  uint8_t sum = 0;
  for (char *p = buf_; p < star; p++) sum ^= (uint8_t)*p;
  int hi = hexval(star[1]), lo = hexval(star[2]);
  if (hi < 0 || lo < 0 || sum != (uint8_t)((hi << 4) | lo)) {
    bad_++;
    return false;
  }
  *star = 0;

  // split into fields in place (empty fields stay as "")
  const char *f[20];
  int n = 0;
  f[n++] = buf_;
  for (char *p = buf_; *p && n < 20; p++) {
    if (*p == ',') {
      *p = 0;
      f[n++] = p + 1;
    }
  }
  if (strlen(f[0]) < 5) return false;
  const char *type = f[0] + strlen(f[0]) - 3;

  if (strcmp(type, "GGA") == 0 && n >= 9) {
    // time, lat, N/S, lon, E/W, quality, sats, hdop
    uint32_t quality = parse_uint(f[6]);
    fix_.utc_hhmmss = parse_uint(f[1]);
    fix_.sats = (uint8_t)parse_uint(f[7]);
    fix_.hdop_x10 = (uint16_t)parse_fixed(f[8], 10);
    int32_t lat, lon;
    if (quality > 0 && parse_coord(f[2], f[3][0], lat) && parse_coord(f[4], f[5][0], lon)) {
      fix_.lat_e7 = lat;
      fix_.lon_e7 = lon;
      fix_.valid = true;
    } else {
      fix_.valid = false;
    }
    good_++;
    return true;
  }
  if (strcmp(type, "RMC") == 0 && n >= 10) {
    // time, status, lat, N/S, lon, E/W, speed, course, date
    fix_.utc_hhmmss = parse_uint(f[1]);
    fix_.date_ddmmyy = parse_uint(f[9]);
    fix_.speed_knots_x10 = (uint16_t)parse_fixed(f[7], 10);
    int32_t lat, lon;
    if (f[2][0] == 'A' && parse_coord(f[3], f[4][0], lat) && parse_coord(f[5], f[6][0], lon)) {
      fix_.lat_e7 = lat;
      fix_.lon_e7 = lon;
      fix_.valid = true;
    } else {
      fix_.valid = false;
    }
    good_++;
    return true;
  }
  return false;
}
