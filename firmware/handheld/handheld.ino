// CTv1 handheld beacon: points you to a chosen collar and makes it buzz. Works with no phone or internet.
// Board: RAK3172 (RUI3 core 4.x) + MAX-M10S GPS + LSM303AGR compass + 1.3" OLED + 3 buttons.
//
// It listens to every tag report on the farm channel. When you ask a collar to buzz, the command
// goes out in that collar's receive window right after its next report (ctv1_handheld_reply_delay_ms),
// after the home beacon's own ACK, so both can answer without colliding.
//
// Buttons:  < / >  choose collar     OK  buzz (press again to cancel)     hold OK  2-minute reports for 1 h
//           hold < and > together for 2 s: calibrate the compass (turn slowly in every direction)
#include <Wire.h>
#include <CTv1.h>
#include <ctv1_rui3.h>
#include "config.h"
#include "buttons.h"
#include "oled.h"

#if DEBUG_PRINT
#define LOG(...) Serial.printf(__VA_ARGS__)
#else
#define LOG(...)
#endif

struct TagEntry {
  bool used;
  Ctv1Uplink last;
  int16_t rssi;
  uint32_t rx_ms;
  bool has_pos;
  Ctv1CmdState cmd;
  uint32_t cmd_sent_ms;
};
static TagEntry tags[MAX_TAGS];
static uint8_t selected = 0;  // index into tags[]

struct PendingAck {
  bool armed;
  uint16_t tag_id;
  uint8_t seq, cmd, arg;
  uint32_t due_ms;
};
static PendingAck pending = {};

struct RxPacket {
  uint8_t buf[32];
  uint8_t len;
  int16_t rssi;
  uint32_t ms;
};
static RxPacket rx_queue[4];
static volatile uint8_t rx_head = 0, rx_tail = 0;
static volatile bool tx_done = false;

static Oled oled;
static Ctv1Gfx gfx;
static Ctv1Nmea nmea;
static uint32_t own_fix_ms = 0;
static bool compass_ok = false;
static float hx = 0, hy = 0;  // low-passed heading as a unit vector
static uint16_t heading = 0;
static uint32_t vib_until = 0;
static bool calibrating = false;
static uint32_t cal_end_ms = 0;
static Ctv1MagCal cal;
static const char *notice = nullptr;  // short full-screen message
static uint32_t notice_until = 0;

// ---------- radio ----------
void on_lora_recv(rui_lora_p2p_recv_t d) {
  if (d.Status != LORA_P2P_RXDONE || d.BufferSize > sizeof(rx_queue[0].buf)) return;
  uint8_t next = (rx_head + 1) % 4;
  if (next == rx_tail) return;
  RxPacket &p = rx_queue[rx_head];
  memcpy(p.buf, d.Buffer, d.BufferSize);
  p.len = d.BufferSize;
  p.rssi = d.Rssi;
  p.ms = millis();
  rx_head = next;
}
void on_lora_sent() { tx_done = true; }

static void vibrate(uint16_t ms) {
  digitalWrite(PIN_VIB, HIGH);
  vib_until = millis() + ms;
}

static TagEntry *find_tag(uint16_t id, bool create) {
  TagEntry *slot = nullptr;
  for (auto &t : tags) {
    if (t.used && t.last.tag_id == id) return &t;
    if (!t.used && !slot) slot = &t;
  }
  if (!create || !slot) return nullptr;
  memset(slot, 0, sizeof(*slot));
  slot->used = true;
  slot->last.tag_id = id;
  return slot;
}

