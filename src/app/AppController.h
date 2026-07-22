#pragma once

#include "AppFlowActions.h"
#include "AppState.h"
#include "../core/AppMessage.h"
#include "../ui/UserCommand.h"

namespace rvf {

struct AppTickPlan {
    bool handleButtons = true;
    bool serviceCameraFlow = true;
    bool monitorWifi = false;
    bool refreshProps = false;
    bool monitorLiveView = false;
    bool refreshWifiCache = true;
    bool updateStatusUi = true;
};

class AppController {
public:
    explicit AppController(AppState initialState = AppState::Booting) : _state(initialState) {}

    void begin(AppState initialState = AppState::BleScan);
    AppTickPlan planTick(uint32_t nowMs) const;
    bool runCameraFlowOnce(const AppFlowActions& actions, uint32_t nowMs);
    bool resumeFromBleReady(const AppFlowActions& actions, const char* reason);
    bool resumeFromWifiCredentialsReady(const AppFlowActions& actions);
    bool connectWifiAfterBleReady(const AppFlowActions& actions);
    bool httpProbeCamera(const AppFlowActions& actions);
    bool startLiveViewFromProbe(const AppFlowActions& actions);
    void recoverCameraConnection(const AppFlowActions& actions, const char* reason);
    void serviceCameraFlowIfNeeded(const AppFlowActions& actions, uint32_t nowMs);
    void monitorWifi(const AppFlowActions& actions);
    void monitorLiveView(const AppFlowActions& actions, uint32_t nowMs);
    void handleUserCommand(const AppFlowActions& actions, UserCommand command);
    void triggerShutterFromButtonA(const AppFlowActions& actions);
    void dispatch(const AppMessage& message);
    bool transitionTo(AppState nextState, const char* reason, uint32_t nowMs);
    void setPreviewRequested(bool requested);
    bool previewRequested() const;
    AppState state() const;
    bool isBleReady() const;
    bool isPowerProtectedFlowState() const;
    bool isPreviewActive() const;
private:
    AppState _state = AppState::Booting;
    bool _previewRequested = false;
    bool _previewRequestChanged = false;
};

}  // namespace rvf
