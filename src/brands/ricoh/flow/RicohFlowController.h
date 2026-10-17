#pragma once

#include "RicohFlowActions.h"
#include "RicohFlowState.h"
#include "../../../core/AppMessage.h"
#include "../../../ui/UserCommand.h"

namespace rvf {

struct RicohTickPlan {
    bool handleButtons = true;
    bool serviceCameraFlow = true;
    bool monitorWifi = false;
    bool refreshProps = false;
    bool monitorLiveView = false;
    bool refreshWifiCache = true;
    bool updateStatusUi = true;
};

class RicohFlowController {
public:
    explicit RicohFlowController(RicohFlowState initialState = RicohFlowState::Booting) : _state(initialState) {}

    void begin(RicohFlowState initialState = RicohFlowState::BleScan);
    RicohTickPlan planTick(uint32_t nowMs) const;
    bool runCameraFlowOnce(const RicohFlowActions& actions, uint32_t nowMs);
    bool resumeFromBleReady(const RicohFlowActions& actions, const char* reason);
    bool resumeFromWifiCredentialsReady(const RicohFlowActions& actions);
    bool connectWifiAfterBleReady(const RicohFlowActions& actions);
    bool startLiveView(const RicohFlowActions& actions);
    void recoverCameraConnection(const RicohFlowActions& actions, const char* reason);
    void serviceCameraFlowIfNeeded(const RicohFlowActions& actions, uint32_t nowMs);
    void monitorWifi(const RicohFlowActions& actions);
    void monitorLiveView(const RicohFlowActions& actions, uint32_t nowMs);
    void handleUserCommand(const RicohFlowActions& actions, UserCommand command);
    void triggerShutterFromButtonA(const RicohFlowActions& actions);
    void dispatch(const AppMessage& message);
    bool transitionTo(RicohFlowState nextState, const char* reason, uint32_t nowMs);
    void setPreviewRequested(bool requested);
    bool previewRequested() const;
    RicohFlowState state() const;
    bool isBleReady() const;
    bool isPowerProtectedFlowState() const;
    bool isPreviewActive() const;
private:
    RicohFlowState _state = RicohFlowState::Booting;
    bool _previewRequested = false;
    bool _previewRequestChanged = false;
};

}  // namespace rvf
