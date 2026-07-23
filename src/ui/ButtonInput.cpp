#include "ButtonInput.h"

namespace rvf {

UserCommand ButtonInput::poll() {
    return UserCommand::None;
}

UserCommand ButtonInput::commandFromEvents(const ButtonEvents& events) {
    if (events.powerOff) {
        return UserCommand::PowerOff;
    }
    if (events.resetPairing) {
        return UserCommand::ResetPairing;
    }
    if (events.toggleDisplayMirror) {
        return UserCommand::ToggleDisplayMirror;
    }
    if (events.toggleDisplayRotation) {
        return UserCommand::ToggleDisplayRotation;
    }
    if (events.buttonA) {
        return UserCommand::Shoot;
    }
    return UserCommand::None;
}

}  // namespace rvf
