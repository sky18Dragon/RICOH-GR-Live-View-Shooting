#pragma once

#include <Arduino.h>
#include <Preferences.h>

#include "camera_profile_schema.h"
#include "camera_protocol_profile.h"

struct WifiCredential {
  String ssid;
  String passphrase;
  String bssid;
  String cameraIp;
  uint16_t frequencyMhz = 0;
  uint8_t channel = 0;
  bool cached = false;
  bool credentialsValid = false;
  WifiCredentialSource source = WifiCredentialSource::Unknown;
};

struct CameraProfile {
  String cameraName;
  String bleAddress;
  uint8_t bleAddressType = 0;
  bool bleAddressTypeKnown = false;
  bool bleBonded = false;
  RicohProtocolGeneration protocolGeneration = RicohProtocolGeneration::Unknown;
  bool protocolGenerationKnown = false;
  uint16_t capabilityVersion = CAMERA_CAPABILITY_SCHEMA_VERSION;
  WifiCredential wifi;
  uint32_t profileVersion = CAMERA_PROFILE_SCHEMA_VERSION;
};

class CameraProfileStore {
public:
  bool begin();
  bool load(CameraProfile& profile);
  bool save(const CameraProfile& profile);
  bool saveWifiCredentials(const String& bleAddress, const WifiCredential& wifi);
  bool clearWifiCredentials();
  bool clearBlePairing();
  bool saveBleIdentity(const String& cameraName, const String& bleAddress);
  bool saveBleIdentity(const String& cameraName, const String& bleAddress, uint8_t bleAddressType, bool bleBonded);
  bool clear();

private:
  Preferences _prefs;
  bool _begun = false;
};
