#include "AppRuntime.h"

#include <cstdio>

#include "../ui/UiCoordinator.h"

namespace rvf {

void AppRuntime::startCamera() {
    _camera.runStartupFlow(_pairingGuide.active());
    if (_pairingGuide.active()) {
        _camera.onModelSelectionShown();
        _platform.refreshUi();
    }
}

void AppRuntime::tick() {
    if (_camera.provisioningActive()) {
        serviceProvisioning();
        return;
    }
    if (_camera.beginTick(_platform.nowMs())) {
        handleButtons();
    }
    if (_pairingGuide.active()) {
        _platform.serviceDevicePower();
        _platform.refreshUi();
        _platform.idle();
        return;
    }
    if (_camera.autoPowerOffDue()) {
        _platform.logLine("Power: CameraSleep idle timeout reached");
        _platform.shutdown();
        return;
    }
    _camera.serviceFlowTick();
    _platform.superviseSystem();
    _camera.serviceBackgroundTick();
    _platform.serviceDevicePower();
    _platform.refreshUi();
    _platform.idle();
}

UiOrientation AppRuntime::applyPreviewOrientation(UiOrientation sensed) {
    const UiOrientation orientation =
        UiCoordinator::resolvePreviewOrientation(sensed, _liveViewLocked);
    _camera.setUiPreviewIntent(orientation == UiOrientation::Landscape,
                               _pairingGuide.active());
    return orientation;
}

void AppRuntime::requestModelSelection(CameraModelId defaultModel) {
    _pairingGuide.activate(defaultModel);
}

void AppRuntime::handleButtons() {
    const ButtonEvents events = _platform.pollButtons();

    if (events.powerOff || _platform.pollPowerButtonHold()) {
        _platform.shutdown();
        return;
    }

    if (_pairingGuide.active()) {
        const PairingGuideAction action = _pairingGuide.handle(events);
        // Never forward guide A/B input into the global UI command/animation
        // pipeline. This prevents shutter, mirror, LiveView lock and reset actions.
        if (action == PairingGuideAction::Confirmed) {
            (void)_camera.confirmModel(_pairingGuide.selection());
        } else {
            _platform.refreshUi(ButtonEvents{});
        }
        return;
    }

    const UserCommand command = ButtonInput::commandFromEvents(events);

    if (command == UserCommand::ResetPairing) {
        _liveViewLocked = false;
        _camera.resetPairing();
        return;
    }

    if (command == UserCommand::ToggleDisplayMirror) {
        _platform.toggleDisplayMirror();
    }

    if (command == UserCommand::ToggleLiveViewLock) {
        toggleLiveViewLock();
    }

    if (command == UserCommand::Shoot) {
        _camera.shoot();
    }
    _platform.refreshUi(events);
}

void AppRuntime::serviceProvisioning() {
    _camera.serviceProvisioningPortal();
    const ButtonEvents events = _platform.pollButtons();

    if (events.powerOff || _platform.pollPowerButtonHold()) {
        _camera.stopProvisioning();
        _platform.shutdown();
        return;
    }

    if (_camera.handleProvisioningInput(events.resetPairing)) {
        _platform.refreshUi(ButtonEvents{});
        return;
    }

    _platform.serviceDevicePower();
    _platform.refreshUi(events);
    _platform.idle();
}

void AppRuntime::toggleLiveViewLock() {
    // The UI may already have rendered the portrait remote scene one loop before
    // serviceCameraFlowIfNeeded() acts on the orientation change. Keep that
    // one-loop handoff available for a Button B double-click while the underlying
    // LiveView session is still active.
    if (!_liveViewLocked && !_camera.previewActive()) {
        _platform.logLine("LiveView lock: ignored outside LiveView");
        return;
    }
    _liveViewLocked = !_liveViewLocked;
    char line[40];
    std::snprintf(line, sizeof(line), "LiveView lock: enabled=%d\n", _liveViewLocked ? 1 : 0);
    _platform.log(line);
}

}  // namespace rvf
