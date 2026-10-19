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
  // Legacy aliases retained for schema <= 3 and fixed-address GR IV units.
  String bleAddress;
  uint8_t bleAddressType = 0;
  bool bleAddressTypeKnown = false;
  bool bleBonded = false;
  bool bleAuthenticated = false;
  RicohProtocolGeneration protocolGeneration = RicohProtocolGeneration::Unknown;
  bool protocolGenerationKnown = false;
  RicohSecurityProfileId securityProfile = RicohSecurityProfileId::Unknown;
  bool securityProfileKnown = false;
  String peerIdentityAddress;
  uint8_t peerIdentityAddressType = 0;
  bool peerIdentityKnown = false;
  String lastSeenOtaAddress;
  uint8_t lastSeenOtaAddressType = 0;
  uint16_t capabilityVersion = CAMERA_CAPABILITY_SCHEMA_VERSION;
  WifiCredential wifi;
  uint32_t profileVersion = CAMERA_PROFILE_SCHEMA_VERSION;
};

// The camera binding for the Ricoh driver: one JSON snapshot in NVS
// namespace "cam.ricoh". Firmware upgrades always erase flash, so only the
// current schema is read; a snapshot of any other version reads as unpaired.
class CameraProfileStore {
public:
  // An empty profile (and true) when nothing is stored; false if NVS fails.
  bool load(CameraProfile& profile);
  bool save(const CameraProfile& profile);
  // Replaces the stored Wi-Fi credentials when `bleAddress` is the stored camera.
  bool saveWifiCredentials(const String& bleAddress, const WifiCredential& wifi);
  // Forgets the camera; NimBLE bonds are cleared separately.
  bool clearProfile();

private:
  bool begin();

  Preferences _prefs;
  bool _begun = false;
};
