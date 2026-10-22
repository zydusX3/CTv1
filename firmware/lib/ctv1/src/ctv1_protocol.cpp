#include "ctv1_protocol.h"
#include <string.h>
#include "ctv1_aes.h"

static void put16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
static void put32(uint8_t *p, uint32_t v) { for (int i = 0; i < 4; i++) p[i] = (uint8_t)(v >> (8 * i)); }
static uint16_t get16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t get32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void mic(const uint8_t key[16], const uint8_t *msg, size_t len, uint8_t out[CTV1_MIC_LEN]) {
  uint8_t full[16];
  ctv1_aes_cmac(key, msg, len, full);
  memcpy(out, full, CTV1_MIC_LEN);
}

uint32_t ctv1_lora_airtime_ms(uint8_t payload_len, uint8_t sf) {
  // Semtech AN1200.13 formula, in microseconds; one symbol at 125 kHz = 2^sf * 8 us
  uint32_t tsym_us = (1UL << sf) * 8;
  int de = sf >= 11 ? 1 : 0;  // low data-rate optimisation (symbol > 16 ms)
  int32_t num = 8 * payload_len - 4 * sf + 28 + 16;
  int32_t den = 4 * (sf - 2 * de);
  int32_t blocks = num > 0 ? (num + den - 1) / den : 0;
  uint32_t symbols_x4 = (8 + 4) * 4 + 1 + 4 * (8 + blocks * 5);  // preamble + 4.25, times 4
  return (symbols_x4 * tsym_us / 4 + 999) / 1000;
}

uint32_t ctv1_handheld_reply_delay_ms(uint8_t sf) {
  return CTV1_ACK_DELAY_MS + ctv1_lora_airtime_ms(CTV1_DOWNLINK_LEN, sf) + 100;  // 100 ms guard
}

uint32_t ctv1_tag_rx_window_ms(uint8_t sf) {
  uint32_t w = ctv1_handheld_reply_delay_ms(sf) + ctv1_lora_airtime_ms(CTV1_DOWNLINK_LEN, sf) + 200;
  return w < 1500 ? 1500 : w;
}

void ctv1_derive_tag_key(const uint8_t farm_key[16], uint16_t tag_id, uint8_t tag_key[16]) {
  uint8_t block[16] = {'C', 'T', '1'};
  put16(&block[3], tag_id);
  ctv1_aes128_encrypt(farm_key, block, tag_key);
}

size_t ctv1_encode_uplink(const Ctv1Uplink &up, const uint8_t tag_key[16], uint8_t out[CTV1_UPLINK_LEN]) {
  uint16_t mv = up.batt_mv < 2000 ? 2000 : up.batt_mv;
  uint16_t batt = (uint16_t)((mv - 2000) / 10);
  out[0] = up.type;
  put16(&out[1], up.tag_id);
  out[3] = up.seq;
  put32(&out[4], (uint32_t)up.lat_e7);
  put32(&out[8], (uint32_t)up.lon_e7);
  out[12] = (uint8_t)(batt > 255 ? 255 : batt);
  out[13] = up.flags;
  out[14] = (uint8_t)(((up.sats > 15 ? 15 : up.sats) << 4) | (up.hdop_x2 > 15 ? 15 : up.hdop_x2));
  out[15] = up.activity;
  mic(tag_key, out, 16, &out[16]);
  return CTV1_UPLINK_LEN;
}

size_t ctv1_encode_downlink(const Ctv1Downlink &dn, const uint8_t tag_key[16], uint8_t out[CTV1_DOWNLINK_LEN]) {
  out[0] = dn.type;
  put16(&out[1], dn.tag_id);
  out[3] = dn.seq;
  out[4] = dn.cmd;
  out[5] = dn.arg;
  mic(tag_key, out, 6, &out[6]);
  return CTV1_DOWNLINK_LEN;
}

bool ctv1_decode_uplink(const uint8_t *buf, size_t len, Ctv1Uplink &up) {
  if (len != CTV1_UPLINK_LEN || (buf[0] != CTV1_UP_POSITION && buf[0] != CTV1_UP_NOFIX)) return false;
  up.type = buf[0];
  up.tag_id = get16(&buf[1]);
  up.seq = buf[3];
  up.lat_e7 = (int32_t)get32(&buf[4]);
  up.lon_e7 = (int32_t)get32(&buf[8]);
  up.batt_mv = (uint16_t)(2000 + buf[12] * 10);
  up.flags = buf[13];
  up.sats = buf[14] >> 4;
  up.hdop_x2 = buf[14] & 0x0f;
  up.activity = buf[15];
  return true;
}

bool ctv1_decode_downlink(const uint8_t *buf, size_t len, Ctv1Downlink &dn) {
  if (len != CTV1_DOWNLINK_LEN || (buf[0] != CTV1_DOWN_ACK && buf[0] != CTV1_DOWN_ACK_CMD)) return false;
  dn.type = buf[0];
  dn.tag_id = get16(&buf[1]);
  dn.seq = buf[3];
  dn.cmd = buf[4];
  dn.arg = buf[5];
  return true;
}

bool ctv1_peek_tag_id(const uint8_t *buf, size_t len, uint16_t &tag_id) {
  if (len < 3) return false;
  tag_id = get16(&buf[1]);
  return true;
}

bool ctv1_check_mic(const uint8_t *buf, size_t len, const uint8_t tag_key[16]) {
  if (len <= CTV1_MIC_LEN) return false;
  uint8_t expect[CTV1_MIC_LEN];
  mic(tag_key, buf, len - CTV1_MIC_LEN, expect);
  uint8_t diff = 0;  // constant time compare
  for (int i = 0; i < CTV1_MIC_LEN; i++) diff |= (uint8_t)(expect[i] ^ buf[len - CTV1_MIC_LEN + i]);
  return diff == 0;
}