static void handle_packet(const RxPacket &p) {
  if (p.len != CTV1_UPLINK_LEN) return;  // ACKs from the home beacon are 10 bytes
  uint16_t id;
  uint8_t key[16];
  Ctv1Uplink up;
  if (!ctv1_peek_tag_id(p.buf, p.len, id)) return;
  ctv1_derive_tag_key(ctv1_cfg.farm_key, id, key);
  if (!ctv1_check_mic(p.buf, p.len, key) || !ctv1_decode_uplink(p.buf, p.len, up)) return;

  bool is_new = find_tag(id, false) == nullptr;
  TagEntry *t = find_tag(id, true);
  if (!t) return;
  bool was_outside = t->last.flags & CTV1_FLAG_OUTSIDE;
  bool repeat = !is_new && t->last.seq == up.seq;
  Ctv1Point keep = {t->last.lat_e7, t->last.lon_e7};
  t->last = up;
  if (!(up.flags & (CTV1_FLAG_FIX_OK | CTV1_FLAG_STILL)) && t->has_pos) {  // no fix: keep the last known spot
    t->last.lat_e7 = keep.lat_e7;
    t->last.lon_e7 = keep.lon_e7;
  }
  t->has_pos = t->has_pos || (up.flags & (CTV1_FLAG_FIX_OK | CTV1_FLAG_STILL));
  t->rssi = p.rssi;
  t->rx_ms = p.ms;
  LOG("tag %u seq %u rssi %d%s\r\n", id, up.seq, p.rssi, repeat ? " (repeat)" : "");

  if (is_new) vibrate(80);
  if ((up.flags & CTV1_FLAG_OUTSIDE) && !was_outside) vibrate(600);

  if ((t->cmd == CTV1_HH_BUZZ_WAITING || t->cmd == CTV1_HH_FAST_WAITING)) {
    pending.armed = true;
    pending.tag_id = id;
    pending.seq = up.seq;
    pending.cmd = t->cmd == CTV1_HH_BUZZ_WAITING ? CTV1_CMD_BUZZ : CTV1_CMD_FAST;
    pending.arg = t->cmd == CTV1_HH_BUZZ_WAITING ? 30 : FAST_HOURS;
    pending.due_ms = p.ms + ctv1_handheld_reply_delay_ms(ctv1_cfg.sf);  // after the home beacon's ACK
  }
}

static void send_pending_ack() {
  if (!pending.armed || (int32_t)(millis() - pending.due_ms) < 0) return;
  pending.armed = false;
  uint8_t key[16], pkt[CTV1_DOWNLINK_LEN];
  ctv1_derive_tag_key(ctv1_cfg.farm_key, pending.tag_id, key);
  Ctv1Downlink dn = {CTV1_DOWN_ACK_CMD, pending.tag_id, pending.seq, pending.cmd, pending.arg};
  ctv1_encode_downlink(dn, key, pkt);
  tx_done = false;
  bool ok = api.lora.psend(sizeof(pkt), pkt);
  uint32_t start = millis();
  while (ok && !tx_done && millis() - start < 1000) delay(1);
  api.lora.precv(65533);
  TagEntry *t = find_tag(pending.tag_id, false);
  if (t && ok) {
    t->cmd = pending.cmd == CTV1_CMD_BUZZ ? CTV1_HH_BUZZ_SENT : CTV1_HH_FAST_SENT;
    t->cmd_sent_ms = millis();
    vibrate(250);
  }
  LOG("sent %s to tag %u: %s\r\n", pending.cmd == CTV1_CMD_BUZZ ? "buzz" : "fast", pending.tag_id, ok ? "ok" : "FAILED");
}

// ---------- GPS ----------
static void gps_poll() {
  static bool ubx_sent = false;
  while (Serial1.available()) {
    if (!ubx_sent) {
      uint8_t ubx[48];
      Serial1.write(ubx, ctv1_ubx_nmea_only_gga_rmc(ubx, sizeof(ubx)));
      ubx_sent = true;
    }
    if (nmea.feed((char)Serial1.read()) && nmea.fix().valid) own_fix_ms = millis();
  }
}
static bool own_fix() { return own_fix_ms && millis() - own_fix_ms < OWN_FIX_MAX_AGE_MS; }

