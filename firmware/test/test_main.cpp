// Host-side unit tests for the shared CTv1 library. Run: make -C firmware/test
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../lib/ctv1/src/CTv1.h"

static int failures = 0, checks = 0;
#define CHECK(cond)                                                    \
  do {                                                                 \
    checks++;                                                          \
    if (!(cond)) {                                                     \
      failures++;                                                      \
      printf("  FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);         \
    }                                                                  \
  } while (0)
#define CHECK_EQ(a, b)                                                                     \
  do {                                                                                     \
    checks++;                                                                              \
    long long _a = (long long)(a), _b = (long long)(b);                                    \
    if (_a != _b) {                                                                        \
      failures++;                                                                          \
      printf("  FAIL %s:%d  %s == %s  (%lld vs %lld)\n", __FILE__, __LINE__, #a, #b, _a, _b); \
    }                                                                                      \
  } while (0)

static size_t unhex(const char *h, uint8_t *out) { return ctv1_hex_decode(h, out, 64); }

static void test_aes() {
  uint8_t key[16], pt[16], ct[16], want[16];
  unhex("000102030405060708090a0b0c0d0e0f", key);  // FIPS-197 appendix C.1
  unhex("00112233445566778899aabbccddeeff", pt);
  unhex("69c4e0d86a7b0430d8cdb78070b4c55a", want);
  ctv1_aes128_encrypt(key, pt, ct);
  CHECK(memcmp(ct, want, 16) == 0);
}

static void test_cmac() {  // RFC 4493 section 4
  uint8_t key[16], msg[64], mac[16], want[16];
  unhex("2b7e151628aed2a6abf7158809cf4f3c", key);
  unhex("6bc1bee22e409f96e93d7e117393172aae2d8a571e03ac9c9eb76fac45af8e51"
        "30c81c46a35ce411e5fbc1191a0a52eff69f2445df4f9b17ad2b417be66c3710", msg);
  const struct { size_t len; const char *mac; } cases[] = {
    {0, "bb1d6929e95937287fa37d129b756746"},
    {16, "070a16b46b4d4144f79bdd9dd04a287c"},
    {40, "dfa66747de9ae63030ca32611497c827"},
    {64, "51f0bebf7e3b9d92fc49741779363cfe"},
  };
  for (auto &c : cases) {
    unhex(c.mac, want);
    ctv1_aes_cmac(key, msg, c.len, mac);
    CHECK(memcmp(mac, want, 16) == 0);
  }
}

static const uint8_t FARM_KEY[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

static void test_protocol() {
  uint8_t k7[16], k8[16], buf[CTV1_UPLINK_LEN];
  ctv1_derive_tag_key(FARM_KEY, 7, k7);
  ctv1_derive_tag_key(FARM_KEY, 8, k8);
  CHECK(memcmp(k7, k8, 16) != 0);

  Ctv1Uplink up = {CTV1_UP_POSITION, 7, 200, 108512345, 762712345, 3917, CTV1_FLAG_FIX_OK | CTV1_FLAG_MOVING, 9, 3, 42};
  CHECK_EQ(ctv1_encode_uplink(up, k7, buf), 20);
  CHECK(ctv1_check_mic(buf, sizeof(buf), k7));
  CHECK(!ctv1_check_mic(buf, sizeof(buf), k8));  // wrong tag key

  uint16_t id = 0;
  CHECK(ctv1_peek_tag_id(buf, sizeof(buf), id));
  CHECK_EQ(id, 7);

  Ctv1Uplink back;
  CHECK(ctv1_decode_uplink(buf, sizeof(buf), back));
  CHECK_EQ(back.tag_id, 7);
  CHECK_EQ(back.seq, 200);
  CHECK_EQ(back.lat_e7, 108512345);
  CHECK_EQ(back.lon_e7, 762712345);
  CHECK_EQ(back.batt_mv, 3910);  // 10 mV steps
  CHECK_EQ(back.flags, CTV1_FLAG_FIX_OK | CTV1_FLAG_MOVING);
  CHECK_EQ(back.sats, 9);
  CHECK_EQ(back.hdop_x2, 3);
  CHECK_EQ(back.activity, 42);

  // negative coordinates survive the round trip
  up.lat_e7 = -338688000;
  up.lon_e7 = -1512093000;
  ctv1_encode_uplink(up, k7, buf);
  ctv1_decode_uplink(buf, sizeof(buf), back);
  CHECK_EQ(back.lat_e7, -338688000);
  CHECK_EQ(back.lon_e7, -1512093000);

  // any flipped bit breaks the MIC
  for (int i = 0; i < 20; i++) {
    uint8_t t[20];
    memcpy(t, buf, 20);
    t[i] ^= 0x10;
    CHECK(!ctv1_check_mic(t, 20, k7));
  }
  CHECK(!ctv1_decode_uplink(buf, 19, back));

  uint8_t d[CTV1_DOWNLINK_LEN];
  Ctv1Downlink dn = {CTV1_DOWN_ACK_CMD, 7, 200, CTV1_CMD_BUZZ, 30}, dback;
  CHECK_EQ(ctv1_encode_downlink(dn, k7, d), 10);
  CHECK(ctv1_check_mic(d, sizeof(d), k7));
  CHECK(ctv1_decode_downlink(d, sizeof(d), dback));
  CHECK_EQ(dback.cmd, CTV1_CMD_BUZZ);
  CHECK_EQ(dback.arg, 30);
  CHECK(!ctv1_decode_uplink(d, sizeof(d), back));  // a downlink is never taken for an uplink

  // airtime against the Semtech LoRa calculator (125 kHz, CR 4/5, preamble 8, CRC on)
  CHECK_EQ(ctv1_lora_airtime_ms(20, 9), 186);   // 185.3 ms rounded up
  CHECK_EQ(ctv1_lora_airtime_ms(20, 10), 371);
  CHECK_EQ(ctv1_lora_airtime_ms(10, 10), 289);
  CHECK_EQ(ctv1_lora_airtime_ms(20, 12), 1319);
  for (uint8_t sf = 7; sf <= 12; sf++) {
    uint32_t beacon_done = CTV1_ACK_DELAY_MS + ctv1_lora_airtime_ms(CTV1_DOWNLINK_LEN, sf);
    uint32_t hh = ctv1_handheld_reply_delay_ms(sf);
    CHECK(hh > beacon_done);                                                         // no overlap
    CHECK(ctv1_tag_rx_window_ms(sf) > hh + ctv1_lora_airtime_ms(CTV1_DOWNLINK_LEN, sf));  // tag still listening
  }
}

static int feed(Ctv1Nmea &n, const char *s) {
  int updates = 0;
  while (*s) updates += n.feed(*s++);
  return updates;
}

static void test_nmea() {
  Ctv1Nmea n;
  CHECK_EQ(feed(n, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n"), 1);
  CHECK(n.fix().valid);
  CHECK_EQ(n.fix().lat_e7, 481173000);  // 48 + 7.038/60
  CHECK_EQ(n.fix().lon_e7, 115166667);  // 11 + 31/60
  CHECK_EQ(n.fix().sats, 8);
  CHECK_EQ(n.fix().hdop_x10, 9);
  CHECK_EQ(n.fix().utc_hhmmss, 123519);

  CHECK_EQ(feed(n, "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n"), 1);
  CHECK(n.fix().valid);
  CHECK_EQ(n.fix().date_ddmmyy, 230394);
  CHECK_EQ(n.fix().speed_knots_x10, 224);

  // corrupted checksum is ignored and counted
  CHECK_EQ(feed(n, "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*48\r\n"), 0);
  CHECK_EQ(n.checksum_errors(), 1);

  // no fix yet (typical during a cold start), GN talker
  char s[100];
  const char *body = "GNGGA,000001.00,,,,,0,00,99.99,,,,,,";
  uint8_t sum = 0;
  for (const char *p = body; *p; p++) sum ^= (uint8_t)*p;
  snprintf(s, sizeof(s), "$%s*%02X\r\n", body, sum);
  CHECK_EQ(feed(n, s), 1);
  CHECK(!n.fix().valid);

  // south / west, 5 decimal minutes (u-blox M10 style)
  body = "GNGGA,101010.00,3352.12800,S,15112.55800,W,1,11,0.82,20.0,M,0.0,M,,";
  sum = 0;
  for (const char *p = body; *p; p++) sum ^= (uint8_t)*p;
  snprintf(s, sizeof(s), "$%s*%02x\r\n", body, sum);
  CHECK_EQ(feed(n, s), 1);
  CHECK(n.fix().valid);
  CHECK_EQ(n.fix().lat_e7, -338688000);
  CHECK_EQ(n.fix().lon_e7, -1512093000);
  CHECK_EQ(n.fix().hdop_x10, 8);

  // other sentences and junk are ignored
  CHECK_EQ(feed(n, "$GPGSV,3,1,11,03,03,111,00,04,15,270,00,06,01,010,00,13,06,292,00*74\r\nnoise$$\r\n"), 0);
}

// The sample paddock from the UI mockup, converted to lat/lon (degrees * 1e7).
static Ctv1Point mp(double x, double y) {  // mockup metres -> point (origin 10.85 N, 76.27 E)
  Ctv1Point p;
  p.lat_e7 = (int32_t)((10.85 + (350 - y) / 111320.0) * 1e7);
  p.lon_e7 = (int32_t)((76.27 + (x - 500) / (111320.0 * 0.98212)) * 1e7);
  return p;
}

static void test_geo() {
  Ctv1Point paddock[] = {mp(230, 170), mp(720, 150), mp(800, 330), mp(760, 520), mp(300, 540), mp(200, 380)};
  CHECK(ctv1_point_in_polygon(mp(480, 350), paddock, 6));   // trough
  CHECK(ctv1_point_in_polygon(mp(780, 330), paddock, 6));   // just inside east corner
  CHECK(!ctv1_point_in_polygon(mp(895, 300), paddock, 6));  // maize plot
  CHECK(!ctv1_point_in_polygon(mp(135, 600), paddock, 6));  // house

  uint32_t d = ctv1_distance_m(mp(135, 600), mp(535, 300));  // 3-4-5 triangle, 500 m
  CHECK(d >= 495 && d <= 505);
  CHECK_EQ(ctv1_bearing_deg(mp(500, 350), mp(500, 100)), 0);   // north
  CHECK_EQ(ctv1_bearing_deg(mp(500, 350), mp(700, 350)), 90);  // east
  uint16_t sw = ctv1_bearing_deg(mp(500, 350), mp(400, 450));
  CHECK(sw >= 224 && sw <= 226);

  uint32_t e = ctv1_distance_to_edge_m(mp(840, 330), paddock, 6);  // 40 m east of the corner
  CHECK(e >= 38 && e <= 42);

  Ctv1FenceState st;
  CHECK_EQ(st.update(true, 50), CTV1_FENCE_NONE);
  CHECK_EQ(st.update(false, 5), CTV1_FENCE_NONE);  // one fix just outside: GPS jitter?
  CHECK_EQ(st.update(true, 5), CTV1_FENCE_NONE);   // back in, forget it
  CHECK_EQ(st.update(false, 5), CTV1_FENCE_NONE);
  CHECK_EQ(st.update(false, 8), CTV1_FENCE_LEFT);  // second in a row
  CHECK(!st.inside);
  CHECK_EQ(st.update(true, 10), CTV1_FENCE_NONE);
  CHECK_EQ(st.update(true, 30), CTV1_FENCE_RETURNED);
  Ctv1FenceState st2;
  CHECK_EQ(st2.update(false, 60), CTV1_FENCE_LEFT);  // one fix far outside is enough
}

static void test_power() {
  CHECK_EQ(ctv1_batt_percent(4250), 100);
  CHECK_EQ(ctv1_batt_percent(2900), 0);
  CHECK_EQ(ctv1_batt_percent(3820), 50);
  CHECK_EQ(ctv1_batt_percent(3960), 75);

  Ctv1ScheduleIn in = {80, false, false, false, 600};
  CHECK_EQ(ctv1_next_interval_s(in), 600);
  in.fast_mode = true;
  CHECK_EQ(ctv1_next_interval_s(in), 120);
  in.fast_mode = false;
  in.outside_fence = true;
  CHECK_EQ(ctv1_next_interval_s(in), 120);
  in.outside_fence = false;
  in.batt_pct = 15;
  CHECK_EQ(ctv1_next_interval_s(in), 1800);
  in.batt_pct = 5;
  CHECK_EQ(ctv1_next_interval_s(in), 3600);
  in.outside_fence = true;
  CHECK_EQ(ctv1_next_interval_s(in), 600);  // still reports a lost animal on a nearly flat battery
  CHECK(ctv1_jitter_s(123456789u, 30) <= 30);
  CHECK_EQ(ctv1_jitter_s(5, 0), 0);
}

static void test_gwlink() {
  uint8_t pkt[3] = {0x01, 0xab, 0xff};
  char line[64];
  CHECK(ctv1_gw_format_packet(line, sizeof(line), "UP", -97, -3, pkt, 3) > 0);
  CHECK(strcmp(line, "UP,-97,-3,01abff") == 0);
  CHECK_EQ(ctv1_gw_format_packet(line, 10, "UP", -97, -3, pkt, 3), 0);  // too small

  uint8_t out[4];
  CHECK_EQ(ctv1_hex_decode("01ABff\r\n", out, sizeof(out)), 3);
  CHECK_EQ(out[1], 0xab);
  CHECK_EQ(ctv1_hex_decode("01a", out, sizeof(out)), 0);
  CHECK_EQ(ctv1_hex_decode("zz", out, sizeof(out)), 0);

  Ctv1GwCommand c;
  CHECK(ctv1_gw_parse_cmd("CMD,7,1,30\r\n", c));
  CHECK_EQ(c.tag_id, 7);
  CHECK_EQ(c.cmd, 1);
  CHECK_EQ(c.arg, 30);
  CHECK(!ctv1_gw_parse_cmd("CMD,7,9,30", c));    // unknown command
  CHECK(!ctv1_gw_parse_cmd("CMD,7,1,300", c));   // arg out of range
  CHECK(!ctv1_gw_parse_cmd("CMD,7,1", c));
  CHECK(!ctv1_gw_parse_cmd("CMD,7,1,30x", c));
  CHECK(!ctv1_gw_parse_cmd("PING", c));
}

static void test_json() {
  uint8_t key[16] = {0}, raw[20];
  Ctv1Uplink up = {CTV1_UP_POSITION, 7, 201, 108512345, -762712345, 3910, CTV1_FLAG_FIX_OK | CTV1_FLAG_OUTSIDE, 9, 3, 42};
  ctv1_encode_uplink(up, key, raw);
  char js[400];
  size_t n = ctv1_json_uplink(js, sizeof(js), "home", up, -97, -3, 1792600000LL, raw, 20);
  CHECK(n > 0);
  CHECK(strstr(js, "\"tag\":7,\"seq\":201,\"type\":\"position\"") != nullptr);
  CHECK(strstr(js, "\"lat\":10.8512345,\"lon\":-76.2712345") != nullptr);
  CHECK(strstr(js, "\"flags\":[\"fix\",\"outside\"]") != nullptr);
  CHECK(strstr(js, "\"hdop\":1.5,") != nullptr);
  CHECK(strstr(js, "\"rssi\":-97,\"snr\":-3,\"ts\":1792600000,") != nullptr);
  CHECK(strstr(js, "\"batt_pct\":68") != nullptr);
  CHECK(strstr(js, "\"raw\":\"010700c9") != nullptr);
  CHECK_EQ(ctv1_json_uplink(js, 100, "home", up, -97, -3, 0, raw, 20), 0);  // too small
  ctv1_json_uplink(js, sizeof(js), "home", up, -97, -3, 0, raw, 20);
  CHECK(strstr(js, "\"ts\":null") != nullptr);

  char e7[16];
  ctv1_format_e7(e7, sizeof(e7), -5);
  CHECK(strcmp(e7, "-0.0000005") == 0);

  Ctv1GwCommand c;
  CHECK(ctv1_json_parse_down("{\"tag\":7,\"cmd\":\"buzz\",\"arg\":20}", c));
  CHECK_EQ(c.tag_id, 7);
  CHECK_EQ(c.cmd, CTV1_CMD_BUZZ);
  CHECK_EQ(c.arg, 20);
  CHECK(ctv1_json_parse_down("{ \"cmd\" : \"buzz\" , \"tag\" : 12 }", c));  // default 30 s
  CHECK_EQ(c.arg, 30);
  CHECK(ctv1_json_parse_down("{\"tag\":3,\"cmd\":3,\"arg\":2}", c));
  CHECK_EQ(c.cmd, CTV1_CMD_FAST);
  CHECK(ctv1_json_parse_down("{\"tag\":3,\"cmd\":\"interval\",\"arg\":5}", c));
  CHECK_EQ(c.cmd, CTV1_CMD_SET_INTERVAL);
  CHECK(!ctv1_json_parse_down("{\"tag\":3,\"cmd\":\"explode\"}", c));
  CHECK(!ctv1_json_parse_down("{\"cmd\":\"buzz\"}", c));
  CHECK(!ctv1_json_parse_down("{\"tag\":3,\"cmd\":\"buzz\",\"arg\":999}", c));
  CHECK(!ctv1_json_parse_down("{\"tag\":70000,\"cmd\":1}", c));

  Ctv1Point pts[8];
  CHECK_EQ(ctv1_json_parse_fence("{\"name\":\"Main\",\"points\":[[10.85162,76.26634], [10.8518,76.27082],[10.85018 , 76.271556],[-33.8688,-151.2093]]}", pts, 8), 4);
  CHECK_EQ(pts[0].lat_e7, 108516200);
  CHECK_EQ(pts[2].lon_e7, 762715560);
  CHECK_EQ(pts[3].lat_e7, -338688000);
  CHECK_EQ(pts[3].lon_e7, -1512093000);
  CHECK_EQ(ctv1_json_parse_fence("[[1,2],[3,4],[5,6]]", pts, 8), 3);
  CHECK_EQ(ctv1_json_parse_fence("[[1,2],[3,4]]", pts, 8), 0);              // needs 3 corners
  CHECK_EQ(ctv1_json_parse_fence("[[1,2],[3,4],[5,6]]", pts, 2), 0);        // too many for the buffer
  CHECK_EQ(ctv1_json_parse_fence("[[1,2],[3,x],[5,6]]", pts, 8), 0);
  CHECK_EQ(ctv1_json_parse_fence("[[91,2],[3,4],[5,6]]", pts, 8), 0);       // latitude out of range
  CHECK_EQ(ctv1_json_parse_fence("{\"points\":[[1,2],[3,4],[5,6]", pts, 8), 0);  // truncated
}

static void test_math() {
  double worst_s = 0, worst_a = 0;
  for (double d = -720; d <= 720; d += 0.37) {
    double e = fabs(ctv1_sin_deg((float)d) - sin(d * M_PI / 180));
    if (e > worst_s) worst_s = e;
    e = fabs(ctv1_cos_deg((float)d) - cos(d * M_PI / 180));
    if (e > worst_s) worst_s = e;
  }
  for (double a = 0; a < 360; a += 0.53) {
    static const double radii[] = {0.001, 1.0, 1234.5};
    for (double r : radii) {
      double y = r * sin(a * M_PI / 180), x = r * cos(a * M_PI / 180);
      double e = fabs(ctv1_atan2_deg((float)y, (float)x) - atan2(y, x) * 180 / M_PI);
      if (e > 180) e = 360 - e;
      if (e > worst_a) worst_a = e;
    }
  }
  CHECK(worst_s < 2e-4);
  CHECK(worst_a < 0.01);
  CHECK(fabs(ctv1_atan_deg(1e6f) - 90) < 0.01);
  CHECK_EQ((int)ctv1_atan2_deg(0, 0), 0);
}

static void test_settings() {
  CHECK_EQ(ctv1_crc32((const uint8_t *)"123456789", 9), 0xCBF43926u);  // standard check value

  Ctv1Settings s = {};
  s.tag_id = 7;
  for (int i = 0; i < 16; i++) s.farm_key[i] = (uint8_t)(i * 17);
  s.freq_hz = 865062500;
  s.sf = 9;
  s.tx_dbm = 14;
  s.interval_s = 600;
  s.mag_offset[0] = -120;
  s.mag_offset[2] = 455;
  s.mag_calibrated = true;
  uint8_t blob[CTV1_SETTINGS_BLOB_LEN];
  ctv1_settings_pack(s, blob);
  Ctv1Settings t = {};
  CHECK(ctv1_settings_unpack(blob, sizeof(blob), t));
  CHECK_EQ(t.tag_id, 7);
  CHECK_EQ(t.freq_hz, 865062500);
  CHECK_EQ(t.mag_offset[0], -120);
  CHECK_EQ(t.mag_offset[2], 455);
  CHECK(t.mag_calibrated);
  CHECK(memcmp(t.farm_key, s.farm_key, 16) == 0);

  blob[10] ^= 1;  // corrupted flash
  CHECK(!ctv1_settings_unpack(blob, sizeof(blob), t));
  uint8_t blank[CTV1_SETTINGS_BLOB_LEN];
  memset(blank, 0xFF, sizeof(blank));  // erased flash
  CHECK(!ctv1_settings_unpack(blank, sizeof(blank), t));

  CHECK_EQ(ctv1_settings_set(s, "id", "12"), CTV1_SET_OK);
  CHECK_EQ(s.tag_id, 12);
  CHECK_EQ(ctv1_settings_set(s, "ID", "0"), CTV1_SET_BAD_VALUE);
  CHECK_EQ(ctv1_settings_set(s, "ID", "12a"), CTV1_SET_BAD_VALUE);
  CHECK_EQ(ctv1_settings_set(s, "KEY", "00112233445566778899aabbccddeeff"), CTV1_SET_OK);
  CHECK_EQ(s.farm_key[15], 0xff);
  CHECK_EQ(ctv1_settings_set(s, "KEY", "0011"), CTV1_SET_BAD_VALUE);
  CHECK_EQ(ctv1_settings_set(s, "FREQ", "868100000"), CTV1_SET_OK);
  CHECK_EQ(ctv1_settings_set(s, "FREQ", "2400000000"), CTV1_SET_BAD_VALUE);
  CHECK_EQ(ctv1_settings_set(s, "FREQ", "970000000"), CTV1_SET_BAD_VALUE);  // outside the radio's 150-960 MHz
  CHECK_EQ(ctv1_settings_set(s, "SF", "13"), CTV1_SET_BAD_VALUE);
  CHECK_EQ(ctv1_settings_set(s, "PWR", "20"), CTV1_SET_OK);
  CHECK_EQ(ctv1_settings_set(s, "INT", "30"), CTV1_SET_BAD_VALUE);
  CHECK_EQ(ctv1_settings_set(s, "INT", "300"), CTV1_SET_OK);
  CHECK_EQ(ctv1_settings_set(s, "COLOUR", "red"), CTV1_SET_UNKNOWN);
  CHECK_EQ(s.sf, 9);  // unchanged by the rejected value

  char d[120];
  ctv1_settings_describe(s, d, sizeof(d));
  CHECK(strstr(d, "ID=12 FREQ=868100000 SF=9 PWR=20 INT=300 KEY=set") != nullptr);
  CHECK(strstr(d, "0011") == nullptr);  // the key itself is never printed
}

static void test_gps_policy() {
  uint16_t to;
  Ctv1GpsPolicyIn in = {false, false, 0, 0, 0, 1};
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_RUN);  // first fix ever
  CHECK_EQ(to, 180);
  in.ever_fixed = true;
  in.activity_since_fix = 3;
  in.secs_since_fix = 600;
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_SKIP_STILL);  // lying down
  in.secs_since_fix = 1800;
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_RUN);  // too old, refresh anyway
  CHECK_EQ(to, 45);
  in.secs_since_fix = 600;
  in.activity_since_fix = 50;
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_RUN);  // walked
  in.fails_in_row = 3;
  in.report_no = 5;
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_SKIP_BACKOFF);  // in a shed
  in.report_no = 8;
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_RUN);
  CHECK_EQ(to, 90);
  in.report_no = 5;
  in.urgent = true;
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_RUN);  // lost animal: always try
  in.activity_since_fix = 0;
  CHECK_EQ(ctv1_gps_decide(in, to), CTV1_GPS_RUN);
}

