#include "camera_identity.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cstring>

namespace {

bool startsWith(const char* value, const char* prefix) {
  return std::strncmp(value, prefix, std::strlen(prefix)) == 0;
}

bool allDigits(const char* value) {
  if (value[0] == '\0') {
    return false;
  }
  for (const char* cursor = value; *cursor != '\0'; ++cursor) {
    if (!std::isdigit(static_cast<unsigned char>(*cursor))) {
      return false;
    }
  }
  return true;
}

}  // namespace

std::string deriveBleNameFromWifiSsid(const char* ssid) {
  if (ssid == nullptr || !startsWith(ssid, "GR_")) {
    return std::string();
  }

  const size_t prefixLen = startsWith(ssid, "GR_H") ? 4 : 3;
  const char* suffix = ssid + prefixLen;
  if (suffix[0] == '\0' || !allDigits(suffix)) {
    return std::string(ssid);
  }

  const uint32_t serial = static_cast<uint32_t>(std::strtoul(suffix, nullptr, 10));
  char name[20];
  std::snprintf(name,
                sizeof(name),
                "%.*s%0*lu",
                static_cast<int>(prefixLen),
                ssid,
                static_cast<int>(std::strlen(suffix)),
                static_cast<unsigned long>(serial + 1));
  return std::string(name);
}
