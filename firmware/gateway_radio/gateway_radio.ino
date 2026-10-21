// CTv1 home beacon, radio side (RAK3172, RUI3 core 4.x).
// Listens all the time, checks each packet's MIC, ACKs the tag (with a queued command if any)
// and passes the packet to the ESP32 / PC as one text line. See lib/ctv1/src/ctv1_gwlink.h.
#include <CTv1.h>
#include "config.h"

#if LINK_ON_USB
#define LINK Serial
#else
#define LINK Serial1
#endif

static const uint8_t farm_key[16] = FARM_KEY;

struct TagEntry {
  uint16_t id;
  bool used;
  bool seen;          // last_seq is valid
  uint8_t last_seq;
  bool cmd_pending;   // command waiting for this tag
  bool cmd_sent;      // ... and it has gone out in at least one ACK
  uint8_t cmd, arg;
};
static TagEntry tags[MAX_TAGS];

struct RxPacket {
  uint8_t buf[32];
  uint8_t len;
  int16_t rssi;
  int8_t snr;
};
static RxPacket rx_queue[4];
static volatile uint8_t rx_head = 0, rx_tail = 0;
static volatile bool tx_done = false;
static uint32_t rx_count = 0, bad_count = 0, last_status_ms = 0;
static char line_in[64];
static uint8_t line_len = 0;

void on_lora_recv(rui_lora_p2p_recv_t d) {
  if (d.Status != LORA_P2P_RXDONE || d.BufferSize > sizeof(rx_queue[0].buf)) return;
  uint8_t next = (rx_head + 1) % 4;
  if (next == rx_tail) return;  // queue full, drop
  RxPacket &p = rx_queue[rx_head];
  memcpy(p.buf, d.Buffer, d.BufferSize);
  p.len = d.BufferSize;
  p.rssi = d.Rssi;
  p.snr = d.Snr;
  rx_head = next;
}
void on_lora_sent() { tx_done = true; }

static TagEntry *find_tag(uint16_t id, bool create) {
  TagEntry *free_slot = nullptr;
  for (auto &t : tags) {
    if (t.used && t.id == id) return &t;
    if (!t.used && !free_slot) free_slot = &t;
  }
  if (!create || !free_slot) return nullptr;
  memset(free_slot, 0, sizeof(*free_slot));
  free_slot->used = true;
  free_slot->id = id;
  return free_slot;
}

static void send_line(const char *kind, const RxPacket &p) {
  char out[96];
  if (ctv1_gw_format_packet(out, sizeof(out), kind, p.rssi, p.snr, p.buf, p.len)) LINK.println(out);
}

static void send_ack(const Ctv1Downlink &dn, const uint8_t key[16]) {
  uint8_t pkt[CTV1_DOWNLINK_LEN];
  ctv1_encode_downlink(dn, key, pkt);
  delay(ACK_DELAY_MS);
  tx_done = false;
  if (api.lora.psend(sizeof(pkt), pkt)) {
    uint32_t start = millis();
    while (!tx_done && millis() - start < 2000) delay(1);
  }
  api.lora.precv(65533);  // back to listening
}

static void handle_packet(const RxPacket &p) {
  rx_count++;
  uint16_t id;
  uint8_t key[16];
  Ctv1Uplink up;
  if (p.len != CTV1_UPLINK_LEN || !ctv1_peek_tag_id(p.buf, p.len, id)) {
    bad_count++;
    send_line("BAD", p);
    return;
  }
  ctv1_derive_tag_key(farm_key, id, key);
  if (!ctv1_check_mic(p.buf, p.len, key) || !ctv1_decode_uplink(p.buf, p.len, up)) {
    bad_count++;
    send_line("BAD", p);
    return;
  }

  TagEntry *t = find_tag(id, true);
  bool dup = t && t->seen && t->last_seq == up.seq;
  if (dup) {
    LINK.printf("DUP,%u,%u\r\n", id, up.seq);
  } else {
    if (t) {
      if (t->cmd_pending && t->cmd_sent && (up.flags & CTV1_FLAG_CMD_DONE)) {
        t->cmd_pending = false;
        LINK.printf("OK,done,%u,%u\r\n", id, t->cmd);
      }
      t->seen = true;
      t->last_seq = up.seq;
    }
    send_line("UP", p);
  }

  Ctv1Downlink dn;
  dn.tag_id = id;
  dn.seq = up.seq;
  dn.type = CTV1_DOWN_ACK;
  dn.cmd = CTV1_CMD_NONE;
  dn.arg = 0;
  if (t && t->cmd_pending) {
    dn.type = CTV1_DOWN_ACK_CMD;
    dn.cmd = t->cmd;
    dn.arg = t->arg;
    t->cmd_sent = true;
  }
  send_ack(dn, key);
}

static void handle_line(const char *line) {
  Ctv1GwCommand c;
  if (ctv1_gw_parse_cmd(line, c)) {
    TagEntry *t = find_tag(c.tag_id, true);
    if (!t) {
      LINK.println("ERR,table full");
      return;
    }
    t->cmd = c.cmd;
    t->arg = c.arg;
    t->cmd_pending = c.cmd != CTV1_CMD_NONE;
    t->cmd_sent = false;
    LINK.printf("OK,queued,%u,%u\r\n", c.tag_id, c.cmd);
  } else if (strcmp(line, "PING") == 0) {
    LINK.println("OK,pong");
  } else if (line[0]) {
    LINK.println("ERR,unknown");
  }
}

static void read_link() {
  while (LINK.available()) {
    char ch = (char)LINK.read();
    if (ch == '\r' || ch == '\n') {
      line_in[line_len] = 0;
      handle_line(line_in);
      line_len = 0;
    } else if (line_len < sizeof(line_in) - 1) {
      line_in[line_len++] = ch;
    }
  }
}

static void send_status() {
  uint8_t queued = 0;
  for (auto &t : tags) queued += t.used && t.cmd_pending;
  LINK.printf("ST,%lu,%lu,%lu,%u\r\n", millis() / 1000, rx_count, bad_count, queued);
}

void setup() {
#if LINK_ON_USB
  Serial.begin(LINK_BAUD, RAK_CUSTOM_MODE);  // plain text link, AT commands off
#else
  Serial.begin(115200);                      // AT console stays available for setup
  Serial1.begin(LINK_BAUD, RAK_CUSTOM_MODE);
#endif
  if (api.lora.nwm.get() != 0) api.lora.nwm.set();  // P2P mode (restarts once)
  bool ok = api.lora.pfreq.set(LORA_FREQ_HZ) && api.lora.psf.set(LORA_SF) && api.lora.pbw.set(LORA_BW_KHZ) &&
            api.lora.pcr.set(LORA_CR) && api.lora.ppl.set(LORA_PREAMBLE) && api.lora.ptp.set(LORA_TX_DBM);
  api.lora.registerPRecvCallback(on_lora_recv);
  api.lora.registerPSendCallback(on_lora_sent);
  api.lora.precv(65533);  // receive continuously, can still transmit ACKs
  LINK.printf("# CTv1 home beacon radio %s, %lu Hz SF%u\r\n", ok ? "ready" : "RADIO SETUP FAILED",
              (unsigned long)LORA_FREQ_HZ, LORA_SF);
}

void loop() {
  while (rx_tail != rx_head) {
    RxPacket p = rx_queue[rx_tail];
    rx_tail = (rx_tail + 1) % 4;
    handle_packet(p);
  }
  read_link();
  if (millis() - last_status_ms >= STATUS_EVERY_MS) {
    last_status_ms = millis();
    send_status();
  }
  delay(2);
}