static void test_ubx() {
  uint8_t f[64];
  CHECK_EQ(ctv1_ubx_frame(0x0A, 0x04, nullptr, 0, f, sizeof(f)), 8);  // UBX-MON-VER poll
  CHECK_EQ(f[6], 0x0E);
  CHECK_EQ(f[7], 0x34);
  size_t n = ctv1_ubx_nmea_only_gga_rmc(f, sizeof(f));
  CHECK_EQ(n, 42);
  CHECK_EQ(f[2], 0x06);
  CHECK_EQ(f[3], 0x8A);
  CHECK_EQ(f[4], 34);
  CHECK_EQ(f[7], 0x03);                                         // RAM + BBR
  CHECK(f[10] == 0xbb && f[11] == 0x00 && f[12] == 0x91 && f[13] == 0x20 && f[14] == 1);  // GGA on
  CHECK_EQ(ctv1_ubx_nmea_only_gga_rmc(f, 20), 0);
}

// Earth field / gravity rotated into the body frame (yaw psi, pitch theta, roll phi; AN4248 order).
static void to_body(const double v[3], double psi, double th, double ph, float out[3]) {
  double c = cos(psi), s = sin(psi);
  double a[3] = {c * v[0] + s * v[1], -s * v[0] + c * v[1], v[2]};
  c = cos(th), s = sin(th);
  double b[3] = {c * a[0] - s * a[2], a[1], s * a[0] + c * a[2]};
  c = cos(ph), s = sin(ph);
  out[0] = (float)b[0];
  out[1] = (float)(c * b[1] + s * b[2]);
  out[2] = (float)(-s * b[1] + c * b[2]);
}

