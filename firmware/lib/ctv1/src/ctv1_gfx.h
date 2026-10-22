#pragma once
// Tiny 1-bit graphics for a 128x64 OLED (SH1106 / SSD1306 page layout). No heap, ~1 KB RAM.
// Kept free of Arduino code so screens can be rendered and checked on a PC.
#include <stdint.h>

#define CTV1_GFX_W 128
#define CTV1_GFX_H 64

class Ctv1Gfx {
 public:
  uint8_t buf[CTV1_GFX_W * CTV1_GFX_H / 8];  // 8 pages of 128 columns, LSB = top pixel of the page

  void clear();
  void pixel(int x, int y, bool on = true);
  bool get(int x, int y) const;
  void line(int x0, int y0, int x1, int y1);
  void rect(int x, int y, int w, int h, bool fill = false);
  void circle(int cx, int cy, int r);
  void triangle(int x0, int y0, int x1, int y1, int x2, int y2);  // filled
  // Text in the 5x7 font; scale 1 = 6 px per character. on = false erases (text on a filled bar).
  // Returns x after the text.
  int text(int x, int y, const char *s, int scale = 1, bool on = true);
  static int text_width(const char *s, int scale = 1);
  // Arrow pointing at angle_deg (0 = up, clockwise), centred on (cx, cy), tip at radius r.
  void arrow(int cx, int cy, int r, int angle_deg);
};
