#include "g11_button.h"

#include "config.h"

namespace {
volatile bool g_g11PressPending = false;

void IRAM_ATTR handleG11PressInterrupt() {
  g_g11PressPending = true;
}
}  // namespace

void G11Button::begin() {
  const uint8_t pin = static_cast<uint8_t>(G11_BUTTON_GPIO);
  pinMode(pin, INPUT_PULLUP);
  _lastRawPressed = digitalRead(pin) == LOW;
  _lastTriggerMs = 0;

  noInterrupts();
  g_g11PressPending = false;
  interrupts();
  attachInterrupt(digitalPinToInterrupt(pin), handleG11PressInterrupt, FALLING);
}

G11ButtonEvents G11Button::poll() {
  G11ButtonEvents events;
  const uint32_t now = millis();
  const uint8_t pin = static_cast<uint8_t>(G11_BUTTON_GPIO);

  bool interruptPending = false;
  noInterrupts();
  if (g_g11PressPending) {
    g_g11PressPending = false;
    interruptPending = true;
  }
  interrupts();

  const bool rawPressed = digitalRead(pin) == LOW;
  const bool rawPressEdge = rawPressed && !_lastRawPressed;
  _lastRawPressed = rawPressed;

  if (!interruptPending && !rawPressEdge) {
    return events;
  }

  if (_lastTriggerMs != 0 && (now - _lastTriggerMs) < G11_RETRIGGER_GUARD_MS) {
    return events;
  }

  _lastTriggerMs = now;
  events.pressed = true;
  events.any = true;
  return events;
}
