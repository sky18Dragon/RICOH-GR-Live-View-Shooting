#pragma once

#include "AppMessage.h"

namespace rvf {

struct AppEventSink {
    // Publishing is synchronous. Consumers that retain detail must copy it
    // before returning because AppMessage text is non-owning.
    void (*publish)(const AppMessage& message) = nullptr;
};

}  // namespace rvf
