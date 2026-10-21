#include "ctv1_settings.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ctv1_gwlink.h"

static const uint8_t MAGIC[4] = {'C', 'T', 'S', 1};

uint32_t ctv1_crc32(const uint8_t *data, size_t len) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < len; i++) {
    crc ^= data[i];
    for (int b = 0; b < 8; b++) crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
  }
  return ~crc;
}

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static bool valid_freq(uint32_t f) { return f >= 150000000u && f <= 960000000u; }  // RUI3 P2P range
static bool valid(const Ctv1Settings &s) {
  return valid_freq(s.freq_hz) && s.sf >= 7 && s.sf <= 12 && s.tx_dbm >= 2 && s.tx_dbm <= 22 &&
         s.interval_s >= 60 && s.interval_s <= 7200;
}

// Layout: magic[4] tag_id[2] key[16] freq[4] sf tx interval[2] mag[6] flags pad[7] crc[4]
void ctv1_settings_pack(const Ctv1Settings &s, uint8_t out[CTV1_SETTINGS_BLOB_LEN]) {
  memset(out, 0, CTV1_SETTINGS_BLOB_LEN);
  memcpy(out, MAGIC, 4);
  put16(&out[4], s.tag_id);
  memcpy(&out[6], s.farm_key, 16);
  put32(&out[22], s.freq_hz);
  out[26] = s.sf;
  out[27] = s.tx_dbm;
  put16(&out[28], s.interval_s);
  for (int i = 0; i < 3; i++) put16(&out[30 + 2 * i], (uint16_t)s.mag_offset[i]);
  out[36] = s.mag_calibrated ? 1 : 0;
  put32(&out[44], ctv1_crc32(out, 44));
}

bool ctv1_settings_unpack(const uint8_t *in, size_t len, Ctv1Settings &s) {
  if (len < CTV1_SETTINGS_BLOB_LEN || memcmp(in, MAGIC, 4) != 0) return false;
  if (get32(&in[44]) != ctv1_crc32(in, 44)) return false;
  Ctv1Settings t;
  t.tag_id = get16(&in[4]);
  memcpy(t.farm_key, &in[6], 16);
  t.freq_hz = get32(&in[22]);
  t.sf = in[26];
  t.tx_dbm = in[27];
  t.interval_s = get16(&in[28]);
  for (int i = 0; i < 3; i++) t.mag_offset[i] = (int16_t)get16(&in[30 + 2 * i]);
  t.mag_calibrated = in[36] == 1;
  if (!valid(t)) return false;
  s = t;
  return true;
}

static bool parse_u32(const char *v, uint32_t lo, uint32_t hi, uint32_t &out) {
  if (!v || !isdigit((unsigned char)*v)) return false;
  char *end;
  unsigned long x = strtoul(v, &end, 10);
  if (*end && *end != '\r' && *end != '\n') return false;
  if (x < lo || x > hi) return false;
  out = (uint32_t)x;
  return true;
}

static bool name_is(const char *a, const char *b) {
  while (*a && *b) {
    if (toupper((unsigned char)*a++) != *b++) return false;
  }
  return !*a && !*b;
}

Ctv1SettingResult ctv1_settings_set(Ctv1Settings &s, const char *name, const char *value) {
  uint32_t v;
  if (name_is(name, "ID")) {
    if (!parse_u32(value, 1, 65535, v)) return CTV1_SET_BAD_VALUE;
    s.tag_id = (uint16_t)v;
  } else if (name_is(name, "KEY")) {
    uint8_t k[16];
    if (!value || strlen(value) != 32 || ctv1_hex_decode(value, k, sizeof(k)) != 16) return CTV1_SET_BAD_VALUE;
    memcpy(s.farm_key, k, 16);
  } else if (name_is(name, "FREQ")) {
    if (!parse_u32(value, 150000000u, 960000000u, v)) return CTV1_SET_BAD_VALUE;
    s.freq_hz = v;
  } else if (name_is(name, "SF")) {
    if (!parse_u32(value, 7, 12, v)) return CTV1_SET_BAD_VALUE;
    s.sf = (uint8_t)v;
  } else if (name_is(name, "PWR")) {
    if (!parse_u32(value, 2, 22, v)) return CTV1_SET_BAD_VALUE;
    s.tx_dbm = (uint8_t)v;
  } else if (name_is(name, "INT")) {
    if (!parse_u32(value, 60, 7200, v)) return CTV1_SET_BAD_VALUE;
    s.interval_s = (uint16_t)v;
  } else {
    return CTV1_SET_UNKNOWN;
  }
  return CTV1_SET_OK;
}

size_t ctv1_settings_describe(const Ctv1Settings &s, char *out, size_t out_len) {
  bool key_set = false;
  for (int i = 0; i < 16; i++) key_set = key_set || s.farm_key[i];
  int n = snprintf(out, out_len, "ID=%u FREQ=%lu SF=%u PWR=%u INT=%u KEY=%s COMPASS=%s", s.tag_id,
                   (unsigned long)s.freq_hz, s.sf, s.tx_dbm, s.interval_s, key_set ? "set" : "not set",
                   s.mag_calibrated ? "calibrated" : "not calibrated");
  return n < 0 ? 0 : (size_t)n;
}
