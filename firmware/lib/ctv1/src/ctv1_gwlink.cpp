#include "ctv1_gwlink.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

size_t ctv1_gw_format_packet(char *out, size_t out_len, const char *kind, int16_t rssi, int8_t snr,
                             const uint8_t *pkt, size_t pkt_len) {
  int n = snprintf(out, out_len, "%s,%d,%d,", kind, rssi, snr);
  if (n < 0 || (size_t)n + pkt_len * 2 + 1 > out_len) return 0;
  static const char HEX[] = "0123456789abcdef";
  for (size_t i = 0; i < pkt_len; i++) {
    out[n++] = HEX[pkt[i] >> 4];
    out[n++] = HEX[pkt[i] & 0x0f];
  }
  out[n] = 0;
  return (size_t)n;
}

static int nib(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

size_t ctv1_hex_decode(const char *hex, uint8_t *out, size_t out_max) {
  size_t n = 0;
  while (hex[0] && hex[0] != '\r' && hex[0] != '\n') {
    int hi = nib(hex[0]), lo = hex[1] ? nib(hex[1]) : -1;
    if (hi < 0 || lo < 0 || n >= out_max) return 0;
    out[n++] = (uint8_t)((hi << 4) | lo);
    hex += 2;
  }
  return n;
}

static bool next_num(const char *&p, long lo, long hi, long &v) {
  if (*p != ',') return false;
  char *end;
  v = strtol(p + 1, &end, 10);
  if (end == p + 1 || v < lo || v > hi) return false;
  p = end;
  return true;
}

bool ctv1_gw_parse_cmd(const char *line, Ctv1GwCommand &out) {
  if (strncmp(line, "CMD", 3) != 0) return false;
  const char *p = line + 3;
  long tag, cmd, arg;
  if (!next_num(p, 0, 65535, tag) || !next_num(p, 0, 3, cmd) || !next_num(p, 0, 255, arg)) return false;
  if (*p && *p != '\r' && *p != '\n') return false;
  out.tag_id = (uint16_t)tag;
  out.cmd = (uint8_t)cmd;
  out.arg = (uint8_t)arg;
  return true;
}
