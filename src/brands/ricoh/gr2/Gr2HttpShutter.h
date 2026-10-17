#pragma once

#include <cstdint>

class GrApi;

// GR II follows the official GR Remote sequence: try the one-shot endpoint
// with camera autofocus, then fall back to start + finish when the camera
// reports the legacy precondition response.
bool shootGr2(GrApi& api, uint32_t timeoutMs);