static void test_compass() {
  const double D = M_PI / 180, field[3] = {0.30, 0.0, 0.25}, grav[3] = {0, 0, 1};
  const double cases[][3] = {{0, 0, 0}, {90, 0, 0}, {225, 0, 0}, {30, 20, -15}, {300, -25, 30}, {135, 10, 40}};
  for (auto &k : cases) {
    float a[3], m[3];
    to_body(grav, k[0] * D, k[1] * D, k[2] * D, a);
    to_body(field, k[0] * D, k[1] * D, k[2] * D, m);
    int h = ctv1_compass_heading(a[0], a[1], a[2], m[0], m[1], m[2]);
    int err = abs(h - (int)k[0]);
    if (err > 180) err = 360 - err;
    CHECK(err <= 1);
  }
  CHECK_EQ(ctv1_relative_bearing(90, 80, 10), 0);   // facing it
  CHECK_EQ(ctv1_relative_bearing(10, 350, 0), 20);  // a little to the right
  CHECK_EQ(ctv1_relative_bearing(270, 0, 0), 270);  // to the left

  Ctv1MagCal cal;
  cal.reset();
  CHECK(!cal.good());
  for (int i = 0; i < 360; i += 5) {
    double r = i * D;
    cal.add((int16_t)(100 + 400 * cos(r)), (int16_t)(-50 + 400 * sin(r)), (int16_t)(30 + 300 * sin(r * 2)));
  }
  CHECK(cal.good());
  int16_t off[3];
  cal.offset(off);
  CHECK(abs(off[0] - 100) <= 2 && abs(off[1] + 50) <= 2 && abs(off[2] - 30) <= 2);
}

