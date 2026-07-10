#include "camera_profile_store.h"

namespace {
constexpr const char* kNamespace = "ricoh2";

String getStringIfPresent(Preferences& prefs, const char* key) {
  return prefs.isKey(key) ? prefs.getString(key, "") : String();
}

void logWriteFailure(const char* key) {
  Serial.printf("Profile: NVS write failed key='%s'\n",
                key != nullptr ? key : "");
}

bool putUIntChecked(Preferences& prefs, const char* key, uint32_t value) {
  const bool ok = prefs.putUInt(key, value) == sizeof(value);
  if (!ok) {
    logWriteFailure(key);
  }
  return ok;
}

bool putUCharChecked(Preferences& prefs, const char* key, uint8_t value) {
  const bool ok = prefs.putUChar(key, value) == sizeof(value);
  if (!ok) {
    logWriteFailure(key);
  }
  return ok;
}

bool putBoolChecked(Preferences& prefs, const char* key, bool value) {
  const bool ok = prefs.putBool(key, value) == sizeof(value);
  if (!ok) {
    logWriteFailure(key);
  }
  return ok;
}

bool putStringChecked(Preferences& prefs,
                      const char* key,
                      const String& value) {
  const size_t written = prefs.putString(key, value);
  const bool ok = written == value.length() &&
                  prefs.isKey(key) &&
                  prefs.getString(key, "") == value;
  if (!ok) {
    logWriteFailure(key);
  }
  return ok;
}

bool removeChecked(Preferences& prefs, const char* key) {
  if (!prefs.isKey(key)) {
    return true;
  }
  const bool ok = prefs.remove(key);
  if (!ok) {
    Serial.printf("Profile: NVS remove failed key='%s'\n", key);
  }
  return ok;
}

bool clearWifiCredentialKeys(Preferences& prefs) {
  bool ok = true;
  ok = removeChecked(prefs, "wifi_valid") && ok;
  ok = removeChecked(prefs, "wifi_ble_addr") && ok;
  ok = removeChecked(prefs, "wifi_ssid") && ok;
  ok = removeChecked(prefs, "wifi_pass") && ok;
  ok = removeChecked(prefs, "wifi_bssid") && ok;
  ok = removeChecked(prefs, "wifi_freq") && ok;
  ok = removeChecked(prefs, "wifi_ch") && ok;
  return ok;
}

bool sameBleAddress(const String& left, const String& right) {
  return left.length() > 0 && right.length() > 0 &&
         left.equalsIgnoreCase(right);
}

bool persistCameraProfile(Preferences& prefs, const CameraProfile& profile) {
  bool ok = true;
  ok = putUIntChecked(prefs, "proto_ver", profile.profileVersion) && ok;
  ok = putUCharChecked(prefs,
                       "camera_model",
                       static_cast<uint8_t>(profile.model)) && ok;
  ok = putStringChecked(prefs, "cam_name", profile.cameraName) && ok;
  ok = putStringChecked(prefs, "ble_addr", profile.bleAddress) && ok;
  if (profile.bleAddress.length() > 0 && profile.bleAddressTypeKnown) {
    ok = putUCharChecked(prefs,
                         "ble_addr_type",
                         profile.bleAddressType) && ok;
  } else {
    ok = removeChecked(prefs, "ble_addr_type") && ok;
  }
  ok = putBoolChecked(prefs,
                      "ble_bonded",
                      profile.bleAddress.length() > 0 &&
                          profile.bleBonded) && ok;
  ok = putStringChecked(prefs, "cam_ip", profile.wifi.cameraIp) && ok;
  return ok;
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

  profile = CameraProfile{};
  const uint32_t storedProfileVersion = _prefs.getUInt("proto_ver", 3);
  const bool hasCameraModelKey = _prefs.isKey("camera_model");
  const uint8_t rawCameraModel = hasCameraModelKey
                                   ? _prefs.getUChar("camera_model", 0)
                                   : 0;
  profile.model = rvf::cameraModelFromStoredProfile(hasCameraModelKey,
                                                    storedProfileVersion,
                                                    rawCameraModel);
  profile.cameraName = getStringIfPresent(_prefs, "cam_name");
  profile.bleAddress = getStringIfPresent(_prefs, "ble_addr");
  profile.bleAddressTypeKnown = profile.bleAddress.length() > 0 &&
                                _prefs.isKey("ble_addr_type");
  profile.bleAddressType = profile.bleAddressTypeKnown
                             ? _prefs.getUChar("ble_addr_type", 0)
                             : 0;
  profile.bleBonded = profile.bleAddress.length() > 0 &&
                      _prefs.getBool("ble_bonded", false);
  profile.wifi.cameraIp = getStringIfPresent(_prefs, "cam_ip");

  const String wifiBleAddress =
      getStringIfPresent(_prefs, "wifi_ble_addr");
  if (_prefs.getBool("wifi_valid", false) &&
      sameBleAddress(profile.bleAddress, wifiBleAddress)) {
    profile.wifi.ssid = getStringIfPresent(_prefs, "wifi_ssid");
    profile.wifi.passphrase = getStringIfPresent(_prefs, "wifi_pass");
    profile.wifi.bssid = getStringIfPresent(_prefs, "wifi_bssid");
    profile.wifi.frequencyMhz =
        static_cast<uint16_t>(_prefs.getUInt("wifi_freq", 0));
    profile.wifi.channel =
        static_cast<uint8_t>(_prefs.getUInt("wifi_ch", 0));
    profile.wifi.cached = profile.wifi.ssid.length() > 0;
  }
  return true;
}

bool CameraProfileStore::save(const CameraProfile& profile) {
  if (!begin()) {
    return false;
  }
  return persistCameraProfile(_prefs, profile);
}

bool CameraProfileStore::saveConnectedCamera(const CameraProfile& profile) {
  if (!begin()) {
    return false;
  }

  const String previousWifiBle =
      getStringIfPresent(_prefs, "wifi_ble_addr");
  bool ok = persistCameraProfile(_prefs, profile);
  if (previousWifiBle.length() > 0 &&
      !sameBleAddress(previousWifiBle, profile.bleAddress)) {
    ok = clearWifiCredentialKeys(_prefs) && ok;
  }
  return ok;
}

bool CameraProfileStore::saveWifiCredentials(const String& bleAddress,
                                             const WifiCredential& wifi) {
  if (!begin()) {
    return false;
  }
  if (bleAddress.length() == 0 || wifi.ssid.length() == 0) {
    clearWifiCredentialKeys(_prefs);
    return false;
  }

  bool ok = putBoolChecked(_prefs, "wifi_valid", false);
  ok = putStringChecked(_prefs, "wifi_ble_addr", bleAddress) && ok;
  ok = putStringChecked(_prefs, "wifi_ssid", wifi.ssid) && ok;
  ok = putStringChecked(_prefs, "wifi_pass", wifi.passphrase) && ok;
  ok = putStringChecked(_prefs, "wifi_bssid", wifi.bssid) && ok;
  ok = putUIntChecked(_prefs, "wifi_freq", wifi.frequencyMhz) && ok;
  ok = putUIntChecked(_prefs, "wifi_ch", wifi.channel) && ok;
  if (ok) {
    ok = putBoolChecked(_prefs, "wifi_valid", true);
  }
  return ok;
}

bool CameraProfileStore::clearWifiCredentials() {
  if (!begin()) {
    return false;
  }
  return clearWifiCredentialKeys(_prefs);
}

bool CameraProfileStore::clearBlePairing() {
  if (!begin()) {
    return false;
  }

  bool ok = true;
  ok = removeChecked(_prefs, "cam_name") && ok;
  ok = removeChecked(_prefs, "camera_model") && ok;
  ok = removeChecked(_prefs, "ble_addr") && ok;
  ok = removeChecked(_prefs, "ble_addr_type") && ok;
  ok = removeChecked(_prefs, "ble_bonded") && ok;
  ok = clearWifiCredentialKeys(_prefs) && ok;
  return ok;
}

bool CameraProfileStore::clear() {
  if (!begin()) {
    return false;
  }
  return _prefs.clear();
}
