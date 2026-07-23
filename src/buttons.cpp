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

  const rvf::Key2Gesture gesture =
      _key2Gestures.update(key2Pressed(),
                           millis(),
                           KEY2_DOUBLE_PRESS_WINDOW_MS,
                           KEY2_PAIRING_RESET_HOLD_MS);
  switch (gesture) {
    case rvf::Key2Gesture::SinglePress:
      events.toggleDisplayRotation = true;
      events.any = true;
      break;
    case rvf::Key2Gesture::DoublePress:
      events.toggleDisplayMirror = true;
      events.any = true;
      break;
    case rvf::Key2Gesture::LongHold:
      events.resetPairing = true;
      events.any = true;
      break;
    case rvf::Key2Gesture::None:
      break;
  }
  return events;
}

bool Buttons::key2Pressed() const {
  return M5.BtnB.isPressed() || digitalRead(KEY2_FALLBACK_GPIO) == LOW;
}
