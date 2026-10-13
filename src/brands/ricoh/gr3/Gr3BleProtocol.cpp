#include "Gr3BleProtocol.h"

#include "../common/RicohBleUuids.h"
#include "Gr3BleConfig.h"

#include <cstring>

RicohProtocolGeneration Gr3BleProtocol::generation() const {
  return RicohProtocolGeneration::Gr3Family;
}

RicohSecurityProfileId Gr3BleProtocol::securityProfile() const {
  return RicohSecurityProfileId::Gr3Passkey;
}

const CameraProtocolProfile& Gr3BleProtocol::profile() const {
  return cameraProtocolProfile(generation());
}

bool Gr3BleProtocol::triggerPairingWithProtectedRead() const {
  return true;
}

bool Gr3BleProtocol::requireAuthenticatedBond() const {
  return true;
}

bool Gr3BleProtocol::rediscoverServicesAfterSecurity() const {
  return true;
}

bool Gr3BleProtocol::decodePowerState(uint8_t value, bool& isOn, bool& isOff) const {
  isOn = value == 0x01;
  isOff = value == 0x00 || value == 0x02;
  return isOn || isOff;
}

// GR III family: Wi-Fi, power and operation mode are UUID characteristics,
// and every write needs an authenticated, encrypted link.

namespace {

bool authenticated(const RicohGattSession& gatt) {
  const RicohLinkSecurity security = gatt.security();
  return security.encrypted && security.authenticated;
}

}  // namespace

bool Gr3BleProtocol::openWifi(RicohGattSession& gatt, const RicohWifiGate& gate, std::string& error) const {
  if (!authenticated(gatt)) {
    error = "BLE UUID WiFi activation requires authenticated encryption";
    return false;
  }
  // Delegate to the shared policy rather than repeating it. The two used
  // to be separate copies of the same allowlist, which is how one of them
  // came to be fixed on its own; whichever mode the camera reports, both
  // gates now agree on whether it is safe to raise the WLAN.
  const bool modeAllowsWifi =
      gate.startupModeProbe
          ? operationModeAllowsWifiProbe(profile(), gate.operationMode, gate.operationModeValid)
          : operationModeAllowsWifi(profile(), gate.operationMode, gate.operationModeValid);
  if (!modeAllowsWifi) {
    error = "BLE UUID WiFi activation blocked by camera operation mode";
    return false;
  }
  const uint8_t apMode[] = {RICOH_BLE_GR3_WLAN_AP_VALUE};
  switch (gatt.writeCharacteristic(RICOH_BLE_GR3_WLAN_SERVICE_UUID,
                                   RICOH_BLE_GR3_WLAN_NETWORK_TYPE_UUID,
                                   apMode,
                                   sizeof(apMode),
                                   "NetworkType",
                                   error)) {
    case RicohGattWrite::Unavailable:
      error = "BLE Network Type characteristic unavailable";
      return false;
    case RicohGattWrite::Failed:
      return false;
    case RicohGattWrite::Ok:
      break;
  }
  error.clear();
  gatt.println("BLE WLAN activation method=NETWORK_TYPE_UUID result=OK");
  return true;
}

bool Gr3BleProtocol::closeWifi(RicohGattSession& gatt, std::string& error) const {
  if (!authenticated(gatt)) {
    error = "BLE UUID WiFi deactivation requires authenticated encryption";
    return false;
  }
  const uint8_t offMode[] = {RICOH_BLE_GR3_WLAN_OFF_VALUE};
  switch (gatt.writeCharacteristic(RICOH_BLE_GR3_WLAN_SERVICE_UUID,
                                   RICOH_BLE_GR3_WLAN_NETWORK_TYPE_UUID,
                                   offMode,
                                   sizeof(offMode),
                                   "NetworkTypeOff",
                                   error)) {
    case RicohGattWrite::Unavailable:
      error = "BLE Network Type characteristic unavailable";
      return false;
    case RicohGattWrite::Failed:
      return false;
    case RicohGattWrite::Ok:
      break;
  }
  error.clear();
  gatt.println("BLE WLAN deactivation method=NETWORK_TYPE_UUID value=0x00 result=OK");
  return true;
}

