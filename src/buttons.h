#pragma once

#include <Arduino.h>
#include <M5Unified.h>

struct ButtonEvents {
  bool buttonA = false;
  bool buttonB = false;
  bool liveviewToggle = false;
  bool any = false;
};

class Buttons {
public:
  void begin();
  ButtonEvents poll();
};
