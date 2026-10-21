#pragma once
// RAK3172 (RUI3) only: settings in user flash + ATC+ commands to change them.
// Header-only on purpose, so PC tests and the ESP32 build never compile it.
//
//   ATC+CTINFO=?                 show all settings (never the key)
//   ATC+CTID=7      ATC+CTID=?   collar number
//   ATC+CTKEY=<32 hex digits>    farm key (write only)
//   ATC+CTFREQ=865062500         radio frequency in Hz
//   ATC+CTSF=9                   spreading factor 7..12
//   ATC+CTPWR=14                 transmit power dBm 2..22
//   ATC+CTINT=600                normal report interval in seconds (tag)
//   ATZ                          restart to apply radio, ID and key changes
#include <Arduino.h>
#include "ctv1_settings.h"

static Ctv1Settings ctv1_cfg;  // current settings, valid after ctv1_cfg_begin()

static inline bool ctv1_cfg_save() {
  uint8_t blob[CTV1_SETTINGS_BLOB_LEN];
  ctv1_settings_pack(ctv1_cfg, blob);
  return api.system.flash.set(0, blob, sizeof(blob));
}

static int ctv1_at_setting(const char *name, stParam *param, bool readable) {
  if (param->argc == 1 && !strcmp(param->argv[0], "?")) {
    char line[120];
    if (!strcmp(name, "INFO")) {
      ctv1_settings_describe(ctv1_cfg, line, sizeof(line));
    } else if (!strcmp(name, "ID")) {
      snprintf(line, sizeof(line), "%u", ctv1_cfg.tag_id);
    } else if (!strcmp(name, "FREQ")) {
      snprintf(line, sizeof(line), "%lu", (unsigned long)ctv1_cfg.freq_hz);
    } else if (!strcmp(name, "SF")) {
      snprintf(line, sizeof(line), "%u", ctv1_cfg.sf);
    } else if (!strcmp(name, "PWR")) {
      snprintf(line, sizeof(line), "%u", ctv1_cfg.tx_dbm);
    } else if (!strcmp(name, "INT")) {
      snprintf(line, sizeof(line), "%u", ctv1_cfg.interval_s);
    } else {
      return readable ? AT_ERROR : AT_PARAM_ERROR;
    }
    Serial.printf("ATC+CT%s=%s\r\n", name, line);
    return AT_OK;
  }
  if (param->argc != 1 || !strcmp(name, "INFO")) return AT_PARAM_ERROR;
  Ctv1Settings next = ctv1_cfg;
  if (ctv1_settings_set(next, name, param->argv[0]) != CTV1_SET_OK) return AT_PARAM_ERROR;
  ctv1_cfg = next;
  if (!ctv1_cfg_save()) return AT_ERROR;
  Serial.printf("saved. %s\r\n", strcmp(name, "INT") ? "Restart with ATZ to use it." : "Used from the next report.");
  return AT_OK;
}

#define CTV1_AT_HANDLER(NAME, READ) \
  [](SERIAL_PORT, char *, stParam *p) -> int { return ctv1_at_setting(NAME, p, READ); }

// Loads settings from flash (falling back to defaults) and registers the ATC+ commands.
// Returns true if saved settings were found.
static inline bool ctv1_cfg_begin(const Ctv1Settings &defaults, bool tag_commands) {
  uint8_t blob[CTV1_SETTINGS_BLOB_LEN];
  ctv1_cfg = defaults;
  bool loaded = api.system.flash.get(0, blob, sizeof(blob)) && ctv1_settings_unpack(blob, sizeof(blob), ctv1_cfg);
  const unsigned RW = RAK_ATCMD_PERM_WRITE | RAK_ATCMD_PERM_READ;
  api.system.atMode.add((char *)"CTINFO", (char *)"Show CTv1 settings", (char *)"CTINFO", CTV1_AT_HANDLER("INFO", true), RAK_ATCMD_PERM_READ);
  api.system.atMode.add((char *)"CTKEY", (char *)"Farm key, 32 hex digits", (char *)"CTKEY", CTV1_AT_HANDLER("KEY", false), RAK_ATCMD_PERM_WRITE);
  api.system.atMode.add((char *)"CTFREQ", (char *)"Radio frequency in Hz", (char *)"CTFREQ", CTV1_AT_HANDLER("FREQ", true), RW);
  api.system.atMode.add((char *)"CTSF", (char *)"Spreading factor 7-12", (char *)"CTSF", CTV1_AT_HANDLER("SF", true), RW);
  api.system.atMode.add((char *)"CTPWR", (char *)"Transmit power dBm 2-22", (char *)"CTPWR", CTV1_AT_HANDLER("PWR", true), RW);
  if (tag_commands) {
    api.system.atMode.add((char *)"CTID", (char *)"Collar number 1-65535", (char *)"CTID", CTV1_AT_HANDLER("ID", true), RW);
    api.system.atMode.add((char *)"CTINT", (char *)"Report interval s 60-7200", (char *)"CTINT", CTV1_AT_HANDLER("INT", true), RW);
  }
  return loaded;
}
