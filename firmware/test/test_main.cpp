// Host-side unit tests for the shared CTv1 library. Run: make -C firmware/test
#include <stdio.h>
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

int main() {
  struct { const char *name; void (*fn)(); } tests[] = {
    {"aes", test_aes}, {"cmac", test_cmac}, {"protocol", test_protocol}, {"nmea", test_nmea},
    {"geo", test_geo}, {"power", test_power}, {"gwlink", test_gwlink}, {"json", test_json},
  };
  for (auto &t : tests) {
    int before = failures;
    t.fn();
    printf("%-9s %s\n", t.name, failures == before ? "ok" : "FAILED");
  }
  printf("%d checks, %d failed\n", checks, failures);
  return failures ? 1 : 0;
}
