#pragma once

#include <cstdint>

uint8_t normalizedPeerAddressType(uint8_t type);
bool attErrorMeansInsufficientAuth(int rc);
bool explicitBondSecurityFailure(int rc);

class PairingRecoveryPolicy {
public:
  bool onBondedSecurityFailure(int rc);
  bool onInsufficientAuthRead(int rc);
  void onPairingSuccess();
  void onAuthenticatedRead();
  void reset();

private:
  uint8_t _securityFailures = 0;
  uint8_t _rejectedReads = 0;
};

class PasskeyDigitCollector {
public:
  int32_t feed(char c);
  void reset();

private:
  int32_t _value = 0;
  uint8_t _count = 0;
};

enum class PasskeyEntryStatus : uint8_t {
  Idle,
  Editing,
  Complete,
  TimedOut,
};

class PasskeyButtonEntry {
public:
  void start(uint32_t nowMs, uint32_t timeoutMs);
  void reset();
  void shortPress();
  PasskeyEntryStatus confirmDigit();
  PasskeyEntryStatus status(uint32_t nowMs) const;
  int32_t code() const;
  uint8_t activeIndex() const { return _index; }
  const uint8_t* digits() const { return _digits; }

private:
  uint8_t _digits[6] = {0, 0, 0, 0, 0, 0};
  uint8_t _index = 0;
  uint32_t _startedAtMs = 0;
  uint32_t _timeoutMs = 0;
  bool _active = false;
};

