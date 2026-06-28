#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct WifiCredential {
  String ssid;
  String passphrase;
  String bssid;
  String cameraIp;
};

struct CameraProfile {
  String cameraName;
  String bleAddress;
  WifiCredential wifi;
  uint32_t profileVersion = 3;
};

class CameraProfileStore {
public:
  bool begin();
  bool load(CameraProfile& profile);
  bool save(const CameraProfile& profile);
  bool saveBleIdentity(const String& cameraName, const String& bleAddress);
  bool clear();

private:
  Preferences _prefs;
  bool _begun = false;
};
