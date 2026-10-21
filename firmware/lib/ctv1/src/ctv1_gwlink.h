#pragma once
// Text line protocol on the UART between the home beacon's RAK3172 and its ESP32-S3.
// One message per line, comma separated, ending in "\n". Easy to read in a serial monitor.
//
// radio -> ESP32
//   UP,<rssi>,<snr>,<hex packet>     verified uplink (new seq)
//   DUP,<tag>,<seq>                  repeat of an uplink already forwarded (re-ACKed only)
//   BAD,<rssi>,<hex packet>          wrong MIC or unknown format
//   ST,<uptime_s>,<rx>,<bad>,<queued commands>   status, every 60 s
//   OK,<what> / ERR,<what>           reply to a command line
//   # anything                       debug text, ignore
// ESP32 -> radio
//   CMD,<tag>,<cmd>,<arg>            queue a command for a tag (sent with its next ACK)
//   PING
#include <stddef.h>
#include <stdint.h>

// Writes "UP,<rssi>,<snr>,<hex>" (no newline). Returns length, or 0 if out is too small.
size_t ctv1_gw_format_packet(char *out, size_t out_len, const char *kind, int16_t rssi, int8_t snr,
                             const uint8_t *pkt, size_t pkt_len);

// Parses lowercase or uppercase hex into bytes. Returns byte count, 0 on error.
size_t ctv1_hex_decode(const char *hex, uint8_t *out, size_t out_max);

struct Ctv1GwCommand {
  uint16_t tag_id;
  uint8_t cmd;
  uint8_t arg;
};

// Parses "CMD,<tag>,<cmd>,<arg>". Returns false for anything else or out-of-range values.
bool ctv1_gw_parse_cmd(const char *line, Ctv1GwCommand &out);
