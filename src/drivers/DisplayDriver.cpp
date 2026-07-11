#include "DisplayDriver.h"

namespace rvf {

bool DisplayDriver::begin() {
    // Reserved adapter point for boards that do not use M5DisplaySurface.
    return true;
}

}  // namespace rvf
