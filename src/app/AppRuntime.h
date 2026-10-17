#pragma once

#include "../camera/ICameraDriver.h"
#include "../ui/ButtonInput.h"
#include "../ui/PairingGuide.h"
#include "../ui/UiTypes.h"

namespace rvf {

// Device services the app needs: input, screen, power and logging. main.cpp
// implements it on the StickS3; native tests use a recording fake.
class IAppPlatform {
public:
    virtual ~IAppPlatform() = default;

    virtual uint32_t nowMs() = 0;
    virtual ButtonEvents pollButtons() = 0;
    virtual bool pollPowerButtonHold() = 0;
    // Releases the camera and powers the device off; does not return on hardware.
    virtual void shutdown() = 0;
    virtual void serviceDevicePower() = 0;
    // Redraws the UI with the last input, or with `input`.
    virtual void refreshUi() = 0;
    virtual void refreshUi(const ButtonEvents& input) = 0;
    virtual void superviseSystem() = 0;
    virtual void toggleDisplayMirror() = 0;
    // Yields once at the end of a loop pass.
    virtual void idle() = 0;
    virtual void log(const char* text) = 0;      // as-is
    virtual void logLine(const char* text) = 0;  // with a line ending
};

// Brand-neutral app logic: one loop pass, button routing, the model picker,
// the LiveView lock and the preview intent. Owns no hardware.
class AppRuntime {
public:
    AppRuntime(ICameraDriver& camera, IAppPlatform& platform)
        : _camera(camera), _platform(platform) {}

    // After ICameraDriver::begin() and attachPreviewBuffer().
    void startCamera();
    // One Arduino loop() pass.
    void tick();

    // Resolves the orientation the UI renders in and tells the driver whether
    // a preview is wanted. Called on every UI refresh.
    UiOrientation applyPreviewOrientation(UiOrientation sensed);

    void requestModelSelection(CameraModelId defaultModel);
    void releaseLiveViewLock() { _liveViewLocked = false; }
    bool liveViewLocked() const { return _liveViewLocked; }
    const PairingGuide& pairingGuide() const { return _pairingGuide; }

private:
    void handleButtons();
    void serviceProvisioning();
    void toggleLiveViewLock();

    ICameraDriver& _camera;
    IAppPlatform& _platform;
    PairingGuide _pairingGuide;
    bool _liveViewLocked = false;
};

}  // namespace rvf
