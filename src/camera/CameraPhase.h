#pragma once

#include <cstdint>

namespace rvf {

// Brand-neutral connection phase the app and UI work from. Each driver maps
// its own workflow states onto these.
enum class CameraPhase : uint8_t {
    Booting,
    Searching,   // looking for the camera
    Connecting,  // camera found; links and services coming up
    Ready,       // camera reachable without a preview
    Previewing,  // live preview requested and running
    Sleeping,    // camera asleep or off; waits for the user, no auto wake
    Error,
    Idle,        // none of the above; the UI falls back to the link flags
};

}  // namespace rvf
