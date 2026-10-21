#pragma once
// Screens for the handheld beacon (128x64). Drawing only, no hardware, so they can be previewed on a PC.
#include <stdint.h>
#include "ctv1_gfx.h"

enum Ctv1CmdState : uint8_t { CTV1_HH_CMD_NONE, CTV1_HH_BUZZ_WAITING, CTV1_HH_BUZZ_SENT, CTV1_HH_FAST_WAITING, CTV1_HH_FAST_SENT };

struct Ctv1FindView {
  uint16_t tag_id;
  uint8_t tag_index, tag_count;  // 1-based position in the list of tags heard
  uint8_t tag_batt_pct;
  bool tag_outside;              // the tag says it is outside its fence
  bool tag_has_pos;              // tag has ever sent a position
  uint32_t age_s;                // since the tag's last report
  int16_t rssi;
  bool have_own_fix;
  uint8_t own_sats;
  uint32_t distance_m;
  uint16_t bearing_true;         // from the handheld to the tag, 0 = north
  bool have_heading;             // compass working and calibrated
  uint16_t arrow_deg;            // where to point on screen (0 = straight ahead); used when have_heading
  Ctv1CmdState cmd;
};

void ctv1_draw_find(Ctv1Gfx &g, const Ctv1FindView &v);

// Full-screen message: a title and up to three short lines.
void ctv1_draw_message(Ctv1Gfx &g, const char *title, const char *l1, const char *l2 = nullptr, const char *l3 = nullptr);

// "N", "NE", ... for a bearing.
const char *ctv1_compass_point(uint16_t deg);

// "now", "45s", "12m", "3h05" for a duration.
void ctv1_format_age(uint32_t s, char *out, int out_len);
