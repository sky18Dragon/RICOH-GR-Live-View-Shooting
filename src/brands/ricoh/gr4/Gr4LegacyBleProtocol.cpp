#include "Gr4LegacyBleProtocol.h"

#include <cstdio>

#include "Gr4BleConfig.h"
#include "Gr4WifiCredentialParser.h"

RicohProtocolGeneration Gr4LegacyBleProtocol::generation() const {
  return RicohProtocolGeneration::Gr4Family;
}

RicohSecurityProfileId Gr4LegacyBleProtocol::securityProfile() const {
  return RicohSecurityProfileId::Gr4Legacy;
}

const CameraProtocolProfile& Gr4LegacyBleProtocol::profile() const {
  return cameraProtocolProfile(generation());
}

bool Gr4LegacyBleProtocol::triggerPairingWithProtectedRead() const {
  return false;
}

bool Gr4LegacyBleProtocol::requireAuthenticatedBond() const {
  return false;
}

bool Gr4LegacyBleProtocol::rediscoverServicesAfterSecurity() const {
  return false;
}

bool Gr4LegacyBleProtocol::decodePowerState(uint8_t value, bool& isOn, bool& isOff) const {
  isOn = value == 0x01;
  isOff = value == 0x00;
  return isOn || isOff;
}

// GR IV: Wi-Fi and power live on fixed ATT handles (see Gr4BleConfig.h).

bool Gr4LegacyBleProtocol::openWifi(RicohGattSession& gatt, const RicohWifiGate&, std::string& error) const {
  const uint8_t payload[] = {RICOH_BLE_GR4_WLAN_ON_VALUE};
  if (!gatt.writeHandle(RICOH_BLE_GR4_WLAN_POWER_HANDLE, payload, sizeof(payload), error)) {
    return false;
  }
  error.clear();
  gatt.println("BLE WLAN activation method=FIXED_HANDLE result=OK");
  return true;
}

bool Gr4LegacyBleProtocol::closeWifi(RicohGattSession& gatt, std::string& error) const {
  const uint8_t payload[] = {RICOH_BLE_GR4_WLAN_OFF_VALUE};
  if (!gatt.writeHandle(RICOH_BLE_GR4_WLAN_POWER_HANDLE, payload, sizeof(payload), error)) {
    return false;
  }
  error.clear();
  char line[96];
  std::snprintf(line,
                sizeof(line),
                "BLE WLAN deactivation method=FIXED_HANDLE handle=0x%04X value=0x%02X result=OK\n",
                RICOH_BLE_GR4_WLAN_POWER_HANDLE,
                RICOH_BLE_GR4_WLAN_OFF_VALUE);
  gatt.print(line);
  return true;
}

bool Gr4LegacyBleProtocol::readPowerValue(RicohGattSession& gatt,
                                          std::vector<uint8_t>& value,
                                          std::string& error) const {
  std::string readError;
  if (!gatt.readHandle(RICOH_BLE_GR4_POWER_STATE_HANDLE, value, readError)) {
    error = "BLE power read failed: " + readError;
    return false;
  }
  error.clear();
  return true;
}

bool Gr4LegacyBleProtocol::enablePowerNotify(RicohGattSession& gatt, std::string& error) const {
  const uint8_t payload[] = {0x01, 0x00};
  gatt.routeHandleNotify(RICOH_BLE_GR4_POWER_STATE_HANDLE, RicohNotifyTarget::PowerState);
  std::string writeError;
  if (!gatt.writeHandle(RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE, payload, sizeof(payload), writeError)) {
    gatt.routeHandleNotify(0, RicohNotifyTarget::PowerState);
    error = "BLE power notify enable failed: " + writeError;
    return false;
  }
  error.clear();
  char line[64];
  std::snprintf(line, sizeof(line), "BLE: power notify enabled cccd=0x%04X\n", RICOH_BLE_GR4_POWER_STATE_CCCD_HANDLE);
  gatt.print(line);
  return true;
}

bool Gr4LegacyBleProtocol::requestCaptureOperationMode(RicohGattSession&, std::string& error) const {
  error = "BLE capture mode request is GR III family only";
  return false;
}

bool Gr4LegacyBleProtocol::readWifiCredentials(RicohGattSession& gatt,
                                               RicohWifiCredentials& credentials,
                                               std::string&) const {
  struct WlanParamHandle {
    uint16_t handle;
    const char* label;
  };
  static const WlanParamHandle kWlanParamHandles[] = {
      {RICOH_BLE_GR4_WLAN_SSID_HANDLE, "ssid"},
      {RICOH_BLE_GR4_WLAN_PASSPHRASE_HANDLE, "passphrase"},
      {RICOH_BLE_GR4_WLAN_SECURITY_HANDLE, "security"},
      {RICOH_BLE_GR4_WLAN_FREQUENCY_HANDLE, "frequency"},
      {RICOH_BLE_GR4_WLAN_BSSID_HANDLE, "bssid"},
  };
  for (const WlanParamHandle& item : kWlanParamHandles) {
    std::vector<uint8_t> value;
    std::string readError;
    if (gatt.readHandle(item.handle, value, readError)) {
      mergeGr4CredentialValue(credentials, item.label, value);
    }
    gatt.pause(20);
  }
  return true;
}


// GR IV pairs on its own (legacy security); nothing to provoke.
bool Gr4LegacyBleProtocol::armPairing(RicohGattSession&) const {
  return false;
}

bool Gr4LegacyBleProtocol::statusNotifyAllowed(const RicohGattSession&, std::string& error) const {
  error.clear();
  return true;
}

// Not reached: GR IV does not require an authenticated bond.
int Gr4LegacyBleProtocol::probeAuthenticationError(RicohGattSession&, int lastError) const {
  return lastError;
}
