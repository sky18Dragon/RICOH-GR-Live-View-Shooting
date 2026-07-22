#include "buttons.h"

#include "config.h"

void Buttons::begin() {
  M5.update();
  M5.BtnPWR.setHoldThresh(POWER_BUTTON_HOLD_MS);
  pinMode(KEY2_FALLBACK_GPIO, INPUT_PULLUP);
  _input.reset();
}

ButtonEvents Buttons::poll() {
  M5.update();
  return _input.update(M5.BtnA.isPressed(),
                       key2Pressed(),
                       M5.BtnPWR.wasHold(),
                       millis());
}

bool Buttons::key2Pressed() const {
  return M5.BtnB.isPressed() || digitalRead(KEY2_FALLBACK_GPIO) == LOW;
}
