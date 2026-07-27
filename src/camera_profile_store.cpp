#include "camera_profile_store.h"

#include <ArduinoJson.h>

namespace {
constexpr const char* kNamespace = "ricoh2";
constexpr const char* kAtomicProfileKey = "profile_v4";

String getStringIfPresent(Preferences& prefs, const char* key) {
  return prefs.isKey(key) ? prefs.getString(key, "") : String();
}

void clearWifiCredentialKeys(Preferences& prefs) {
  prefs.remove("wifi_valid");
  prefs.remove("wifi_ble_addr");
  prefs.remove("wifi_ssid");
  prefs.remove("wifi_pass");
  prefs.remove("wifi_bssid");
  prefs.remove("wifi_freq");
  prefs.remove("wifi_ch");
  prefs.remove("wifi_src");
  prefs.remove("wifi_cred_ok");
}

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

  if (decodeAtomicProfile(getStringIfPresent(_prefs, kAtomicProfileKey), profile)) {
    return true;
  }

  profile = CameraProfile{};
  StoredCameraProfileMetadata storedMetadata;
  storedMetadata.schemaVersion = _prefs.getUInt("proto_ver", 3);
  const String legacyBleAddress = getStringIfPresent(_prefs, "ble_addr");
  storedMetadata.legacyBleIdentityPresent = legacyBleAddress.length() > 0;
  storedMetadata.protocolGenerationPresent = _prefs.isKey("cam_gen") || _prefs.isKey("proto_gen");
  storedMetadata.protocolGenerationValue = _prefs.isKey("cam_gen")
                                             ? _prefs.getUInt("cam_gen", 0)
                                             : _prefs.getUInt("proto_gen", 0);
  storedMetadata.securityProfilePresent = _prefs.isKey("sec_profile");
  storedMetadata.securityProfileValue = _prefs.getUInt("sec_profile", 0);
  storedMetadata.bleAuthenticatedPresent = _prefs.isKey("ble_auth");
  storedMetadata.bleAuthenticatedValue = _prefs.getBool("ble_auth", false);
  storedMetadata.capabilityVersionPresent = _prefs.isKey("cap_ver");
  storedMetadata.capabilityVersionValue = _prefs.getUInt("cap_ver", 0);
  storedMetadata.wifiSourcePresent = _prefs.isKey("wifi_src");
  storedMetadata.wifiSourceValue = _prefs.getUInt("wifi_src", 0);
  storedMetadata.wifiCredentialValidityPresent = _prefs.isKey("wifi_cred_ok");
  storedMetadata.wifiCredentialValidityValue = _prefs.getBool("wifi_cred_ok", false);
  storedMetadata.legacyWifiValid = _prefs.getBool("wifi_valid", false);
  const CameraProfileMetadata metadata = decodeCameraProfileMetadata(storedMetadata);

  profile.profileVersion = metadata.schemaVersion;
  profile.protocolGeneration = static_cast<RicohProtocolGeneration>(metadata.protocolGeneration);
  profile.protocolGenerationKnown = metadata.protocolGenerationKnown;
  profile.securityProfile = static_cast<RicohSecurityProfileId>(metadata.securityProfile);
  profile.securityProfileKnown = metadata.securityProfileKnown;
  profile.bleAuthenticated = metadata.bleAuthenticated;
  profile.capabilityVersion = metadata.capabilityVersion;
  profile.wifi.source = metadata.wifiSource;
  profile.wifi.credentialsValid = metadata.wifiCredentialsValid;
  profile.cameraName = getStringIfPresent(_prefs, "cam_name");
  profile.bleAddress = legacyBleAddress;
  profile.bleAddressTypeKnown = profile.bleAddress.length() > 0 && _prefs.isKey("ble_addr_type");
  profile.bleAddressType = profile.bleAddressTypeKnown ? static_cast<uint8_t>(_prefs.getUInt("ble_addr_type", 0)) : 0;
  profile.bleBonded = profile.bleAddress.length() > 0 && _prefs.getBool("ble_bonded", false);
  profile.peerIdentityAddress = getStringIfPresent(_prefs, "peer_id_addr");
  profile.peerIdentityKnown = _prefs.getBool("peer_id_known", false) &&
                              profile.peerIdentityAddress.length() > 0;
  profile.peerIdentityAddressType = profile.peerIdentityKnown
                                      ? static_cast<uint8_t>(_prefs.getUInt("peer_id_type", 0))
                                      : 0;
  profile.lastSeenOtaAddress = getStringIfPresent(_prefs, "last_ota_addr");
  profile.lastSeenOtaAddressType = profile.lastSeenOtaAddress.length() > 0
                                    ? static_cast<uint8_t>(_prefs.getUInt("last_ota_type", 0))
                                    : 0;
  if (metadata.migratedLegacyGr4) {
    profile.peerIdentityAddress = profile.bleAddress;
    profile.peerIdentityAddressType = profile.bleAddressType;
    profile.peerIdentityKnown = profile.bleAddressTypeKnown;
  }
  if (profile.lastSeenOtaAddress.length() == 0) {
    profile.lastSeenOtaAddress = profile.bleAddress;
    profile.lastSeenOtaAddressType = profile.bleAddressType;
  }
  profile.wifi.cameraIp = getStringIfPresent(_prefs, "cam_ip");

