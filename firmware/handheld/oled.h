#pragma once
// Minimal SH1106 / SSD1306 128x64 I2C driver: init + push a Ctv1Gfx frame. ~300 bytes of code.
#include <Wire.h>
#include <CTv1.h>
#include "config.h"

class Oled {
 public:
  bool begin() {
    Wire.beginTransmission(OLED_ADDR);
    if (Wire.endTransmission() != 0) return false;
    static const uint8_t init[] = {
        0xAE, 0xD5, 0x80, 0xA8, 0x3F, 0xD3, 0x00, 0x40,
#if OLED_SH1106
        0xAD, 0x8B,  // DC-DC on
#else
        0x8D, 0x14, 0x20, 0x02,  // charge pump on, page addressing
#endif
        0xA1, 0xC8, 0xDA, 0x12, 0x81, 0xCF, 0xD9, 0xF1, 0xDB, 0x40, 0xA4, 0xA6, 0xAF};
    for (uint8_t c : init) cmd(c);
    return true;
  }

  void show(const Ctv1Gfx &g) {
    const uint8_t col = OLED_SH1106 ? 2 : 0;  // SH1106 RAM is 132 wide, panel starts at column 2
    for (uint8_t page = 0; page < 8; page++) {
      cmd(0xB0 | page);
      cmd(0x00 | (col & 0x0F));
      cmd(0x10 | (col >> 4));
      for (uint8_t x = 0; x < CTV1_GFX_W; x += 16) {
        Wire.beginTransmission(OLED_ADDR);
        Wire.write(0x40);
        Wire.write(&g.buf[page * CTV1_GFX_W + x], 16);
        Wire.endTransmission();
      }
    }
  }

  void power(bool on) { cmd(on ? 0xAF : 0xAE); }

 private:
  void cmd(uint8_t c) {
    Wire.beginTransmission(OLED_ADDR);
    Wire.write(0x00);
    Wire.write(c);
    Wire.endTransmission();
  }
};
