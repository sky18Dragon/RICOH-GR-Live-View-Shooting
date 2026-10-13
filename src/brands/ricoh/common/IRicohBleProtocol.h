#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "../../../camera_protocol_profile.h"
#include "RicohGattSession.h"
#include "RicohWifiCredentials.h"

// How well a read-only GATT discovery matches one model.
enum class RicohEvidenceMatch : uint8_t {
  None,
  Partial,  // some of the model's markers, not all
  Full,     // every marker the model requires
};

// What the camera last reported about its operation mode, for models that
// gate Wi-Fi activation on it.
struct RicohWifiGate {
  bool startupModeProbe = false;
  RicohCameraOperationMode operationMode = RicohCameraOperationMode::Unknown;
  bool operationModeValid = false;
};

// One Ricoh model's BLE protocol: how it pairs and how it drives the camera
// over a connected link. Operations write `error` on failure and clear it on
// success; RicohBleClient keeps the connection and the model-neutral checks.
class IRicohBleProtocol {
public:
  virtual ~IRicohBleProtocol() = default;

  virtual RicohProtocolGeneration generation() const = 0;
  virtual RicohSecurityProfileId securityProfile() const = 0;
  virtual const CameraProtocolProfile& profile() const = 0;
  // Whether pairing must be provoked with armPairing() on an unbonded link
  // (after a full service discovery).
  virtual bool triggerPairingWithProtectedRead() const = 0;
  virtual bool requireAuthenticatedBond() const = 0;
  virtual bool rediscoverServicesAfterSecurity() const = 0;
  virtual bool decodePowerState(uint8_t value, bool& isOn, bool& isOff) const = 0;

  // Detection: a model judges only its own markers. A more specific model
  // (higher priority) wins a tie and vetoes a less specific match when it sees
  // partial markers of itself; see RicohBleProtocolRouter::detect().
  virtual RicohEvidenceMatch matchEvidence(const ProtocolDetectionEvidence& evidence) const = 0;
  virtual uint8_t detectionPriority() const = 0;

  virtual bool openWifi(RicohGattSession& gatt, const RicohWifiGate& gate, std::string& error) const = 0;
  virtual bool closeWifi(RicohGattSession& gatt, std::string& error) const = 0;
  // Raw power-state bytes; decodePowerState() interprets the first one.
  virtual bool readPowerValue(RicohGattSession& gatt, std::vector<uint8_t>& value, std::string& error) const = 0;
  virtual bool enablePowerNotify(RicohGattSession& gatt, std::string& error) const = 0;
  virtual bool requestCaptureOperationMode(RicohGattSession& gatt, std::string& error) const = 0;
  // Makes the camera request pairing; only called when
  // triggerPairingWithProtectedRead() is true.
  virtual bool armPairing(RicohGattSession& gatt) const = 0;
  // After requireAuthenticatedBond() failed: the ATT error a protected read
  // gives now, for pairing recovery; `lastError` when nothing could be read.
  virtual int probeAuthenticationError(RicohGattSession& gatt, int lastError) const = 0;
  // Whether battery / shot-count / capture-status notifications may be
  // subscribed on this link.
  virtual bool statusNotifyAllowed(const RicohGattSession& gatt, std::string& error) const = 0;
  // One polling pass over the camera's Wi-Fi parameters, merged into
  // `credentials` (which carries earlier passes). False only when the pass
  // must stop polling; `error` then explains.
  virtual bool readWifiCredentials(RicohGattSession& gatt,
                                   RicohWifiCredentials& credentials,
                                   std::string& error) const = 0;
};
