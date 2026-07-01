#include "camera_profile_store.h"

namespace {
constexpr const char* kNamespace = "ricoh2";

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
}

bool sameBleAddress(const String& left, const String& right) {
  return left.length() > 0 && right.length() > 0 && left.equalsIgnoreCase(right);
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
  profile.profileVersion = _prefs.getUInt("proto_ver", 3);
  profile.cameraName = getStringIfPresent(_prefs, "cam_name");
  profile.bleAddress = getStringIfPresent(_prefs, "ble_addr");
  profile.bleAddressTypeKnown = profile.bleAddress.length() > 0 && _prefs.isKey("ble_addr_type");
  profile.bleAddressType = profile.bleAddressTypeKnown ? static_cast<uint8_t>(_prefs.getUInt("ble_addr_type", 0)) : 0;
  profile.bleBonded = profile.bleAddress.length() > 0 && _prefs.getBool("ble_bonded", false);
  profile.wifi.cameraIp = getStringIfPresent(_prefs, "cam_ip");

  const String wifiBleAddress = getStringIfPresent(_prefs, "wifi_ble_addr");
  if (_prefs.getBool("wifi_valid", false) && sameBleAddress(profile.bleAddress, wifiBleAddress)) {
    profile.wifi.ssid = getStringIfPresent(_prefs, "wifi_ssid");
    profile.wifi.passphrase = getStringIfPresent(_prefs, "wifi_pass");
    profile.wifi.bssid = getStringIfPresent(_prefs, "wifi_bssid");
    profile.wifi.frequencyMhz = static_cast<uint16_t>(_prefs.getUInt("wifi_freq", 0));
    profile.wifi.channel = static_cast<uint8_t>(_prefs.getUInt("wifi_ch", 0));
    profile.wifi.cached = profile.wifi.ssid.length() > 0;
  }
  return true;
}

bool CameraProfileStore::save(const CameraProfile& profile) {
  if (!begin()) {
    return false;
  }

  _prefs.putUInt("proto_ver", profile.profileVersion);
  _prefs.putString("cam_name", profile.cameraName);
  _prefs.putString("ble_addr", profile.bleAddress);
  if (profile.bleAddress.length() > 0 && profile.bleAddressTypeKnown) {
    _prefs.putUInt("ble_addr_type", profile.bleAddressType);
  } else {
    _prefs.remove("ble_addr_type");
  }
  _prefs.putBool("ble_bonded", profile.bleAddress.length() > 0 && profile.bleBonded);
  _prefs.putString("cam_ip", profile.wifi.cameraIp);
  return true;
}

bool CameraProfileStore::saveWifiCredentials(const String& bleAddress, const WifiCredential& wifi) {
  if (!begin()) {
    return false;
  }
  if (bleAddress.length() == 0 || wifi.ssid.length() == 0) {
    clearWifiCredentialKeys(_prefs);
    return false;
  }

  _prefs.putBool("wifi_valid", true);
  _prefs.putString("wifi_ble_addr", bleAddress);
  _prefs.putString("wifi_ssid", wifi.ssid);
  _prefs.putString("wifi_pass", wifi.passphrase);
  _prefs.putString("wifi_bssid", wifi.bssid);
  _prefs.putUInt("wifi_freq", wifi.frequencyMhz);
  _prefs.putUInt("wifi_ch", wifi.channel);
  return true;
}

bool CameraProfileStore::clearWifiCredentials() {
  if (!begin()) {
    return false;
  }
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
