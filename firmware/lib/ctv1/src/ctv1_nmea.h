#pragma once
// Minimal NMEA 0183 reader for GGA and RMC from any talker ($GP, $GN, $GL...).
// Feed it one character at a time from the GPS UART. Integer maths only (the STM32WL has no FPU).
#include <stdint.h>

struct Ctv1Fix {
  bool valid;          // GGA quality > 0 and RMC status A (whichever arrived last)
  int32_t lat_e7;      // degrees * 1e7, south negative
  int32_t lon_e7;      // degrees * 1e7, west negative
  uint8_t sats;
  uint16_t hdop_x10;   // HDOP * 10
  uint32_t utc_hhmmss; // 123519 = 12:35:19 UTC
  uint32_t date_ddmmyy;
  uint16_t speed_knots_x10;
};

class Ctv1Nmea {
 public:
  Ctv1Nmea() { reset(); }
  void reset();
  // Returns true when a GGA or RMC sentence with a good checksum has updated fix().
  bool feed(char c);
  const Ctv1Fix &fix() const { return fix_; }
  uint32_t sentences() const { return good_; }
  uint32_t checksum_errors() const { return bad_; }

 private:
  bool parse();
  char buf_[96];
  uint8_t len_;
  bool in_;
  Ctv1Fix fix_;
  uint32_t good_, bad_;
};