// ---------- compass (LSM303AGR) ----------
static void i2c_write(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

static bool i2c_read(uint8_t addr, uint8_t reg, uint8_t *out, uint8_t n) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission() != 0 || Wire.requestFrom(addr, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) out[i] = (uint8_t)Wire.read();
  return true;
}

static bool compass_begin() {
  uint8_t who[1];
  if (!i2c_read(ACC_ADDR, 0x0F, who, 1) || who[0] != 0x33) return false;  // WHO_AM_I_A
  if (!i2c_read(MAG_ADDR, 0x4F, who, 1) || who[0] != 0x40) return false;  // WHO_AM_I_M
  i2c_write(ACC_ADDR, 0x20, 0x57);  // CTRL_REG1_A: 100 Hz, X/Y/Z on
  i2c_write(ACC_ADDR, 0x23, 0x80);  // CTRL_REG4_A: block data update, +/-2 g
  i2c_write(MAG_ADDR, 0x60, 0x80);  // CFG_REG_A_M: temperature compensation, 10 Hz, continuous
  i2c_write(MAG_ADDR, 0x62, 0x10);  // CFG_REG_C_M: block data update
  return true;
}

// Reads both sensors and maps them to body axes (x forward, y right, z down).
static bool compass_read(float acc[3], int16_t mag_raw[3]) {
  uint8_t a[6], m[6];
  if (!i2c_read(ACC_ADDR, 0x28 | 0x80, a, 6) || !i2c_read(MAG_ADDR, 0x68, m, 6)) return false;
  int16_t ar[3], mr[3];
  for (int i = 0; i < 3; i++) {
    ar[i] = (int16_t)(a[2 * i] | (a[2 * i + 1] << 8)) >> 6;  // 10-bit, left aligned
    mr[i] = (int16_t)(m[2 * i] | (m[2 * i + 1] << 8));
  }
  const int axis[3] = {AXIS_FWD, AXIS_RIGHT, AXIS_DOWN}, sign[3] = {SIGN_FWD, SIGN_RIGHT, SIGN_DOWN};
  for (int i = 0; i < 3; i++) {
    acc[i] = -(float)(sign[i] * ar[axis[i]]);  // sensor feels the table pushing up; gravity points the other way
    mag_raw[i] = (int16_t)(sign[i] * mr[axis[i]]);
  }
  return true;
}

static void compass_poll() {
  static uint32_t last = 0;
  if (millis() - last < 100) return;
  last = millis();
  float acc[3];
  int16_t mag[3];
  if (!compass_read(acc, mag)) return;
  if (calibrating) {
    cal.add(mag[0], mag[1], mag[2]);
    return;
  }
  if (!ctv1_cfg.mag_calibrated) return;
  float m[3];
  for (int i = 0; i < 3; i++) m[i] = (float)(mag[i] - ctv1_cfg.mag_offset[i]);
  uint16_t h = ctv1_compass_heading(acc[0], acc[1], acc[2], m[0], m[1], m[2]);
  hx = 0.75f * hx + 0.25f * ctv1_cos_deg(h);  // smooth without jumping at 359 -> 0
  hy = 0.75f * hy + 0.25f * ctv1_sin_deg(h);
  float deg = ctv1_atan2_deg(hy, hx);
  heading = (uint16_t)(deg < 0 ? deg + 360.0f : deg) % 360;
}

static void calibration_poll() {
  if (!calibrating || (int32_t)(millis() - cal_end_ms) < 0) return;
  calibrating = false;
  if (cal.good()) {
    cal.offset(ctv1_cfg.mag_offset);
    ctv1_cfg.mag_calibrated = true;
    ctv1_cfg_save();
    notice = "Compass ready";
    vibrate(300);
  } else {
    notice = "Try again: turn more";
  }
  notice_until = millis() + 2500;
  LOG("compass calibration %s, offset %d %d %d\r\n", cal.good() ? "ok" : "failed", ctv1_cfg.mag_offset[0],
      ctv1_cfg.mag_offset[1], ctv1_cfg.mag_offset[2]);
}

