#pragma once

#include <Arduino.h>
#include <M5Unified.h>

#include "ui/ButtonInput.h"

using ButtonEvents = rvf::ButtonEvents;

class Buttons {
public:
  Buttons();
  void begin();
  ButtonEvents poll();

private:
  bool key2Pressed() const;
  rvf::ButtonInput _input;
};
