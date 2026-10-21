#pragma once
// Settings kept in the RAK3172's user flash, so one firmware build fits every device.
// Set them over the programming port with AT commands (see firmware/README.md):
//   AT+CTID=7   AT+CTKEY=<32 hex>   AT+CTFREQ=865062500   AT+CTSF=9   AT+CTPWR=14   AT+CTINT=600
// Stored as a small blob with a magic number and CRC-32; anything invalid falls back to the defaults
// from the sketch's config.h.
#include <stddef.h>
#include <stdint.h>

#define CTV1_SETTINGS_BLOB_LEN 48

struct Ctv1Settings {
  uint16_t tag_id;        // collar number (tag only)
  uint8_t farm_key[16];
  uint32_t freq_hz;
  uint8_t sf;             // 7..12
  uint8_t tx_dbm;         // 2..22
  uint16_t interval_s;    // normal report interval (tag only), 60..7200
  int16_t mag_offset[3];  // compass hard-iron calibration (handheld only)
  bool mag_calibrated;
};

enum Ctv1SettingResult : uint8_t {
  CTV1_SET_OK,
  CTV1_SET_UNKNOWN,    // no such setting
  CTV1_SET_BAD_VALUE,  // wrong format or out of range
};

uint32_t ctv1_crc32(const uint8_t *data, size_t len);

void ctv1_settings_pack(const Ctv1Settings &s, uint8_t out[CTV1_SETTINGS_BLOB_LEN]);
// False if the blob is blank, from another version, corrupted, or holds out-of-range values.
bool ctv1_settings_unpack(const uint8_t *in, size_t len, Ctv1Settings &s);

// name: ID, KEY, FREQ, SF, PWR, INT (case-insensitive). Changes s only when the value is valid.
Ctv1SettingResult ctv1_settings_set(Ctv1Settings &s, const char *name, const char *value);

// Human-readable summary without the key, e.g. "ID=7 FREQ=865062500 SF=9 PWR=14 INT=600 KEY=set".
size_t ctv1_settings_describe(const Ctv1Settings &s, char *out, size_t out_len);
