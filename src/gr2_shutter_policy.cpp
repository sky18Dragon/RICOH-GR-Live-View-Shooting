#include "gr2_shutter_policy.h"

#include <cctype>
#include <cstring>

namespace {

bool extractJsonString(const char* json, const char* key, std::string& value) {
  value.clear();
  if (json == nullptr || key == nullptr || key[0] == '\0') {
    return false;
  }

  const std::string quotedKey = std::string("\"") + key + "\"";
  const char* keyPosition = std::strstr(json, quotedKey.c_str());
  if (keyPosition == nullptr) {
    return false;
  }

  const char* cursor = keyPosition + quotedKey.length();
  while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor))) {
    ++cursor;
  }
  if (*cursor != ':') {
    return false;
  }
  ++cursor;
  while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor))) {
    ++cursor;
  }
  if (*cursor != '"') {
    return false;
  }

  ++cursor;
  bool escaped = false;
  for (; *cursor != '\0'; ++cursor) {
    const char character = *cursor;
    if (escaped) {
      value.push_back(character);
      escaped = false;
    } else if (character == '\\') {
      escaped = true;
    } else if (character == '"') {
      return true;
    } else {
      value.push_back(character);
    }
  }
  value.clear();
  return false;
}

}  // namespace

Gr2InitialShutterDecision evaluateGr2InitialShutterResponse(const char* responseBody) {
  std::string errMsg;
  if (!extractJsonString(responseBody, "errMsg", errMsg) || errMsg == "OK") {
    return {Gr2InitialShutterAction::Complete, std::string()};
  }
  if (errMsg == "Precondition Failed") {
    return {Gr2InitialShutterAction::PressAndRelease, std::string()};
  }
  return {Gr2InitialShutterAction::Fail, std::string("GR II shoot failed: ") + errMsg};
}

bool gr2ShutterPhaseSucceeded(const char* responseBody,
                             const char* phaseName,
                             std::string& errorMessage) {
  std::string errMsg;
  if (!extractJsonString(responseBody, "errMsg", errMsg) || errMsg == "OK") {
    errorMessage.clear();
    return true;
  }

  errorMessage = "GR II shoot ";
  errorMessage += phaseName != nullptr ? phaseName : "phase";
  errorMessage += " failed: ";
  errorMessage += errMsg;
  return false;
}
