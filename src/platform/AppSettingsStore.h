#pragma once

#include <Preferences.h>

// Device settings that belong to the app, not to any camera binding, so a
// pairing reset leaves them alone. NVS namespace "app".
class AppSettingsStore {
public:
  bool loadDisplayMirror(bool& mirrored);
  bool saveDisplayMirror(bool mirrored);

private:
  bool begin();

  Preferences _prefs;
  bool _begun = false;
};