bool Gr3BleProtocol::readPowerValue(RicohGattSession& gatt,
                                    std::vector<uint8_t>& value,
                                    std::string& error) const {
  int rc = 0;
  switch (gatt.readCharacteristic(RICOH_BLE_CAMERA_SERVICE_UUID,
                                  RICOH_BLE_CAMERA_POWER_UUID,
                                  "CameraPower",
                                  value,
                                  error,
                                  rc)) {
    case RicohGattRead::Ok:
      error.clear();
      return true;
    case RicohGattRead::Unavailable:
      error = "BLE Camera Power characteristic unavailable";
      return false;
    case RicohGattRead::Rejected:
      return false;
    case RicohGattRead::Failed:
      error = "BLE power UUID read failed rc=" + std::to_string(rc);
      return false;
  }
  return false;
}

bool Gr3BleProtocol::enablePowerNotify(RicohGattSession& gatt, std::string& error) const {
  if (gatt.subscribe(RICOH_BLE_CAMERA_SERVICE_UUID, RICOH_BLE_CAMERA_POWER_UUID, RicohNotifyTarget::PowerState)) {
    error.clear();
    gatt.println("BLE: power notify enabled method=UUID_DESCRIPTOR");
    return true;
  }
  error = "BLE power notify subscription unavailable";
  return false;
}

bool Gr3BleProtocol::requestCaptureOperationMode(RicohGattSession& gatt, std::string& error) const {
  if (!authenticated(gatt)) {
    error = "BLE capture mode request requires authenticated encryption";
    return false;
  }
  std::vector<uint8_t> list;
  if (!gatt.readCharacteristicRaw(RICOH_BLE_CAMERA_SERVICE_UUID, RICOH_BLE_OPERATION_MODE_LIST_UUID, list)) {
    error = "BLE operation mode list unavailable";
    return false;
  }
  const std::string line = "BLE: operation mode list=[" + ricohHexBytes(list.data(), list.size()) + "]\n";
  gatt.print(line.c_str());
  if (!operationModeListContains(list.data(), list.size(), RicohCameraOperationMode::Capture)) {
    error = "BLE operation mode list does not offer CAPTURE";
    return false;
  }
  const uint8_t captureMode[] = {static_cast<uint8_t>(RicohCameraOperationMode::Capture)};
  switch (gatt.writeCharacteristic(RICOH_BLE_CAMERA_SERVICE_UUID,
                                   RICOH_BLE_OPERATION_MODE_UUID,
                                   captureMode,
                                   sizeof(captureMode),
                                   "OperationMode",
                                   error)) {
    case RicohGattWrite::Unavailable:
      error = "BLE operation mode not writable";
      return false;
    case RicohGattWrite::Failed:
      return false;
    case RicohGattWrite::Ok:
      break;
  }
  error.clear();
  gatt.println("BLE: operation mode write CAPTURE result=OK");
  return true;
}

bool Gr3BleProtocol::readWifiCredentials(RicohGattSession& gatt,
                                         RicohWifiCredentials& credentials,
                                         std::string& error) const {
  struct Gr3CredentialCharacteristic {
    const char* uuid;
    const char* label;
  };
  const Gr3CredentialCharacteristic parameters[] = {
      {RICOH_BLE_GR3_WLAN_SSID_UUID, "ssid"},
      {RICOH_BLE_GR3_WLAN_PASSPHRASE_UUID, "passphrase"},
      {RICOH_BLE_GR3_WLAN_CHANNEL_UUID, "channel"},
  };

  for (const Gr3CredentialCharacteristic& parameter : parameters) {
    std::vector<uint8_t> value;
    int rc = 0;
    const RicohGattRead result =
        gatt.readCharacteristic(RICOH_BLE_GR3_WLAN_SERVICE_UUID, parameter.uuid, parameter.label, value, error, rc);
    if (result == RicohGattRead::Rejected) {
      return false;
    }
    if (result != RicohGattRead::Ok) {
      continue;
    }

    if (std::strcmp(parameter.label, "channel") == 0) {
      credentials.channel = value[0];
    } else {
      std::string text;
      text.reserve(value.size());
      for (uint8_t byte : value) {
        if (byte != 0) {
          text += static_cast<char>(byte);
        }
      }
      // Arduino String::trim(): strip leading and trailing whitespace.
      const size_t first = text.find_first_not_of(" \t\n\v\f\r");
      const size_t last = text.find_last_not_of(" \t\n\v\f\r");
      text = first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
      if (std::strcmp(parameter.label, "ssid") == 0) {
        credentials.ssid = text;
      } else {
        credentials.passphrase = text;
        credentials.encryptedPassphrase = false;
      }
    }
    gatt.pause(20);
  }
  credentials.valid = validGr3WifiCredentials(credentials.ssid.c_str(),
                                              credentials.passphrase.c_str(),
                                              credentials.channel);
  return true;
}
