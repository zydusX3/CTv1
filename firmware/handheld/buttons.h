#pragma once
// Debounced push buttons with short and long press (in a header so the Arduino IDE's
// generated prototypes in the .ino can see the types).
#include <Arduino.h>
#include "config.h"

struct Button {
  uint8_t pin;
  bool down;
  uint32_t since;
  bool long_done;
};
enum Press { NONE, SHORT, LONG };

static inline Press button_poll(Button &b) {
  bool now_down = digitalRead(b.pin) == LOW;
  uint32_t now = millis();
  Press p = NONE;
  if (now_down && !b.down) {
    b.since = now;
    b.long_done = false;
  } else if (now_down && !b.long_done && now - b.since > LONG_PRESS_MS) {
    b.long_done = true;
    p = LONG;
  } else if (!now_down && b.down && !b.long_done && now - b.since > 30) {
    p = SHORT;
  }
  b.down = now_down;
  return p;
}