  const String wifiBleAddress = getStringIfPresent(_prefs, "wifi_ble_addr");
  if (_prefs.getBool("wifi_valid", false) && sameBleAddress(profile.bleAddress, wifiBleAddress)) {
    profile.wifi.ssid = getStringIfPresent(_prefs, "wifi_ssid");
    profile.wifi.passphrase = getStringIfPresent(_prefs, "wifi_pass");
    profile.wifi.bssid = getStringIfPresent(_prefs, "wifi_bssid");
    profile.wifi.frequencyMhz = static_cast<uint16_t>(_prefs.getUInt("wifi_freq", 0));
    profile.wifi.channel = static_cast<uint8_t>(_prefs.getUInt("wifi_ch", 0));
    profile.wifi.cached = profile.wifi.credentialsValid && profile.wifi.ssid.length() > 0;
  }
  if (metadata.migratedLegacyGr4) {
    // Persist the migration without touching legacy Wi-Fi keys or NimBLE bonds.
    (void)save(profile);
  }
  return true;
}

bool CameraProfileStore::save(const CameraProfile& profile) {
  if (!begin()) {
    return false;
  }

  CameraProfileMetadata metadata;
  metadata.protocolGeneration = static_cast<uint8_t>(profile.protocolGeneration);
  metadata.protocolGenerationKnown = profile.protocolGenerationKnown;
  metadata.securityProfile = static_cast<uint8_t>(profile.securityProfile);
  metadata.securityProfileKnown = profile.securityProfileKnown;
  metadata.bleAuthenticated = profile.bleAuthenticated;
  metadata.capabilityVersion = profile.capabilityVersion;
  metadata.wifiSource = profile.wifi.source;
  metadata.wifiCredentialsValid = profile.wifi.credentialsValid;
  const StoredCameraProfileMetadata storedMetadata = encodeCameraProfileMetadata(metadata);

  // One NVS entry is the authoritative v4 snapshot. The individual keys below
  // remain a compatibility mirror for older firmware and migration tooling.
  const String atomicProfile = encodeAtomicProfile(profile);
  if (atomicProfile.length() == 0 ||
      _prefs.putString(kAtomicProfileKey, atomicProfile) != atomicProfile.length()) {
    return false;
  }

  _prefs.putUInt("proto_ver", storedMetadata.schemaVersion);
  if (storedMetadata.protocolGenerationPresent) {
    _prefs.putUInt("cam_gen", storedMetadata.protocolGenerationValue);
    _prefs.putUInt("proto_gen", storedMetadata.protocolGenerationValue);
  } else {
    _prefs.remove("cam_gen");
    _prefs.remove("proto_gen");
  }
  if (storedMetadata.securityProfilePresent) {
    _prefs.putUInt("sec_profile", storedMetadata.securityProfileValue);
  } else {
    _prefs.remove("sec_profile");
  }
  _prefs.putBool("ble_auth", storedMetadata.bleAuthenticatedValue);
  _prefs.putUInt("cap_ver", storedMetadata.capabilityVersionValue);
  if (storedMetadata.wifiSourcePresent) {
    _prefs.putUInt("wifi_src", storedMetadata.wifiSourceValue);
  } else {
    _prefs.remove("wifi_src");
  }
  _prefs.putBool("wifi_cred_ok", storedMetadata.wifiCredentialValidityValue);
  _prefs.putString("cam_name", profile.cameraName);
  _prefs.putString("ble_addr", profile.bleAddress);
  if (profile.bleAddress.length() > 0 && profile.bleAddressTypeKnown) {
    _prefs.putUInt("ble_addr_type", profile.bleAddressType);
  } else {
    _prefs.remove("ble_addr_type");
  }
  _prefs.putBool("ble_bonded", profile.bleAddress.length() > 0 && profile.bleBonded);
  if (profile.peerIdentityKnown && profile.peerIdentityAddress.length() > 0) {
    _prefs.putString("peer_id_addr", profile.peerIdentityAddress);
    _prefs.putUInt("peer_id_type", profile.peerIdentityAddressType);
    _prefs.putBool("peer_id_known", true);
  } else {
    _prefs.remove("peer_id_addr");
    _prefs.remove("peer_id_type");
    _prefs.putBool("peer_id_known", false);
  }
  if (profile.lastSeenOtaAddress.length() > 0) {
    _prefs.putString("last_ota_addr", profile.lastSeenOtaAddress);
    _prefs.putUInt("last_ota_type", profile.lastSeenOtaAddressType);
  } else {
    _prefs.remove("last_ota_addr");
    _prefs.remove("last_ota_type");
  }
  _prefs.putString("cam_ip", profile.wifi.cameraIp);
  if (profile.bleAddress.length() > 0 &&
      profile.wifi.ssid.length() > 0 &&
      profile.wifi.credentialsValid) {
    _prefs.putBool("wifi_valid", true);
    _prefs.putString("wifi_ble_addr", profile.bleAddress);
    _prefs.putString("wifi_ssid", profile.wifi.ssid);
    _prefs.putString("wifi_pass", profile.wifi.passphrase);
    _prefs.putString("wifi_bssid", profile.wifi.bssid);
    _prefs.putUInt("wifi_freq", profile.wifi.frequencyMhz);
    _prefs.putUInt("wifi_ch", profile.wifi.channel);
  }
  return true;
}

