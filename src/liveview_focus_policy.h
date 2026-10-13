#pragma once

#include <cstddef>
#include <cstdio>
#include <cstring>

// R-Core GR IV 1.11 webapid: the only writable lens field is focusSetting,
// sent as an unescaped key=value body on PUT /v1/params/lens. spot is the
// documented single-point (center) AF mode. Legal values at runtime come from
// /v1/props focusSettingList.
constexpr const char* LIVEVIEW_SPOT_FOCUS_SETTING = "spot";

inline bool liveviewFocusSettingAllowed(const char* setting) {
  if (setting == nullptr || setting[0] == '\0') {
    return false;
  }
  static const char* const kAllowed[] = {
      "manual",
      "snap",
      "inf",
      "multiauto",
      "spot",
      "pinpoint",
      "tracking",
      "continuous",
      "multiauto_center",
      "zone_select",
  };
  for (size_t i = 0; i < sizeof(kAllowed) / sizeof(kAllowed[0]); ++i) {
    if (std::strcmp(setting, kAllowed[i]) == 0) {
      return true;
    }
  }
  return false;
}

inline bool liveviewFocusSettingIsCenterPoint(const char* setting) {
  return setting != nullptr &&
         (std::strcmp(setting, "spot") == 0 || std::strcmp(setting, "pinpoint") == 0 ||
          std::strcmp(setting, "multiauto_center") == 0);
}

inline bool formatLiveviewFocusSettingBody(char* dest, size_t destSize, const char* setting) {
  if (dest == nullptr || destSize < 32 || !liveviewFocusSettingAllowed(setting)) {
    return false;
  }
  const int written = snprintf(dest, destSize, "focusSetting=%s", setting);
  return written > 0 && static_cast<size_t>(written) < destSize;
}

// GR IIIx v1.60 webapid: POST /v1/camera/shoot parses form body keys, not
// query and not JSON. af=off maps to 1 (default), af=on maps to 2, anything
// else is an error. Empty body therefore does not request AF. pos is an
// IntPairString; the parser's default branch writes 50,50. Coordinate units
// and range are unverified on GR III; 50,50 is the firmware default, not a
// proven center in a 0-1000 space.
constexpr const char* LIVEVIEW_SHOOT_AF_ON_BODY = "af=on&pos=50,50";

inline bool formatLiveviewShootAfOnBody(char* dest, size_t destSize) {
  if (dest == nullptr || destSize < 24) {
    return false;
  }
  const int written = snprintf(dest, destSize, "%s", LIVEVIEW_SHOOT_AF_ON_BODY);
  return written > 0 && static_cast<size_t>(written) < destSize;
}

// GR cameras often keep HTTP 200 and put the real result in JSON errMsg.
// Missing errMsg (empty body) is treated as success, matching GR II Remote.
inline bool ricohHttpJsonSucceeded(const char* json) {
  if (json == nullptr || json[0] == '\0') {
    return true;
  }

  const char key[] = "\"errMsg\"";
  const char* keyPosition = std::strstr(json, key);
  if (keyPosition == nullptr) {
    return true;
  }

  const char* cursor = keyPosition + sizeof(key) - 1;
  while (*cursor != '\0' && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
    ++cursor;
  }
  if (*cursor != ':') {
    return true;
  }
  ++cursor;
  while (*cursor != '\0' && (*cursor == ' ' || *cursor == '\t' || *cursor == '\r' || *cursor == '\n')) {
    ++cursor;
  }
  if (*cursor != '"') {
    return true;
  }
  ++cursor;
  const char* valueStart = cursor;
  while (*cursor != '\0' && *cursor != '"') {
    ++cursor;
  }
  if (*cursor != '"') {
    return true;
  }
  const size_t valueLength = static_cast<size_t>(cursor - valueStart);
  return valueLength == 2 && valueStart[0] == 'O' && valueStart[1] == 'K';
}
