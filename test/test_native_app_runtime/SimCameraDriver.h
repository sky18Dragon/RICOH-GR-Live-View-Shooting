#pragma once

#include <string>
#include <vector>

#include "app/AppRuntime.h"
#include "camera/ICameraDriver.h"

namespace rvf {

// Camera driver stand-in for native tests. Every call is appended to a shared
// call log so tests can assert the exact order of app <-> driver <-> platform
// interaction; return values are plain fields the test sets up front.
class SimCameraDriver : public ICameraDriver {
public:
    explicit SimCameraDriver(std::vector<std::string>& calls) : _calls(calls) {}

    bool beginTickResult = true;
    bool autoPowerOffDueResult = false;
    bool provisioningActiveResult = false;
    bool handleProvisioningInputResult = false;
    bool previewActiveResult = false;
    bool previewRunningResult = false;
    CameraStatus statusResult;

    bool lastPreviewLandscape = false;
    bool lastPreviewModelSelection = false;
    CameraModelId lastConfirmedModel = 0;
    bool lastStartupModelSelection = false;
    bool lastProvisioningReset = false;

    void begin(ICameraHost&) override { record("cam.begin"); }
    bool attachPreviewBuffer(uint8_t*, size_t, PreviewFrameBufferStorage) override {
        record("cam.attachPreviewBuffer");
        return true;
    }
    void runStartupFlow(bool modelSelectionActive) override {
        lastStartupModelSelection = modelSelectionActive;
        record("cam.runStartupFlow");
    }
    void onModelSelectionShown() override { record("cam.onModelSelectionShown"); }

    bool beginTick(uint32_t) override {
        record("cam.beginTick");
        return beginTickResult;
    }
    bool autoPowerOffDue() const override { return autoPowerOffDueResult; }
    void serviceFlowTick() override { record("cam.serviceFlowTick"); }
    void serviceBackgroundTick() override { record("cam.serviceBackgroundTick"); }

    // Called on every UI refresh; recorded as state, not as a call, so call
    // logs stay readable.
    void setUiPreviewIntent(bool landscape, bool modelSelectionActive) override {
        lastPreviewLandscape = landscape;
        lastPreviewModelSelection = modelSelectionActive;
    }
    bool confirmModel(CameraModelId model) override {
        lastConfirmedModel = model;
        record("cam.confirmModel");
        return true;
    }
    void resetPairing() override { record("cam.resetPairing"); }
    void shoot() override { record("cam.shoot"); }
    void shutdown() override { record("cam.shutdown"); }

    bool provisioningActive() const override { return provisioningActiveResult; }
    void serviceProvisioningPortal() override { record("cam.serviceProvisioningPortal"); }
    void stopProvisioning() override { record("cam.stopProvisioning"); }
    bool handleProvisioningInput(bool resetRequested) override {
        lastProvisioningReset = resetRequested;
        record("cam.handleProvisioningInput");
        return handleProvisioningInputResult;
    }

    bool previewRunning() const override { return previewRunningResult; }
    bool previewActive() const override { return previewActiveResult; }
    CameraStatus status() const override { return statusResult; }

private:
    void record(const char* call) { _calls.push_back(call); }

    std::vector<std::string>& _calls;
};

// Platform stand-in: hands out queued button events and records the rest.
class FakeAppPlatform : public IAppPlatform {
public:
    explicit FakeAppPlatform(std::vector<std::string>& calls) : _calls(calls) {}

    ButtonEvents nextButtons;
    bool powerButtonHoldResult = false;
    ButtonEvents lastRefreshInput;
    std::string logText;

    uint32_t nowMs() override { return 1000; }
    ButtonEvents pollButtons() override {
        _calls.push_back("platform.pollButtons");
        const ButtonEvents events = nextButtons;
        nextButtons = ButtonEvents{};
        return events;
    }
    bool pollPowerButtonHold() override {
        _calls.push_back("platform.pollPowerButtonHold");
        return powerButtonHoldResult;
    }
    void shutdown() override { _calls.push_back("platform.shutdown"); }
    void serviceDevicePower() override { _calls.push_back("platform.serviceDevicePower"); }
    void refreshUi() override { _calls.push_back("platform.refreshUi"); }
    void refreshUi(const ButtonEvents& input) override {
        lastRefreshInput = input;
        _calls.push_back("platform.refreshUi(input)");
    }
    void superviseSystem() override { _calls.push_back("platform.superviseSystem"); }
    void toggleDisplayMirror() override { _calls.push_back("platform.toggleDisplayMirror"); }
    void idle() override { _calls.push_back("platform.idle"); }
    void log(const char* text) override { logText += text; }
    void logLine(const char* text) override {
        logText += text;
        logText += "\n";
    }

private:
    std::vector<std::string>& _calls;
};

}  // namespace rvf
