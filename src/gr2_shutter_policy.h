#pragma once

#include <string>

static const char GR2_SHUTTER_ONESHOT_PATH[] = "/v1/camera/shoot?af=camera";
static const char GR2_SHUTTER_START_PATH[] = "/v1/camera/shoot/start?af=camera";
static const char GR2_SHUTTER_FINISH_PATH[] = "/v1/camera/shoot/finish";

enum class Gr2InitialShutterAction {
  Complete,
  PressAndRelease,
  Fail,
};

struct Gr2InitialShutterDecision {
  Gr2InitialShutterDecision(Gr2InitialShutterAction selectedAction,
                            const std::string& selectedErrorMessage)
      : action(selectedAction), errorMessage(selectedErrorMessage) {}

  Gr2InitialShutterAction action;
  std::string errorMessage;
};

// Interpret the JSON bodies returned by the GR II camera/shoot endpoint.
// Missing errMsg remains a success because some firmware returns an empty body.
Gr2InitialShutterDecision evaluateGr2InitialShutterResponse(const char* responseBody);

// Interpret the camera/shoot/start and camera/shoot/finish response bodies.
// Missing errMsg remains a success for compatibility with empty legacy replies.
bool gr2ShutterPhaseSucceeded(const char* responseBody,
                             const char* phaseName,
                             std::string& errorMessage);
