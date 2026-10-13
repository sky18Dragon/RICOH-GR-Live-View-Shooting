#include "camera_profile_store.h"

#include <ArduinoJson.h>

namespace {
constexpr const char* kNamespace = "cam.ricoh";
constexpr const char* kProfileKey = "profile";

bool sameBleAddress(const String& left, const String& right) {
  return left.length() > 0 && right.length() > 0 && left.equalsIgnoreCase(right);
}

bool decodeAtomicProfile(const String& encoded, CameraProfile& profile) {
  if (encoded.length() == 0) {
    return false;
  }
  JsonDocument doc;
  if (deserializeJson(doc, encoded) != DeserializationError::Ok ||
      doc["profileVersion"].as<uint32_t>() != CAMERA_PROFILE_SCHEMA_VERSION) {
    return false;
  }

  profile = CameraProfile{};
  profile.profileVersion = doc["profileVersion"] | CAMERA_PROFILE_SCHEMA_VERSION;
  profile.cameraName = doc["cameraName"] | "";
  profile.bleAddress = doc["bleAddress"] | "";
  profile.bleAddressType = doc["bleAddressType"] | 0;
  profile.bleAddressTypeKnown = doc["bleAddressTypeKnown"] | false;
  profile.bleBonded = doc["bleBonded"] | false;
  profile.bleAuthenticated = doc["bleAuthenticated"] | false;
  profile.protocolGeneration =
      static_cast<RicohProtocolGeneration>(doc["generation"] | 0);
  profile.protocolGenerationKnown = doc["generationKnown"] | false;
  profile.securityProfile =
      static_cast<RicohSecurityProfileId>(doc["securityProfile"] | 0);
  profile.securityProfileKnown = doc["securityProfileKnown"] | false;
  profile.peerIdentityAddress = doc["peerIdentityAddress"] | "";
  profile.peerIdentityAddressType = doc["peerIdentityAddressType"] | 0;
  profile.peerIdentityKnown = doc["peerIdentityKnown"] | false;
  profile.lastSeenOtaAddress = doc["lastSeenOtaAddress"] | "";
  profile.lastSeenOtaAddressType = doc["lastSeenOtaAddressType"] | 0;
  profile.capabilityVersion = doc["capabilityVersion"] | CAMERA_CAPABILITY_SCHEMA_VERSION;

  JsonObjectConst wifi = doc["wifi"];
  profile.wifi.ssid = wifi["ssid"] | "";
  profile.wifi.passphrase = wifi["passphrase"] | "";
  profile.wifi.bssid = wifi["bssid"] | "";
  profile.wifi.cameraIp = wifi["cameraIp"] | "";
  profile.wifi.frequencyMhz = wifi["frequencyMhz"] | 0;
  profile.wifi.channel = wifi["channel"] | 0;
  profile.wifi.credentialsValid = wifi["credentialsValid"] | false;
  profile.wifi.source =
      static_cast<WifiCredentialSource>(wifi["source"] | 0);
  profile.wifi.cached = profile.wifi.credentialsValid &&
                        profile.wifi.ssid.length() > 0;
  return true;
}

String encodeAtomicProfile(const CameraProfile& profile) {
  JsonDocument doc;
  doc["profileVersion"] = CAMERA_PROFILE_SCHEMA_VERSION;
  doc["cameraName"] = profile.cameraName;
  doc["bleAddress"] = profile.bleAddress;
  doc["bleAddressType"] = profile.bleAddressType;
  doc["bleAddressTypeKnown"] = profile.bleAddressTypeKnown;
  doc["bleBonded"] = profile.bleBonded;
  doc["bleAuthenticated"] = profile.bleAuthenticated;
  doc["generation"] = static_cast<uint8_t>(profile.protocolGeneration);
  doc["generationKnown"] = profile.protocolGenerationKnown;
  doc["securityProfile"] = static_cast<uint8_t>(profile.securityProfile);
  doc["securityProfileKnown"] = profile.securityProfileKnown;
  doc["peerIdentityAddress"] = profile.peerIdentityAddress;
  doc["peerIdentityAddressType"] = profile.peerIdentityAddressType;
  doc["peerIdentityKnown"] = profile.peerIdentityKnown;
  doc["lastSeenOtaAddress"] = profile.lastSeenOtaAddress;
  doc["lastSeenOtaAddressType"] = profile.lastSeenOtaAddressType;
  doc["capabilityVersion"] = profile.capabilityVersion;
  JsonObject wifi = doc["wifi"].to<JsonObject>();
  wifi["ssid"] = profile.wifi.ssid;
  wifi["passphrase"] = profile.wifi.passphrase;
  wifi["bssid"] = profile.wifi.bssid;
  wifi["cameraIp"] = profile.wifi.cameraIp;
  wifi["frequencyMhz"] = profile.wifi.frequencyMhz;
  wifi["channel"] = profile.wifi.channel;
  wifi["credentialsValid"] = profile.wifi.credentialsValid;
  wifi["source"] = static_cast<uint8_t>(profile.wifi.source);

  String encoded;
  serializeJson(doc, encoded);
  return encoded;
}
}  // namespace

bool CameraProfileStore::begin() {
  if (_begun) {
    return true;
  }
  _begun = _prefs.begin(kNamespace, false);
  return _begun;
}

bool CameraProfileStore::load(CameraProfile& profile) {
  if (!begin()) {
    return false;
  }
  const String encoded = _prefs.isKey(kProfileKey) ? _prefs.getString(kProfileKey, "") : String();
  if (!decodeAtomicProfile(encoded, profile)) {
    profile = CameraProfile{};
  }
  return true;
}

bool CameraProfileStore::save(const CameraProfile& profile) {
  if (!begin()) {
    return false;
  }
  const String encoded = encodeAtomicProfile(profile);
  return encoded.length() > 0 && _prefs.putString(kProfileKey, encoded) == encoded.length();
}

bool CameraProfileStore::saveWifiCredentials(const String& bleAddress, const WifiCredential& wifi) {
  if (bleAddress.length() == 0 || wifi.ssid.length() == 0 || !wifi.credentialsValid) {
    return false;
  }
  CameraProfile profile;
  if (!load(profile) || !sameBleAddress(profile.bleAddress, bleAddress)) {
    return false;
  }
  profile.wifi = wifi;
  profile.wifi.cached = true;
  return save(profile);
}

bool CameraProfileStore::clearProfile() {
  if (!begin()) {
    return false;
  }
  return !_prefs.isKey(kProfileKey) || _prefs.remove(kProfileKey);
}
