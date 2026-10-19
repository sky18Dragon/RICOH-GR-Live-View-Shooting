#pragma once

#include <cstddef>

#include "CameraModel.h"

namespace rvf {

// Models compiled into this firmware, in pairing-guide order. Stage 5 of the
// multi-brand plan generates this list per build env.
size_t cameraModelCount();
const CameraModelDescriptor& cameraModelAt(size_t index);
// Preselected when the user is asked to pick a model.
size_t defaultCameraModelIndex();
// Index of `id`, or the default model's index when this firmware lacks it.
size_t cameraModelIndex(CameraModelId id);

}  // namespace rvf
