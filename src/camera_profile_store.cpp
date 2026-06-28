#include "camera_profile_store.h"

namespace {
constexpr const char* kNamespace = "ricoh2";

String getStringIfPresent(Preferences& prefs, const char* key) {
  return prefs.isKey(key) ? prefs.getString(key, "") : String();
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
  profile.wifi.cameraIp = getStringIfPresent(_prefs, "cam_ip");
  return true;
}

bool CameraProfileStore::save(const CameraProfile& profile) {
  if (!begin()) {
    return false;
  }

  _prefs.putUInt("proto_ver", profile.profileVersion);
  _prefs.putString("cam_name", profile.cameraName);
  _prefs.putString("ble_addr", profile.bleAddress);
  _prefs.putString("cam_ip", profile.wifi.cameraIp);
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
    _prefs.putString("ble_addr", bleAddress);
  }
  return true;
}

bool CameraProfileStore::clear() {
  if (!begin()) {
    return false;
  }
  return _prefs.clear();
}
