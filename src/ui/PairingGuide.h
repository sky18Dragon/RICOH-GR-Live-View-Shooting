#pragma once

#include "ButtonInput.h"
#include "../camera_protocol_profile.h"

namespace rvf {

enum class PairingGuideAction : uint8_t {
    None,
    SelectionChanged,
    Confirmed,
};

class PairingGuide {
public:
    void activate(RicohProtocolGeneration selection = RicohProtocolGeneration::Gr3Family) {
        _active = true;
        _selection = selection;
    }

    void dismiss() { _active = false; }

    PairingGuideAction handle(const ButtonEvents& events) {
        if (!_active) return PairingGuideAction::None;

        if (events.buttonADown) {
            _active = false;
            return PairingGuideAction::Confirmed;
        }

        // ButtonInput resolves a B single/double click into these raw events.
        // Inside the guide both mean exactly one model-selection step. A long
        // B press emits resetPairing and is deliberately consumed as no-op.
        if (events.buttonBClicked || events.buttonBDoubleClicked) {
            _selection = _selection == RicohProtocolGeneration::Gr3Family
                             ? RicohProtocolGeneration::Gr4Family
                             : RicohProtocolGeneration::Gr3Family;
            return PairingGuideAction::SelectionChanged;
        }

        return PairingGuideAction::None;
    }

    bool active() const { return _active; }
    RicohProtocolGeneration selection() const { return _selection; }

private:
    bool _active = false;
    RicohProtocolGeneration _selection = RicohProtocolGeneration::Gr3Family;
};

}  // namespace rvf