// ---------- buttons ----------
static Button btn_prev = {PIN_BTN_PREV}, btn_ok = {PIN_BTN_OK}, btn_next = {PIN_BTN_NEXT};

static int tag_count() {
  int n = 0;
  for (auto &t : tags) n += t.used;
  return n;
}

static void step_selection(int dir) {
  for (int i = 1; i <= MAX_TAGS; i++) {
    int j = (selected + dir * i + MAX_TAGS * 2) % MAX_TAGS;
    if (tags[j].used) {
      selected = (uint8_t)j;
      return;
    }
  }
}

static void buttons_poll() {
  Press p = button_poll(btn_prev), o = button_poll(btn_ok), n = button_poll(btn_next);
  if (btn_prev.down && btn_next.down && millis() - btn_prev.since > CAL_HOLD_MS && millis() - btn_next.since > CAL_HOLD_MS &&
      !calibrating) {
    calibrating = true;
    cal.reset();
    cal_end_ms = millis() + CAL_SECONDS * 1000UL;
    btn_prev.long_done = btn_next.long_done = true;  // don't also change the selection
    vibrate(150);
    return;
  }
  if (calibrating) return;
  if (p == SHORT) step_selection(-1);
  if (n == SHORT) step_selection(1);
  TagEntry &t = tags[selected];
  if (!t.used) return;
  if (o == SHORT) {
    t.cmd = t.cmd == CTV1_HH_BUZZ_WAITING ? CTV1_HH_CMD_NONE : CTV1_HH_BUZZ_WAITING;
    vibrate(60);
  } else if (o == LONG) {
    t.cmd = t.cmd == CTV1_HH_FAST_WAITING ? CTV1_HH_CMD_NONE : CTV1_HH_FAST_WAITING;
    vibrate(120);
  }
}

