#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#include "key2_gesture.h"

struct ButtonEvents {
  bool buttonA = false;
  bool toggleDisplayRotation = false;
  bool toggleDisplayMirror = false;
  bool resetPairing = false;
  bool powerOff = false;
  bool any = false;
};

class Buttons {
public:
  void begin();
  ButtonEvents poll();

private:
  bool key2Pressed() const;

  rvf::Key2GestureTracker _key2Gestures;
};
