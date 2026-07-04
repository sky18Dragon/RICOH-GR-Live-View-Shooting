#pragma once

#include "../buttons.h"
#include "UserCommand.h"

namespace rvf {

class ButtonInput {
public:
    UserCommand poll();

    static UserCommand commandFromEvents(const ButtonEvents& events);
};

}  // namespace rvf
