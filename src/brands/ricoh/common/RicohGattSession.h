#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

// Outcome of an authenticated characteristic read.
enum class RicohGattRead : uint8_t {
  Ok,           // value holds the bytes; counted as an authenticated read
  Unavailable,  // characteristic missing or not readable
  Rejected,     // insufficient authentication handled; `error` explains
  Failed,       // empty read for another reason; `rc` holds the host error
};

enum class RicohGattWrite : uint8_t {
  Ok,
  Unavailable,  // characteristic missing or not writable
  Failed,       // `error` explains
};

enum class RicohGattStart : uint8_t {
  Started,
  Unavailable,  // characteristic missing
  Failed,       // host refused to start; `rc` holds the error
};

enum class RicohNotifyTarget : uint8_t {
  PowerState,
};

struct RicohLinkSecurity {
  bool encrypted = false;
  bool authenticated = false;
};

// GATT primitives a Ricoh model needs from the connected camera link. No
// model logic lives behind it: RicohBleClient implements it on NimBLE, native
// tests on a recording fake.
class RicohGattSession {
public:
  virtual ~RicohGattSession() = default;

  virtual bool writeHandle(uint16_t handle, const uint8_t* data, size_t length, std::string& error) = 0;
  virtual bool readHandle(uint16_t handle, std::vector<uint8_t>& value, std::string& error) = 0;

  // Reads with pairing-recovery bookkeeping: a successful read counts as
  // authenticated, an insufficient-authentication error may flag the bond.
  virtual RicohGattRead readCharacteristic(const char* serviceUuid,
                                           const char* characteristicUuid,
                                           const char* label,
                                           std::vector<uint8_t>& value,
                                           std::string& error,
                                           int& rc) = 0;
  // Plain read without that bookkeeping. False when the characteristic is
  // missing or not readable; an empty value is still a successful call.
  virtual bool readCharacteristicRaw(const char* serviceUuid,
                                     const char* characteristicUuid,
                                     std::vector<uint8_t>& value) = 0;
  // Write with response. `label` names the value in error text.
  virtual RicohGattWrite writeCharacteristic(const char* serviceUuid,
                                             const char* characteristicUuid,
                                             const uint8_t* data,
                                             size_t length,
                                             const char* label,
                                             std::string& error) = 0;
  // Reads a characteristic only for the host error it produces; the value is
  // discarded. False when the characteristic is missing.
  virtual bool probeRead(const char* serviceUuid, const char* characteristicUuid, int& rc) = 0;
  // Starts a read and returns without waiting for or using the result. Used to
  // make a camera that guards a characteristic start pairing.
  virtual RicohGattStart startRead(const char* serviceUuid, const char* characteristicUuid, int& rc) = 0;
  // Enables notifications on a characteristic.
  virtual bool subscribe(const char* serviceUuid, const char* characteristicUuid, RicohNotifyTarget target) = 0;
  // Routes raw notifications on a fixed handle; 0 stops routing.
  virtual void routeHandleNotify(uint16_t handle, RicohNotifyTarget target) = 0;

  virtual RicohLinkSecurity security() const = 0;

  // Lets the camera breathe between back-to-back reads (delay + yield).
  virtual void pause(uint32_t ms) = 0;

  virtual void print(const char* text) = 0;
  virtual void println(const char* text) = 0;
};

// "01 0A FF" formatting used in Ricoh BLE logs.
inline std::string ricohHexBytes(const uint8_t* data, size_t length) {
  static const char kDigits[] = "0123456789ABCDEF";
  std::string text;
  for (size_t i = 0; i < length; ++i) {
    if (i > 0) {
      text += ' ';
    }
    text += kDigits[data[i] >> 4];
    text += kDigits[data[i] & 0x0F];
  }
  return text;
}