// ---------- screen ----------
static void draw() {
  static uint32_t last = 0;
  if (millis() - last < 250) return;
  last = millis();
  char l2[24], l3[24];

  if (calibrating) {
    snprintf(l3, sizeof(l3), "%lu s left", (unsigned long)((cal_end_ms - millis()) / 1000 + 1));
    ctv1_draw_message(gfx, "COMPASS", "Turn me slowly", "in every direction", l3);
  } else if (notice && (int32_t)(millis() - notice_until) < 0) {
    ctv1_draw_message(gfx, "CTv1 HANDHELD", notice);
  } else if (!tags[selected].used) {
    snprintf(l2, sizeof(l2), "on %lu.%02lu MHz", (unsigned long)(ctv1_cfg.freq_hz / 1000000),
             (unsigned long)((ctv1_cfg.freq_hz / 10000) % 100));
    snprintf(l3, sizeof(l3), own_fix() ? "my GPS: %u sats" : "my GPS: searching", nmea.fix().sats);
    ctv1_draw_message(gfx, "CTv1 HANDHELD", "Listening for tags", l2, l3);
  } else {
    TagEntry &t = tags[selected];
    if ((t.cmd == CTV1_HH_BUZZ_SENT && millis() - t.cmd_sent_ms > 30000) ||
        (t.cmd == CTV1_HH_FAST_SENT && millis() - t.cmd_sent_ms > 5000))
      t.cmd = CTV1_HH_CMD_NONE;
    Ctv1FindView v = {};
    v.tag_id = t.last.tag_id;
    int idx = 0;
    for (int i = 0; i <= selected; i++) idx += tags[i].used;
    v.tag_index = (uint8_t)idx;
    v.tag_count = (uint8_t)tag_count();
    v.tag_batt_pct = ctv1_batt_percent(t.last.batt_mv);
    v.tag_outside = t.last.flags & CTV1_FLAG_OUTSIDE;
    v.tag_has_pos = t.has_pos;
    v.age_s = (millis() - t.rx_ms) / 1000;
    v.rssi = t.rssi;
    v.have_own_fix = own_fix();
    v.own_sats = nmea.fix().sats;
    if (v.have_own_fix && t.has_pos) {
      Ctv1Point me = {nmea.fix().lat_e7, nmea.fix().lon_e7}, cow = {t.last.lat_e7, t.last.lon_e7};
      v.distance_m = ctv1_distance_m(me, cow);
      v.bearing_true = ctv1_bearing_deg(me, cow);
    }
    v.have_heading = ctv1_cfg.mag_calibrated && compass_ok;
    v.arrow_deg = ctv1_relative_bearing(v.bearing_true, heading, MAG_DECLINATION_DEG);
    v.cmd = t.cmd;
    ctv1_draw_find(gfx, v);
  }
  oled.show(gfx);
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_GPS_EN, OUTPUT);
  digitalWrite(PIN_GPS_EN, HIGH);  // GPS stays on while the handheld is switched on
  pinMode(PIN_VIB, OUTPUT);
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_BTN_PREV, INPUT_PULLUP);
  pinMode(PIN_BTN_OK, INPUT_PULLUP);
  pinMode(PIN_BTN_NEXT, INPUT_PULLUP);

  Ctv1Settings defaults = {};
  const uint8_t default_key[16] = DEFAULT_FARM_KEY;
  memcpy(defaults.farm_key, default_key, 16);
  defaults.tag_id = 1;
  defaults.freq_hz = DEFAULT_FREQ_HZ;
  defaults.sf = DEFAULT_SF;
  defaults.tx_dbm = DEFAULT_TX_DBM;
  defaults.interval_s = 600;
  ctv1_cfg_begin(defaults, false);

  Wire.begin();
  if (!oled.begin()) LOG("display not found at 0x%02x\r\n", OLED_ADDR);
  compass_ok = compass_begin();
  if (!compass_ok) LOG("compass not found\r\n");
  Serial1.begin(GPS_BAUD);

  if (api.lora.nwm.get() != 0) api.lora.nwm.set();  // P2P mode (restarts once)
  bool ok = api.lora.pfreq.set(ctv1_cfg.freq_hz) && api.lora.psf.set(ctv1_cfg.sf) && api.lora.pbw.set(LORA_BW_KHZ) &&
            api.lora.pcr.set(LORA_CR) && api.lora.ppl.set(LORA_PREAMBLE) && api.lora.ptp.set(ctv1_cfg.tx_dbm);
  api.lora.registerPRecvCallback(on_lora_recv);
  api.lora.registerPSendCallback(on_lora_sent);
  api.lora.precv(65533);  // listen all the time, can still transmit

  char info[120];
  ctv1_settings_describe(ctv1_cfg, info, sizeof(info));
  LOG("\r\nCTv1 handheld %s: %s\r\n", ok ? "ready" : "RADIO SETUP FAILED", info);
  if (!ctv1_cfg.mag_calibrated) {
    notice = "Hold < and > to set compass";
    notice_until = millis() + 4000;
  }
  vibrate(150);
}

void loop() {
  while (rx_tail != rx_head) {
    RxPacket p = rx_queue[rx_tail];
    rx_tail = (rx_tail + 1) % 4;
    handle_packet(p);
  }
  send_pending_ack();
  gps_poll();
  compass_poll();
  calibration_poll();
  buttons_poll();
  if ((int32_t)(millis() - vib_until) >= 0) digitalWrite(PIN_VIB, LOW);
  draw();

  static uint32_t last_log = 0;
  if (millis() - last_log > 2000) {
    last_log = millis();
    LOG("hdg %u  gps %s sats %u  tags %d\r\n", heading, own_fix() ? "fix" : "none", nmea.fix().sats, tag_count());
  }
  delay(2);
}
