#include "buttons.h"

#include "config.h"

void Buttons::begin() {
  M5.update();
  M5.BtnPWR.setHoldThresh(POWER_BUTTON_HOLD_MS);
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
  return events;
}
