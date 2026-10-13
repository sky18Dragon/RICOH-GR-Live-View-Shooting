#pragma once

#include <Arduino.h>

#include <cstddef>
#include <cstdint>

#include "../app/AppState.h"
#include "../services/PreviewFrameBuffer.h"

namespace rvf {

// Brand-defined model id. The registry (multi-brand plan stage 3) will give
// these brand-neutral names; until then the Ricoh driver uses
// RicohProtocolGeneration values.
using CameraModelId = uint8_t;

enum class PasskeyPollAction : uint8_t {
    Start,
    Poll,
    Cancel,
};

// Outcome of drawing one preview frame. `attempted` is true once the decoder
// ran, whether or not it succeeded, so the driver can account render time.
struct PreviewRenderResult {
    bool attempted = false;
    uint32_t decodeMs = 0;
    uint32_t renderMs = 0;
    int width = 0;
    int height = 0;
};

// What the camera driver needs from the device it runs on: screen, buttons
// and model selection. The driver never touches display or input directly.
class ICameraHost {
public:
    virtual ~ICameraHost() = default;

    virtual void showStatus(const String& line1,
                            const String& line2,
                            const String& line3,
                            const String& line4,
                            bool force) = 0;
    virtual void refreshUi() = 0;

    // Keeps input and UI alive while the driver blocks. Returns true when the
    // user asked to reset pairing.
    virtual bool pollInputDuringBlockingOperation() = 0;
    // Returns the entered passkey, -1 while pending or -2 when aborted.
    // `resetRequested` is set when the abort was a pairing-reset request.
    virtual int32_t pollPasskeyEntry(PasskeyPollAction action, bool& resetRequested) = 0;

    virtual void requestModelSelection(CameraModelId defaultModel) = 0;
    virtual void onShutterStarted() = 0;
    virtual void onShutterResult(bool success) = 0;
    virtual void onManualWake() = 0;
    virtual void onPairingResetStarted() = 0;
    virtual void releaseLiveViewLock() = 0;

    virtual PreviewRenderResult renderPreviewFrame(const uint8_t* data, size_t len) = 0;
};

// Snapshot of driver state for the UI and the supervisor. Pointers stay valid
// until the next driver call.
struct CameraStatus {
    AppState appState = AppState::Booting;
    bool linkHealthy = false;
    bool wifiConnected = false;
    bool previewRunning = false;
    bool shutterReady = false;
    bool sleepGuardActive = false;
    bool resettingPairing = false;
    bool provisioningActive = false;
    bool provisioningPreparing = false;
    bool liveviewEnabled = true;
    bool recoveryInProgress = false;
    int32_t rssi = 0;
    uint32_t droppedFrames = 0;
    uint32_t lastFrameAt = 0;
    uint32_t lastLiveViewActivityAt = 0;
    uint32_t liveViewStallTimeoutMs = 0;
    const char* cameraModel = nullptr;
    const char* provisioningSsid = nullptr;
    const char* provisioningPassword = nullptr;
    const char* provisioningUrl = nullptr;
};

// One camera brand/model family. The app expresses intent (preview wanted,
// shoot, reset pairing); the driver owns the whole connection workflow.
class ICameraDriver {
public:
    virtual ~ICameraDriver() = default;

    // Loads the stored binding. Calls host.requestModelSelection() when no
    // camera is configured yet.
    virtual void begin(ICameraHost& host) = 0;
    virtual bool attachPreviewBuffer(uint8_t* buffer,
                                     size_t capacity,
                                     PreviewFrameBufferStorage storage) = 0;
    virtual void runStartupFlow(bool modelSelectionActive) = 0;
    virtual void onModelSelectionShown() = 0;

    // Per-tick work, in this order: beginTick() returns whether the app should
    // handle buttons this tick; then serviceFlowTick(), then
    // serviceBackgroundTick().
    virtual bool beginTick(uint32_t nowMs) = 0;
    virtual bool autoPowerOffDue() const = 0;
    virtual void serviceFlowTick() = 0;
    virtual void serviceBackgroundTick() = 0;

    virtual void setUiPreviewIntent(bool landscape, bool modelSelectionActive) = 0;
    virtual bool confirmModel(CameraModelId model) = 0;
    virtual void resetPairing() = 0;
    virtual void shoot() = 0;
    virtual void shutdown() = 0;

    // Wi-Fi provisioning portal for cameras without BLE.
    virtual bool provisioningActive() const = 0;
    virtual void serviceProvisioningPortal() = 0;
    virtual void stopProvisioning() = 0;
    // Returns true when the input was consumed and the tick should end.
    virtual bool handleProvisioningInput(bool resetRequested) = 0;

    virtual bool previewRunning() const = 0;
    virtual bool previewActive() const = 0;
    virtual CameraStatus status() const = 0;
};

}  // namespace rvf
