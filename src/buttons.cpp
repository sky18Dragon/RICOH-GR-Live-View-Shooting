#include "buttons.h"

#include "config.h"

void Buttons::begin() {
  M5.update();
  M5.BtnPWR.setHoldThresh(POWER_BUTTON_HOLD_MS);
  pinMode(KEY2_FALLBACK_GPIO, INPUT_PULLUP);
}

ButtonEvents Buttons::poll() {
  M5.update();

  ButtonEvents events;
  if (M5.BtnA.wasPressed()) {
    events.buttonA = true;
    events.any = true;
  }
  if (M5.BtnPWR.wasHold()) {
    events.powerOff = true;
    events.any = true;
  }
  const bool key2Down = key2Pressed();
  if (!key2Down) {
    _key2PressedSince = 0;
    _key2HoldReported = false;
  } else {
    const uint32_t now = millis();
    if (_key2PressedSince == 0) {
      _key2PressedSince = now;
    }
    if (!_key2HoldReported && (now - _key2PressedSince) >= KEY2_PAIRING_RESET_HOLD_MS) {
      _key2HoldReported = true;
      events.resetPairing = true;
      events.any = true;
    }
  }
  return events;
}

bool Buttons::key2Pressed() const {
  return M5.BtnB.isPressed() || digitalRead(KEY2_FALLBACK_GPIO) == LOW;
}
