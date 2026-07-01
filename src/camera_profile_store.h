#pragma once

#include <Arduino.h>
#include <Preferences.h>

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
  String cameraName;
  String bleAddress;
  uint8_t bleAddressType = 0;
  bool bleAddressTypeKnown = false;
  bool bleBonded = false;
  WifiCredential wifi;
  uint32_t profileVersion = 3;
};

class CameraProfileStore {
public:
  bool begin();
  bool load(CameraProfile& profile);
  bool save(const CameraProfile& profile);
  bool saveWifiCredentials(const String& bleAddress, const WifiCredential& wifi);
  bool clearWifiCredentials();
  bool saveBleIdentity(const String& cameraName, const String& bleAddress);
  bool saveBleIdentity(const String& cameraName, const String& bleAddress, uint8_t bleAddressType, bool bleBonded);
  bool clear();

private:
  Preferences _prefs;
  bool _begun = false;
};
