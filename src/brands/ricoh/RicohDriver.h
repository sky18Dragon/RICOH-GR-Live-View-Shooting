#pragma once

#include "../../camera/ICameraDriver.h"
#include "../../camera_profile_store.h"

namespace rvf {

// RICOH GR II / III / IV driver: BLE wake + camera AP + HTTP MJPEG preview.
// State lives in RicohDriver.cpp; only one instance may exist.
class RicohDriver : public ICameraDriver {
public:
    // The store is shared with the app, which keeps its display setting there.
    explicit RicohDriver(CameraProfileStore& profileStore);

    void begin(ICameraHost& host) override;
    bool attachPreviewBuffer(uint8_t* buffer,
                             size_t capacity,
                             PreviewFrameBufferStorage storage) override;
    void runStartupFlow(bool modelSelectionActive) override;
    void onModelSelectionShown() override;

    bool beginTick(uint32_t nowMs) override;
    bool autoPowerOffDue() const override;
    void serviceFlowTick() override;
    void serviceBackgroundTick() override;

    void setUiPreviewIntent(bool landscape, bool modelSelectionActive) override;
    bool confirmModel(CameraModelId model) override;
    void resetPairing() override;
    void shoot() override;
    void shutdown() override;

    bool provisioningActive() const override;
    void serviceProvisioningPortal() override;
    void stopProvisioning() override;
    bool handleProvisioningInput(bool resetRequested) override;

    bool previewRunning() const override;
    bool previewActive() const override;
    CameraStatus status() const override;
};

}  // namespace rvf
