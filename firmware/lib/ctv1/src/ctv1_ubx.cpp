#include "ctv1_ubx.h"
#include <string.h>

size_t ctv1_ubx_frame(uint8_t cls, uint8_t id, const uint8_t *payload, uint16_t len, uint8_t *out, size_t out_max) {
  if ((size_t)len + 8 > out_max) return 0;
  out[0] = 0xB5;
  out[1] = 0x62;
  out[2] = cls;
  out[3] = id;
  out[4] = (uint8_t)len;
  out[5] = (uint8_t)(len >> 8);
  if (len) memcpy(&out[6], payload, len);
  uint8_t a = 0, b = 0;
  for (size_t i = 2; i < (size_t)len + 6; i++) {
    a = (uint8_t)(a + out[i]);
    b = (uint8_t)(b + a);
  }
  out[len + 6] = a;
  out[len + 7] = b;
  return (size_t)len + 8;
}

size_t ctv1_ubx_nmea_only_gga_rmc(uint8_t *out, size_t out_max) {
  // CFG-MSGOUT-NMEA_ID_xxx_UART1 keys (U1 values)
  static const struct { uint32_t key; uint8_t rate; } items[] = {
      {0x209100bb, 1},  // GGA on
      {0x209100ac, 1},  // RMC on
      {0x209100c5, 0},  // GSV off
      {0x209100c0, 0},  // GSA off
      {0x209100ca, 0},  // GLL off
      {0x209100b1, 0},  // VTG off
  };
  uint8_t p[4 + 6 * 5];
  p[0] = 0x00;  // version
  p[1] = 0x03;  // layers: RAM | BBR
  p[2] = p[3] = 0;
  size_t n = 4;
  for (auto &it : items) {
    for (int i = 0; i < 4; i++) p[n++] = (uint8_t)(it.key >> (8 * i));
    p[n++] = it.rate;
  }
  return ctv1_ubx_frame(0x06, 0x8A, p, (uint16_t)n, out, out_max);
}
