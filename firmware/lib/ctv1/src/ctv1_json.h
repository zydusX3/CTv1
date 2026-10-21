#pragma once
// Small JSON helpers for the home beacon's MQTT messages (no heap, no JSON library).
//
//   ctv1/gw/<gw>/up     {"gw":"home","tag":7,"seq":201,"type":"position","lat":10.8512345,"lon":76.2712345,
//                        "batt_mv":3910,"batt_pct":72,"flags":["fix","moving"],"sats":9,"hdop":1.5,
//                        "activity":42,"rssi":-97,"snr":7,"ts":1792600000,"raw":"01..."}
//   ctv1/gw/<gw>/down   {"tag":7,"cmd":"buzz","arg":30}      cmd: buzz | interval | fast | none (or 0-3)
//   ctv1/gw/<gw>/fence  {"name":"Main paddock","points":[[10.85162,76.26634],[10.85180,76.27082],...]}
#include <stddef.h>
#include <stdint.h>
#include "ctv1_geo.h"
#include "ctv1_gwlink.h"
#include "ctv1_protocol.h"

// ts_unix = 0 means "time unknown" and writes "ts":null. Returns length, 0 if out is too small.
size_t ctv1_json_uplink(char *out, size_t out_len, const char *gw_id, const Ctv1Uplink &up, int16_t rssi,
                        int8_t snr, int64_t ts_unix, const uint8_t *raw, size_t raw_len);

// Parses a down command. Returns false if tag or cmd is missing or out of range.
bool ctv1_json_parse_down(const char *json, Ctv1GwCommand &out);

// Parses fence points (decimal degrees). Returns the number of points (3..max), or 0 if invalid.
uint8_t ctv1_json_parse_fence(const char *json, Ctv1Point *pts, uint8_t max);

// Writes degrees * 1e7 as a decimal string ("-33.8688000"). Returns length.
size_t ctv1_format_e7(char *out, size_t out_len, int32_t v);
