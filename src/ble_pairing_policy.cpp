#include "ble_pairing_policy.h"

namespace {

constexpr uint8_t kAddrPublic = 0x00;
constexpr uint8_t kAddrRandom = 0x01;
constexpr uint8_t kAddrPublicId = 0x02;
constexpr uint8_t kAddrRandomId = 0x03;

constexpr int kAttErrBase = 0x100;
constexpr int kAttInsufficientAuthentication = kAttErrBase + 0x05;
constexpr int kAttInsufficientAuthorization = kAttErrBase + 0x08;
constexpr int kAttInsufficientEncryption = kAttErrBase + 0x0F;
constexpr int kRemoteUserDisconnect = 0x213;

constexpr uint8_t kBondedSecurityFailureLimit = 3;
constexpr uint8_t kInsufficientAuthReadLimit = 2;

}  // namespace

uint8_t normalizedPeerAddressType(uint8_t type) {
  if (type == kAddrPublicId) {
    return kAddrPublic;
  }
  if (type == kAddrRandomId) {
    return kAddrRandom;
  }
  return type;
}

bool attErrorMeansInsufficientAuth(int rc) {
  return rc == kAttInsufficientAuthentication ||
         rc == kAttInsufficientAuthorization ||
         rc == kAttInsufficientEncryption;
}

bool explicitBondSecurityFailure(int rc) {
  return rc == kRemoteUserDisconnect || attErrorMeansInsufficientAuth(rc);
}

bool PairingRecoveryPolicy::onBondedSecurityFailure(int rc) {
  if (!explicitBondSecurityFailure(rc)) {
    _securityFailures = 0;
    return false;
  }
  if (++_securityFailures < kBondedSecurityFailureLimit) {
    return false;
  }
  _securityFailures = 0;
  return true;
}

bool PairingRecoveryPolicy::onInsufficientAuthRead(int rc) {
  if (!attErrorMeansInsufficientAuth(rc)) {
    _rejectedReads = 0;
    return false;
  }
  if (++_rejectedReads < kInsufficientAuthReadLimit) {
    return false;
  }
  _rejectedReads = 0;
  return true;
}

void PairingRecoveryPolicy::onPairingSuccess() {
  _securityFailures = 0;
}

void PairingRecoveryPolicy::onAuthenticatedRead() {
  _rejectedReads = 0;
}

void PairingRecoveryPolicy::reset() {
  _securityFailures = 0;
  _rejectedReads = 0;
}

int32_t PasskeyDigitCollector::feed(char c) {
  if (c < '0' || c > '9') {
    return -1;
  }
  _value = _value * 10 + (c - '0');
  if (++_count < 6) {
    return -1;
  }
  const int32_t code = _value;
  reset();
  return code;
}

void PasskeyDigitCollector::reset() {
  _value = 0;
  _count = 0;
}

void PasskeyButtonEntry::start(uint32_t nowMs, uint32_t timeoutMs) {
  reset();
  _active = true;
  _startedAtMs = nowMs;
  _timeoutMs = timeoutMs;
}

void PasskeyButtonEntry::reset() {
  for (uint8_t i = 0; i < 6; ++i) {
    _digits[i] = 0;
  }
  _index = 0;
  _startedAtMs = 0;
  _timeoutMs = 0;
  _active = false;
}

void PasskeyButtonEntry::shortPress() {
  if (_active && _index < 6) {
    _digits[_index] = static_cast<uint8_t>((_digits[_index] + 1) % 10);
  }
}

PasskeyEntryStatus PasskeyButtonEntry::confirmDigit() {
  if (!_active) {
    return PasskeyEntryStatus::Idle;
  }
  if (_index < 6) {
    ++_index;
  }
  return _index >= 6 ? PasskeyEntryStatus::Complete : PasskeyEntryStatus::Editing;
}

PasskeyEntryStatus PasskeyButtonEntry::status(uint32_t nowMs) const {
  if (!_active) {
    return PasskeyEntryStatus::Idle;
  }
  if (_index >= 6) {
    return PasskeyEntryStatus::Complete;
  }
  if (_timeoutMs > 0 && static_cast<uint32_t>(nowMs - _startedAtMs) >= _timeoutMs) {
    return PasskeyEntryStatus::TimedOut;
  }
  return PasskeyEntryStatus::Editing;
}

int32_t PasskeyButtonEntry::code() const {
  int32_t code = 0;
  for (uint8_t i = 0; i < 6; ++i) {
    code = code * 10 + _digits[i];
  }
  return code;
}