// Writes the screen as a PBM image when CTV1_SCREENS=<dir> is set, for a visual check.
static void save_screen(const Ctv1Gfx &g, const char *name) {
  const char *dir = getenv("CTV1_SCREENS");
  if (!dir) return;
  char path[256];
  snprintf(path, sizeof(path), "%s/%s.pbm", dir, name);
  FILE *f = fopen(path, "w");
  if (!f) return;
  fprintf(f, "P1\n%d %d\n", CTV1_GFX_W, CTV1_GFX_H);
  for (int y = 0; y < CTV1_GFX_H; y++) {
    for (int x = 0; x < CTV1_GFX_W; x++) fputc(g.get(x, y) ? '1' : '0', f);
    fputc('\n', f);
  }
  fclose(f);
}

static void test_gfx() {
  static Ctv1Gfx g;
  g.clear();
  g.pixel(3, 9);
  CHECK(g.get(3, 9));
  CHECK_EQ(g.buf[128 + 3], 0x02);  // page 1, bit 1
  g.pixel(-1, 0);
  g.pixel(200, 70);  // ignored, no crash
  CHECK_EQ(Ctv1Gfx::text_width("ab"), 11);
  g.clear();
  CHECK_EQ(g.text(0, 0, "A"), 6);
  CHECK(g.get(2, 0) && !g.get(0, 0));  // 'A' apex is in the middle column

  Ctv1FindView v = {};
  v.tag_id = 7; v.tag_index = 2; v.tag_count = 8; v.tag_batt_pct = 74; v.tag_has_pos = true;
  v.age_s = 130; v.rssi = -96; v.have_own_fix = true; v.own_sats = 9; v.distance_m = 312;
  v.bearing_true = 40; v.have_heading = true; v.arrow_deg = 35;
  ctv1_draw_find(g, v);
  save_screen(g, "1-find");
  int lit = 0;
  for (auto b : g.buf) lit += __builtin_popcount(b);
  CHECK(lit > 300);

  v.distance_m = 2450; v.cmd = CTV1_HH_BUZZ_WAITING; v.arrow_deg = 250;
  ctv1_draw_find(g, v);
  save_screen(g, "2-far-buzz");
  v.distance_m = 18; v.cmd = CTV1_HH_BUZZ_SENT; v.age_s = 4; v.arrow_deg = 0;
  ctv1_draw_find(g, v);
  save_screen(g, "3-close-buzzing");
  v.cmd = CTV1_HH_CMD_NONE; v.have_heading = false; v.tag_outside = true; v.distance_m = 640; v.bearing_true = 120; v.age_s = 4000;
  ctv1_draw_find(g, v);
  save_screen(g, "4-no-compass-outside");
  v.have_own_fix = false;
  ctv1_draw_find(g, v);
  save_screen(g, "5-no-own-gps");
  ctv1_draw_message(g, "CTv1 HANDHELD", "Listening for tags", "on 865.06 MHz", "none heard yet");
  save_screen(g, "6-waiting");
  ctv1_draw_message(g, "COMPASS", "Turn me slowly", "in every direction", "12 s left");
  save_screen(g, "7-calibrate");

  char age[8];
  ctv1_format_age(3, age, sizeof(age));
  CHECK(strcmp(age, "now") == 0);
  ctv1_format_age(3725, age, sizeof(age));
  CHECK(strcmp(age, "1h02") == 0);
  CHECK(strcmp(ctv1_compass_point(359), "N") == 0);
  CHECK(strcmp(ctv1_compass_point(200), "S") == 0);
}

int main() {
  struct { const char *name; void (*fn)(); } tests[] = {
    {"aes", test_aes}, {"cmac", test_cmac}, {"protocol", test_protocol}, {"nmea", test_nmea},
    {"geo", test_geo}, {"power", test_power}, {"gwlink", test_gwlink}, {"json", test_json},
    {"math", test_math}, {"settings", test_settings}, {"gps", test_gps_policy}, {"ubx", test_ubx}, {"compass", test_compass}, {"gfx", test_gfx},
  };
  for (auto &t : tests) {
    int before = failures;
    t.fn();
    printf("%-9s %s\n", t.name, failures == before ? "ok" : "FAILED");
  }
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
