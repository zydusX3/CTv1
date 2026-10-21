#include "ctv1_json.h"
#include "ctv1_power.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *FLAG_NAMES[] = {"fix", "moving", "low_batt", "charging", "outside", "cmd_done"};
static const char *CMD_NAMES[] = {"none", "buzz", "interval", "fast"};

size_t ctv1_format_e7(char *out, size_t out_len, int32_t v) {
  int64_t a = v < 0 ? -(int64_t)v : v;
  int n = snprintf(out, out_len, "%s%ld.%07ld", v < 0 ? "-" : "", (long)(a / 10000000), (long)(a % 10000000));
  return n < 0 ? 0 : (size_t)n;
}

size_t ctv1_json_uplink(char *out, size_t out_len, const char *gw_id, const Ctv1Uplink &up, int16_t rssi,
                        int8_t snr, int64_t ts_unix, const uint8_t *raw, size_t raw_len) {
  char lat[16], lon[16], flags[80] = "", ts[24] = "null", hex[2 * 32 + 1];
  ctv1_format_e7(lat, sizeof(lat), up.lat_e7);
  ctv1_format_e7(lon, sizeof(lon), up.lon_e7);
  for (int i = 0; i < 6; i++) {
    if (!(up.flags & (1 << i))) continue;
    size_t l = strlen(flags);
    snprintf(flags + l, sizeof(flags) - l, "%s\"%s\"", l ? "," : "", FLAG_NAMES[i]);
  }
  if (ts_unix > 0) snprintf(ts, sizeof(ts), "%lld", (long long)ts_unix);
  if (raw_len > 32) raw_len = 32;
  for (size_t i = 0; i < raw_len; i++) snprintf(hex + 2 * i, 3, "%02x", raw[i]);
  hex[2 * raw_len] = 0;
  int n = snprintf(out, out_len,
                   "{\"gw\":\"%s\",\"tag\":%u,\"seq\":%u,\"type\":\"%s\",\"lat\":%s,\"lon\":%s,\"batt_mv\":%u,"
                   "\"batt_pct\":%u,\"flags\":[%s],\"sats\":%u,\"hdop\":%u.%u,\"activity\":%u,\"rssi\":%d,"
                   "\"snr\":%d,\"ts\":%s,\"raw\":\"%s\"}",
                   gw_id, up.tag_id, up.seq, up.type == CTV1_UP_POSITION ? "position" : "nofix", lat, lon,
                   up.batt_mv, ctv1_batt_percent(up.batt_mv), flags, up.sats, up.hdop_x2 / 2,
                   (up.hdop_x2 % 2) * 5, up.activity, rssi, snr, ts, hex);
  return (n < 0 || (size_t)n >= out_len) ? 0 : (size_t)n;
}

// Points at the value after "key": , or nullptr. Good enough for the flat objects above.
static const char *find_value(const char *json, const char *key) {
  size_t klen = strlen(key);
  for (const char *p = strchr(json, '"'); p; p = strchr(p + 1, '"')) {
    if (strncmp(p + 1, key, klen) == 0 && p[1 + klen] == '"') {
      const char *v = p + 2 + klen;
      while (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n') v++;
      if (*v != ':') continue;
      v++;
      while (*v == ' ' || *v == '\t' || *v == '\r' || *v == '\n') v++;
      return v;
    }
  }
  return nullptr;
}

static bool get_long(const char *json, const char *key, long lo, long hi, long &out) {
  const char *v = find_value(json, key);
  if (!v) return false;
  char *end;
  out = strtol(v, &end, 10);
  return end != v && out >= lo && out <= hi;
}

bool ctv1_json_parse_down(const char *json, Ctv1GwCommand &out) {
  long tag, cmd = -1, arg = 0;
  if (!get_long(json, "tag", 0, 65535, tag)) return false;
  const char *v = find_value(json, "cmd");
  if (!v) return false;
  if (*v == '"') {
    for (long i = 0; i < 4; i++) {
      size_t l = strlen(CMD_NAMES[i]);
      if (strncmp(v + 1, CMD_NAMES[i], l) == 0 && v[1 + l] == '"') cmd = i;
    }
  } else if (!get_long(json, "cmd", 0, 3, cmd)) {
    return false;
  }
  if (cmd < 0) return false;
  if (find_value(json, "arg") && !get_long(json, "arg", 0, 255, arg)) return false;
  if (cmd == CTV1_CMD_BUZZ && arg == 0) arg = 30;
  out.tag_id = (uint16_t)tag;
  out.cmd = (uint8_t)cmd;
  out.arg = (uint8_t)arg;
  return true;
}

// "-33.86880" -> -338688000 without floating point. Advances p past the number.
static bool parse_deg_e7(const char *&p, int32_t &out) {
  bool neg = *p == '-';
  if (neg || *p == '+') p++;
  if (*p < '0' || *p > '9') return false;
  int64_t whole = 0, frac = 0;
  while (*p >= '0' && *p <= '9') whole = whole * 10 + (*p++ - '0');
  int digits = 0;
  if (*p == '.') {
    p++;
    while (*p >= '0' && *p <= '9') {
      if (digits < 7) {
        frac = frac * 10 + (*p - '0');
        digits++;
      }
      p++;
    }
  }
  while (digits++ < 7) frac *= 10;
  int64_t v = whole * 10000000 + frac;
  if (whole > 180) return false;
  out = (int32_t)(neg ? -v : v);
  return true;
}

uint8_t ctv1_json_parse_fence(const char *json, Ctv1Point *pts, uint8_t max) {
  const char *p = find_value(json, "points");
  if (!p) p = json;
  while (*p && *p != '[') p++;
  if (*p != '[') return 0;
  p++;
  uint8_t n = 0;
  for (;;) {
    while (*p == ' ' || *p == ',' || *p == '\r' || *p == '\n' || *p == '\t') p++;
    if (*p == ']') break;  // end of the points list
    if (*p != '[' || n >= max) return 0;
    p++;
    int32_t lat, lon;
    while (*p == ' ') p++;
    if (!parse_deg_e7(p, lat)) return 0;
    while (*p == ' ' || *p == ',') p++;
    if (!parse_deg_e7(p, lon)) return 0;
    while (*p == ' ') p++;
    if (*p != ']' || lat > 900000000 || lat < -900000000) return 0;
    p++;
    pts[n].lat_e7 = lat;
    pts[n].lon_e7 = lon;
    n++;
  }
  return n >= 3 ? n : 0;
}
