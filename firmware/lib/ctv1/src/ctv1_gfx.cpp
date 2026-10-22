#include "ctv1_gfx.h"
#include "ctv1_math.h"
#include <string.h>

extern const uint8_t CTV1_FONT5X7[95 * 5];

void Ctv1Gfx::clear() { memset(buf, 0, sizeof(buf)); }

void Ctv1Gfx::pixel(int x, int y, bool on) {
  if (x < 0 || y < 0 || x >= CTV1_GFX_W || y >= CTV1_GFX_H) return;
  uint8_t &b = buf[(y / 8) * CTV1_GFX_W + x];
  if (on) b |= (uint8_t)(1 << (y & 7));
  else b &= (uint8_t)~(1 << (y & 7));
}

bool Ctv1Gfx::get(int x, int y) const {
  if (x < 0 || y < 0 || x >= CTV1_GFX_W || y >= CTV1_GFX_H) return false;
  return buf[(y / 8) * CTV1_GFX_W + x] & (1 << (y & 7));
}

void Ctv1Gfx::line(int x0, int y0, int x1, int y1) {
  int dx = x1 > x0 ? x1 - x0 : x0 - x1, sx = x0 < x1 ? 1 : -1;
  int dy = y1 > y0 ? y0 - y1 : y1 - y0, sy = y0 < y1 ? 1 : -1;
  int err = dx + dy;
  for (;;) {
    pixel(x0, y0);
    if (x0 == x1 && y0 == y1) break;
    int e2 = 2 * err;
    if (e2 >= dy) { err += dy; x0 += sx; }
    if (e2 <= dx) { err += dx; y0 += sy; }
  }
}

void Ctv1Gfx::rect(int x, int y, int w, int h, bool fill) {
  for (int j = 0; j < h; j++)
    for (int i = 0; i < w; i++)
      if (fill || i == 0 || j == 0 || i == w - 1 || j == h - 1) pixel(x + i, y + j);
}

void Ctv1Gfx::circle(int cx, int cy, int r) {
  int x = r, y = 0, err = 1 - r;
  while (x >= y) {
    const int px[8] = {x, y, -y, -x, -x, -y, y, x}, py[8] = {y, x, x, y, -y, -x, -x, -y};
    for (int i = 0; i < 8; i++) pixel(cx + px[i], cy + py[i]);
    y++;
    if (err < 0) err += 2 * y + 1;
    else { x--; err += 2 * (y - x) + 1; }
  }
}

void Ctv1Gfx::triangle(int x0, int y0, int x1, int y1, int x2, int y2) {
  int miny = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2);
  int maxy = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
  const int xs[3] = {x0, x1, x2}, ys[3] = {y0, y1, y2};
  for (int y = miny; y <= maxy; y++) {
    float lo = 1e9f, hi = -1e9f;
    for (int i = 0; i < 3; i++) {
      int j = (i + 1) % 3;
      int ya = ys[i], yb = ys[j];
      if (ya == yb) {
        if (ya == y) {
          if (xs[i] < lo) lo = (float)xs[i];
          if (xs[i] > hi) hi = (float)xs[i];
          if (xs[j] < lo) lo = (float)xs[j];
          if (xs[j] > hi) hi = (float)xs[j];
        }
        continue;
      }
      if ((y < ya && y < yb) || (y > ya && y > yb)) continue;
      float x = xs[i] + (float)(y - ya) * (xs[j] - xs[i]) / (float)(yb - ya);
      if (x < lo) lo = x;
      if (x > hi) hi = x;
    }
    for (int x = (int)(lo + 0.5f); x <= (int)(hi + 0.5f); x++) pixel(x, y);
  }
}

int Ctv1Gfx::text(int x, int y, const char *s, int scale, bool on) {
  for (; *s; s++) {
    unsigned char c = (unsigned char)*s;
    if (c < 0x20 || c > 0x7E) c = '?';
    const uint8_t *g = &CTV1_FONT5X7[(c - 0x20) * 5];
    for (int col = 0; col < 5; col++)
      for (int row = 0; row < 7; row++)
        if (g[col] & (1 << row))
          for (int dy = 0; dy < scale; dy++)
            for (int dx = 0; dx < scale; dx++) pixel(x + col * scale + dx, y + row * scale + dy, on);
    x += 6 * scale;
  }
  return x;
}

int Ctv1Gfx::text_width(const char *s, int scale) { return (int)strlen(s) * 6 * scale - scale; }

void Ctv1Gfx::arrow(int cx, int cy, int r, int angle_deg) {
  float s = ctv1_sin_deg((float)angle_deg), c = ctv1_cos_deg((float)angle_deg);
  // arrow shape in its own frame (pointing up): tip, two barbs, tail
  auto rnd = [](float v) { return (int)(v + (v >= 0 ? 0.5f : -0.5f)); };
  auto px = [&](float x, float y) { return cx + rnd(x * c - y * s); };
  auto py = [&](float x, float y) { return cy + rnd(x * s + y * c); };
  float tip = -(float)r, barb = -(float)r * 0.15f, half = (float)r * 0.55f, tail = (float)r * 0.75f, shaft = (float)r * 0.18f;
  triangle(px(0, tip), py(0, tip), px(-half, barb), py(-half, barb), px(half, barb), py(half, barb));
  triangle(px(-shaft, barb), py(-shaft, barb), px(shaft, barb), py(shaft, barb), px(shaft, tail), py(shaft, tail));
  triangle(px(-shaft, barb), py(-shaft, barb), px(shaft, tail), py(shaft, tail), px(-shaft, tail), py(-shaft, tail));
}
