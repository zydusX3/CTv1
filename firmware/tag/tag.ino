// CTv1 collar tag: wake, GPS fix, send 20-byte position over LoRa P2P, wait for ACK, sleep.
// Board: RAK3172 (STM32WLE5) with the RAKwireless RUI3 core 4.x. See firmware/README.md.
#include <Wire.h>
#include <CTv1.h>
#include <ctv1_rui3.h>
#include "config.h"

#if DEBUG_PRINT
#define LOG(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

#if FENCE_ENABLED
static const Ctv1Point fence_pts[] = FENCE_POINTS;
static const uint8_t fence_n = sizeof(fence_pts) / sizeof(fence_pts[0]);
#endif

static uint8_t tag_key[16];
static uint8_t seq = 0;
static Ctv1Nmea nmea;
static Ctv1Point last_pos = {0, 0};
static bool ever_fixed = false;
static Ctv1FenceState fence;
static uint32_t activity_since_fix = 0;  // motion events since the last good GPS fix
static uint32_t last_fix_clock_s = 0;
static uint8_t gps_fails = 0;            // GPS attempts in a row without a fix
static uint32_t report_no = 0;
static bool ubx_sent = false;            // GPS told to send only GGA + RMC (kept in its backup RAM)

static uint32_t clock_s = 0;           // seconds since boot, advanced by each scheduled interval
static uint32_t next_interval_s = FIRST_REPORT_S;
static uint32_t fast_until_s = 0;
static bool cmd_done = false;          // tell the gateway the last command was carried out

static volatile bool report_due = false;
static volatile uint16_t activity = 0;
static volatile bool tx_done = false, rx_done = false, rx_failed = false;
static uint8_t rx_buf[32];
static volatile uint8_t rx_len = 0;

// ---------- callbacks (keep them short) ----------
void on_report_timer(void *) { report_due = true; }
void on_motion() { if (activity < 0xffff) activity++; }
void on_lora_sent() { tx_done = true; }
void on_lora_recv(rui_lora_p2p_recv_t d) {
  if (d.Status == LORA_P2P_RXDONE && d.BufferSize <= sizeof(rx_buf)) {
    memcpy(rx_buf, d.Buffer, d.BufferSize);
    rx_len = d.BufferSize;
    rx_done = true;
  } else {
    rx_failed = true;  // timeout or CRC error
  }
}

// ---------- hardware helpers ----------
static void accel_write(uint8_t reg, uint8_t val) {
  Wire.beginTransmission(ACCEL_I2C_ADDR);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static int accel_read(uint8_t reg) {
  Wire.beginTransmission(ACCEL_I2C_ADDR);
  Wire.write(reg);
  if (Wire.endTransmission() != 0) return -1;
  if (Wire.requestFrom(ACCEL_I2C_ADDR, 1) != 1) return -1;
  return Wire.read();
}

static bool accel_setup() {
  Wire.begin();
  if (accel_read(0x0F) != 0x33) return false;  // WHO_AM_I
  accel_write(0x20, 0x2F);             // CTRL_REG1: 10 Hz, low-power mode, X/Y/Z on (~6 uA)
  accel_write(0x21, 0x01);             // CTRL_REG2: high-pass filter on the INT1 path (ignores gravity/tilt)
  accel_write(0x22, 0x40);             // CTRL_REG3: IA1 interrupt on INT1 pin
  accel_write(0x23, 0x00);             // CTRL_REG4: +/-2 g
  accel_write(0x24, 0x00);             // CTRL_REG5: not latched, INT1 pulses while moving
  accel_write(0x32, ACCEL_THRESHOLD);  // INT1_THS
  accel_write(0x33, 0x00);             // INT1_DURATION
  accel_read(0x26);                    // REFERENCE: reset the high-pass filter
  accel_write(0x30, 0x2A);             // INT1_CFG: X, Y or Z above threshold
  pinMode(PIN_ACC_INT, INPUT);
  attachInterrupt(PIN_ACC_INT, on_motion, RISING);
  api.system.sleep.setup(RUI_WAKEUP_RISING_EDGE, PIN_ACC_INT);
  return true;
}

static uint16_t read_batt_mv() {
  uint32_t sum = 0;
  for (int i = 0; i < 8; i++) sum += analogRead(PIN_BATT);
  float mv = (float)(sum / 8) * 3000.0f / 4095.0f * BATT_DIVIDER * BATT_CALIBRATION;
  return (uint16_t)mv;
}

static void gps_power(bool on) {
  if (on) {
    digitalWrite(PIN_GPS_EN, HIGH);
    Serial1.begin(GPS_BAUD);
  } else {
    Serial1.end();  // stop driving TX into an unpowered GPS
    digitalWrite(PIN_GPS_EN, LOW);
  }
}

// Powers the GPS until a good fix or the timeout. Returns true with fix filled in on success.
static bool gps_get_fix(Ctv1Fix &fix, uint16_t timeout_s) {
  uint32_t timeout_ms = 1000UL * timeout_s;
  bool have = false;
  nmea.reset();
  gps_power(true);
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (!ubx_sent && Serial1.available()) {  // GPS is awake: switch off the sentences we don't use
      uint8_t ubx[48];
      size_t n = ctv1_ubx_nmea_only_gga_rmc(ubx, sizeof(ubx));
      Serial1.write(ubx, n);
      ubx_sent = true;
    }
    while (Serial1.available()) {
      if (nmea.feed((char)Serial1.read()) && nmea.fix().valid) {
        fix = nmea.fix();
        have = true;
        if (fix.hdop_x10 <= GPS_GOOD_HDOP_X10 && fix.sats >= 4) goto done;
      }
    }
    delay(5);
  }
done:
  gps_power(false);
  LOG("gps: %s in %lu ms, sats %u hdop %u.%u\r\n", have ? "fix" : "no fix", millis() - start,
      fix.sats, fix.hdop_x10 / 10, fix.hdop_x10 % 10);
  return have;
}

