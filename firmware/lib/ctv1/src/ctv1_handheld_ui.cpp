#include "ctv1_handheld_ui.h"
#include <stdio.h>
#include <string.h>

const char *ctv1_compass_point(uint16_t deg) {
  static const char *pts[] = {"N", "NE", "E", "SE", "S", "SW", "W", "NW"};
  return pts[((deg + 22) / 45) % 8];
}

void ctv1_format_age(uint32_t s, char *out, int out_len) {
  if (s < 10) snprintf(out, out_len, "now");
  else if (s < 60) snprintf(out, out_len, "%lus", (unsigned long)s);
  else if (s < 3600) snprintf(out, out_len, "%lum", (unsigned long)(s / 60));
  else snprintf(out, out_len, "%luh%02lu", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60));
}

static void inverse_bar(Ctv1Gfx &g, int y, const char *s) {
  g.rect(0, y, CTV1_GFX_W, 9, true);
  g.text((CTV1_GFX_W - Ctv1Gfx::text_width(s)) / 2, y + 1, s, 1, false);
}

void ctv1_draw_find(Ctv1Gfx &g, const Ctv1FindView &v) {
  char s[24];
  g.clear();

  // top row: tag number, position in list, tag battery
  snprintf(s, sizeof(s), "#%02u", v.tag_id);
  g.text(0, 0, s);
  snprintf(s, sizeof(s), "%u/%u", v.tag_index, v.tag_count);
  g.text(46, 0, s);
  snprintf(s, sizeof(s), "%u%%", v.tag_batt_pct);
  int bx = CTV1_GFX_W - Ctv1Gfx::text_width(s) - 1;
  g.text(bx, 0, s);
  g.rect(bx - 12, 1, 9, 5);  // battery icon
  g.rect(bx - 3, 2, 1, 3, true);
  g.rect(bx - 11, 2, (v.tag_batt_pct * 7 + 50) / 100, 3, true);
  g.line(0, 9, CTV1_GFX_W - 1, 9);

  // left: arrow in a ring
  const int cx = 24, cy = 36, r = 22;
  g.circle(cx, cy, r);
  if (!v.have_own_fix || !v.tag_has_pos) {
    g.text(cx - 5, cy - 7, "?", 2);
  } else if (v.have_heading) {
    g.arrow(cx, cy, r - 3, v.arrow_deg);
  } else {
    // no compass: point relative to north and mark north at the top of the ring
    g.arrow(cx, cy, r - 6, v.bearing_true);
    g.rect(cx - 4, cy - r - 4, 9, 9, true);
    static const uint8_t N[5] = {0x7F, 0x04, 0x08, 0x10, 0x7F};  // small "N" cut out of the marker
    for (int col = 0; col < 5; col++)
      for (int row = 0; row < 7; row++)
        if (N[col] & (1 << row)) g.pixel(cx - 2 + col, cy - r - 3 + row, false);
  }

  // right: distance, direction + age, signal + own GPS
  const int x = 54;
  if (!v.tag_has_pos) {
    g.text(x, 16, "no fix", 2);
    g.text(x, 34, "from tag");
  } else if (!v.have_own_fix) {
    g.text(x, 16, "GPS..", 2);
    g.text(x, 34, "finding you");
  } else {
    char num[8];
    const char *unit;
    if (v.distance_m < 1000) {
      snprintf(num, sizeof(num), "%lu", (unsigned long)v.distance_m);
      unit = "m";
    } else if (v.distance_m < 10000) {
      snprintf(num, sizeof(num), "%lu.%lu", (unsigned long)(v.distance_m / 1000), (unsigned long)((v.distance_m % 1000) / 100));
      unit = "km";
    } else {
      snprintf(num, sizeof(num), "%lu", (unsigned long)(v.distance_m / 1000));
      unit = "km";
    }
    int scale = (int)strlen(num) <= 3 ? 3 : 2;
    int end = g.text(x, 13, num, scale);
    g.text(end + 1, 13 + 7 * scale - 7, unit);
    char age[8];
    ctv1_format_age(v.age_s, age, sizeof(age));
    snprintf(s, sizeof(s), "%-2s  %s ago", ctv1_compass_point(v.bearing_true), age);
    if (!strcmp(age, "now")) snprintf(s, sizeof(s), "%-2s  just now", ctv1_compass_point(v.bearing_true));
    g.text(x, 38, s);
  }
  if (v.have_own_fix) snprintf(s, sizeof(s), "%ddB GPS%u", v.rssi, v.own_sats);
  else snprintf(s, sizeof(s), "%ddB GPS-", v.rssi);
  g.text(x, 47, s);

  // bottom: alert or command state, else a hint
  const char *bar = nullptr;
  switch (v.cmd) {
    case CTV1_HH_BUZZ_WAITING: bar = "BUZZ: next report"; break;
    case CTV1_HH_BUZZ_SENT: bar = "BUZZING NOW"; break;
    case CTV1_HH_FAST_WAITING: bar = "FAST: next report"; break;
    case CTV1_HH_FAST_SENT: bar = "REPORTS EVERY 2 MIN"; break;
    default: break;
  }
  if (!bar && v.tag_outside) bar = "OUTSIDE THE FENCE";
  if (bar) inverse_bar(g, 55, bar);
  else g.text(0, 57, "< >tag  OK buzz");
}

void ctv1_draw_message(Ctv1Gfx &g, const char *title, const char *l1, const char *l2, const char *l3) {
  g.clear();
  inverse_bar(g, 0, title);
  const char *lines[3] = {l1, l2, l3};
  for (int i = 0; i < 3; i++)
    if (lines[i]) g.text((CTV1_GFX_W - Ctv1Gfx::text_width(lines[i])) / 2, 18 + i * 14, lines[i]);
}
