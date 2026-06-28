#pragma once

#include <Arduino.h>

struct G11ButtonEvents {
  bool pressed = false;
  bool any = false;
};

class G11Button {
public:
  void begin();
  G11ButtonEvents poll();

private:
  bool _lastRawPressed = false;
  uint32_t _lastTriggerMs = 0;
};