#pragma once

#include <cstddef>

#include "../../camera/CameraModel.h"
#include "../../camera_protocol_profile.h"

namespace rvf {

inline constexpr CameraModelDescriptor kRicohModels[] = {
    {static_cast<CameraModelId>(RicohProtocolGeneration::Gr2Family), "GR II", "Enable camera WiFi first"},
    {static_cast<CameraModelId>(RicohProtocolGeneration::Gr3Family), "GR III", "Power on camera first"},
    {static_cast<CameraModelId>(RicohProtocolGeneration::Gr4Family), "GR IV", "Power on camera first"},
};
inline constexpr size_t kRicohModelCount = sizeof(kRicohModels) / sizeof(kRicohModels[0]);
// GR III: the most common pairing target.
inline constexpr size_t kRicohDefaultModelIndex = 1;

}  // namespace rvf
