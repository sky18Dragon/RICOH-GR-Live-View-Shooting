#include "Gr2HttpShutter.h"

#include <Arduino.h>

#include "../../../gr_api.h"
#include "Gr2ShutterPolicy.h"

bool shootGr2(GrApi& api, uint32_t timeoutMs) {
  String body;
  if (!api.postCommand(GR2_SHUTTER_ONESHOT_PATH, timeoutMs, body)) {
    return false;
  }

  // GR Remote treats errMsg=OK as the completed one-shot path. Some GR II
  // states instead return HTTP 200 with "Precondition Failed" in the JSON and
  // require the explicit press/release pair used by older firmware.
  const Gr2InitialShutterDecision decision = evaluateGr2InitialShutterResponse(body.c_str());
  if (decision.action == Gr2InitialShutterAction::Complete) {
    api.setError("");
    return true;
  }
  if (decision.action == Gr2InitialShutterAction::Fail) {
    api.setError(decision.errorMessage.c_str());
    return false;
  }

  if (!api.postCommand(GR2_SHUTTER_START_PATH, timeoutMs, body)) {
    return false;
  }
  std::string phaseError;
  if (!gr2ShutterPhaseSucceeded(body.c_str(), "start", phaseError)) {
    api.setError(phaseError.c_str());
    return false;
  }
  delay(80);
  if (!api.postCommand(GR2_SHUTTER_FINISH_PATH, timeoutMs, body)) {
    return false;
  }
  if (!gr2ShutterPhaseSucceeded(body.c_str(), "finish", phaseError)) {
    api.setError(phaseError.c_str());
    return false;
  }

  api.setError("");
  return true;
}
