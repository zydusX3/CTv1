#pragma once
// CTv1 radio packets (LoRa P2P). Layout is documented in docs/01-system-overview.md.
// All multi-byte fields are little-endian.
#include <stddef.h>
#include <stdint.h>

#define CTV1_UPLINK_LEN 20
#define CTV1_DOWNLINK_LEN 10
#define CTV1_MIC_LEN 4

enum Ctv1UplinkType : uint8_t {
  CTV1_UP_POSITION = 0x01,  // fresh GPS fix
  CTV1_UP_NOFIX = 0x02,     // no fix this time; lat/lon hold the last known fix (or 0,0)
};

enum Ctv1DownlinkType : uint8_t {
  CTV1_DOWN_ACK = 0x81,
  CTV1_DOWN_ACK_CMD = 0x82,
};

enum Ctv1Command : uint8_t {
  CTV1_CMD_NONE = 0x00,
  CTV1_CMD_BUZZ = 0x01,          // arg = seconds (1..60)
  CTV1_CMD_SET_INTERVAL = 0x02,  // arg = minutes (1..120)
  CTV1_CMD_FAST = 0x03,          // arg = hours of 2-minute reporting, 0 = stop
};

enum Ctv1Flags : uint8_t {
  CTV1_FLAG_FIX_OK = 1 << 0,
  CTV1_FLAG_MOVING = 1 << 1,
  CTV1_FLAG_LOW_BATT = 1 << 2,
  CTV1_FLAG_CHARGING = 1 << 3,
  CTV1_FLAG_OUTSIDE = 1 << 4,   // tag's own fence check (only if a fence is compiled in)
  CTV1_FLAG_CMD_DONE = 1 << 5,  // the last command received was carried out
  CTV1_FLAG_STILL = 1 << 6,     // GPS skipped: the animal has not moved since the last fix, lat/lon still current
};

struct Ctv1Uplink {
  uint8_t type;
  uint16_t tag_id;
  uint8_t seq;
  int32_t lat_e7;  // degrees * 1e7
  int32_t lon_e7;
  uint16_t batt_mv;  // 2000..4550 representable, 10 mV steps
  uint8_t flags;
  uint8_t sats;      // 0..15
  uint8_t hdop_x2;   // HDOP * 2, 0..15 (15 = 7.5 or worse)
  uint8_t activity;  // accelerometer events since last report, saturates at 255
};

struct Ctv1Downlink {
  uint8_t type;
  uint16_t tag_id;
  uint8_t seq;  // the uplink seq being acknowledged
  uint8_t cmd;
  uint8_t arg;
};

// Time on air in ms for a LoRa packet at 125 kHz, coding rate 4/5, preamble 8, explicit header, CRC on
// (the settings every CTv1 device uses).
uint32_t ctv1_lora_airtime_ms(uint8_t payload_len, uint8_t sf);

// Reply timing after a tag's report, so the home beacon and a handheld never talk over each other:
//   beacon ACK at +CTV1_ACK_DELAY_MS, handheld command after the beacon's ACK has finished,
//   tag listens until the handheld's reply has finished too.
#define CTV1_ACK_DELAY_MS 50
uint32_t ctv1_handheld_reply_delay_ms(uint8_t sf);
uint32_t ctv1_tag_rx_window_ms(uint8_t sf);

// Per-tag key = AES(farm_key, "CT1" | tag_id_le | zeros). The gateway only needs the farm key.
void ctv1_derive_tag_key(const uint8_t farm_key[16], uint16_t tag_id, uint8_t tag_key[16]);

// Build packets including the 4-byte MIC. Returns bytes written.
size_t ctv1_encode_uplink(const Ctv1Uplink &up, const uint8_t tag_key[16], uint8_t out[CTV1_UPLINK_LEN]);
size_t ctv1_encode_downlink(const Ctv1Downlink &dn, const uint8_t tag_key[16], uint8_t out[CTV1_DOWNLINK_LEN]);

// Parse without checking the MIC (use ctv1_peek_tag_id + ctv1_check_mic first). False on bad length/type.
bool ctv1_decode_uplink(const uint8_t *buf, size_t len, Ctv1Uplink &up);
bool ctv1_decode_downlink(const uint8_t *buf, size_t len, Ctv1Downlink &dn);

// Tag id is bytes 1..2 in both directions, so the receiver can pick the key before verifying.
bool ctv1_peek_tag_id(const uint8_t *buf, size_t len, uint16_t &tag_id);

// True if the last 4 bytes are the correct MIC over the rest of the packet.
bool ctv1_check_mic(const uint8_t *buf, size_t len, const uint8_t tag_key[16]);
