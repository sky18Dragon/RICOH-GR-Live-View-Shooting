#pragma once

#include <Arduino.h>

#include "../core/AppMessage.h"
#include "../core/Result.h"
#include "../ricoh_ble_client.h"

namespace rvf {

class BleCameraService {
public:
    BleCameraService() = default;
    explicit BleCameraService(RicohBleClient& client) : _client(&client) {}

    void attach(RicohBleClient& client);
    bool attached() const;

    Result begin();
    Result begin(RicohBleClient& client);

    Result scan();
    bool consumeEvent(AppMessage& message);
    RicohBleDeviceInfo scanCamera(const String& preferredAddress,
                                  const String& preferredName,
                                  uint32_t scanSeconds);
    bool isBonded(const RicohBleDeviceInfo& info);

    Result connect();
    Result connectCamera(const RicohBleDeviceInfo& info, uint32_t timeoutMs);
    Result connectCamera(const RicohBleDeviceInfo& info, const RicohBleConnectOptions& options);

    void disconnect();
    bool isConnected() const;
    bool shutterReady() const;

    Result shoot(bool autofocus = true);
    Result openWifi();
    Result readPowerState(RicohCameraPowerState& state);
    Result readOperationMode(RicohCameraOperationMode& mode);
    Result enablePowerStateNotify();
    bool consumePowerOffNotification();
    Result waitForWifiCredentials(RicohBleWifiCredentials& credentials, uint32_t timeoutMs);

    int consumeDisconnectReason();
    void clearDisconnectReason();
    Result deleteAllBonds();
    void resetStack(bool clearObjects = false);
    bool lastFailureWasResourceExhausted() const;

    String statusText() const;
    String lastError() const;

private:
    Result requireClient(const char* operation) const;
    void publish(AppEventType type, int code = 0, const char* detail = nullptr);
    void publishPowerState(RicohCameraPowerState state);

    RicohBleClient* _client = nullptr;
    AppMessage _pendingEvent;
    bool _hasPendingEvent = false;
};

}  // namespace rvf