static void buzz(uint8_t seconds) {
  if (seconds > 60) seconds = 60;
  uint32_t start = millis();
  while (millis() - start < 1000UL * seconds) {
    uint32_t beep = millis();
    while (millis() - beep < 400) {  // ~2.7 kHz, the buzzer's loudest tone
      digitalWrite(PIN_BUZZER, HIGH);
      delayMicroseconds(185);
      digitalWrite(PIN_BUZZER, LOW);
      delayMicroseconds(185);
    }
    delay(250);
  }
}

static void blink(uint16_t ms) {
  digitalWrite(PIN_LED, HIGH);
  delay(ms);
  digitalWrite(PIN_LED, LOW);
}

// ---------- radio ----------
static bool radio_setup() {
  if (api.lora.nwm.get() != 0) {
    LOG("switching to LoRa P2P mode (the module restarts once)\r\n");
    api.lora.nwm.set();
  }
  bool ok = api.lora.pfreq.set(ctv1_cfg.freq_hz) && api.lora.psf.set(ctv1_cfg.sf) && api.lora.pbw.set(LORA_BW_KHZ) &&
            api.lora.pcr.set(LORA_CR) && api.lora.ppl.set(LORA_PREAMBLE) && api.lora.ptp.set(ctv1_cfg.tx_dbm);
  api.lora.registerPRecvCallback(on_lora_recv);
  api.lora.registerPSendCallback(on_lora_sent);
  return ok;
}

static bool wait_flag(volatile bool &flag, volatile bool *fail, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (!flag && !(fail && *fail) && millis() - start < timeout_ms) delay(1);
  return flag;
}

// Listens until window_end for a downlink meant for this report. Fills dn on success.
static bool receive_downlink(uint32_t window_end, Ctv1Downlink &dn) {
  while ((int32_t)(window_end - millis()) > 50) {
    rx_done = rx_failed = false;
    api.lora.precv(window_end - millis());
    if (!wait_flag(rx_done, &rx_failed, window_end - millis() + 300)) return false;
    if (ctv1_check_mic(rx_buf, rx_len, tag_key) && ctv1_decode_downlink(rx_buf, rx_len, dn) &&
        dn.tag_id == ctv1_cfg.tag_id && dn.seq == seq)
      return true;
    // someone else's packet: keep listening for the rest of the window
  }
  return false;
}