bool CameraProfileStore::saveWifiCredentials(const String& bleAddress, const WifiCredential& wifi) {
  if (!begin()) {
    return false;
  }
  if (bleAddress.length() == 0 || wifi.ssid.length() == 0 || !wifi.credentialsValid) {
    clearWifiCredentialKeys(_prefs);
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

bool CameraProfileStore::clearWifiCredentials() {
  if (!begin()) {
    return false;
  }
  CameraProfile profile;
  if (!load(profile)) {
    return false;
  }
  profile.wifi = WifiCredential{};
  clearWifiCredentialKeys(_prefs);
  return save(profile);
}

bool CameraProfileStore::clearBlePairing() {
  if (!begin()) {
    return false;
  }

  _prefs.remove("cam_name");
  _prefs.remove("ble_addr");
  _prefs.remove("ble_addr_type");
  _prefs.remove("ble_bonded");
  _prefs.remove("ble_auth");
  _prefs.remove("cam_gen");
  _prefs.remove("proto_gen");
  _prefs.remove("sec_profile");
  _prefs.remove("peer_id_addr");
  _prefs.remove("peer_id_type");
  _prefs.remove("peer_id_known");
  _prefs.remove("last_ota_addr");
  _prefs.remove("last_ota_type");
  _prefs.remove("cap_ver");
  _prefs.remove(kAtomicProfileKey);
  clearWifiCredentialKeys(_prefs);
  return true;
}

bool CameraProfileStore::saveBleIdentity(const String& cameraName, const String& bleAddress) {
  if (!begin()) {
    return false;
  }
  if (cameraName.length() > 0) {
    _prefs.putString("cam_name", cameraName);
  }
  if (bleAddress.length() > 0) {
    const String previousWifiBle = getStringIfPresent(_prefs, "wifi_ble_addr");
    _prefs.putString("ble_addr", bleAddress);
    _prefs.remove("ble_addr_type");
    _prefs.putBool("ble_bonded", false);
    if (previousWifiBle.length() > 0 && !sameBleAddress(previousWifiBle, bleAddress)) {
      clearWifiCredentialKeys(_prefs);
    }
  }
  return true;
}

bool CameraProfileStore::saveBleIdentity(const String& cameraName,
                                         const String& bleAddress,
                                         uint8_t bleAddressType,
                                         bool bleBonded) {
  if (!begin()) {
    return false;
  }
  if (cameraName.length() > 0) {
    _prefs.putString("cam_name", cameraName);
  }
  if (bleAddress.length() > 0) {
    const String previousWifiBle = getStringIfPresent(_prefs, "wifi_ble_addr");
    _prefs.putString("ble_addr", bleAddress);
    _prefs.putUInt("ble_addr_type", bleAddressType);
    _prefs.putBool("ble_bonded", bleBonded);
    if (previousWifiBle.length() > 0 && !sameBleAddress(previousWifiBle, bleAddress)) {
      clearWifiCredentialKeys(_prefs);
    }
  }
  return true;
}

bool CameraProfileStore::clear() {
  if (!begin()) {
    return false;
  }
  return _prefs.clear();
}
