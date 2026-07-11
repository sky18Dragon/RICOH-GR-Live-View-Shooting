#pragma once

#include "AppFlowActions.h"
#include "RecoveryCause.h"
#include "AppState.h"
#include "../core/AppEventSink.h"
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
};

class AppController {
public:
    explicit AppController(AppState initialState = AppState::Booting) : _state(initialState) {}

    void begin(AppState initialState = AppState::BleScan);
    void setEventSink(AppEventSink sink);
    AppTickPlan planTick(uint32_t nowMs) const;
    bool runCameraFlowOnce(const AppFlowActions& actions, uint32_t nowMs);
    bool resumeFromBleReady(const AppFlowActions& actions, const char* reason);
    bool connectWifiAfterBleReady(const AppFlowActions& actions);
    bool httpProbeCamera(const AppFlowActions& actions);
    bool startLiveViewFromProbe(const AppFlowActions& actions);
    void recoverCameraConnection(const AppFlowActions& actions, RecoveryCause cause);
    void serviceCameraFlowIfNeeded(const AppFlowActions& actions, uint32_t nowMs);
    void monitorWifi(const AppFlowActions& actions);
    void monitorLiveView(const AppFlowActions& actions, uint32_t nowMs);
    void handleUserCommand(const AppFlowActions& actions, UserCommand command);
    void triggerShutterFromButtonA(const AppFlowActions& actions);
    void dispatch(const AppMessage& message);
    bool transitionTo(AppState nextState, const char* reason, uint32_t nowMs);
    AppState state() const;
    bool isBleReady() const;
    bool isPowerProtectedFlowState() const;
    bool isPreviewActive() const;
private:
    void publish(AppEventType type, int code, const char* detail, uint32_t nowMs) const;

    AppState _state = AppState::Booting;
    AppEventSink _eventSink;
};

}  // namespace rvf
