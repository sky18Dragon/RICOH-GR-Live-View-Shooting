#include "AppSettingsStore.h"

namespace {
constexpr const char* kNamespace = "app";
constexpr const char* kDisplayMirrorKey = "disp_mirror";
}  // namespace

bool AppSettingsStore::begin() {
  if (_begun) {
    return true;
  }
  _begun = _prefs.begin(kNamespace, false);
  return _begun;
}

bool AppSettingsStore::loadDisplayMirror(bool& mirrored) {
  if (!begin()) {
    return false;
  }
  mirrored = _prefs.getBool(kDisplayMirrorKey, false);
  return true;
}

bool AppSettingsStore::saveDisplayMirror(bool mirrored) {
  if (!begin()) {
    return false;
  }
  return _prefs.putBool(kDisplayMirrorKey, mirrored) == sizeof(mirrored);
}
