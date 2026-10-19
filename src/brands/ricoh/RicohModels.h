#pragma once

#include <cstddef>

#include "../../camera/CameraModel.h"
#include "../../camera_protocol_profile.h"
#include "RicohModelSelection.h"

namespace rvf {

// Pairing-guide order. Only models compiled into this firmware are listed.
inline constexpr CameraModelDescriptor kRicohModels[] = {
#if RVF_RICOH_GR2
    {static_cast<CameraModelId>(RicohProtocolGeneration::Gr2Family), "GR II", "Enable camera WiFi first"},
#endif
#if RVF_RICOH_GR3
    {static_cast<CameraModelId>(RicohProtocolGeneration::Gr3Family), "GR III", "Power on camera first"},
#endif
#if RVF_RICOH_GR4
    {static_cast<CameraModelId>(RicohProtocolGeneration::Gr4Family), "GR IV", "Power on camera first"},
#endif
};
inline constexpr size_t kRicohModelCount = sizeof(kRicohModels) / sizeof(kRicohModels[0]);

// GR III, the most common pairing target, when present (it follows GR II in
// the list above); else the first model.
#if RVF_RICOH_GR3
inline constexpr size_t kRicohDefaultModelIndex = RVF_RICOH_GR2 ? 1 : 0;
#else
inline constexpr size_t kRicohDefaultModelIndex = 0;
#endif

}  // namespace rvf
