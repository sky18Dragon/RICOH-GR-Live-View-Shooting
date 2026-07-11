#pragma once

#define UI_VARIANT_RICOH 1
#define UI_VARIANT_MINIMAL 2
#define UI_VARIANT_DEBUG 3
#define UI_VARIANT_KAWAII 4

#ifndef UI_VARIANT
#define UI_VARIANT UI_VARIANT_RICOH
#endif

#if UI_VARIANT == UI_VARIANT_RICOH

#include "../variants/ricoh/RicohUiRenderer.h"

namespace rvf::ui {
using ActiveUiRenderer = RicohUiRenderer;
}

#elif UI_VARIANT == UI_VARIANT_MINIMAL

#include "../variants/minimal/MinimalUiRenderer.h"

namespace rvf::ui {
using ActiveUiRenderer = MinimalUiRenderer;
}

#elif UI_VARIANT == UI_VARIANT_DEBUG

#include "../variants/debug/DebugUiRenderer.h"

namespace rvf::ui {
using ActiveUiRenderer = DebugUiRenderer;
}

#elif UI_VARIANT == UI_VARIANT_KAWAII

#include "../variants/kawaii/KawaiiUiRenderer.h"

namespace rvf::ui {
using ActiveUiRenderer = KawaiiUiRenderer;
}

#else
#error "Unsupported UI_VARIANT"
#endif
