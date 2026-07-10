#pragma once

#include <stddef.h>

namespace rvf {

class CameraDiscoveryRegistry {
public:
    static size_t serviceUuidCount();
    static const char* serviceUuidAt(size_t index);
};

}  // namespace rvf
