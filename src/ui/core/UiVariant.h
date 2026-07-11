#pragma once

#define UI_VARIANT_RICOH 1
#define UI_VARIANT_MINIMAL 2
#define UI_VARIANT_DEBUG 3

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

#else
#error "Unsupported UI_VARIANT"
#endif