// Sends one uplink and waits for the ACK. The home beacon answers after ~50 ms; a handheld that
// wants this collar to buzz answers after ~400 ms. So after a plain ACK, keep listening for the
// rest of the window in case a command follows (costs ~1 s at 5 mA per report).
static bool send_and_wait_ack(const uint8_t *pkt, uint8_t len, Ctv1Downlink &dn) {
  tx_done = false;
  if (!api.lora.psend(len, (uint8_t *)pkt)) return false;
  if (!wait_flag(tx_done, nullptr, 5000)) return false;
  uint32_t window_end = millis() + RX_WINDOW_MS;
  if (!receive_downlink(window_end, dn)) return false;
  Ctv1Downlink more;
  if (dn.type == CTV1_DOWN_ACK && receive_downlink(window_end, more) && more.type == CTV1_DOWN_ACK_CMD) dn = more;
  return true;
}

static void handle_command(const Ctv1Downlink &dn) {
  if (dn.type != CTV1_DOWN_ACK_CMD) return;
  LOG("command %u arg %u\r\n", dn.cmd, dn.arg);
  switch (dn.cmd) {
    case CTV1_CMD_BUZZ:
      buzz(dn.arg ? dn.arg : 30);
      break;
    case CTV1_CMD_SET_INTERVAL:
      ctv1_cfg.interval_s = (uint16_t)(60UL * (dn.arg < 1 ? 1 : (dn.arg > 120 ? 120 : dn.arg)));
      ctv1_cfg_save();
      break;
    case CTV1_CMD_FAST:
      fast_until_s = dn.arg ? clock_s + 3600UL * dn.arg : 0;
      break;
    default:
      return;
  }
  cmd_done = true;
}

// ---------- one report cycle ----------
static void do_report() {
  uint16_t batt_mv = read_batt_mv();
  uint8_t batt_pct = ctv1_batt_percent(batt_mv);

  uint16_t act = activity;
  activity = 0;
  activity_since_fix += act;
  bool moving = act >= MOVING_THRESHOLD;

  // GPS only when it can tell us something new (see ctv1_gps_decide)
  Ctv1GpsPolicyIn gp;
  gp.ever_fixed = ever_fixed;
  gp.urgent = !fence.inside || fast_until_s > clock_s;
  gp.activity_since_fix = activity_since_fix > 65535 ? 65535 : (uint16_t)activity_since_fix;
  gp.secs_since_fix = clock_s - last_fix_clock_s;
  gp.fails_in_row = gps_fails;
  gp.report_no = report_no++;
  uint16_t gps_timeout_s;
  Ctv1GpsDecision decision = ctv1_gps_decide(gp, gps_timeout_s);

  Ctv1Fix fix;
  memset(&fix, 0, sizeof(fix));
  bool have_fix = false, still = false;
  if (decision == CTV1_GPS_RUN) {
    have_fix = gps_get_fix(fix, gps_timeout_s);
    if (have_fix) {
      last_pos.lat_e7 = fix.lat_e7;
      last_pos.lon_e7 = fix.lon_e7;
      ever_fixed = true;
      last_fix_clock_s = clock_s;
      activity_since_fix = 0;
      gps_fails = 0;
    } else if (gps_fails < 255) {
      gps_fails++;
    }
  } else if (decision == CTV1_GPS_SKIP_STILL) {
    still = true;  // hasn't moved: the last fix is still where it is
    LOG("gps: skipped, animal still (%lu events since fix)\r\n", activity_since_fix);
  } else {
    LOG("gps: skipped, %u failed attempts in a row\r\n", gps_fails);
  }

  bool outside = false;
#if FENCE_ENABLED
  if (have_fix) {
    bool inside = ctv1_point_in_polygon(last_pos, fence_pts, fence_n);
    uint32_t edge = ctv1_distance_to_edge_m(last_pos, fence_pts, fence_n);
    if (fence.update(inside, edge) != CTV1_FENCE_NONE) LOG("fence: %s\r\n", fence.inside ? "back inside" : "LEFT");
  }
  outside = !fence.inside;
#endif

  Ctv1Uplink up;
  up.type = (have_fix || still) ? CTV1_UP_POSITION : CTV1_UP_NOFIX;
  up.tag_id = ctv1_cfg.tag_id;
  up.seq = seq;
  up.lat_e7 = last_pos.lat_e7;
  up.lon_e7 = last_pos.lon_e7;
  up.batt_mv = batt_mv;
  up.flags = (have_fix ? CTV1_FLAG_FIX_OK : 0) | (moving ? CTV1_FLAG_MOVING : 0) |
             (batt_pct < CTV1_LOW_BATT_PCT ? CTV1_FLAG_LOW_BATT : 0) | (outside ? CTV1_FLAG_OUTSIDE : 0) |
             (cmd_done ? CTV1_FLAG_CMD_DONE : 0) | (still ? CTV1_FLAG_STILL : 0);
  up.sats = fix.sats;
  up.hdop_x2 = (uint8_t)((fix.hdop_x10 + 2) / 5);  // x10 -> x2
  up.activity = act > 255 ? 255 : (uint8_t)act;

  uint8_t pkt[CTV1_UPLINK_LEN];
  ctv1_encode_uplink(up, tag_key, pkt);

  Ctv1Downlink dn;
  bool acked = send_and_wait_ack(pkt, sizeof(pkt), dn);
  if (!acked) {  // one retry with the same seq after a random pause
    LOG("no ACK, retrying\r\n");
    api.system.sleep.all((uint32_t)random(RETRY_MIN_MS, RETRY_MAX_MS));
    acked = send_and_wait_ack(pkt, sizeof(pkt), dn);
  }
  LOG("report seq %u: %s, batt %u mV (%u%%), activity %u\r\n", seq, acked ? "ACK" : "no ACK", batt_mv, batt_pct, act);
  if (acked) {
    cmd_done = false;  // the gateway has now seen the flag
    blink(20);
    handle_command(dn);
  }
  seq++;

  Ctv1ScheduleIn s;
  s.batt_pct = batt_pct;
  s.outside_fence = outside;
  s.moving = moving;
  s.fast_mode = fast_until_s > clock_s;
  s.base_interval_s = ctv1_cfg.interval_s;
  next_interval_s = ctv1_next_interval_s(s) + ctv1_jitter_s((uint32_t)random(0, 0x7fffffff), REPORT_JITTER_S);
}

