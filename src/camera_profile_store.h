#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "protocol/CameraProfileMigration.h"

struct WifiCredential {
  String ssid;
  String passphrase;
  String bssid;
  String cameraIp;
  uint16_t frequencyMhz = 0;
  uint8_t channel = 0;
  bool cached = false;
};

struct CameraProfile {
  rvf::CameraModel model = rvf::CameraModel::Unknown;
  String cameraName;
  String bleAddress;
  uint8_t bleAddressType = 0;
  bool bleAddressTypeKnown = false;
  bool bleBonded = false;
  WifiCredential wifi;
  uint32_t profileVersion = rvf::kCameraProfileVersion;
};

class CameraProfileStore {
public:
  bool begin();
  bool load(CameraProfile& profile);
  bool save(const CameraProfile& profile);
  bool saveConnectedCamera(const CameraProfile& profile);
  bool saveWifiCredentials(const String& bleAddress, const WifiCredential& wifi);
  bool clearWifiCredentials();
  bool clearBlePairing();
  bool clear();

private:
  Preferences _prefs;
  bool _begun = false;
};
