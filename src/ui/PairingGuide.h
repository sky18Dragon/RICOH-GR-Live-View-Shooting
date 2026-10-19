#pragma once

#include <cstddef>

#include "ButtonInput.h"
#include "../camera/CameraRegistry.h"

namespace rvf {

enum class PairingGuideAction : uint8_t {
    None,
    SelectionChanged,
    Confirmed,
};

// Model picker shown before pairing; the options come from CameraRegistry.
class PairingGuide {
public:
    void activate() { activate(cameraModelAt(defaultCameraModelIndex()).id); }

    void activate(CameraModelId selection) {
        _active = true;
        _selectedIndex = cameraModelIndex(selection);
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
            _selectedIndex = (_selectedIndex + 1) % cameraModelCount();
            return PairingGuideAction::SelectionChanged;
        }

        return PairingGuideAction::None;
    }

    bool active() const { return _active; }
    size_t selectedIndex() const { return _selectedIndex; }
    CameraModelId selection() const { return cameraModelAt(_selectedIndex).id; }

private:
    bool _active = false;
    size_t _selectedIndex = defaultCameraModelIndex();
};

}  // namespace rvf