static void schedule_next() {
  api.system.timer.stop(RAK_TIMER_0);
  api.system.timer.start(RAK_TIMER_0, next_interval_s * 1000UL, NULL);
  LOG("next report in %lu s\r\n", next_interval_s);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, LOW);
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  digitalWrite(PIN_BUZZER, LOW);
  analogReadResolution(12);
  analogReference(AR_INTERNAL_3_0);

  Ctv1Settings defaults = {};
  const uint8_t default_key[16] = DEFAULT_FARM_KEY;
  defaults.tag_id = DEFAULT_TAG_ID;
  memcpy(defaults.farm_key, default_key, 16);
  defaults.freq_hz = DEFAULT_FREQ_HZ;
  defaults.sf = DEFAULT_SF;
  defaults.tx_dbm = DEFAULT_TX_DBM;
  defaults.interval_s = DEFAULT_INTERVAL_S;
  bool saved = ctv1_cfg_begin(defaults, true);

  ctv1_derive_tag_key(ctv1_cfg.farm_key, ctv1_cfg.tag_id, tag_key);
  randomSeed(ctv1_cfg.tag_id * 7919UL + analogRead(PIN_BATT));
  seq = (uint8_t)random(0, 256);  // so a rebooted tag is not mistaken for a repeat

  char info[120];
  ctv1_settings_describe(ctv1_cfg, info, sizeof(info));
  LOG("\r\nCTv1 tag starting, %s settings: %s\r\n", saved ? "saved" : "default", info);
  if (!radio_setup()) LOG("radio setup FAILED\r\n");
  if (!accel_setup()) LOG("accelerometer not found\r\n");

  api.system.timer.create(RAK_TIMER_0, on_report_timer, RAK_TIMER_ONESHOT);
  api.system.lpm.set(1);
  blink(200);
  schedule_next();
}

void loop() {
  if (report_due) {
    report_due = false;
    clock_s += next_interval_s;
    do_report();
    schedule_next();
  }
  api.system.sleep.all();  // until the report timer or a motion interrupt
}
