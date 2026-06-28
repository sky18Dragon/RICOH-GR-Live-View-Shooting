#include "buttons.h"

void Buttons::begin() {
  M5.update();
}

ButtonEvents Buttons::poll() {
  M5.update();

  ButtonEvents events;
  if (M5.BtnA.wasPressed()) {
    events.buttonA = true;
    events.any = true;
  }
  if (M5.BtnB.wasPressed()) {
    events.buttonB = true;
    events.liveviewToggle = true;
    events.any = true;
  }
  return events;
}
